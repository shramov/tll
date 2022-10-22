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

#include <chrono>

#include <ifaddrs.h>
#include <net/if.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/errqueue.h>
#include <linux/sockios.h>
#include <linux/net_tstamp.h>
#endif

#ifndef IPV6_ADD_MEMBERSHIP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#endif

#if defined(__APPLE__) && !defined(MSG_NOSIGNAL)
#  define MSG_NOSIGNAL 0
#endif//__APPLE__

#ifdef __linux__
static constexpr std::string_view control_scheme = R"(yamls://
- name: Time
  id: 10
  fields:
    - {name: time, type: int64, options.type: duration, options.resolution: ns}
)";
#endif

static constexpr int time_msgid = 10;
using time_type = std::chrono::duration<int64_t, std::nano>;

using namespace tll;
using tll::network::AddressFamily;

namespace {
template <typename T> constexpr size_t _sizeof() { return sizeof(T); }
template <> constexpr size_t _sizeof<void>() { return 0; }
} // namespace ''

template <typename T, typename Frame>
class UdpSocket : public tll::channel::Base<T>
{
 protected:
	static constexpr size_t frame_size = _sizeof<Frame>();
	tll::network::sockaddr_any _addr;
	tll::network::sockaddr_any _peer;
	std::vector<char> _buf;
	std::vector<char> _buf_control;
	size_t _sndbuf = 0;
	size_t _rcvbuf = 0;
	unsigned _ttl = 0;

	std::array<long long, 8> _tx_seq;
	unsigned _tx_idx = 0;

	bool _timestamping = false;
	bool _timestamping_tx = false;

	bool _multi = false;
	bool _mcast_loop = true;
	std::optional<std::string> _mcast_interface;
	std::optional<in_addr> _mcast_source;

	int _mcast_ifindex = 0;
	std::optional<in_addr> _mcast_ifaddr4; // Only for ipv4 multicast structures, ipv6 use interface index

	int _nametoindex()
	{
		if (!_mcast_interface || _mcast_ifindex)
			return 0;
		if (this->_addr->sa_family == AF_INET) {
			_mcast_ifaddr4 = {};
			struct ifaddrs * ifa;
			if (getifaddrs(&ifa))
				return this->_log.fail(EINVAL, "Failed to get interface list: {}", strerror(errno));
			for (auto i = ifa; i; i = i->ifa_next) {
				if (i->ifa_name == *_mcast_interface && i->ifa_addr && i->ifa_addr->sa_family == AF_INET) {
					_mcast_ifaddr4 = ((const sockaddr_in *) i->ifa_addr)->sin_addr;
					break;
				}
			}
			freeifaddrs(ifa);
			if (!_mcast_ifaddr4)
				return this->_log.fail(EINVAL, "No ipv4 address for interface {}", *_mcast_interface);
			this->_log.debug("Interface {} addr {}", *_mcast_interface, *_mcast_ifaddr4);
		}

		_mcast_ifindex = if_nametoindex(this->_mcast_interface->c_str());
		if (_mcast_ifindex == 0)
			return this->_log.fail(EINVAL, "Interface '{}' not found: {}", *this->_mcast_interface, strerror(errno));
		return 0;
	}

	time_type _cmsg_timestamp(msghdr *msg);
	uint32_t _cmsg_seq(msghdr *msg);
	int _process_errqueue()
	{
		iovec iov = {_buf.data(), _buf.size()};
		msghdr mhdr = {};
		mhdr.msg_iov = &iov;
		mhdr.msg_iovlen = 1;
		mhdr.msg_control = _buf_control.data();
		mhdr.msg_controllen = _buf_control.size();
		return _process_errqueue(&mhdr);
	}
	int _process_errqueue(msghdr * mhdr);

 public:
	static constexpr std::string_view channel_protocol() { return "udp"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();

	int _process(long timeout, int flags);
	int _send(const tll_msg_t *msg, const tll::network::sockaddr_any &addr);
};

template <typename Frame>
class UdpClient : public UdpSocket<UdpClient<Frame>, Frame>
{
	using udp_socket_t = UdpSocket<UdpClient<Frame>, Frame>;

	AddressFamily _af = AddressFamily::UNSPEC;
	std::string _host;
	unsigned short _port = 0;

 public:
	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);

	int _post(const tll_msg_t *msg, int flags)
	{
		return udp_socket_t::_send(msg, this->_addr);
	}
};

template <typename Frame>
class UdpServer : public UdpSocket<UdpServer<Frame>, Frame>
{
	using udp_socket_t = UdpSocket<UdpServer<Frame>, Frame>;

	AddressFamily _af = AddressFamily::UNSPEC;
	std::string _host;
	unsigned short _port = 0;
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

template <typename T, typename F>
int UdpSocket<T, F>::_init(const Channel::Url &url, Channel *master)
{
	auto reader = this->channel_props_reader(url);
	auto size = reader.getT("size", util::Size {64 * 1024});
	_sndbuf = reader.getT("sndbuf", util::Size {0});
	_rcvbuf = reader.getT("rcvbuf", util::Size {0});
	_ttl = reader.getT("ttl", 0u);

	_timestamping = reader.getT("timestamping", false);
	_timestamping_tx = reader.getT("timestamping-tx", false);

	_multi = reader.getT("multicast", false);
	if (_multi) {
		_mcast_loop = reader.getT("loop", true);
		_mcast_interface = reader.get("interface");
		auto source = reader.get("source");
		if (reader.has("source"))
			_mcast_source = reader.template getT<in_addr>("source");
	}
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());
	_buf.resize(size);
	if (_timestamping) {
#ifdef __linux__
		_buf_control.resize(256);
		if (_timestamping_tx) {
			this->_scheme_control.reset(this->context().scheme_load(control_scheme));
			if (!this->_scheme_control.get())
				return this->_log.fail(EINVAL, "Failed to load control scheme");
		}
#else
		this->_log.info("Packet timestamping supported only on linux");
#endif
	}
	return 0;
}

template <typename T, typename F>
int UdpSocket<T, F>::_open(const ConstConfig &url)
{
	using namespace tll::network;
	if (int r = nonblock(this->fd()))
		return this->_log.fail(EINVAL, "Failed to set nonblock: {}", strerror(r));

	_tx_idx = -1;
	_tx_seq = {};
	if (_timestamping) {
#ifdef __linux__
		int v = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE | SOF_TIMESTAMPING_SOFTWARE;
		if (_timestamping_tx)
			v |= SOF_TIMESTAMPING_TX_SOFTWARE | SOF_TIMESTAMPING_TX_HARDWARE | SOF_TIMESTAMPING_OPT_TSONLY | SOF_TIMESTAMPING_OPT_ID;
		if (setsockoptT<int>(this->fd(), SOL_SOCKET, SO_TIMESTAMPING, v))
			return this->_log.fail(EINVAL, "Failed to set SO_TIMESTAMPING: {}", strerror(errno));
#endif
	}

	if (_sndbuf && setsockoptT<int>(this->fd(), SOL_SOCKET, SO_SNDBUF, _sndbuf))
		return this->_log.fail(EINVAL, "Failed to set sndbuf to {}: {}", _sndbuf, strerror(errno));

	if (_rcvbuf && setsockoptT<int>(this->fd(), SOL_SOCKET, SO_RCVBUF, _rcvbuf))
		return this->_log.fail(EINVAL, "Failed to set rcvbuf to {}: {}", _rcvbuf, strerror(errno));

	if (_ttl) {
		if (_multi && _addr->sa_family == AF_INET6) {
			if (setsockoptT<int>(this->fd(), IPPROTO_IPV6, IPV6_MULTICAST_HOPS, _ttl))
				return this->_log.fail(EINVAL, "Failed to set IPv6 multicast ttl to {}: {}", _ttl, strerror(errno));
		} else if (_multi && _addr->sa_family == AF_INET) {
			if (setsockoptT<int>(this->fd(), IPPROTO_IP, IP_MULTICAST_TTL, _ttl))
				return this->_log.fail(EINVAL, "Failed to set IP multicast ttl to {}: {}", _ttl, strerror(errno));
		} else if (_addr->sa_family == AF_INET6) {
			if (setsockoptT<int>(this->fd(), IPPROTO_IPV6, IPV6_UNICAST_HOPS, _ttl))
				return this->_log.fail(EINVAL, "Failed to set IPv6 ttl to {}: {}", _ttl, strerror(errno));
		} else if (setsockoptT<int>(this->fd(), IPPROTO_IP, IP_TTL, _ttl))
			return this->_log.fail(EINVAL, "Failed to set IP ttl to {}: {}", _ttl, strerror(errno));
	}
	if (_multi && _mcast_loop) {
		if (_addr->sa_family == AF_INET6) {
			if (setsockoptT<int>(this->fd(), IPPROTO_IPV6, IPV6_MULTICAST_LOOP, _mcast_loop))
				return this->_log.fail(EINVAL, "Failed to set IPv6 multicast loop: {}", strerror(errno));
		} else if (setsockoptT<int>(this->fd(), IPPROTO_IP, IP_MULTICAST_LOOP, _mcast_loop))
			return this->_log.fail(EINVAL, "Failed to set IP multicast loop: {}", strerror(errno));
	}

	if (_multi && _mcast_interface) {
		if (this->_nametoindex())
			return this->_log.fail(EINVAL, "Failed to get interface list");

		this->_log.info("Set multicast interface to {}: index {}", *_mcast_interface, _mcast_ifindex);
		if (_addr->sa_family == AF_INET6) {
			if (setsockoptT<int>(this->fd(), IPPROTO_IPV6, IPV6_MULTICAST_IF, _mcast_ifindex))
				return this->_log.fail(EINVAL, "Failed to set IPv6 multicast index to {} ({}): {}", _mcast_ifindex, *_mcast_interface, strerror(errno));
		} else {
			ip_mreqn mreq = {};
			mreq.imr_ifindex = _mcast_ifindex;
			if (setsockoptT(this->fd(), IPPROTO_IP, IP_MULTICAST_IF, mreq))
				return this->_log.fail(EINVAL, "Failed to set IP multicast index to {} ({}): {}", _mcast_ifindex, *_mcast_interface, strerror(errno));
		}
	}

	this->_dcaps_poll(dcaps::CPOLLIN);
	return 0;
}

template <typename T, typename F>
int UdpSocket<T, F>::_close()
{
	auto fd = this->_update_fd(-1);
	if (fd != -1)
		::close(fd);
	_mcast_ifindex = 0;
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
	_tx_seq[++_tx_idx % _tx_seq.size()] = msg->seq;
	auto r = sendmsg(this->fd(), &m, MSG_NOSIGNAL);
	if (r < 0) {
		if (errno == EAGAIN)
			return EAGAIN;
		return this->_log.fail(errno, "Failed to post data: {}", strerror(errno));
	} else if ((size_t) r != size)
		return this->_log.fail(errno, "Failed to post data (truncated): {}", strerror(errno));
	r = _process_errqueue();
	if (r == EAGAIN)
		return 0;
	this->_log.debug("Post done");
	return r;
}

template <typename T, typename Frame>
int UdpSocket<T, Frame>::_process(long timeout, int flags)
{
	iovec iov = {_buf.data(), _buf.size()};
	msghdr mhdr = {};
	mhdr.msg_name = _peer.buf;
	mhdr.msg_namelen = sizeof(_peer.buf);
	mhdr.msg_iov = &iov;
	mhdr.msg_iovlen = 1;
	mhdr.msg_control = _buf_control.data();
	mhdr.msg_controllen = _buf_control.size();
	auto r = recvmsg(this->fd(), &mhdr, MSG_NOSIGNAL);
	if (r < 0) {
		if (errno == EAGAIN)
			return _process_errqueue(&mhdr);
		return this->_log.fail(EINVAL, "Failed to receive data: {}", strerror(errno));
	}
	if ((size_t) r < frame_size)
		return this->_log.fail(EMSGSIZE, "Packet size {} < frame size {}", r, frame_size);
	_peer.size = mhdr.msg_namelen;
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

	if (_timestamping)
		msg.time = _cmsg_timestamp(&mhdr).count();

	this->_callback_data(&msg);
	return 0;
}

template <typename T, typename Frame>
time_type UdpSocket<T, Frame>::_cmsg_timestamp(msghdr * msg)
{
	using namespace std::chrono;
	nanoseconds r = {};
#ifdef __linux__
	for (auto cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if(cmsg->cmsg_level != SOL_SOCKET)
			continue;

		if (cmsg->cmsg_type == SO_TIMESTAMPING) {
			auto ts = (struct timespec *) CMSG_DATA(cmsg);
			if (ts[2].tv_sec || ts[2].tv_nsec) // Get HW timestamp if available
				r = seconds(ts[2].tv_sec) + nanoseconds(ts[2].tv_nsec);
			else
				r = seconds(ts->tv_sec) + nanoseconds(ts->tv_nsec);
		}
	}
#endif
	return r;
}

template <typename T, typename Frame>
uint32_t UdpSocket<T, Frame>::_cmsg_seq(msghdr * msg)
{
#ifdef __linux__
	for (auto cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {
		if (!((cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR) ||
			(cmsg->cmsg_level == SOL_IPV6 && cmsg->cmsg_type == IPV6_RECVERR)))
			continue;

		auto err = (sock_extended_err *) CMSG_DATA(cmsg);
		if (err->ee_errno == ENOMSG && err->ee_origin == SO_EE_ORIGIN_TIMESTAMPING)
			return err->ee_data;
	}
#endif
	return 0;
}

template <typename T, typename Frame>
int UdpSocket<T, Frame>::_process_errqueue(msghdr * mhdr)
{
#ifdef __linux__
	if (!_timestamping_tx)
		return 0;
	auto r = recvmsg(this->fd(), mhdr, MSG_NOSIGNAL | MSG_ERRQUEUE);
	if (r < 0) {
		if (errno == EAGAIN)
			return EAGAIN;
		return this->_log.fail(EINVAL, "Failed to receive errqueue message: {}", strerror(errno));
	}

	auto time = _cmsg_timestamp(mhdr);
	auto seq = _cmsg_seq(mhdr);

	tll_msg_t msg = {};
	if (_tx_idx - seq > _tx_seq.size())
		msg.seq = -1;
	else
		msg.seq = _tx_seq[seq % _tx_seq.size()];
	msg.type = TLL_MESSAGE_CONTROL;
	msg.msgid = time_msgid;
	msg.time = time.count();
	msg.data = &time;
	msg.size = sizeof(time);
	this->_callback(&msg);
#endif
	return 0;
}

template <typename F>
int UdpClient<F>::_init(const Channel::Url &url, Channel *master)
{
	auto reader = this->channel_props_reader(url);
	_af = reader.getT("af", AddressFamily::UNSPEC);
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (udp_socket_t::_init(url, master))
		return this->_log.fail(EINVAL, "Failed to init Udp socket");

	auto host = url.host();
	if (_af == AddressFamily::UNSPEC && host.find('/') != host.npos)
		_af = AddressFamily::UNIX;

	if (_af != AddressFamily::UNIX) {
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
int UdpClient<F>::_open(const ConstConfig &url)
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
int UdpServer<F>::_init(const Channel::Url &url, Channel *master)
{
	auto reader = this->channel_props_reader(url);
	_af = reader.getT("af", AddressFamily::UNSPEC);
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (udp_socket_t::_init(url, master))
		return this->_log.fail(EINVAL, "Failed to init Udp socket");

	auto host = url.host();
	if (_af == AddressFamily::UNSPEC && host.find('/') != host.npos)
		_af = AddressFamily::UNIX;

	if (_af != AddressFamily::UNIX) {
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
int UdpServer<F>::_open(const ConstConfig &url)
{
	using namespace tll::network;
	auto addr = tll::network::resolve(_af, SOCK_DGRAM, _host.c_str(), _port);
	if (!addr)
		return this->_log.fail(EINVAL, "Failed to resolve '{}': {}", _host, addr.error());
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
	_unlink_socket = _af == AF_UNIX;

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
		this->_log.info("Unlink unix socket {}", this->_host);
		if (unlink(this->_host.c_str()))
			this->_log.warning("Failed to unlink socket {}: {}", this->_host, strerror(errno));
	}
	_unlink_socket = false;
	return udp_socket_t::_close();
}
