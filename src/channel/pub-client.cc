/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/pub-client.h"
#include "channel/pub-scheme.h"

#include "tll/channel/tcp.hpp"
#include "tll/util/size.h"

using namespace tll;

TLL_DEFINE_IMPL(ChPubClient);

int ChPubClient::_init(const Channel::Url &url, tll::Channel *master)
{
	_size = 0;
	auto r = tcp_client_t::_init(url, master);
	if (r)
		return _log.fail(r, "Tcp socket init failed");

	auto reader = channel_props_reader(url);
	_hello = reader.getT("hello", true);
	_size = reader.getT<util::Size>("size", 128 * 1024);

	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	return 0;
}

int ChPubClient::_open(const PropsView &url)
{
	_state = Closed;
	return tcp_client_t::_open(url);
}

int ChPubClient::_post_hello()
{
	if (!_hello) {
		_log.debug("Hello disabled, connection active");
		_state = Active;
		_dcaps_poll(dcaps::CPOLLIN);
		state(state::Active);
		return 0;
	}

	_log.debug("Sending hello to server");
	tll::pub::client_hello hello = {};
	hello.version = tll::pub::version;
	tll_frame_t frame = { sizeof(hello), tll::pub::client_hello::id, 0 };
	auto r = _sendv(iov_t {(void *) &frame, sizeof(frame)}, iov_t {(void *) &hello, sizeof(hello)});
	if (r < 0)
		return _log.fail(EINVAL, "Failed to send hello to client: {}", strerror(errno));
	if (r != sizeof(hello) + sizeof(frame))
		return _log.fail(EINVAL, "Failed to send hello to client: truncated write");

	_dcaps_poll(dcaps::CPOLLIN);

	return 0;
}

int ChPubClient::_process_open()
{
	_log.debug("Process open");
	auto r = _recv();
	if (!r)
		return _log.fail(EINVAL, "Failed to receive handshake");
	if (*r == 0)
		return EAGAIN;
	if (rsize() < sizeof(tll_frame_t))
		return EAGAIN;

	auto frame = rdataT<tll_frame_t>();
	if (!frame)
		return EAGAIN;
        if (frame->msgid != tll::pub::server_hello::id)
		return _log.fail(EINVAL, "Invalid server hello id: {} (expected {})",
				frame->msgid, tll::pub::server_hello::id);
	if (frame->size < sizeof(tll::pub::server_hello))
		return _log.fail(EMSGSIZE, "Server hello size too small: {}", frame->size);

	auto hello = rdataT<tll::pub::server_hello>(sizeof(*frame), frame->size);
	if (!hello)
		return EAGAIN;
	if (hello->version != tll::pub::version)
		return _log.fail(EINVAL, "Server sent invalid version: {} (expected {})",
				hello->version, tll::pub::version);
	rdone(sizeof(*frame) + frame->size);

	_log.debug("Handshake finished");
	_state = Active;
	state(state::Active);

	return 0;
}

int ChPubClient::_process_pending()
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

int ChPubClient::_process_data()
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

int ChPubClient::_process(long timeout, int flags)
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
