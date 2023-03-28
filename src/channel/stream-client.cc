/*
 * Copyright (c) 2022 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/stream-client.h"
#include "channel/stream-control.h"
#include "channel/stream-scheme.h"

#include "tll/util/size.h"

using namespace tll;
using namespace tll::channel;

TLL_DEFINE_IMPL(StreamClient);

int StreamClient::_init(const Channel::Url &url, tll::Channel *master)
{
	auto r = Base::_init(url, master);
	if (r)
		return _log.fail(r, "Base channel init failed");

	auto reader = channel_props_reader(url);
	auto size = reader.getT<util::Size>("size", 128 * 1024);
	_peer = reader.getT<std::string>("peer", "");

	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	_log.debug("Data buffer size: {}, messages {}", size, size / 64);
	_ring.resize(size / 64);
	_ring.data_resize(size);

	auto curl = url.getT<tll::Channel::Url>("request");
	if (!curl)
		return _log.fail(EINVAL, "Failed to get request url: {}", curl.error());
	child_url_fill(*curl, "request");

	_request = context().channel(*curl, master);
	if (!_request)
		return _log.fail(EINVAL, "Failed to create request channel");
	_request->callback_add(_on_request_state, this, TLL_MESSAGE_MASK_STATE);
	_request->callback_add(_on_request_data, this, TLL_MESSAGE_MASK_DATA);
	_child_add(_request.get(), "request");

	_scheme_control.reset(context().scheme_load(stream_control_scheme::scheme_string));
	if (!_scheme_control.get())
		return _log.fail(EINVAL, "Failed to load control scheme");

	return 0;
}

int StreamClient::_open(const ConstConfig &url)
{
	_state = State::Closed;
	_ring.clear();
	_open_seq = {};
	_seq = _server_seq = -1;

	auto reader = channel_props_reader(url);

	auto r = stream_scheme::Request::bind(_request_buf);
	_request_buf.resize(r.meta_size());
	memset(_request_buf.data(), 0, _request_buf.size());

	if (_peer.size())
		r.set_client(_peer);

	if (url.has("seq")) {
		_open_seq = reader.getT<long long>("seq");
		if (*_open_seq < 0)
			return _log.fail(EINVAL, "Invalid seq parameter: negative value {}", *_open_seq);
		r.set_seq(*_open_seq);
	} else if (url.has("block")) {
		auto block = reader.getT<unsigned>("block");
		auto type = reader.getT<std::string>("block-type", "default");
		r.set_seq(block);
		r.set_block(type);
	} else
		_request_buf.resize(0);

	if (!reader)
		return _log.fail(EINVAL, "Invalid open parameters: {}", reader.error());

	return Base::_open(url);
}

int StreamClient::_close(bool force)
{
	if (_request->state() != tll::state::Closed)
		_request->close(force || state() == tll::state::Error);
	return Base::_close(force || state() == tll::state::Error);
}

int StreamClient::_report_online()
{
	_log.info("Stream is online on seq {}", _seq);
	tll_msg_t msg = { TLL_MESSAGE_CONTROL };
	msg.seq = _seq;
	msg.msgid = stream_control_scheme::Online::meta_id();
	_callback(msg);
	return 0;
}

int StreamClient::_on_active()
{
	if (!_request_buf.size()) {
		_log.debug("Stream channel active, skip request channel in online-only mode");
		_state = State::Online;
		state(tll::state::Active);
		return 0;
	}
	_log.debug("Stream channel active, open request channel from seq {}", *_open_seq);
	return _request->open();
}

int StreamClient::_on_request_error() { return 0; }
int StreamClient::_on_request_active()
{
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	msg.msgid = stream_scheme::Request::meta_id();
	msg.data = _request_buf.data();
	msg.size = _request_buf.size();
	if (auto r = _request->post(&msg); r)
		return state_fail(EINVAL, "Failed to post request message");
	_log.info("Posted request for seq {}, change state to Active", *_open_seq);
	_state = State::Opening;
	return 0;
}

int StreamClient::_on_data(const tll_msg_t *msg)
{
	if (_state == State::Online) {
		_seq = msg->seq;
		return _callback_data(msg);
	}

	tll_frame_t frame = { (uint32_t) msg->size, msg->msgid, (int64_t) msg->seq };
	if (sizeof(frame) + msg->size > _ring.data_capacity() / 2)
		return _log.fail(EMSGSIZE, "Message too large for buffer {}: {}", _ring.data_size(), msg->size);

	do {
		if (_ring.push_back(frame, msg->data, msg->size) != nullptr)
			break;
		_ring.pop_front();
	} while (true);

	return 0;
}

int StreamClient::_on_request_data(const tll_msg_t *msg)
{
	_log.debug("Seq {}, state {}, ring {}", msg->seq, (int) _state, _ring.empty());
	if (_state == State::Connected) {
		_seq = msg->seq;
		_callback_data(msg);
		if (_seq == _server_seq && _ring.empty()) {
			_log.info("Reached reported server seq {}, no online data", _server_seq);
			_state = State::Online;
			_report_online(); // FIXME: Do it through pending, 2 messages from one process
			_request->close();
			return 0;
		}

		if (_ring.empty())
			return 0;
		if (_ring.front().frame->seq > msg->seq)
			return 0;

		while (!_ring.empty() && _ring.front().frame->seq <= msg->seq) {
			_log.debug("Drop seq {} from ring, {} already processed", _ring.front().frame->seq, msg->seq);
			_ring.pop_front();
		}

		if (_ring.data_free() > _ring.data_size() / 2) {
			_log.info("Request stream overlapping with online buffer on seq {}", msg->seq);
			_update_dcaps(dcaps::Process | dcaps::Pending);
			_state = State::Overlapped;
		}
		return 0;
	} else if (_state == State::Opening) {
		if (msg->msgid == stream_scheme::Error::meta_id()) {
			auto data = stream_scheme::Error::bind(*msg);
			if (data.meta_size() > msg->size)
				return state_fail(0, "Invalid Error message size: {} < min {}", msg->size, data.meta_size());
			return state_fail(0, "Server error: {}", data.get_error());
		} else if (msg->msgid != stream_scheme::Reply::meta_id())
			return state_fail(0, "Unknown message from server: {}", msg->msgid);
		auto data = stream_scheme::Reply::bind(*msg);
		if (msg->size < data.meta_size())
			return state_fail(0, "Invalid reply size: {} < minimum {}", msg->size, data.meta_size());
		_server_seq = data.get_last_seq();
		_log.info("Server seq: {}", _server_seq);
		_state = State::Connected;
		state(tll::state::Active);
		if (!_open_seq) {
			_log.info("Translated block request to seq {}", data.get_requested_seq());
			_open_seq = data.get_requested_seq();
		}
		if (_server_seq == -1) {
			return state_fail(0, "Server has no data for now, can not open from seq {}", *_open_seq);
		} else if (_server_seq + 1 == *_open_seq) {
			_log.info("Server has no old data for us, channel is online (seq {})", _server_seq);
			_seq = _server_seq;
			_state = State::Online;
			_report_online();
			_request->close();
		} else if (_server_seq < *_open_seq) {
			return state_fail(0, "Invalid server seq: {} < requested {}", _server_seq, *_open_seq);
		}
		return 0;
	} else if (_state != State::Overlapped)
		return 0;
	if (msg->seq <= _seq) // Message already forwarded to client
		return 0;
	_seq = msg->seq;
	_callback_data(msg);
	return 0;
}

int StreamClient::_process(long timeout, int flags)
{
	if (_state != State::Overlapped || _ring.empty()) {
		_report_online();
		_update_dcaps(0, dcaps::Process | dcaps::Pending);
		return EAGAIN;
	}

	auto data = _ring.front();
	tll_msg_t msg = {};
	msg.msgid = data.frame->msgid;
	msg.seq = data.frame->seq;
	msg.size = data.frame->size;
	msg.data = data.begin();

	_ring.pop_front();

	if (msg.seq <= _seq) // Skip already processed message
		return 0;

	_seq = msg.seq;
	_callback_data(&msg);

	return 0;
}
