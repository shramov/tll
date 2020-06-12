/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_IMPL_CHANNEL_TCP_HPP
#define _TLL_IMPL_CHANNEL_TCP_HPP

#include "tll/channel/tcp.h"

#include "tll/util/conv-fmt.h"
#include "tll/util/size.h"
#include "tll/util/sockaddr.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace tll::channel {

namespace {
int nonblock(int fd)
{
	auto f = fcntl(fd, F_GETFL);
	if (f == -1) return errno;
	return fcntl(fd, F_SETFL, f | O_NONBLOCK);
}
} // namespace ''

template <typename T>
int TcpSocket<T>::_init(const UrlView &url, tll::Channel *master)
{
	_rbuf.resize(_size);
	_wbuf.resize(_size);
	return 0;
}

template <typename T>
int TcpSocket<T>::_open(const PropsView &url)
{
	if (this->fd() == -1) {
		auto fd = url.getT<int>("fd");
		if (!fd)
			return this->_log.fail(EINVAL, "Invalid fd parameter: {}", fd.error());
		this->_update_fd(*fd);
	}
	this->_dcaps_poll(dcaps::CPOLLIN);
	// Fd set by server
	this->state(state::Active);
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
int TcpSocket<T>::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	this->_log.debug("Post {} bytes of data", msg->size);
	int r = send(this->fd(), msg->data, msg->size, MSG_NOSIGNAL | MSG_DONTWAIT);
	if (r < 0)
		return this->_log.fail(errno, "Failed to post data: {}", strerror(errno));
	else if ((size_t) r != msg->size)
		return this->_log.fail(errno, "Failed to post data (truncated): {}", strerror(errno));
	return 0;
}

template <typename T>
std::optional<size_t> TcpSocket<T>::_recv(size_t size)
{
	if (_rsize == _rbuf.size()) return EAGAIN;

	auto left = _rbuf.size() - _rsize;
	if (size != 0)
		size = std::min(size, left);
	else
		size = left;
	int r = recv(this->fd(), _rbuf.data() + _rsize, _rbuf.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
	if (r < 0) {
		if (errno == EAGAIN)
			return 0;
		return this->_log.fail(std::nullopt, "Failed to receive data: {}", strerror(errno));
	} else if (r == 0) {
		this->_log.debug("Connection closed");
		this->close();
		return 0;
	}
	_rsize += r;
	this->_log.trace("Got {} bytes of data", r);
	return r;
}

template <typename T>
template <typename ... Args>
int TcpSocket<T>::_sendv(Args ... args)
{
	constexpr unsigned N = sizeof...(Args);
	std::array<iov_t, N> data({iov_t(std::forward<Args>(args))...});
	struct iovec iov[N] = {};
	size_t full = 0;
	for (unsigned i = 0; i < N; i++) {
		iov[i].iov_base = (void *) data[i].first;
		iov[i].iov_len = data[i].second;
		full += data[i].second;
	}
	struct msghdr msg = {};
	msg.msg_iov = iov;
	msg.msg_iovlen = N;
	return sendmsg(this->fd(), &msg, MSG_NOSIGNAL | MSG_DONTWAIT);
}

template <typename T>
int TcpSocket<T>::_process(long timeout, int flags)
{
	auto r = _recv(_rbuf.size());
	if (!r)
		return EINVAL;
	if (!*r)
		return EAGAIN;
	this->_log.debug("Got data: {}", *r);
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	msg.data = _rbuf.data();
	msg.size = *r;
	msg.addr = (int64_t ) this;
	this->_callback_data(&msg);
	rdone(*r);
	rshift();
	return 0;
}

template <typename T, typename S>
int TcpClient<T, S>::_init(const UrlView &url, tll::Channel *master)
{
	auto reader = this->channel_props_reader(url);
	_af = reader.getT("af", AF_UNSPEC, {{"unix", AF_UNIX}, {"ipv4", AF_INET}, {"ipv6", AF_INET6}});
	this->_size = reader.template getT<util::Size>("size", 128 * 1024);
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	TcpSocket<T>::_init(url, master);

	if (_af == AF_UNSPEC && url.host.find('/') != url.host.npos)
		_af = AF_UNIX;

	if (_af != AF_UNIX) {
		auto sep = url.host.find_last_of(':');
		if (sep == url.host.npos)
			return this->_log.fail(EINVAL, "Invalid host:port pair: {}", url.host);
		auto p = conv::to_any<unsigned short>(url.host.substr(sep + 1));
		if (!p)
			return this->_log.fail(EINVAL, "Invalid port '{}': {}", url.host.substr(sep + 1), p.error());

		_port = *p;
		_host = url.host.substr(0, sep);
	} else
		_host = url.host;
	this->_log.debug("Connection to {}:{}", _host, _port);
	return 0;
}

template <typename T, typename S>
int TcpClient<T, S>::_open(const PropsView &url)
{
	auto addr = tll::network::resolve(_af, SOCK_STREAM, _host.c_str(), _port);
	if (!addr)
		return this->_log.fail(EINVAL, "Failed to resolve '{}': {}", _host, addr.error());
	std::swap(_addr_list, *addr);
	_addr = _addr_list.begin();

	auto fd = socket((*_addr)->sa_family, SOCK_STREAM, 0);
	if (fd == -1)
		return this->_log.fail(errno, "Failed to create socket: {}", strerror(errno));
	this->_update_fd(fd);

	if (int r = nonblock(this->fd()))
		return this->_log.fail(EINVAL, "Failed to set nonblock: {}", strerror(r));

	this->_log.info("Connect to {}", *_addr);
	if (connect(this->fd(), *_addr, _addr->size)) {
		if (errno == EINPROGRESS) {
			this->_dcaps_poll(dcaps::CPOLLOUT);
			return 0;
		}
		return this->_log.fail(errno, "Failed to connect: {}", strerror(errno));
	}

	this->_dcaps_poll(dcaps::CPOLLIN);

	this->state(state::Active);
	return 0;
}

template <typename T, typename S>
int TcpClient<T, S>::_process_connect()
{
	struct pollfd pfd = { this->fd(), POLLOUT };
	auto r = poll(&pfd, 1, 0);
	if (r < 0)
		return this->_log.fail(errno, "Failed to poll: {}", strerror(errno));
	if (r == 0 || (pfd.revents & POLLOUT) == 0)
		return EAGAIN;

	this->_log.info("Connected");

	int err = 0;
	socklen_t len = sizeof(err);
	if (getsockopt(this->fd(), SOL_SOCKET, SO_ERROR, &err, &len))
		return this->_log.fail(errno, "Failed to get connect status: {}", strerror(errno));
	if (err)
		return this->_log.fail(err, "Failed to connect: {}", strerror(err));

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
int TcpServerSocket<T>::_init(const UrlView &url, tll::Channel *master)
{
	return 0;
}

template <typename T>
int TcpServerSocket<T>::_open(const PropsView &url)
{
	if (this->fd() == -1) {
		auto fd = url.getT<int>("fd");
		if (!fd)
			return this->_log.fail(EINVAL, "Invalid fd parameter: {}", fd.error());
		this->_update_fd(*fd);
	}
	this->state(state::Active);
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

	int fd = accept(this->fd(), addr, &addr.size); //XXX: accept4
	if (fd == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return EAGAIN;
		return this->_log.fail(errno, "Accept failed: {}", strerror(errno));
	}

	if (addr->sa_family != AF_UNIX)
		this->_log.info("Connection {} from {}", fd, addr);
	else
		this->_log.info("Connection {} from {}", fd, "unix socket");

	if (int e = nonblock(fd)) {
		::close(fd);
		return this->_log.fail(e, "Failed to set nonblock: {}", strerror(e));
	}
	tll_msg_t msg = {};
	msg.type = TLL_MESSAGE_DATA;
	msg.size = sizeof(fd);
	msg.data = &fd;
	this->_callback_data(&msg);
	return 0;
}

template <typename T, typename C>
int TcpServer<T, C>::_init(const UrlView &url, tll::Channel *master)
{
	auto reader = this->channelT()->channel_props_reader(url);
	_af = reader.getT("af", AF_UNSPEC, {{"unix", AF_UNIX}, {"ipv4", AF_INET}, {"ipv6", AF_INET6}});
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (_af == AF_UNSPEC && url.host.find('/') != url.host.npos)
		_af = AF_UNIX;

	if (_af != AF_UNIX) {
		auto sep = url.host.find_last_of(':');
		if (sep == url.host.npos)
			return this->_log.fail(EINVAL, "Invalid host:port pair: {}", url.host);
		auto p = conv::to_any<unsigned short>(url.host.substr(sep + 1));
		if (!p)
			return this->_log.fail(EINVAL, "Invalid port '{}': {}", url.host.substr(sep + 1), p.error());

		_port = *p;
		_host = url.host.substr(0, sep);
	} else
		_host = url.host;
	this->_log.debug("Listen on {}:{}", _host, _port);
	return 0;
}

template <typename T, typename C>
int TcpServer<T, C>::_open(const PropsView &url)
{
	_cleanup_flag = false;

	auto addr = tll::network::resolve(_af, SOCK_STREAM, _host.c_str(), _port);
	if (!addr)
		return this->_log.fail(EINVAL, "Failed to resolve '{}': {}", _host, addr.error());

	for (auto & a : *addr) {
		if (this->_bind(a))
			return this->_log.fail(EINVAL, "Failed to listen on {}", a);
	}

	this->state(state::Active);
	return 0;
}

template <typename T, typename C>
int TcpServer<T, C>::_bind(const tll::network::sockaddr_any &addr)
{
	this->_log.info("Listen on {}", conv::to_string(addr));

#ifdef SOCK_NONBLOCK
	static constexpr int sflags = SOCK_STREAM | SOCK_NONBLOCK;
#else
	static constexpr int sflags = SOCK_STREAM;
#endif

	tll::network::scoped_socket fd(socket(addr->sa_family, sflags, 0));
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

	auto r = this->context().channel(fmt::format("tcp://;fd-mode=yes;tll.internal=yes;name={}/{}", this->name, fd), this->self(), &tcp_server_socket_t::impl);
	if (!r)
		return this->_log.fail(EINVAL, "Failed to init server socket channel");

	auto c = channel_cast<tcp_server_socket_t>(r.get());
	c->bind(fd.release());
	tll_channel_callback_add(r.get(), _cb_socket, this, TLL_MESSAGE_MASK_ALL);

	this->_custom_add(r.get());
	_sockets.emplace_back((tll::Channel *) r.release());

	if (c->open(""))
		return this->_log.fail(EINVAL, "Failed to open server socket channel");

	return 0;
}

template <typename T, typename C>
int TcpServer<T, C>::_close()
{
	if (this->_af == AF_UNIX && _sockets.size()) {
		this->_log.info("Unlink unix socket {}", this->_host);
		if (unlink(this->_host.c_str()))
			this->_log.warning("Failed to unlink socket {}: {}", this->_host, strerror(errno));
	}
	for (auto & c : _clients)
		tll_channel_free(*c);
	_clients.clear();
	_sockets.clear();
	return 0;
}

template <typename T, typename C>
int TcpServer<T, C>::_post(const tll_msg_t *msg, int flags)
{
	if (!msg->addr)
		return this->_log.fail(EINVAL, "Invalid address");
	auto ca = (tcp_socket_t *) msg->addr;
	for (auto & c : _clients) {
		if (c == ca)
			return c->post(msg, flags);
	}
	return this->_log.fail(ENOENT, "Address not found");
}

template <typename T, typename C>
void TcpServer<T, C>::_cleanup()
{
	if (!_cleanup_flag) return;

	for (auto i = _clients.begin(); i != _clients.end();)
	{
		switch ((*i)->state()) {
		case state::Error:
		case state::Closed:
			this->_log.debug("Cleanup client {} @{}", (*i)->name, (void *) *i);
			this->_custom_del(**i);
			delete (*i);
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
int TcpServer<T, C>::_cb_state(const tll_channel_t *c, const tll_msg_t *msg)
{
	if (msg->msgid == state::Error)
		_cleanup_flag = true;
	else if (msg->msgid == state::Closing)
		_cleanup_flag = true;
	return 0;
}

template <typename T, typename C>
int TcpServer<T, C>::_cb_data(const tll_channel_t *c, const tll_msg_t *msg)
{
	return this->_callback_data(msg);
}

template <typename T, typename C>
int TcpServer<T, C>::_cb_socket(const tll_channel_t *c, const tll_msg_t *msg)
{
	_cleanup();

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
	if (msg->size != sizeof(int))
		return this->_log.fail(EMSGSIZE, "Invalid fd size: {}", msg->size);
	auto fd = * (int *) msg->data;
	this->_log.debug("Got connection fd {}", fd);
	if (this->state() != tll::state::Active) {
		this->_log.debug("Close incoming connection, current state is {}", tll_state_str(this->state()));
		::close(fd);
		return 0;
	}

	auto r = this->context().channel(fmt::format("tcp://;fd-mode=yes;tll.internal=yes;name={}/{}", this->name, fd), this->self(), &tcp_socket_t::impl);
	if (!r)
		return this->_log.fail(EINVAL, "Failed to init client socket channel");

	auto client = channel_cast<tcp_socket_t>(r.get());
	//r.release();
	client->bind(fd);
	tll_channel_callback_add(r.get(), _cb_state, this, TLL_MESSAGE_MASK_STATE);
	tll_channel_callback_add(r.get(), _cb_data, this, TLL_MESSAGE_MASK_DATA);
	if (this->channelT()->_on_accept(r.get())) {
		this->_log.debug("Client channel rejected");
		return 0;
	}

	_clients.push_back(client);
	this->_custom_add(r.release());
	client->open("");
	return 0;
}

} // namespace tll::channel

#endif//_TLL_IMPL_CHANNEL_TCP_HPP
