/*
 * Copyright (c) 2022 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/stream-server.h"

#include "channel/stream-client.h"
#include "channel/stream-scheme.h"
#include "channel/stream-control-server.h"
#include "channel/blocks.scheme.h"

#include "tll/util/size.h"

#include <sys/fcntl.h>
#include <unistd.h>

using namespace tll;
using namespace tll::channel;

TLL_DEFINE_IMPL(StreamServer);
TLL_DECLARE_IMPL(StreamClient);

namespace {
const tll::Scheme * merge(tll::channel::Context ctx, const tll::Scheme * client)
{
	if (!client)
		return ctx.scheme_load(stream_server_control_scheme::scheme_string);
	auto s = tll_scheme_dump(client, "yamls+gz");
	auto merged = fmt::format("yamls://[{{name: '', import: ['{}', '{}']}}]", s, stream_server_control_scheme::scheme_string);
	free(s);
	return ctx.scheme_load(merged);
}
}

std::optional<const tll_channel_impl_t *> StreamServer::_init_replace(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	auto client = reader.getT("mode", true, {{"client", true}, {"server", false}});
	if (!reader)
		return _log.fail(std::nullopt, "Invalid url: {}", reader.error());

	if (client)
		return &StreamClient::impl;
	else
		return nullptr;
}

int StreamServer::_init(const Channel::Url &url, tll::Channel *master)
{
	auto r = Base::_init(url, master);
	if (r)
		return _log.fail(r, "Base channel init failed");

	_scheme_control.reset(merge(context(), _child->scheme(TLL_MESSAGE_CONTROL)));
	if (!_scheme_control.get())
		return _log.fail(EINVAL, "Failed to load control scheme");

	auto reader = channel_props_reader(url);
	//auto size = reader.getT<util::Size>("size", 128 * 1024);

	_autoseq.enable = reader.getT("autoseq", false);

	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	//if (_blocks_filename.empty())
	//	return _log.fail(EINVAL, "Need non-empty filename for data blocks");

	{
		auto curl = url.getT<tll::Channel::Url>("request");
		if (!curl)
			return _log.fail(EINVAL, "Failed to get request url: {}", curl.error());
		child_url_fill(*curl, "request");
		if (!curl->has("mode"))
			curl->set("mode", "server");

		_request = context().channel(*curl, master);
		if (!_request)
			return _log.fail(EINVAL, "Failed to create request channel");
	}

	{
		auto curl = url.getT<tll::Channel::Url>("storage");
		if (!curl)
			return _log.fail(EINVAL, "Failed to get storage url: {}", curl.error());
		child_url_fill(*curl, "storage");
		curl->set("dir", "w");

		_storage = context().channel(*curl, master);
		if (!_storage)
			return _log.fail(EINVAL, "Failed to create storage channel");
		_storage_url = *curl;
		_storage_url.set("dir", "r");
		_storage_url.set("name", fmt::format("{}/storage/client", name));
	}

	if (url.sub("blocks")) {
		auto curl = url.getT<tll::Channel::Url>("blocks");
		if (!curl)
			return _log.fail(EINVAL, "Failed to get blocks url: {}", curl.error());
		child_url_fill(*curl, "blocks");
		curl->set("dir", "w");

		_blocks = context().channel(*curl, master);
		if (!_blocks)
			return _log.fail(EINVAL, "Failed to create blocks channel");
		_blocks_url = *curl;
		_blocks_url.set("dir", "r");
		_blocks_url.set("dump", "frame");
		_blocks_url.set("name", fmt::format("{}/blocks/client", name));
	}

	_request->callback_add(_on_request, this, TLL_MESSAGE_MASK_ALL);
	_child_add(_request.get(), "request");

	return 0;
}

int StreamServer::_open(const ConstConfig &url)
{
	_seq = -1;

	Config sopen;
	if (auto sub = url.sub("storage"); sub)
		sopen = sub->copy();
	if (_storage->open(sopen))
		return _log.fail(EINVAL, "Failed to open storage channel");
	if (_storage->state() != tll::state::Active)
		return _log.fail(EINVAL, "Long opening storage is not supported");

	auto last = _storage->config().getT<long long>("info.seq");
	if (!last)
		return _log.fail(EINVAL, "Storage has invalid 'seq' config value: {}", last.error());
	_seq = *last;
	_autoseq.reset(_seq);
	config_info().set_ptr("seq", &_seq);
	_log.info("Last seq in storage: {}", _seq);

	if (_blocks) {
		if (_blocks->open())
			return _log.fail(EINVAL, "Failed to open blocks channel");
		if (_blocks->state() != tll::state::Active)
			return _log.fail(EINVAL, "Long opening blocks is not supported");
	}

	if (_request->open())
		return _log.fail(EINVAL, "Failed to open request channel");

	return Base::_open(url);
}

int StreamServer::_close(bool force)
{
	config_info().setT("seq", _seq);

	for (auto & [_, p] : _clients) {
		p.reset();
	}
	_clients.clear();

	if (_request->state() != tll::state::Closed)
		_request->close(force);
	if (_blocks) {
		if (_blocks->state() != tll::state::Closed)
			_blocks->close(force);
	}
	if (_storage->state() != tll::state::Closed)
		_storage->close(force);
	return Base::_close(force);
}

int StreamServer::_check_state(tll_state_t s)
{
	if (_request->state() != s)
		return 0;
	if (_storage->state() != s)
		return 0;
	if (_child->state() != s)
		return 0;
	if (s == tll::state::Active) {
		_log.info("All sub channels are active");
		if (state() == tll::state::Opening)
			state(tll::state::Active);
	} else if (s == tll::state::Closed) {
		_log.info("All sub channels are closed");
		if (state() == tll::state::Closing)
			state(tll::state::Closed);
	}

	return 0;
}

int StreamServer::_on_request_state(const tll_msg_t *msg)
{
	switch ((tll_state_t) msg->msgid) {
	case tll::state::Active:
		if (auto s = _request->scheme(TLL_MESSAGE_CONTROL); s) {
			if (auto m = s->lookup("WriteFull"); m)
				_control_msgid_full = m->msgid;
			if (auto m = s->lookup("WriteReady"); m)
				_control_msgid_ready = m->msgid;
			if (auto m = s->lookup("Disconnect"); m)
				_control_msgid_disconnect = m->msgid;
		}
		return _check_state(tll::state::Active);
	case tll::state::Error:
		return state_fail(0, "Request channel failed");
	case tll::state::Closing:
		if (state() != tll::state::Closing) {
			_log.info("Request channel is closing");
			close();
		}
		break;
	case tll::state::Closed:
		return _check_state(tll::state::Closed);
	default:
		break;
	}
	return 0;
}

int StreamServer::_on_request_control(const tll_msg_t *msg)
{
	//_log.debug("Got control message {} from request", msg->msgid);
	auto it = _clients.find(msg->addr.u64);
	if (it == _clients.end())
		return 0;
	if (msg->msgid == _control_msgid_disconnect) {
		_log.info("Client {} disconnected", it->second.name);
		it->second.reset();
		_clients.erase(it);
	} else if (msg->msgid == _control_msgid_full) {
		_log.debug("Suspend storage channel");
		it->second.storage->suspend();
	} else if (msg->msgid == _control_msgid_ready) {
		_log.debug("Resume storage channel");
		it->second.storage->resume();
	}
	return 0;
}

int StreamServer::_on_request_data(const tll_msg_t *msg)
{
	auto r = _clients.emplace(msg->addr.u64, this);
	auto & client = r.first->second;

	if (auto error = client.init(msg); !error) {
		_log.error("Failed to init client '{}' from {}: {}", client.name, msg->addr.u64, error.error());

		std::vector<char> data;
		auto r = stream_scheme::Error::bind(data);
		r.view().resize(r.meta_size());
		r.set_error(error.error());

		client.msg.msgid = r.meta_id();
		client.msg.data = r.view().data();
		client.msg.size = r.view().size();
		client.msg.addr = msg->addr;
		if (auto r = _request->post(&client.msg); r)
			_log.error("Failed to post error message");

		if (_control_msgid_disconnect) {
			tll_msg_t m = { TLL_MESSAGE_CONTROL };
			m.addr = msg->addr;
			m.msgid = _control_msgid_disconnect;
			_log.info("Disconnect client '{}' (addr {})", client.name, msg->addr.u64);
			client.reset();
			_request->post(&m);
		} else
			client.reset();
		_clients.erase(msg->addr.u64); // If request reported Disconnect - iterator is not valid
		return 0;
	}
	_child_add(client.storage.get());
	return 0;
}

tll::result_t<int> StreamServer::Client::init(const tll_msg_t *msg)
{
	auto & _log = parent->_log;

	if (msg->msgid != stream_scheme::Request::meta_id())
		return error(fmt::format("Invalid message id: {}", msg->msgid));
	auto req = stream_scheme::Request::bind(*msg);
	if (msg->size < req.meta_size())
		return error(fmt::format("Invalid request size: {} < minimum {}", msg->size, req.meta_size()));

	name = req.get_client();
	seq = req.get_seq();
	auto block = req.get_block();
	_log.info("Request from client '{}' (addr {}) for seq {}, block '{}'", name, msg->addr.u64, seq, block);

	if (seq < 0)
		return error(fmt::format("Negative seq: {}", seq));

	if (block.size()) {
		if (!parent->_blocks)
			return error("Requested block, but no block storage configured");
		blocks = parent->context().channel(parent->_blocks_url, parent->_blocks.get());
		if (!blocks)
			return error("Failed to create blocks channel");
		blocks->callback_add(on_storage, this);

		tll::Config ocfg;
		ocfg.set("block", tll::conv::to_string(req.get_seq()));
		ocfg.set("block-type", block);

		if (blocks->open(ocfg))
			return error("Failed to open blocks channel");
		if (blocks->state() != tll::state::Closed)
			return error("Blocks with data not yet supported");
		blocks->callback_del(on_storage, this);
		blocks.reset();

		_log.info("Translated block type '{}' number {} to seq {}", block, req.get_seq(), seq);
	}

	this->msg = {};
	this->msg.addr = msg->addr;

	if (!storage) {
		auto r = parent->context().channel(parent->_storage_url, parent->_storage.get());
		if (!r)
			return error("Failed to create storage channel");
		r->callback_add(on_storage, this);
		storage = std::move(r);
	}

	if (storage->state() != state::Closed)
		storage->close(true); // Force close

	state = State::Closed;
	tll::Config cfg;
	cfg.set("seq", conv::to_string(seq));
	if (storage->open(cfg))
		return error(fmt::format("Failed to open storage from seq {}", seq));

	std::vector<char> data;
	auto r = stream_scheme::Reply::bind(data);
	r.view().resize(r.meta_size());

	r.set_last_seq(parent->_seq);
	r.set_requested_seq(seq);

	this->msg.msgid = r.meta_id();
	this->msg.data = r.view().data();
	this->msg.size = r.view().size();
	if (auto r = parent->_request->post(&this->msg); r)
		return error("Failed to post reply message");
	return 0;
}

void StreamServer::Client::reset()
{
	if (storage)
		storage->callback_del(on_storage, this);
	storage.reset();
	state = State::Closed;
}

int StreamServer::Client::on_storage(const tll_msg_t * m)
{
	if (m->type != TLL_MESSAGE_DATA) {
		if (m->type == TLL_MESSAGE_STATE)
			return on_storage_state((tll_state_t) m->msgid);
		else if (m->type != TLL_MESSAGE_CONTROL)
			return 0;
		if (!blocks)
			return 0;
		if (m->msgid != blocks_scheme::BlockRange::meta_id())
			return 0;
		auto data = blocks_scheme::BlockRange::bind(*m);
		seq = data.get_begin();
		block_end = data.get_end();
		parent->_log.info("Got BlockRange {}:{} from blocks", seq, block_end);
		return 0;
	}
	msg.type = m->type;
	msg.msgid = m->msgid;
	msg.seq = m->seq;
	msg.flags = m->flags;
	msg.data = m->data;
	msg.size = m->size;
	if (auto r = parent->_request->post(&msg, 0); r) {
		parent->_log.error("Failed to post data for client '{}': seq {}", name, msg.seq);
		state = State::Error;
		storage->close();
		return 0;
	}
	return 0;
}

int StreamServer::Client::on_storage_state(tll_state_t s)
{
	switch (s) {
	case TLL_STATE_ACTIVE:
		state = State::Active;
		break;
	case TLL_STATE_ERROR:
		state = State::Error;
		break;
	case TLL_STATE_CLOSING:
	case TLL_STATE_CLOSED:
		state = State::Closed;
		break;
	case TLL_STATE_OPENING:
	case TLL_STATE_DESTROY:
		break;
	}
	return 0;
}

int StreamServer::_post(const tll_msg_t * msg, int flags)
{
	if (msg->type == TLL_MESSAGE_CONTROL) {
		if (_blocks && msg->msgid == stream_server_control_scheme::Block::meta_id()) {
			if (_seq == -1)
				return _log.fail(EINVAL, "No data in storage, can not create block");

			tll_msg_t m = *msg;
			m.seq = _seq;

			if (auto r = _blocks->post(&m); r)
				return _log.fail(r, "Failed to send Block control message");
			return 0;
		}

		if (auto r = _storage->post(msg); r)
			return _log.fail(r, "Failed to send control message {}", msg->msgid);
		return _child->post(msg);
	}

	msg = _autoseq.update(msg);
	if (msg->seq <= _seq)
		return _log.fail(EINVAL, "Non monotonic seq: {} < last posted {}", msg->seq, _seq);
	if (_blocks) {
		if (auto r = _blocks->post(msg); r)
			return _log.fail(r, "Failed to send Block control message");
	}
	if (auto r = _storage->post(msg); r)
		return _log.fail(r, "Failed to store message {}", msg->seq);
	_seq = msg->seq;
	_last_seq_tx(msg->seq);
	return _child->post(msg);
}

/*
int StreamServer::_process_pending()
{
	_log.debug("Pending data: {}", rsize());
	auto frame = rdataT<tll_frame_t>();
	if (!frame)
		return EAGAIN;
	auto data = rdataT<void>(sizeof(*frame), frame->size);
	if (!data)
		return EAGAIN;

	tll_msg_t msg = { TLL_MESSAGE_DATA };
	msg.msgid = frame->msgid;
	msg.seq = frame->seq;
	msg.data = data;
	msg.size = frame->size;
	_callback_data(&msg);
	rdone(sizeof(*frame) + frame->size);
	return 0;
}

int StreamServer::_process_data()
{
	if (_process_pending() != EAGAIN)
		return 0;

	rshift();

	_log.debug("Fetch data");
	auto r = _recv();
	if (!r)
		return _log.fail(EINVAL, "Failed to receive data");
	if (*r == 0)
		return EAGAIN;
	if (_process_pending() == EAGAIN)
		return EAGAIN;
	return 0;
}

int StreamServer::_process(long timeout, int flags)
{
	if (state() == state::Opening) {
		if (_state == Closed) {
			auto r = _process_connect();
			if (r == 0)
				_state = Connected;
			return r;
		}
		return _process_open();
	}

	auto r = _process_data();
	if (r == EAGAIN)
		_dcaps_pending(false);
	else if (r == 0)
		_dcaps_pending(rsize() != 0);
	return r;
}
*/
