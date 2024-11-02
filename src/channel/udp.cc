/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/udp.h"

#include "tll/channel/frame.h"
#include "tll/channel/lastseq.h"
#include "tll/channel/udp.h"
#include "tll/util/sockaddr.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#endif

using namespace tll;
using tll::network::AddressFamily;

namespace {
template <typename T> constexpr size_t _sizeof() { return sizeof(T); }
template <> constexpr size_t _sizeof<void>() { return 0; }
} // namespace ''

template <typename T, typename Frame>
class FramedSocket : public tll::channel::udp::Socket<T>
{
 protected:
	using Base = tll::channel::udp::Socket<T>;

	static constexpr size_t frame_size = _sizeof<Frame>();

 public:
	int _on_data(const tll::network::sockaddr_any &from, tll_msg_t &msg);
	int _send(const tll_msg_t *, const tll::network::sockaddr_any &addr);
};

template <typename Frame>
class UdpClient : public tll::channel::LastSeqTx<UdpClient<Frame>, FramedSocket<UdpClient<Frame>, Frame>>
{
	using udp_socket_t = tll::channel::LastSeqTx<UdpClient<Frame>, FramedSocket<UdpClient<Frame>, Frame>>;

	tll::network::hostport _host;

 public:
	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);

	int _post(const tll_msg_t *msg, int flags)
	{
		auto r = udp_socket_t::_send(msg, this->_addr);
		if (r == 0 && msg->type == TLL_MESSAGE_DATA)
			this->_last_seq_tx(msg->seq);
		return r;
	}
};

template <typename Frame>
class UdpServer : public tll::channel::LastSeqRx<UdpServer<Frame>, FramedSocket<UdpServer<Frame>, Frame>>
{
	using udp_socket_t = tll::channel::LastSeqRx<UdpServer<Frame>, FramedSocket<UdpServer<Frame>, Frame>>;

	tll::network::hostport _host;
	bool _unlink_socket = false;

 public:
	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();

	int _post(const tll_msg_t *msg, int flags)
	{
		return udp_socket_t::_send(msg, this->_peer);
	}
};

TLL_DEFINE_IMPL(ChUdp);

#define UDP_DEFINE_IMPL(frame) \
template <> tll::channel_impl<UdpClient<frame>> tll::channel::Base<UdpClient<frame>>::impl = {}; \
template <> tll::channel_impl<UdpServer<frame>> tll::channel::Base<UdpServer<frame>>::impl = {}; \

UDP_DEFINE_IMPL(void);
UDP_DEFINE_IMPL(tll_frame_t);
UDP_DEFINE_IMPL(tll_frame_short_t);
UDP_DEFINE_IMPL(tll_frame_seq32_t);

std::optional<const tll_channel_impl_t *> ChUdp::_init_replace(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	auto client = reader.getT("mode", true, {{"client", true}, {"server", false}});
	auto frame = reader.getT<std::string>("frame", "std");
	if (!reader)
		return _log.fail(std::nullopt, "Invalid url: {}", reader.error());

	if (frame == "none") {
		if (client)
			return &UdpClient<void>::impl;
		else
			return &UdpServer<void>::impl;
	}
	for (auto & n : tll::frame::FrameT<tll_frame_t>::name()) {
		if (n == frame) {
			if (client)
				return &UdpClient<tll_frame_t>::impl;
			else
				return &UdpServer<tll_frame_t>::impl;
		}
	}
	for (auto & n : tll::frame::FrameT<tll_frame_short_t>::name()) {
		if (n == frame) {
			if (client)
				return &UdpClient<tll_frame_short_t>::impl;
			else
				return &UdpServer<tll_frame_short_t>::impl;
		}
	}
	for (auto & n : tll::frame::FrameT<tll_frame_seq32_t>::name()) {
		if (n == frame) {
			if (client)
				return &UdpClient<tll_frame_seq32_t>::impl;
			else
				return &UdpServer<tll_frame_seq32_t>::impl;
		}
	}
	return _log.fail(std::nullopt, "Unknown frame '{}", frame);
}

template <typename T, typename Frame>
int FramedSocket<T, Frame>::_send(const tll_msg_t * msg, const tll::network::sockaddr_any &addr)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	if constexpr (!std::is_same_v<Frame, void>) {
		Frame frame;
		tll::frame::FrameT<Frame>::write(msg, &frame);
		iovec iov[2] = {{&frame, frame_size}, {(void *) msg->data, msg->size}};
		return this->_sendv(msg->seq, iov, 2, addr);
	} else {
		iovec iov[1] = {{(void *) msg->data, msg->size}};
		return this->_sendv(msg->seq, iov, 1, addr);
	}
}

template <typename T, typename Frame>
int FramedSocket<T, Frame>::_on_data(const tll::network::sockaddr_any &from, tll_msg_t &msg)
{
	if (msg.size < frame_size)
		return this->_log.fail(EMSGSIZE, "Packet size {} < frame size {}", msg.size, frame_size);

	auto full = msg.size;
	msg.size -= frame_size;
	if constexpr (!std::is_same_v<Frame, void>) {
		auto frame = (const Frame *) msg.data;
		tll::frame::FrameT<Frame>::read(&msg, frame);
	}
	msg.data = static_cast<const char *>(msg.data) + frame_size;
	if (msg.size > full - frame_size)
		return this->_log.fail(EINVAL, "Data size {} < size in frame {}", full - frame_size, msg.size);

	this->_callback_data(&msg);
	return 0;
}

template <typename F>
int UdpClient<F>::_init(const Channel::Url &url, Channel *master)
{
	auto reader = this->channel_props_reader(url);
	auto af = reader.getT("af", AddressFamily::UNSPEC);
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (udp_socket_t::_init(url, master))
		return this->_log.fail(EINVAL, "Failed to init Udp socket");

	auto host = url.host();
	auto r = network::parse_hostport(host, af);
	if (!r)
		return this->_log.fail(EINVAL, "Invalid host string '{}': {}", host, r.error());
	_host = *r;

	this->_log.debug("Connection to {}:{}", _host.host, _host.port);
	return 0;
}

template <typename F>
int UdpClient<F>::_open(const ConstConfig &url)
{
	auto addr = _host.resolve(SOCK_DGRAM);
	if (!addr)
		return this->_log.fail(EINVAL, "Failed to resolve '{}': {}", _host.host, addr.error());
	this->_addr = addr->front();

	auto fd = socket(this->_addr->sa_family, SOCK_DGRAM, 0);
	if (fd == -1)
		return this->_log.fail(errno, "Failed to create socket: {}", strerror(errno));
	this->_update_fd(fd);

	this->_log.info("Send data to {}", this->_addr);

	return udp_socket_t::_open(url);
}

template <typename F>
int UdpServer<F>::_init(const Channel::Url &url, Channel *master)
{
	auto reader = this->channel_props_reader(url);
	auto af = reader.getT("af", AddressFamily::UNSPEC);
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (udp_socket_t::_init(url, master))
		return this->_log.fail(EINVAL, "Failed to init Udp socket");

	auto host = url.host();
	auto r = network::parse_hostport(host, af);
	if (!r)
		return this->_log.fail(EINVAL, "Invalid host string '{}': {}", host, r.error());
	_host = *r;

	this->_log.debug("Listen on {}:{}", _host.host, _host.port);
	return 0;
}

template <typename F>
int UdpServer<F>::_open(const ConstConfig &url)
{
	using namespace tll::network;
	auto addr = _host.resolve(SOCK_DGRAM);
	if (!addr)
		return this->_log.fail(EINVAL, "Failed to resolve '{}': {}", _host.host, addr.error());
	this->_addr = addr->front();

	auto fd = socket(this->_addr->sa_family, SOCK_DGRAM, 0);
	if (fd == -1)
		return this->_log.fail(errno, "Failed to create socket: {}", strerror(errno));
	this->_update_fd(fd);

	if (this->_multi) {
		if (setsockoptT<int>(this->fd(), SOL_SOCKET, SO_REUSEADDR, 1))
			return this->_log.fail(EINVAL, "Failed to set reuseaddr: {}", strerror(errno));
	}

	this->_log.info("Listen on {}", this->_addr);

	if (bind(this->fd(), this->_addr, this->_addr.size))
		return this->_log.fail(errno, "Failed to bind: {}", strerror(errno));
	_unlink_socket = _host.af == AF_UNIX;

	if (this->_multi) {
		if (this->_nametoindex())
			return this->_log.fail(EINVAL, "Failed to get interface list");
		this->_log.info("Join multicast group {}", this->_addr);
		if (this->_addr->sa_family == AF_INET6) {
			ipv6_mreq mreq = {};
			mreq.ipv6mr_multiaddr = this->_addr.in6()->sin6_addr;
			mreq.ipv6mr_interface = this->_mcast_ifindex;
			if (setsockoptT(this->fd(), IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, mreq))
				return this->_log.fail(EINVAL, "Failed to add multicast membership: {}", strerror(errno));
		} else if (this->_mcast_source) {
			this->_log.info("Join multicast group {} with source {}", this->_addr, *this->_mcast_source);
			ip_mreq_source mreq = {};
			mreq.imr_multiaddr = this->_addr.in()->sin_addr;
			mreq.imr_sourceaddr = *this->_mcast_source;
			if (this->_mcast_ifaddr4)
				mreq.imr_interface = *this->_mcast_ifaddr4;
			if (setsockoptT(this->fd(), IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, mreq))
				return this->_log.fail(EINVAL, "Failed to add source multicast membership: {}", strerror(errno));
		} else {
			ip_mreqn mreq = {};
			mreq.imr_multiaddr = this->_addr.in()->sin_addr;
			mreq.imr_ifindex = this->_mcast_ifindex;
			if (setsockoptT(this->fd(), IPPROTO_IP, IP_ADD_MEMBERSHIP, mreq))
				return this->_log.fail(EINVAL, "Failed to add multicast membership: {}", strerror(errno));
		}
	}

	return udp_socket_t::_open(url);
}

template <typename F>
int UdpServer<F>::_close()
{
	if (_unlink_socket) {
		this->_log.info("Unlink unix socket {}", this->_host.host);
		if (unlink(_host.host.c_str()))
			this->_log.warning("Failed to unlink socket {}: {}", _host.host, strerror(errno));
	}
	_unlink_socket = false;
	return udp_socket_t::_close();
}
