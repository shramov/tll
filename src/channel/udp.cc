/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/udp.h"

#include "tll/channel/frame.h"
#include "tll/util/size.h"
#include "tll/util/sockaddr.h"

#include <fcntl.h>
#include <unistd.h>

using namespace tll;

namespace {
template <typename T> constexpr size_t _sizeof() { return sizeof(T); }
template <> constexpr size_t _sizeof<void>() { return 0; }

int nonblock(int fd)
{
	auto f = fcntl(fd, F_GETFL);
	if (f == -1) return errno;
	return fcntl(fd, F_SETFL, f | O_NONBLOCK);
}

template <typename T>
int setsockoptT(int fd, int level, int optname, T v)
{
	return setsockopt(fd, level, optname, &v, sizeof(v));
}
} // namespace ''

template <typename T, typename Frame>
class UdpSocket : public tll::channel::Base<T>
{
 protected:
	static constexpr size_t frame_size = _sizeof<Frame>();
	tll::network::sockaddr_any _addr;
	tll::network::sockaddr_any _peer;
	std::vector<char> _buf;
	size_t _sndbuf = 0;
	size_t _rcvbuf = 0;
	unsigned _ttl = 0;

 public:
	static constexpr std::string_view channel_protocol() { return "udp"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();

	int _process(long timeout, int flags);
	int _send(const tll_msg_t *msg, const tll::network::sockaddr_any &addr);
};

template <typename Frame>
class UdpClient : public UdpSocket<UdpClient<Frame>, Frame>
{
	using udp_socket_t = UdpSocket<UdpClient<Frame>, Frame>;

	int _af = AF_UNSPEC;
	std::string _host;
	unsigned short _port = 0;
 public:
	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::PropsView &);

	int _post(const tll_msg_t *msg, int flags);
};

template <typename Frame>
class UdpServer : public UdpSocket<UdpServer<Frame>, Frame>
{
	using udp_socket_t = UdpSocket<UdpServer<Frame>, Frame>;

	int _af = AF_UNSPEC;
	std::string _host;
	unsigned short _port = 0;
	bool _unlink_socket = false;
 public:
	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();

	int _post(const tll_msg_t *msg, int flags);
};

TLL_DEFINE_IMPL(ChUdp);

TLL_DEFINE_IMPL(UdpClient<void>);
TLL_DEFINE_IMPL(UdpServer<void>);

TLL_DEFINE_IMPL(UdpClient<tll_frame_t>);
TLL_DEFINE_IMPL(UdpServer<tll_frame_t>);

TLL_DEFINE_IMPL(UdpClient<tll_frame_short_t>);
TLL_DEFINE_IMPL(UdpServer<tll_frame_short_t>);

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
	return _log.fail(std::nullopt, "Unknown frame '{}", frame);
}

template <typename T, typename F>
int UdpSocket<T, F>::_init(const Channel::Url &url, Channel *master)
{
	auto reader = this->channel_props_reader(url);
	auto size = reader.getT("size", util::Size {64 * 1024});
	_sndbuf = reader.getT("sndbuf", util::Size {0});
	_rcvbuf = reader.getT("rcvbuf", util::Size {0});
	_ttl = reader.getT("ttl", 0u);
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());
	_buf.resize(size);
	return 0;
}

template <typename T, typename F>
int UdpSocket<T, F>::_open(const PropsView &url)
{
	if (int r = nonblock(this->fd()))
		return this->_log.fail(EINVAL, "Failed to set nonblock: {}", strerror(r));

	if (_sndbuf && setsockoptT<int>(this->fd(), SOL_SOCKET, SO_SNDBUF, _sndbuf))
		return this->_log.fail(EINVAL, "Failed to set sndbuf to {}: {}", _sndbuf, strerror(errno));

	if (_rcvbuf && setsockoptT<int>(this->fd(), SOL_SOCKET, SO_RCVBUF, _rcvbuf))
		return this->_log.fail(EINVAL, "Failed to set rcvbuf to {}: {}", _rcvbuf, strerror(errno));

	if (_ttl && setsockoptT<int>(this->fd(), IPPROTO_IP, IP_TTL, _ttl))
		return this->_log.fail(EINVAL, "Failed to set rcvbuf to {}: {}", _rcvbuf, strerror(errno));

	this->_dcaps_poll(dcaps::CPOLLIN);
	return 0;
}

template <typename T, typename F>
int UdpSocket<T, F>::_close()
{
	auto fd = this->_update_fd(-1);
	if (fd != -1)
		::close(fd);
	return 0;
}

template <typename T, typename Frame>
int UdpSocket<T, Frame>::_send(const tll_msg_t * msg, const tll::network::sockaddr_any &addr)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	this->_log.debug("Post {} + {} bytes of data", frame_size, msg->size);
	auto size = frame_size + msg->size;
	std::conditional_t<std::is_same_v<Frame, void>, char, Frame> frame;
	iovec iov[2] = {{&frame, frame_size}, {(void *) msg->data, msg->size}};
	msghdr m = {};
	m.msg_name = (void *) static_cast<const sockaddr *>(addr);
	m.msg_namelen = addr.size;
	if constexpr (!std::is_same_v<Frame, void>) {
		tll::frame::FrameT<Frame>::write(msg, &frame);
		m.msg_iov = iov;
		m.msg_iovlen = 2;
	} else {
		m.msg_iov = iov + 1;
		m.msg_iovlen = 1;
	}
	auto r = sendmsg(this->fd(), &m, MSG_NOSIGNAL);
	if (r < 0) {
		if (errno == EAGAIN)
			return EAGAIN;
		return this->_log.fail(errno, "Failed to post data: {}", strerror(errno));
	} else if ((size_t) r != size)
		return this->_log.fail(errno, "Failed to post data (truncated): {}", strerror(errno));
	return 0;
}

template <typename T, typename Frame>
int UdpSocket<T, Frame>::_process(long timeout, int flags)
{
	socklen_t peerlen = sizeof(_peer.buf);
	auto r = recvfrom(this->fd(), _buf.data(), _buf.size(), MSG_NOSIGNAL, _peer, &peerlen);
	if (r < 0) {
		if (errno == EAGAIN)
			return EAGAIN;
		return this->_log.fail(EINVAL, "Failed to receive data: {}", strerror(errno));
	}
	if ((size_t) r < frame_size)
		return this->_log.fail(EMSGSIZE, "Packet size {} < frame size {}", r, frame_size);
	_peer.size = peerlen;
	this->_log.debug("Got data from {} {}", _peer, _peer->sa_family);

	tll_msg_t msg = { TLL_MESSAGE_DATA };
	msg.size = r - frame_size;
	msg.data = _buf.data() + frame_size;
	if constexpr (!std::is_same_v<Frame, void>) {
		auto frame = (const Frame *) _buf.data();
		tll::frame::FrameT<Frame>::read(&msg, frame);
	}
	if (msg.size > r - frame_size)
		return this->_log.fail(EINVAL, "Data size {} < size in frame {}", r - frame_size, msg.size);
	this->_callback_data(&msg);
	return 0;
}

template <typename F>
int UdpClient<F>::_init(const Channel::Url &url, Channel *master)
{
	auto reader = this->channel_props_reader(url);
	_af = reader.getT("af", AF_UNSPEC, {{"unix", AF_UNIX}, {"ipv4", AF_INET}, {"ipv6", AF_INET6}});
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (udp_socket_t::_init(url, master))
		return this->_log.fail(EINVAL, "Failed to init Udp socket");

	auto host = url.host();
	if (_af == AF_UNSPEC && host.find('/') != host.npos)
		_af = AF_UNIX;

	if (_af != AF_UNIX) {
		auto sep = host.find_last_of(':');
		if (sep == host.npos)
			return this->_log.fail(EINVAL, "Invalid host:port pair: {}", host);
		auto p = conv::to_any<unsigned short>(host.substr(sep + 1));
		if (!p)
			return this->_log.fail(EINVAL, "Invalid port '{}': {}", host.substr(sep + 1), p.error());

		_port = *p;
		_host = host.substr(0, sep);
	} else
		_host = host;
	this->_log.debug("Connection to {}:{}", _host, _port);
	return 0;
}

template <typename F>
int UdpClient<F>::_open(const PropsView &url)
{
	auto addr = tll::network::resolve(_af, SOCK_DGRAM, _host.c_str(), _port);
	if (!addr)
		return this->_log.fail(EINVAL, "Failed to resolve '{}': {}", _host, addr.error());
	this->_addr = addr->front();

	auto fd = socket(this->_addr->sa_family, SOCK_DGRAM, 0);
	if (fd == -1)
		return this->_log.fail(errno, "Failed to create socket: {}", strerror(errno));
	this->_update_fd(fd);

	this->_log.info("Send data to {}", this->_addr);

	return udp_socket_t::_open(url);
}

template <typename F>
int UdpClient<F>::_post(const tll_msg_t * msg, int flags)
{
	return udp_socket_t::_send(msg, this->_addr);
}

template <typename F>
int UdpServer<F>::_init(const Channel::Url &url, Channel *master)
{
	auto reader = this->channel_props_reader(url);
	_af = reader.getT("af", AF_UNSPEC, {{"unix", AF_UNIX}, {"ipv4", AF_INET}, {"ipv6", AF_INET6}});
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (udp_socket_t::_init(url, master))
		return this->_log.fail(EINVAL, "Failed to init Udp socket");

	auto host = url.host();
	if (_af == AF_UNSPEC && host.find('/') != host.npos)
		_af = AF_UNIX;

	if (_af != AF_UNIX) {
		auto sep = host.find_last_of(':');
		if (sep == host.npos)
			return this->_log.fail(EINVAL, "Invalid host:port pair: {}", host);
		auto p = conv::to_any<unsigned short>(host.substr(sep + 1));
		if (!p)
			return this->_log.fail(EINVAL, "Invalid port '{}': {}", host.substr(sep + 1), p.error());

		_port = *p;
		_host = host.substr(0, sep);
	} else
		_host = host;
	this->_log.debug("Listen on {}:{}", _host, _port);
	return 0;
}

template <typename F>
int UdpServer<F>::_open(const PropsView &url)
{
	auto addr = tll::network::resolve(_af, SOCK_DGRAM, _host.c_str(), _port);
	if (!addr)
		return this->_log.fail(EINVAL, "Failed to resolve '{}': {}", _host, addr.error());
	this->_addr = addr->front();

	auto fd = socket(this->_addr->sa_family, SOCK_DGRAM, 0);
	if (fd == -1)
		return this->_log.fail(errno, "Failed to create socket: {}", strerror(errno));
	this->_update_fd(fd);

	this->_log.info("Listen on {}", this->_addr);

	if (bind(this->fd(), this->_addr, this->_addr.size))
		return this->_log.fail(errno, "Failed to bind: {}", strerror(errno));
	_unlink_socket = _af == AF_UNIX;

	return udp_socket_t::_open(url);
}

template <typename F>
int UdpServer<F>::_close()
{
	if (_unlink_socket) {
		this->_log.info("Unlink unix socket {}", this->_host);
		if (unlink(this->_host.c_str()))
			this->_log.warning("Failed to unlink socket {}: {}", this->_host, strerror(errno));
	}
	_unlink_socket = false;
	return udp_socket_t::_close();
}

template <typename F>
int UdpServer<F>::_post(const tll_msg_t * msg, int flags)
{
	return udp_socket_t::_send(msg, this->_peer);
}
