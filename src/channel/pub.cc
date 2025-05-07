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
	auto r = Base::_init(url, master);
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

int ChPubServer::_open(const ConstConfig &cfg)
{
	_ring.clear();
	if (auto r = cfg.getT<long long>("last-seq", -1); !r) {
		return _log.fail(EINVAL, "Invalid 'last-seq' parameter: {}", r.error());
	} else if (*r >= 0) {
		tll_frame_t frame = { 0, 0, (int64_t) *r };
		_last_seq_tx(*r);
		if (_ring.push_back(frame, nullptr, 0) == nullptr)
			return _log.fail(EINVAL, "Failed to push initial message");
	}
	return Base::_open(cfg);
}

int ChPubServer::_close()
{
	Base::_close();
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

	_last_seq_tx(msg->seq);

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
	_iter = {};
	_seq = -1;

	if (_hello) {
		_rbuf.resize(1024);
		_dcaps_poll(dcaps::CPOLLIN);
		return 0;
	} else
		_rbuf.resize(16);

	_dcaps_poll(dcaps::CPOLLOUT | dcaps::CPOLLIN);
	state(state::Active);
	return 0;
}

int ChPubSocket::_close()
{
	_iter = {};
	_seq = -1;
	return tcp_socket_t::_close();
}

int ChPubSocket::_on_active()
{
	_dcaps_poll(dcaps::CPOLLIN);
	state(state::Active);

	_iter = _ring->end();
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
		return EAGAIN;
        if (frame->msgid != pub_scheme::Hello::meta_id())
		return _log.fail(EINVAL, "Invalid client hello id: {} (expected {})",
				frame->msgid, pub_scheme::Hello::meta_id());
	if (frame->size < pub_scheme::Hello::meta_size())
		return _log.fail(EMSGSIZE, "Client hello size too small: {}", frame->size);

	auto full = frame->size + sizeof(*frame);
	if (full > _rbuf.capacity())
		return _log.fail(EMSGSIZE, "Client hello size too large: {}", frame->size);
	if (full > _rbuf.size())
		return EAGAIN;
	auto hello = pub_scheme::Hello::bind(_rbuf, sizeof(*frame));
	if (hello.get_version() != (int) pub_scheme::Version::Current)
		return _log.fail(EINVAL, "Client sent invalid version: {} (expected {})",
				hello.get_version(), (int) pub_scheme::Version::Current);
	_peer = hello.get_name();
	rdone(rsize());
	_rbuf.resize(16);

	{
		_log.debug("Sending hello to client");
		std::array<char, pub_scheme::HelloReply::meta_size()> data;
		auto hello = pub_scheme::HelloReply::bind(data);
		hello.set_version((int) pub_scheme::Version::Current);
		if (!_ring->empty())
			_seq = _ring->back().frame->seq;
		else
			_seq = -1;
		hello.set_seq(_seq);
		tll_frame_t frame = { hello.meta_size(), hello.meta_id(), 0 };
		if (_sendv(tll::memory {(void *) &frame, sizeof(frame)}, data))
			return _log.fail(EINVAL, "Failed to send hello to client");
		if (_wbuf.size())
			return _log.fail(EINVAL, "Failed to send hello to client: truncated write, {} bytes not sent", _wbuf.size());
	}

	_log.info("Handshake finished, client name '{}'", _peer);
	return _on_active();
}

int ChPubSocket::_process_data(bool pollout)
{
	if (_ring->empty())
		return EAGAIN;
	if (_seq != -1 && _seq < _ring->front().frame->seq)
		return state_fail(EINVAL, "Client '{}' out of data: {} < {}", _peer, _seq, _ring->front().frame->seq);
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
			_dcaps_poll(dcaps::CPOLLOUT | dcaps::CPOLLIN);
			return EAGAIN;
		} else if (errno == EPIPE) {
			_log.warning("Send to '{}' failed: {}", _peer, strerror(errno));
			return _on_send_error(EPIPE);
		}
		return _on_send_error(_log.fail(EINVAL, "Send to '{}' failed: {}", _peer, strerror(errno)));
	}

	_log.trace("Sent {} bytes to client", r);
	if (r != size) {
		_ptr += r;
		for (; _ptr >= (const unsigned char *) _iter->end(); _iter++)
			_seq = _iter->frame->seq;
		_dcaps_poll(dcaps::CPOLLOUT | dcaps::CPOLLIN);
		return 0;
	}

	_iter = i;
	_seq = i->frame->seq;
	_ptr = nullptr;
	if (++_iter != _ring->end())
		return _process_data();

	_dcaps_poll(dcaps::CPOLLIN);
	return 0;
}

int ChPubSocket::_process(long timeout, int flags)
{
	if (state() == state::Opening)
		return _process_open();
	auto r = _process_data(true);
	if (r == EAGAIN) {
		// Check for connection close
		_recv(4);
		if (rsize())
			return _log.fail(EINVAL, "Got unexpected data from client '{}', disconnect him", _peer);
	}
	return r;
}
