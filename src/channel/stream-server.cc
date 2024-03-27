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
#include "tll/scheme/merge.h"

#include <sys/fcntl.h>
#include <unistd.h>

using namespace tll;
using namespace tll::channel;

TLL_DEFINE_IMPL(StreamServer);
TLL_DECLARE_IMPL(StreamClient);

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

	auto reader = channel_props_reader(url);
	//auto size = reader.getT<util::Size>("size", 128 * 1024);

	_autoseq.enable = reader.getT("autoseq", false);

	_init_message = reader.getT("init-message", std::string());
	_init_seq = reader.getT<unsigned long>("init-seq", 0);
	_init_block = reader.getT("init-block", std::string(url.sub("blocks") ? "default" : ""));

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
		if (_scheme_url)
			curl->set("scheme", *_scheme_url);

		_storage = context().channel(*curl, master);
		if (!_storage)
			return _log.fail(EINVAL, "Failed to create storage channel");

		curl->remove("scheme");
		_storage_url = *curl;
		_storage_url.set("dir", "r");
		_storage_url.set("name", fmt::format("{}/storage/client", name));
		if (!_storage_url.has("autoclose"))
			_storage_url.set("autoclose", "no");
	}

	if (url.sub("blocks")) {
		auto curl = url.getT<tll::Channel::Url>("blocks");
		if (!curl)
			return _log.fail(EINVAL, "Failed to get blocks url: {}", curl.error());
		child_url_fill(*curl, "blocks");
		curl->set("dir", "w");
		if (_scheme_url)
			curl->set("scheme", *_scheme_url);

		_blocks = context().channel(*curl, master);
		if (!_blocks)
			return _log.fail(EINVAL, "Failed to create blocks channel");

		curl->remove("scheme");
		_blocks_url = *curl;
		_blocks_url.set("dir", "r");
		_blocks_url.set("dump", "frame");
		_blocks_url.set("name", fmt::format("{}/blocks/client", name));
	}

	if (auto s = _child->scheme(TLL_MESSAGE_CONTROL); s)
		_control_child = s;
	if (auto s = _request->scheme(TLL_MESSAGE_CONTROL); s) {
		_control_request = s;
		if (auto m = s->lookup("WriteFull"); m)
			_control_msgid_full = m->msgid;
		if (auto m = s->lookup("WriteReady"); m)
			_control_msgid_ready = m->msgid;
		if (auto m = s->lookup("Disconnect"); m)
			_control_msgid_disconnect = m->msgid;
	}
	if (auto s = _storage->scheme(TLL_MESSAGE_CONTROL); s)
		_control_storage = s;
	if (_blocks) {
		if (auto s = _blocks->scheme(TLL_MESSAGE_CONTROL); s)
			_control_blocks = s;
	}
	auto control = tll::scheme::merge({_control_child, _control_request, _control_storage, _control_blocks});
	if (!control)
		return _log.fail(EINVAL, "Failed to merge control scheme: {}", control.error());
	_scheme_control.reset(*control);

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
	config_info().set_ptr("seq", &_seq);
	_log.info("Last seq in storage: {}", _seq);

	const bool empty_storage = _seq == -1;
	tll_msg_t initial_message = {};
	std::vector<char> initial_buffer;

	if (_init_message.size() && empty_storage) {
		_log.info("Init empty storage with message {} seq {}", _init_message, _init_seq);
		auto scheme = _storage->scheme();
		if (!scheme)
			return _log.fail(EINVAL, "Can not initialize storage without scheme");
		auto message = scheme->lookup(_init_message);
		if (!message)
			return _log.fail(EINVAL, "Message '{}' not found in scheme", _init_message);
		initial_buffer.resize(message->size);
		initial_message.msgid = message->msgid;
		initial_message.seq = _init_seq;
		initial_message.data = initial_buffer.data();
		initial_message.size = initial_buffer.size();
		if (auto r = _storage->post(&initial_message); r)
			return _log.fail(EINVAL, "Failed to post initial message {} to storage", _init_message);
		_seq = _init_seq;
	}

	_autoseq.reset(_seq);

	if (_blocks) {
		if (_blocks->open())
			return _log.fail(EINVAL, "Failed to open blocks channel");
		if (_blocks->state() != tll::state::Active)
			return _log.fail(EINVAL, "Long opening blocks is not supported");

		if (empty_storage && _init_message.size() && _init_block.size()) {
			_log.info("Post initial message to blocks storage");
			if (_blocks->post(&initial_message))
				return _log.fail(EINVAL, "Failed to post initial message to blocks storage");

			if (!_control_blocks)
				return _log.fail(EINVAL, "Blocks storage has no control scheme, can not initialize");
			auto message = _control_blocks->lookup("Block");
			if (!message)
				return _log.fail(EINVAL, "Blocks storage scheme has no Block message");
			std::vector<char> buf;
			buf.resize(message->size);
			auto field = message->lookup("type");
			if (!field)
				return _log.fail(EINVAL, "Block message has no 'type' field");
			if (field->type == field->Bytes) {
				auto view = tll::make_view(buf).view(field->offset);
				if (field->size < _init_block.size())
					return _log.fail(EINVAL, "Block::type size {} is not enough for init-block '{}'", field->size, _init_block);
				memcpy(view.data(), _init_block.data(), _init_block.size());
			} else
				return _log.fail(EINVAL, "Block::type field is not fixed string: {}", field->type);
			tll_msg_t msg = {};
			msg.type = TLL_MESSAGE_CONTROL;
			msg.msgid = message->msgid;
			msg.data = buf.data();
			msg.size = buf.size();

			_log.info("Post initial block {}", _init_block);
			if (_blocks->post(&msg))
				return _log.fail(EINVAL, "Failed to post initial block '{}'", _init_block);
		}

		auto seq = _blocks->config().getT<long long>("info.seq");
		if (!seq)
			return _log.fail(EINVAL, "Blocks channel last seq invalid: {}", seq.error());
		if (*seq != _seq) {
			_log.info("Blocks seq is behind storage seq: {} < {}, feed from storage", *seq, _seq);
			auto url = _storage_url.copy();
			url.set("autoclose", "yes");
			_storage_load = context().channel(url, _storage.get());
			if (!_storage_load)
				return _log.fail(EINVAL, "Failed to create storage channel");
			_storage_load->callback_add([](const tll_channel_t *, const tll_msg_t * msg, void * user) {
					return static_cast<StreamServer *>(user)->_on_storage_load(msg);
				}, this);
			_child_open = tll::Config();
			_child_open.set("seq", conv::to_string(*seq + 1));
			if (auto r = _storage_load->open(_child_open); r)
				return _log.fail(EINVAL, "Failed to open storage channel for reading");
			_child_add(_storage_load.get(), "storage");
			_child_open = url.copy();
			return 0;
		}
	}

	if (_request->open())
		return _log.fail(EINVAL, "Failed to open request channel");

	return Base::_open(url);
}

int StreamServer::_close(bool force)
{
	_storage_load.reset();
	_child_open = tll::Config();

	config_info().setT("seq", _seq);
	_config.remove("client");

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
		if (state() == tll::state::Opening) {
			auto oclient = _child->config().sub("client");
			auto rclient = _request->config().sub("client.init");
			if (oclient && oclient->sub("init") && rclient) {
				auto client = oclient->copy();
				tll::Channel::Url url = *client.sub("init");
				url.proto(std::string("stream+") + url.proto());
				url.set("mode", "client");
				url.set("request", rclient->copy());
				_config.set("client", client);
			}
			state(tll::state::Active);
		}
	} else if (s == tll::state::Closed) {
		_log.info("All sub channels are closed");
		if (state() == tll::state::Closing)
			state(tll::state::Closed);
	}

	return 0;
}

int StreamServer::_on_storage_load(const tll_msg_t *msg)
{
	if (msg->type == TLL_MESSAGE_DATA) {
		if (auto r = _blocks->post(msg); r)
			return state_fail(0, "Failed to forward message with seq {} to blocks channel", msg->seq);
		return 0;
	}
	if (msg->type != TLL_MESSAGE_STATE)
		return 0;

	switch ((tll_state_t) msg->msgid) {
	case tll::state::Closed:
		if (_request->open())
			return _log.fail(0, "Failed to open request channel");
		if (_storage_load)
			_child_del(_storage_load.get());

		return Base::_open(_child_open);
	case tll::state::Error:
		return state_fail(0, "Storage channel failed");
	default:
		break;
	}
	return 0;
}

int StreamServer::_on_request_state(const tll_msg_t *msg)
{
	switch ((tll_state_t) msg->msgid) {
	case tll::state::Active:
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
	if (msg->msgid == stream_scheme::ClientDone::meta_id()) {
		auto it = _clients.find(msg->addr.u64);
		if (it == _clients.end())
			return _log.fail(0, "Client with addr {} not found", msg->addr.u64);

		it->second.state = Client::State::Closed;
		_log.info("Drop client '{}' (addr {})", it->second.name, msg->addr.u64);
		std::string name = std::move(it->second.name);
		it->second.reset();
		_clients.erase(it);
		_request_disconnect(name, msg->addr);
		return 0;
	} else if (msg->msgid != stream_scheme::Request::meta_id())
		return _log.fail(0, "Invalid message from client: {}", msg->msgid);

	auto r = _clients.emplace(msg->addr.u64, this);
	auto & client = r.first->second;
	client.msg = {};
	client.msg.addr = msg->addr;

	if (auto error = client.init(msg); !error) {
		_log.error("Failed to init client '{}' from {}: {}", client.name, msg->addr.u64, error.error());

		std::vector<char> data;
		auto r = stream_scheme::Error::bind(data);
		r.view().resize(r.meta_size());
		r.set_error(error.error());

		client.msg.msgid = r.meta_id();
		client.msg.data = r.view().data();
		client.msg.size = r.view().size();
		if (auto r = _request->post(&client.msg); r)
			_log.error("Failed to post error message");

		std::string name = std::move(client.name);
		client.reset();
		_clients.erase(msg->addr.u64);
		_request_disconnect(name, msg->addr);
		return 0;
	}
	_child_add(client.storage.get());
	return 0;
}

tll::result_t<int> StreamServer::Client::init(const tll_msg_t *msg)
{
	auto & _log = parent->_log;
	state = State::Opening;
	block_end = -1;

	if (msg->msgid != stream_scheme::Request::meta_id())
		return error(fmt::format("Invalid message id: {}, expected {}", msg->msgid, stream_scheme::Request::meta_id()));
	auto req = stream_scheme::Request::bind(*msg);
	if (msg->size < req.meta_size())
		return error(fmt::format("Invalid request size: {} < minimum {}", msg->size, req.meta_size()));

	if (req.get_version() != stream_scheme::Version::Current)
		return error(fmt::format("Invalid client version: {} differs from server {}", (int) req.get_version(), (int) stream_scheme::Version::Current));

	name = req.get_client();
	seq = req.get_seq();
	auto block = req.get_block();
	_log.info("Request from client '{}' (addr {}) for seq {}, block '{}'", name, msg->addr.u64, seq, block);

	if (seq < 0)
		return error(fmt::format("Negative seq: {}", seq));

	if (block.size()) {
		if (!parent->_blocks)
			return error("Requested block, but no block storage configured");
		auto blocks = parent->context().channel(parent->_blocks_url, parent->_blocks.get());
		if (!blocks)
			return error("Failed to create blocks channel");
		blocks->callback_add(on_storage, this);

		tll::Config ocfg;
		ocfg.set("block", tll::conv::to_string(req.get_seq()));
		ocfg.set("block-type", block);

		if (blocks->open(ocfg))
			return error("Failed to open blocks channel");

		if (auto bseq = blocks->config().getT<long long>("info.seq-begin"); !bseq)
			return error(fmt::format("Failed to get block begin seq: {}", bseq.error()));
		else
			seq = *bseq;

		if (auto bseq = blocks->config().getT<long long>("info.seq"); !bseq)
			return error(fmt::format("Failed to get block end seq: {}", bseq.error()));
		else
			block_end = *bseq + 1;

		if (seq == -1) {
			if (block_end == 0) // -1 + 1
				return error(fmt::format("Failed to get block seq values: reported invalid values -1 and -1"));
			_log.info("Block without data, translated seq points to the end {}", block_end);
			seq = block_end;
			blocks->close();
		}

		if (blocks->state() != tll::state::Closed)
			storage_next = std::move(blocks);

		_log.info("Translated block type '{}' number {} to seq {}, storage seq {}", block, req.get_seq(), seq, block_end);
	}

	storage = parent->context().channel(parent->_storage_url, parent->_storage.get());
	if (!storage)
		return error("Failed to create storage channel");
	storage->callback_add(on_storage, this);

	tll::Config cfg;
	cfg.set("seq", conv::to_string(block_end != -1 ? block_end : seq));
	if (storage->open(cfg))
		return error(fmt::format("Failed to open storage from seq {}", seq));

	if (storage_next)
		std::swap(storage_next, storage);

	std::vector<char> data;
	auto r = stream_scheme::Reply::bind(data);
	r.view().resize(r.meta_size());

	r.set_last_seq(parent->_seq);
	r.set_block_seq(block_end);
	r.set_requested_seq(seq);

	this->msg.msgid = r.meta_id();
	this->msg.data = r.view().data();
	this->msg.size = r.view().size();
	if (auto r = parent->_request->post(&this->msg); r)
		return error("Failed to post reply message");
	state = State::Active;
	return 0;
}

void StreamServer::Client::reset()
{
	state = State::Closed;
	storage.reset();
	storage_next.reset();
}

int StreamServer::Client::on_storage(const tll_msg_t * m)
{
	if (m->type != TLL_MESSAGE_DATA) {
		if (m->type == TLL_MESSAGE_STATE)
			return on_storage_state((tll_state_t) m->msgid);
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
	if (state != State::Active)
		return 0;
	switch (s) {
	case TLL_STATE_ACTIVE:
		state = State::Active;
		break;
	case TLL_STATE_ERROR:
		state = State::Error;
		break;
	case TLL_STATE_CLOSING:
		break;
	case TLL_STATE_CLOSED:
		if (storage_next && storage_next->state() == tll::state::Active) {
			parent->_child_del(storage.get());
			std::swap(storage, storage_next); // Can not destroy in callback
			parent->_child_add(storage.get());
			return 0;
		}
		state = State::Closed;
		parent->_log.info("Client '{}' from {} storage closed, schedule disconnect", name, msg.addr.u64);
		parent->_clients_drop.push_back(msg.addr);
		parent->_update_dcaps(dcaps::Process | dcaps::Pending);
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
		if (!msg->msgid)
			return 0;
		if (_control_blocks && _control_blocks->lookup(msg->msgid)) {
			if (auto r = _blocks->post(msg); r)
				return _log.fail(r, "Failed to send control message {} to blocks", msg->msgid);
		}

		if (_control_storage && _control_storage->lookup(msg->msgid)) {
			if (auto r = _storage->post(msg); r)
				return _log.fail(r, "Failed to send control message {} to storage", msg->msgid);
		}
		if (_control_child && _control_child->lookup(msg->msgid)) {
			if (auto r = _child->post(msg); r)
				return _log.fail(r, "Failed to send control message {}", msg->msgid);
		}
		return 0;
	}

	msg = _autoseq.update(msg);
	if (msg->seq <= _seq)
		return _log.fail(EINVAL, "Non monotonic seq: {} < last posted {}", msg->seq, _seq);
	if (_blocks) {
		if (auto r = _blocks->post(msg); r)
			return _log.fail(r, "Failed to post message into block storage");
	}
	if (auto r = _storage->post(msg); r)
		return _log.fail(r, "Failed to store message {}", msg->seq);
	_seq = msg->seq;
	_last_seq_tx(msg->seq);
	return _child->post(msg);
}

int StreamServer::_request_disconnect(std::string_view name, const tll_addr_t &addr)
{
	if (!_control_msgid_disconnect)
		return 0;
	tll_msg_t m = { TLL_MESSAGE_CONTROL };
	m.addr = addr;
	m.msgid = _control_msgid_disconnect;

	_log.info("Disconnect client '{}' (addr {})", name, addr.u64);
	return _request->post(&m);
}

int StreamServer::_process(long timeout, int flags)
{
	for (auto addr : _clients_drop) {
		auto it = _clients.find(addr.u64);
		if (it == _clients.end())
			continue;
		if (it->second.state != Client::State::Closed) {
			_log.debug("Client '{}' from {} not in closing state, do not drop", it->second.name, addr.u64);
			continue;
		}

		std::string name = std::move(it->second.name);
		it->second.reset();
		_clients.erase(it);
		_request_disconnect(name, addr);
	}
	_clients_drop.clear();
	_update_dcaps(0, dcaps::Process | dcaps::Pending);
	return 0;
}
