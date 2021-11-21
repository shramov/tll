/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/pub.h"
#include "channel/pub-client.h"
#include "channel/pub-scheme.h"

#include "tll/channel/tcp.hpp"
#include "tll/util/size.h"

#include "tll/util/conv-fmt.h"

namespace tll::conv {
template <typename T, bool Const>
struct dump<tll::util::circular_iterator<T, Const>> : public to_string_buf_from_string<tll::util::circular_iterator<T, Const>>
{
	static inline std::string to_string(const tll::util::circular_iterator<T, Const> &v)
	{
		if (Const)
			return fmt::format("const_iterator {{ {} }}", v._idx);
		return fmt::format("iterator {{ {} }}", v._idx);
	}
};
}

using namespace tll;

TLL_DEFINE_IMPL(ChPubServer);
TLL_DEFINE_IMPL(ChPubSocket);
TLL_DEFINE_IMPL(tll::channel::TcpServerSocket<ChPubServer>);
extern template class tll::channel::Base<ChPubClient>;

std::optional<const tll_channel_impl_t *> ChPubServer::_init_replace(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	auto client = reader.getT("mode", true, {{"client", true}, {"server", false}});
	if (!reader)
		return _log.fail(std::nullopt, "Invalid url: {}", reader.error());

	if (client)
		return &ChPubClient::impl;
	else
		return nullptr; //&ChPubServer::impl;
}


int ChPubServer::_init(const Channel::Url &url, tll::Channel *master)
{
	auto r = tcp_server_t::_init(url, master);
	if (r)
		return _log.fail(r, "Tcp server init failed");

	auto reader = channel_props_reader(url);
	_hello = reader.getT("hello", true);
	_size = reader.getT<util::Size>("size", 1024 * 1024);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (_size < 1024)
		return _log.fail(EINVAL, "Buffer size too small: {}", _size);
	_log.debug("Data buffer size: {}, messages {}", _size, _size / 64);
	_ring.data_resize(_size);
	_ring.resize(_size / 64);

	return 0;
}

int ChPubServer::_close()
{
	tcp_server_t::_close();
	return 0;
}

int ChPubServer::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;

	//_log.debug("Free size: {}", _ring.free());
	tll_frame_t frame = { (uint32_t) msg->size, msg->msgid, (int64_t) msg->seq };
	auto full = sizeof(frame) + msg->size;
	if (full > _size / 2)
		return _log.fail(EMSGSIZE, "Message too large for buffer {}: {}", _size, msg->size);

	do {
		if (_ring.push_back(frame, msg->data, msg->size) != nullptr)
			break;
		_ring.pop_front();
	} while (true);

	if (flags & TLL_POST_MORE)
		return 0;

	for (auto & [a, c] : _clients) {
		if (c->state() == state::Active)
			static_cast<ChPubSocket *>(c)->_process_data();
	}
	return 0;
}

int ChPubSocket::_init(const Channel::Url &url, tll::Channel *master)
{
	_size = 0;
	auto r = tcp_socket_t::_init(url, master);
	if (r)
		return _log.fail(r, "Tcp socket init failed");

	if (!master)
		return _log.fail(EINVAL, "Need pub server master");
	auto pub = channel_cast<ChPubServer>(master);
	if (!pub)
		return _log.fail(EINVAL, "Master {} is not pub server", master->name());

	_hello = pub->hello();
	_ring = pub->ring();

	return 0;
}

int ChPubSocket::_open(const ConstConfig &url)
{
	if (_hello) {
		_rbuf.resize(1024);
		_dcaps_poll(dcaps::CPOLLIN);
		return 0;
	}

	_dcaps_poll(dcaps::CPOLLOUT);
	state(state::Active);
	return 0;
}

int ChPubSocket::_close()
{
	_iter = {};
	return tcp_socket_t::_close();
}

int ChPubSocket::_on_active()
{
	_dcaps_poll(0);
	state(state::Active);

	_iter = _ring->end();
	_seq = -1;
	return 0;
}

int ChPubSocket::_process_open()
{
	if (!_hello) {
		_log.debug("Handshake disabled, client active");
		return _on_active();
	}

	_log.debug("Process open");

	auto r = _recv();
	if (!r)
		return _log.fail(EINVAL, "Failed to receive handshake");
	if (*r == 0)
		return EAGAIN;

	auto frame = rdataT<tll_frame_t>();
	if (!frame)
		return _log.fail(EAGAIN, "Not enuf data");
        if (frame->msgid != tll::pub::client_hello::id)
		return _log.fail(EINVAL, "Invalid client hello id: {} (expected {})",
				frame->msgid, tll::pub::client_hello::id);
	if (frame->size < sizeof(tll::pub::client_hello))
		return _log.fail(EMSGSIZE, "Client hello size too small: {}", frame->size);

	auto full = frame->size + sizeof(*frame);
	if (full > rsize())
		return _log.fail(EMSGSIZE, "Client hello size too large: {}", frame->size);
	if (_rsize < full)
		return EAGAIN;
	auto hello = rdataT<tll::pub::client_hello>(sizeof(*frame), frame->size);
	if (!hello)
		return EAGAIN;
	if (hello->version != tll::pub::version)
		return _log.fail(EINVAL, "Client sent invalid version: {} (expected {})",
				hello->version, tll::pub::version);
	_rbuf.resize(0);

	{
		_log.debug("Sending hello to client");
		tll::pub::server_hello hello = {};
		hello.version = tll::pub::version;
		tll_frame_t frame = { sizeof(hello), tll::pub::server_hello::id, 0 };
		auto r = _sendv(iov_t {(void *) &frame, sizeof(frame)}, iov_t {(void *) &hello, sizeof(hello)});
		if (r < 0)
			return _log.fail(EINVAL, "Failed to send hello to client: {}", strerror(errno));
		if (r != sizeof(hello) + sizeof(frame))
			return _log.fail(EINVAL, "Failed to send hello to client: truncated write");
	}

	_log.debug("Handshake finished");
	return _on_active();
}

int ChPubSocket::_process_data(bool pollout)
{
	if (_ring->empty())
		return EAGAIN;
	if (_seq != -1 && _seq < _ring->front().frame->seq)
		return state_fail(EINVAL, "Client out of data: {} < {}", _seq, _ring->front().frame->seq);
	if (_ptr != nullptr && !pollout)
		return EAGAIN;

	if (_iter == _ring->end())
		return EAGAIN;

	if (_ptr == nullptr)
		_ptr = (const unsigned char *) _iter->frame;

	auto i = _iter;
	do {
		auto prev = i++;
		if (i == _ring->end() || (const unsigned char *) i->frame < _ptr) {
			i = prev;
			break;
		}
	} while (true);

	auto size = (const unsigned char *) i->end() - _ptr;
	_log.trace("Data slice: {} +{}", (void *) _ptr, size);

	auto r = send(fd(), _ptr, size, MSG_NOSIGNAL | MSG_DONTWAIT);
	if (r < 0) {
		if (errno == EAGAIN) {
			_dcaps_poll(dcaps::CPOLLOUT);
			return EAGAIN;
		}
		return _log.fail(EINVAL, "Send failed: {}", strerror(errno));
	}

	_log.trace("Sent {} bytes to client", r);
	if (r != size) {
		_ptr += r;
		for (; _ptr >= (const unsigned char *) _iter->end(); _iter++)
			_seq = _iter->frame->seq;
		_dcaps_poll(dcaps::CPOLLOUT);
		return 0;
	}

	_iter = i;
	_seq = i->frame->seq;
	_ptr = nullptr;
	if (++_iter != _ring->end())
		return _process_data();

	_dcaps_poll(0);
	return 0;
}

int ChPubSocket::_process(long timeout, int flags)
{
	if (state() == state::Opening)
		return _process_open();
	return _process_data(true);
}
