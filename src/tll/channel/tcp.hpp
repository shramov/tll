/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_IMPL_CHANNEL_TCP_HPP
#define _TLL_IMPL_CHANNEL_TCP_HPP

#include "tll/channel/tcp.h"
#include "tll/channel/tcp-scheme.h"
#include "tll/channel/tcp-client-scheme.h"

#include "tll/util/conv-fmt.h"
#include "tll/util/size.h"
#include "tll/util/sockaddr.h"

#include <limits.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/sockios.h>
#include <linux/net_tstamp.h>
#include <sys/ioctl.h>

#ifndef IPPROTO_MPTCP
#define IPPROTO_MPTCP 262
#endif
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
#if !defined(MSG_NOSIGNAL)
#  define MSG_NOSIGNAL 0
#endif // MSG_NOSIGNAL

#if !defined(SOL_TCP)
#  define SOL_TCP IPPROTO_TCP
#endif // SOL_TCP
#endif // __APPLE__

template <>
struct tll::conv::parse<tll::channel::tcp_settings_t::Protocol>
{
        static tll::result_t<tll::channel::tcp_settings_t::Protocol> to_any(std::string_view s)
        {
		using Protocol = tll::channel::tcp_settings_t::Protocol;
                return tll::conv::select(s, std::map<std::string_view, Protocol> {
			{"tcp", Protocol::TCP},
			{"mptcp", Protocol::MPTCP},
			{"sctp", Protocol::SCTP},
		});
        }
};

namespace tll::channel {

enum class TcpChannelMode { Client, Server, Socket };

namespace _ {

inline size_t _fill_iovec(size_t full, struct iovec * iov)
{
	return full;
}

template <typename Arg, typename ... Args>
size_t _fill_iovec(size_t full, struct iovec * iov, const Arg &arg, const Args & ... args)
{
	iov->iov_base = (void *) tll::memoryview_api<Arg>::data(arg);
	iov->iov_len = tll::memoryview_api<Arg>::size(arg);
	return _fill_iovec(full + tll::memoryview_api<Arg>::size(arg), iov + 1, std::forward<const Args &>(args)...);
}

inline int select_protocol(tll::Logger &log, const tcp_settings_t &settings, int af)
{
	switch (settings.protocol) {
	case tcp_settings_t::TCP:
		return 0;

	case tcp_settings_t::MPTCP:
		if (af == AF_UNIX) {
			log.info("MPTCP not supported for Unix sockets, fall back to TCP");
			return 0;
		}
#ifdef IPPROTO_MPTCP
		return IPPROTO_MPTCP;
#else
		log.warning("MPTCP not supported on this platform, fall back to TCP");
		return 0;
#endif

	case tcp_settings_t::SCTP:
		return IPPROTO_SCTP;
	}
	return log.fail(-1, "Undefined protocol variant: {}", int(settings.protocol));
}

} // namespace _

template <typename T>
int TcpSocket<T>::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	_rbuf.resize(_size);
	_wbuf.resize(_size);
	return 0;
}

template <typename T>
int TcpSocket<T>::_open(const ConstConfig &url)
{
	_rbuf.clear();
	_wbuf.clear();
	if (this->fd() == -1) {
		auto fd = url.getT<int>("fd");
		if (!fd)
			return this->_log.fail(EINVAL, "Invalid fd parameter: {}", fd.error());
		this->_update_fd(*fd);
	}
	this->_dcaps_poll(dcaps::CPOLLIN);
	// Fd set by server
	return 0;
}

template <typename T>
int TcpSocket<T>::_close()
{
	auto fd = this->_update_fd(-1);
	if (fd != -1)
		::close(fd);
	return 0;
}

template <typename T>
int TcpSocket<T>::_post_data(const tll_msg_t *msg, int flags)
{
	this->_log.trace("Post {} bytes of data", msg->size);
	int r = send(this->fd(), msg->data, msg->size, MSG_NOSIGNAL | MSG_DONTWAIT);
	if (r < 0)
		return this->_on_send_error(this->_log.fail(errno, "Failed to post data: {}", strerror(errno)));
	else if ((size_t) r != msg->size)
		return this->_log.fail(errno, "Failed to post data (truncated): {}", strerror(errno));
	return 0;
}

template <typename T>
int TcpSocket<T>::_post_control(const tll_msg_t *msg, int flags)
{
	if (msg->msgid == tcp_scheme::Disconnect::meta_id()) {
		this->_log.info("Disconnect client on user request");
		this->close();
	}
	return 0;
}

template <typename T>
void TcpSocket<T>::_on_output_full()
{
	tll_msg_t msg = { TLL_MESSAGE_CONTROL };
	msg.msgid = tcp_scheme::WriteFull::meta_id();
	msg.addr = _msg_addr;
	this->_callback(&msg);
}

template <typename T>
void TcpSocket<T>::_on_output_ready()
{
	tll_msg_t msg = { TLL_MESSAGE_CONTROL };
	msg.msgid = tcp_scheme::WriteReady::meta_id();
	msg.addr = _msg_addr;
	this->_callback(&msg);
}

template <typename T>
std::optional<size_t> TcpSocket<T>::_recv(size_t size)
{
	auto left = _rbuf.available();
	if (left == 0)
		return this->_log.fail(std::nullopt, "No space left in recv buffer");

	size = std::min(size, left);

#ifdef __linux__
	struct iovec iov = {_rbuf.end(), size};
	msghdr mhdr = {};
	mhdr.msg_iov = &iov;
	mhdr.msg_iovlen = 1;
	mhdr.msg_control = _cbuf.data();
	mhdr.msg_controllen = _cbuf.size();
	int r = recvmsg(this->fd(), &mhdr, MSG_NOSIGNAL | MSG_DONTWAIT);
#else
	int r = recv(this->fd(), _rbuf.end(), size, MSG_NOSIGNAL | MSG_DONTWAIT);
#endif
	if (r < 0) {
		if (errno == EAGAIN)
			return 0;
		return this->_log.fail(std::nullopt, "Failed to receive data: {}", strerror(errno));
	} else if (r == 0) {
		this->_log.debug("Connection closed");
		this->channelT()->_on_close();
		return 0;
	}
#ifdef __linux__
	if (mhdr.msg_controllen)
		_timestamp = _cmsg_timestamp(&mhdr);
#endif
	_rbuf.extend(r);
	this->_log.trace("Got {} bytes of data", r);
	return r;
}

template <typename T>
int TcpSocket<T>::setup(const tcp_settings_t &settings, int af)
{
	using namespace tll::network;

	_rbuf.resize(settings.rcv_buffer_size);
	_wbuf.resize(settings.snd_buffer_size);

	if (int r = nonblock(this->fd()))
		return this->_log.fail(EINVAL, "Failed to set nonblock: {}", strerror(r));

#ifdef __APPLE__
	if (setsockoptT<int>(this->fd(), SOL_SOCKET, SO_NOSIGPIPE, 1))
		return this->_log.fail(EINVAL, "Failed to set SO_NOSIGPIPE: {}", strerror(errno));
#endif

#ifdef __linux__
	if (settings.timestamping) {
		int v = SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE | SOF_TIMESTAMPING_SOFTWARE;
		if (setsockopt(this->fd(), SOL_SOCKET, SO_TIMESTAMPING, &v, sizeof(v)))
			return this->_log.fail(EINVAL, "Failed to enable timestamping: {}", strerror(errno));
		_cbuf.resize(256);
	}
#endif

	if (settings.keepalive && setsockoptT<int>(this->fd(), SOL_SOCKET, SO_KEEPALIVE, 1))
		return this->_log.fail(EINVAL, "Failed to set keepalive: {}", strerror(errno));

	if (settings.sndbuf && setsockoptT<int>(this->fd(), SOL_SOCKET, SO_SNDBUF, settings.sndbuf))
		return this->_log.fail(EINVAL, "Failed to set sndbuf to {}: {}", settings.sndbuf, strerror(errno));

	if (settings.rcvbuf && setsockoptT<int>(this->fd(), SOL_SOCKET, SO_RCVBUF, settings.rcvbuf))
		return this->_log.fail(EINVAL, "Failed to set rcvbuf to {}: {}", settings.rcvbuf, strerror(errno));

	if (settings.nodelay && af != AF_UNIX && settings.protocol != settings.SCTP && setsockoptT<int>(this->fd(), SOL_TCP, TCP_NODELAY, 1))
		return this->_log.fail(EINVAL, "Failed to set nodelay: {}", strerror(errno));

	return 0;
}

template <typename T>
std::chrono::nanoseconds TcpSocket<T>::_cmsg_timestamp(msghdr * msg)
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

template <typename T>
void TcpSocket<T>::_store_output(const void * base, size_t size, size_t offset)
{
	auto len = size - offset;
	auto data = offset + (const char *) base;
	if (_wbuf.available() < len)
		_wbuf.resize(_wbuf.size() + len);
	memcpy(_wbuf.end(), data, len);
	_wbuf.extend(len);
	if (_wbuf.size() == len) {
		this->channelT()->_on_output_full();
		this->_update_dcaps(dcaps::CPOLLOUT);
	}
}

template <typename T>
int TcpSocket<T>::_sendmsg(const iovec * iov, size_t N)
{
	if (_wbuf.size()) {
		auto old = _wbuf.size();
		for (unsigned i = 0; i < N; i++)
			_store_output(iov[i].iov_base, iov[i].iov_len);
		this->_log.trace("Stored {} bytes of pending data (now {})", _wbuf.size() - old, _wbuf.size());
		return 0;
	}

	size_t full = 0;
	for (unsigned i = 0; i < N; i++)
		full += iov[i].iov_len;

	struct msghdr msg = {};
	msg.msg_iov = (iovec *) iov;
	msg.msg_iovlen = N;
	auto r = sendmsg(this->fd(), &msg, MSG_NOSIGNAL | MSG_DONTWAIT);
	if (r < 0) {
		if (errno != EAGAIN)
			return this->_on_send_error(this->_log.fail(EINVAL, "Failed to send {} bytes of data: {}", full, strerror(errno)));
		r = 0;
	}
	if (r < (ssize_t) full) {
		this->_log.trace("Partial send: {} < {}, {} bytes not sent", r, full, full - r);
		auto old = _wbuf.size();
		for (unsigned i = 0; i < N; i++) {
			auto len = iov[i].iov_len;
			if (r >= (ssize_t) len) {
				r -= len;
				continue;
			}
			_store_output(iov[i].iov_base, iov[i].iov_len, r);
			r = 0;
		}
		this->_log.trace("Stored {} bytes of pending data (now {})", _wbuf.size() - old, _wbuf.size());
	}
	return 0;
}

template <typename T>
template <typename ... Args>
int TcpSocket<T>::_sendv(const Args & ... args)
{
	constexpr unsigned N = sizeof...(Args);
	struct iovec iov[N];
	_::_fill_iovec(0, iov, std::forward<const Args &>(args)...);

	return _sendmsg(iov, N);
}

template <typename T>
int TcpSocket<T>::_process_output()
{
	if (!_wbuf.size())
		return 0;
	auto r = ::send(this->fd(), _wbuf.data(), _wbuf.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
	if (r < 0) {
		if (errno == EAGAIN)
			return 0;
		return this->_on_send_error(this->_log.fail(errno, "Failed to send pending data: {}", strerror(errno)));
	}

	_wbuf.done(r);
	this->_log.trace("Sent {} bytes of pending data, {} bytes left", r, _wbuf.size());
	_wbuf.shift();
	if (!_wbuf.size()) {
		this->_update_dcaps(0, dcaps::CPOLLOUT);
		this->channelT()->_on_output_ready();
	}
	return 0;
}

template <typename T>
int TcpSocket<T>::_process(long timeout, int flags)
{
	auto r = _recv();
	if (!r)
		return EINVAL;
	if (!*r)
		return EAGAIN;
	this->_log.trace("Got data: {}", *r);
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	msg.data = _rbuf.data();
	msg.size = *r;
	msg.addr = _msg_addr;
	msg.time = _timestamp.count();
	this->_callback_data(&msg);
	rdone(*r);
	rshift();
	return 0;
}

template <typename T, typename S>
int TcpClient<T, S>::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	this->_msg_addr.fd = 0;

	auto reader = this->channel_props_reader(url);
	auto af = reader.getT("af", network::AddressFamily::UNSPEC);
	_peer = reader.getT("tll.host", std::optional<tll::network::hostport> {});
	this->_size = reader.template getT<util::Size>("size", 128 * 1024);
	_settings.timestamping = reader.getT("timestamping", false);
	_settings.keepalive = reader.getT("keepalive", true);
	_settings.nodelay = reader.getT("nodelay", true);
	_settings.sndbuf = reader.getT("sndbuf", util::Size { 0 });
	_settings.rcvbuf = reader.getT("rcvbuf", util::Size { 0 });
	{
		auto size = reader.getT("buffer-size", util::Size { 64 * 1024 });
		_settings.snd_buffer_size = reader.getT("send-buffer-size", size);
		_settings.rcv_buffer_size = reader.getT("recv-buffer-size", size);
	}
	_bind_host = reader.getT("bind", std::optional<tll::network::hostport> {});
	_settings.protocol = reader.getT("protocol", tcp_settings_t::Protocol::TCP);
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	S::_init(url, master);

	if (_bind_host && _bind_host->set_af(af))
		return this->_log.fail(EINVAL, "Mismatched address family for bind address: parameter {}, parsed {}", af, _bind_host->af);

	if (_peer) {
		if (_peer->set_af(af))
			return this->_log.fail(EINVAL, "Mismatched address family: parameter {}, parsed {}", af, _peer->af);
		this->_log.debug("Connection to {}:{}", _peer->host, _peer->port);
	} else
		this->_log.debug("Connection address will be provided in open parameters");

	this->_scheme_control.reset(this->context().scheme_load(tcp_client_scheme::scheme_string));
	if (!this->_scheme_control.get())
		return this->_log.fail(EINVAL, "Failed to load control scheme");

	return 0;
}

template <typename T, typename S>
int TcpClient<T, S>::_open(const ConstConfig &url)
{
	tll::network::hostport peer;
	if (!_peer) {
		auto reader = this->channelT()->channel_props_reader(url);
		auto af = reader.getT("af", network::AddressFamily::UNSPEC);
		peer = reader.template getT<tll::network::hostport>("host");
		if (!reader)
			return this->_log.fail(EINVAL, "Invalid open parameters: {}", reader.error());
		if (peer.set_af(af))
			return this->_log.fail(EINVAL, "Mismatched address family: parameter {}, parsed {}", af, peer.af);
	} else
		peer = *_peer;
	auto addr = tll::network::resolve(peer.af, SOCK_STREAM, peer.host, peer.port);
	if (!addr)
		return this->_log.fail(EINVAL, "Failed to resolve '{}': {}", peer.host, addr.error());
	std::swap(_addr_list, *addr);
	_addr = _addr_list.begin();

	auto fd = socket((*_addr)->sa_family, SOCK_STREAM, _::select_protocol(this->_log, _settings, (*_addr)->sa_family));
	if (fd == -1)
		return this->_log.fail(errno, "Failed to create socket: {}", strerror(errno));
	this->_update_fd(fd);

	if (_bind_host) {
		addr = _bind_host->resolve(SOCK_STREAM);
		if (!addr)
			return this->_log.fail(EINVAL, "Failed to resolve bind host '{}': {}", _bind_host->host, addr.error());
		if (bind(fd, addr->front(), addr->front().size))
			return this->_log.fail(EINVAL, "Failed to bind to address {}: {}", *_addr, strerror(errno));
	}

	if (this->setup(_settings, (*_addr)->sa_family))
		return this->_log.fail(EINVAL, "Failed to setup socket");

	if (S::_open(url))
		return this->_log.fail(EINVAL, "Parent open failed");

	this->_log.info("Connect to {}", *_addr);
	auto r = connect(this->fd(), *_addr, _addr->size);
	if (r && errno != EINPROGRESS)
		return this->_log.fail(errno, "Failed to connect: {}", strerror(errno));

	if (auto r = _export_peer(true); r)
		return r;

	if (r) {
		this->_dcaps_poll(dcaps::CPOLLOUT);
		return 0;
	}

	this->_log.info("Connected");
	if (auto r = _export_peer(false); r)
		return r;

	return this->channelT()->_on_connect();
}

template <typename T, typename S>
int TcpClient<T, S>::_export_peer(bool local)
{
	tll::network::sockaddr_any addr;
	addr.size = sizeof(addr);
	const auto func = local ? getsockname : getpeername;
	if (func(this->fd(), addr, &addr.size))
		return this->_log.fail(errno, "Failed to get {} address: {}", local ? "socket" : "peer", strerror(errno));
	if (addr->sa_family != AF_UNIX) {
		this->_log.debug("{} address: {}", local ? "Local" : "Remote", addr);
		auto cfg = this->config_info().sub(local ? "local" : "remote", true);
		if (!cfg)
			return this->_log.fail(EINVAL, "Can not create subtree for {} address", local ? "local" : "remote");
		cfg->setT("af", (network::AddressFamily) addr->sa_family);
		cfg->setT("port", ntohs(addr.in()->sin_port));
		if (addr->sa_family == AF_INET)
			cfg->setT("host", addr.in()->sin_addr);
		else
			cfg->setT("host", addr.in6()->sin6_addr);
	}
	return 0;
}

template <typename T, typename S>
int TcpClient<T, S>::_process_connect()
{
	struct pollfd pfd = { this->fd(), POLLOUT };
	auto r = poll(&pfd, 1, 0);
	if (r < 0)
		return this->_log.fail(errno, "Failed to poll: {}", strerror(errno));
	if (r == 0 || (pfd.revents & (POLLOUT | POLLHUP)) == 0)
		return EAGAIN;

	int err = 0;
	socklen_t len = sizeof(err);
	if (getsockopt(this->fd(), SOL_SOCKET, SO_ERROR, &err, &len))
		return this->_log.fail(errno, "Failed to get connect status: {}", strerror(errno));
	if (err)
		return this->_log.fail(err, "Failed to connect: {}", strerror(err));

	this->_log.info("Connected");
	if (auto r = _export_peer(false); r)
		return r;
	return this->channelT()->_on_connect();
}

template <typename T, typename S>
int TcpClient<T, S>::_process(long timeout, int flags)
{
	if (this->state() == state::Opening)
		return _process_connect();
	return S::_process(timeout, flags);
}

template <typename T>
int TcpServerSocket<T>::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	return 0;
}

template <typename T>
int TcpServerSocket<T>::_open(const ConstConfig &url)
{
	if (this->fd() == -1) {
		auto fd = url.getT<int>("fd");
		if (!fd)
			return this->_log.fail(EINVAL, "Invalid fd parameter: {}", fd.error());
		this->_update_fd(*fd);
	}
	this->_dcaps_poll(dcaps::CPOLLIN);
	return 0;
}

template <typename T>
int TcpServerSocket<T>::_close()
{
	auto fd = this->_update_fd(-1);
	if (fd != -1)
		::close(fd);
	return 0;
}


template <typename T>
int TcpServerSocket<T>::_process(long timeout, int flags)
{
	tll::network::sockaddr_any addr = {};
	addr.size = sizeof(addr.buf);

	tll::network::scoped_socket fd(accept(this->fd(), addr, &addr.size)); //XXX: accept4
	if (fd == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return EAGAIN;
		return this->_log.fail(errno, "Accept failed: {}", strerror(errno));
	}

	if (addr->sa_family != AF_UNIX)
		this->_log.info("Connection {} from {}", fd, addr);
	else
		this->_log.info("Connection {} from {}", fd, "unix socket");

	if (int e = tll::network::nonblock(fd))
		return this->_log.fail(e, "Failed to set nonblock: {}", strerror(e));

#ifdef __APPLE__
	if (tll::network::setsockoptT<int>(fd, SOL_SOCKET, SO_NOSIGPIPE, 1))
		return this->_log.fail(EINVAL, "Failed to set SO_NOSIGPIPE: {}", strerror(errno));
#endif

	tll_msg_t msg = {};
	tcp_connect_t data = {};
	data.fd = fd;
	data.addrlen = addr.size;
	data.addr = addr;

	msg.type = TLL_MESSAGE_DATA;
	msg.size = sizeof(data);
	msg.data = &data;
	this->_callback_data(&msg);
	fd.release();
	return 0;
}

template <typename T, typename C>
int TcpServer<T, C>::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = this->channelT()->channel_props_reader(url);
	auto af = reader.getT("af", network::AddressFamily::UNSPEC);
	_host = reader.template getT<tll::network::hostport>("tll.host");
	_settings.timestamping = reader.getT("timestamping", false);
	_settings.keepalive = reader.getT("keepalive", true);
	_settings.nodelay = reader.getT("nodelay", true);
	_settings.sndbuf = reader.getT("sndbuf", util::Size { 0 });
	_settings.rcvbuf = reader.getT("rcvbuf", util::Size { 0 });
	{
		auto size = reader.getT("buffer-size", util::Size { 64 * 1024 });
		_settings.snd_buffer_size = reader.getT("send-buffer-size", size);
		_settings.rcv_buffer_size = reader.getT("recv-buffer-size", size);
	}
	_settings.protocol = reader.getT("protocol", tcp_settings_t::Protocol::TCP);
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (_host.set_af(af))
		return this->_log.fail(EINVAL, "Mismatched address family: parameter {}, parsed {}", af, _host.af);

	{
		_socket_url.proto(this->channelT()->channel_protocol());
		auto r = url.getT<tll::Channel::Url>("socket", _socket_url);
		if (!r)
			return this->_log.fail(EINVAL, "Invalid {} socket url: {}", this->channelT()->param_prefix(), r.error());
		_socket_url = *r;
		this->child_url_fill(_socket_url, "0");
		_socket_url.set("fd-mode", "yes");
		_socket_url.set("mode", "socket");
	}


	this->_scheme_control.reset(this->context().scheme_load(tcp_scheme::scheme_string));
	if (!this->_scheme_control.get())
		return this->_log.fail(EINVAL, "Failed to load control scheme");

	_client_init.proto(url.proto());
	_client_init.set("mode", "client");

	this->_log.debug("Listen on {}:{}", _host.host, _host.port);
	return 0;
}

template <typename T, typename C>
int TcpServer<T, C>::_open(const ConstConfig &url)
{
	_cleanup_flag = false;
	_addr_seq = 0;

	auto addr = _host.resolve(SOCK_STREAM);
	if (!addr)
		return this->_log.fail(EINVAL, "Failed to resolve '{}': {}", _host.host, addr.error());

	for (auto & a : *addr) {
		if (this->_bind(a))
			return this->_log.fail(EINVAL, "Failed to listen on {}", a);
	}

	if (this->_scheme) {
		auto full = this->_scheme->dump("yamls+gz");
		if (auto s = this->_scheme->dump("sha256"); s) {
			_client_init.set("scheme", *s);
			_client_config.sub("scheme", true)->set(*s, *full);
		} else if (full)
			_client_init.set("scheme", *full);
	}

	auto af = static_cast<network::AddressFamily>(addr->front()->sa_family);
	if (af == network::AddressFamily::UNIX || _host.host != "*") {
		_client_init.host(tll::conv::to_string(addr->front()));
		_client_init.setT("af", af);
	} else {
		if (_host.af != network::AddressFamily::UNSPEC)
			_client_init.setT("af", _host.af);
		_client_init.setT("tll.host.port", ntohs(addr->front().in()->sin_port));
		auto namemax = sysconf(_SC_HOST_NAME_MAX);
		std::vector<char> hostname(namemax);
		if (gethostname(hostname.data(), hostname.size()))
			return this->_log.fail(EINVAL, "Failed to get hostname: {}", strerror(errno));
		_client_init.set("tll.host.host", hostname.data());
		_client_config.set("replace.host.init.tll.host.host", "");
	}

	_client_config.set("init", _client_init);
	this->_config.set("client", _client_config);

	this->state(state::Active);
	return 0;
}

template <typename T, typename C>
int TcpServer<T, C>::_bind(tll::network::sockaddr_any &addr)
{
	this->_log.info("Listen on {}", conv::to_string(addr));

#ifdef SOCK_NONBLOCK
	static constexpr int sflags = SOCK_STREAM | SOCK_NONBLOCK;
#else
	static constexpr int sflags = SOCK_STREAM;
#endif

	tll::network::scoped_socket fd(socket(addr->sa_family, sflags, _::select_protocol(this->_log, _settings, addr->sa_family)));
	if (fd == -1)
		return this->_log.fail(errno, "Failed to create socket: {}", strerror(errno));

#ifndef SOCK_NONBLOCK
	if (int r = nonblock(fd))
		return this->_log.fail(EINVAL, "Failed to set nonblock: {}", strerror(r));
#endif

	int flag = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)))
		return this->_log.fail(EINVAL, "Failed to set SO_REUSEADDR: {}", strerror(errno));

	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)))
		return this->_log.fail(EINVAL, "Failed to set SO_KEEPALIVE: {}", strerror(errno));

	if (bind(fd, addr, addr.size))
		return this->_log.fail(errno, "Failed to bind: {}", strerror(errno));

	if (listen(fd, 10))
		return this->_log.fail(errno, "Failed to listen on socket: {}", strerror(errno));

	if ((addr->sa_family == AF_INET && addr.in()->sin_port == 0) ||
	    (addr->sa_family == AF_INET6 && addr.in6()->sin6_port == 0)) {
		addr.size = sizeof(addr);
		if (getsockname(fd, addr, &addr.size))
			return this->_log.fail(errno, "Failed to get socket address: {}", strerror(errno));

		this->_log.info("Listen on ephemeral address {}", conv::to_string(addr));
	}

	auto r = this->context().channel(fmt::format("tcp://;fd-mode=yes;tll.internal=yes;name={}/{}", this->name, fd), this->self(), &tcp_server_socket_t::impl);
	if (!r)
		return this->_log.fail(EINVAL, "Failed to init server socket channel");

	auto c = channel_cast<tcp_server_socket_t>(r.get());
	c->bind(fd.release());
	tll_channel_callback_add(r.get(), _cb_socket, this, TLL_MESSAGE_MASK_ALL);

	this->_child_add(r.get());
	_sockets.emplace_back((tll::Channel *) r.release());

	if (c->open(tll::ConstConfig()))
		return this->_log.fail(EINVAL, "Failed to open server socket channel");

	return 0;
}

template <typename T, typename C>
int TcpServer<T, C>::_close()
{
	if (this->_host.af == AF_UNIX && _sockets.size()) {
		this->_log.info("Unlink unix socket {}", this->_host.host);
		if (unlink(this->_host.host.c_str()))
			this->_log.warning("Failed to unlink socket {}: {}", this->_host.host, strerror(errno));
	}
	for (auto & c : _clients)
		tll_channel_free(*c.second);
	_clients.clear();
	_sockets.clear();
	this->_config.remove("client");
	return 0;
}

template <typename T, typename C>
typename TcpServer<T, C>::tcp_socket_t * TcpServer<T, C>::_lookup(const tll_addr_t &a)
{
	auto addr = tcp_socket_addr_t::cast(&a);
	if (addr->fd == -1)
		return this->_log.fail(nullptr, "Invalid address");
	auto i = _clients.find(addr->fd);
	if (i == _clients.end())
		return this->_log.fail(nullptr, "Address not found: {}/{}", addr->fd, addr->seq);
	if (addr->seq != i->second->msg_addr().seq)
		return this->_log.fail(nullptr, "Address seq mismatch: {} != {}", addr->seq, i->second->msg_addr().seq);
	return i->second;
}

template <typename T, typename C>
int TcpServer<T, C>::_post(const tll_msg_t *msg, int flags)
{
	auto socket = _lookup(msg->addr);
	if (!socket)
		return EINVAL;
	return socket->post(msg, flags);
}

template <typename T, typename C>
void TcpServer<T, C>::_process_cleanup()
{
	if (!_cleanup_flag) return;

	for (auto i = _clients.begin(); i != _clients.end();)
	{
		switch (i->second->state()) {
		case state::Error:
		case state::Closed:
			_cleanup(i->second);
			i = _clients.erase(i);
			break;
		default:
			i++;
			break;
		}
	}

	_cleanup_flag = false;
}

template <typename T, typename C>
void TcpServer<T, C>::_cleanup(tcp_socket_t * c)
{
	this->_log.debug("Cleanup client {} @{}", c->name, (void *) c);
	this->_child_del(*c);
	delete c->self();
}

template <typename T, typename C>
int TcpServer<T, C>::_cb_other(const tll_channel_t *c, const tll_msg_t *msg)
{
	auto socket = tll::channel_cast<tcp_socket_t>(const_cast<tll_channel_t *>(c))->channelT();
	if (msg->type == TLL_MESSAGE_STATE) {
		if (msg->msgid == state::Error) {
			this->channelT()->_on_child_error(socket);
			_cleanup_flag = true;
			this->_update_dcaps(dcaps::Pending | dcaps::Process);
		} else if (msg->msgid == state::Closing) {
			this->channelT()->_on_child_closing(socket);
			_cleanup_flag = true;
			this->_update_dcaps(dcaps::Pending | dcaps::Process);
		}
	} else if (msg->type == TLL_MESSAGE_CONTROL)
		this->_callback(msg);
	return 0;
}

template <typename T, typename C>
void TcpServer<T, C>::_on_child_connect(tcp_socket_t *socket, const tcp_connect_t * conn)
{
	std::array<char, tcp_scheme::Connect::meta_size()> buf = {};
	auto connect = tcp_scheme::Connect::bind(buf);
	if (conn->addr->sa_family == AF_INET) {
		auto in = (const sockaddr_in *) conn->addr;
		connect.get_host().set_ipv4(in->sin_addr.s_addr);
		connect.set_port(ntohs(in->sin_port));
	} else if (conn->addr->sa_family == AF_INET6) {
		auto in6 = (const sockaddr_in6 *) conn->addr;
		connect.get_host().set_ipv6({(const char *) &in6->sin6_addr, 16 });
		connect.set_port(ntohs(in6->sin6_port));
	} else if (conn->addr->sa_family == AF_UNIX) {
		connect.get_host().set_unix(0);
	}
	tll_msg_t msg = { TLL_MESSAGE_CONTROL };
	msg.msgid = connect.meta_id();
	msg.size = connect.view().size();
	msg.data = connect.view().data();
	msg.addr = socket->msg_addr();
	this->_callback(&msg);
}

template <typename T, typename C>
void TcpServer<T, C>::_on_child_closing(tcp_socket_t *socket)
{
	tll_msg_t m = { TLL_MESSAGE_CONTROL };
	m.msgid = tcp_scheme::Disconnect::meta_id();
	m.addr = socket->msg_addr();
	this->_callback(&m);
}

template <typename T, typename C>
int TcpServer<T, C>::_cb_data(const tll_channel_t *c, const tll_msg_t *msg)
{
	return this->_callback_data(msg);
}

template <typename T, typename C>
int TcpServer<T, C>::_cb_socket(const tll_channel_t *c, const tll_msg_t *msg)
{
	_process_cleanup();

	if (msg->type != TLL_MESSAGE_DATA) {
		if (msg->type == TLL_MESSAGE_STATE) {
			switch (msg->msgid) {
			case state::Error:
				this->_log.error("Listening socket channel failed");
				this->state(tll::state::Error);
				break;
			default:
				break;
			}
		}
		return 0;
	}
	if (msg->size < sizeof(tcp_connect_t))
		return this->_log.fail(EMSGSIZE, "Invalid connect data size: {} < {}", msg->size, sizeof(tcp_connect_t));
	auto conn = (const tcp_connect_t *) msg->data;
	auto fd = conn->fd;
	this->_log.debug("Got connection fd {}", fd);
	if (this->state() != tll::state::Active) {
		this->_log.debug("Close incoming connection, current state is {}", tll_state_str(this->state()));
		::close(fd);
		return 0;
	}

	_socket_url.set("name", fmt::format("{}/{}", this->name, fd));
	auto impl = this->channelT()->socket_impl_policy() == SocketImplPolicy::Fixed ? &tcp_socket_t::impl : nullptr;
	auto r = this->context().channel(_socket_url, this->self(), impl);
	if (!r)
		return this->_log.fail(EINVAL, "Failed to init client socket channel");

	auto client = channel_cast<tcp_socket_t>(r.get());
	if (!client)
		return this->_log.fail(EINVAL, "Failed to cast to tcp socket type, invalid socket protocol {}", _socket_url.proto());
	//r.release();
	client->bind(fd, _addr_seq++);
	client->setup(_settings, conn->addr->sa_family);
	tll_channel_callback_add(r.get(), _cb_other, this, TLL_MESSAGE_MASK_STATE | TLL_MESSAGE_MASK_CONTROL);
	tll_channel_callback_add(r.get(), _cb_data, this, TLL_MESSAGE_MASK_DATA);
	if (this->channelT()->_on_accept(r.get())) {
		this->_log.debug("Client channel rejected");
		return 0;
	}

	auto it = _clients.find(fd);
	if (it != _clients.end()) {
		_cleanup(it->second);
		it->second = client;
	} else
		_clients.emplace(fd, client);
	this->_child_add(r.release());
	client->open(tll::ConstConfig());

	this->channelT()->_on_child_connect(client, conn);

	return 0;
}

} // namespace tll::channel

template <>
struct tll::conv::parse<tll::channel::TcpChannelMode>
{
        static result_t<tll::channel::TcpChannelMode> to_any(std::string_view s)
        {
		using tll::channel::TcpChannelMode;
                return tll::conv::select(s, std::map<std::string_view, TcpChannelMode> {
			{"client", TcpChannelMode::Client},
			{"server", TcpChannelMode::Server},
			{"socket", TcpChannelMode::Socket},
		});
        }
};

#endif//_TLL_IMPL_CHANNEL_TCP_HPP
