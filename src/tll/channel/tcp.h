/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_TCP_H
#define _TLL_CHANNEL_TCP_H

#include "tll/channel/base.h"
#include "tll/util/sockaddr.h"

#include <array>
#include <list>
#include <vector>

namespace tll::channel {

template <typename T>
class TcpSocket : public Base<T>
{
 protected:
	size_t _size = 1024;
	std::vector<char> _rbuf;
	std::vector<char> _wbuf;

	size_t _roff = 0; ///< Unprocessed data offset
	size_t _rsize = 0; ///< Received data size

	std::string _host;
	unsigned short _port;

	using tcp_socket_t = TcpSocket<T>;
 public:
	static constexpr std::string_view param_prefix() { return "tcp"; }

	int _init(const tll::UrlView &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();

	int _post(const tll_msg_t *msg, int flags);
	int _process(long timeout, int flags);

	void bind(int fd) { this->_update_fd(fd); }

 protected:
	size_t rsize() const { return _rsize - _roff; }

	template <typename D>
	const D * rdataT(size_t off = 0, size_t size = sizeof(D)) const
	{
		off += _roff;
		if (size > _rsize - _roff)
			return nullptr;
		return (const D *) (_rbuf.data() + off);
	}

	void rdone(size_t size)
	{
		_roff += size;
		if (_roff == _rsize) {
			_roff = 0;
			_rsize = 0;
		}
	}

	void rshift()
	{
		if (_roff == 0) return;
		memmove(_rbuf.data(), _rbuf.data() + _roff, _rsize - _roff);
		_rsize -= _roff;
		_roff = 0;
	}

	std::optional<size_t> _recv(size_t size = 0);

	template <size_t N>
	int _send(std::array<std::pair<const void *, size_t>, N> data);

	int _send(const void * data, size_t size)
	{
		return _send({{data, size}});
	}
};

template <typename T>
class TcpClient : public TcpSocket<T>
{
 protected:
	int _af = 0;
	std::string _host;
	unsigned short _port = 0;

	using addr_list_t = std::vector<tll::network::sockaddr_any>;
	addr_list_t _addr_list;
	addr_list_t::iterator _addr = _addr_list.end();

	using tcp_client_t = TcpClient<T>;
 public:
	int _init(const tll::UrlView &, tll::Channel *master);
	int _open(const tll::PropsView &);

	int _process(long timeout, int flags);

 protected:
	int _process_connect();

	int _on_connect()
	{
		this->_dcaps_poll(dcaps::CPOLLIN);
		this->state(state::Active);
		return 0;
	}
};

template <typename T>
class TcpServerSocket : public tll::channel::Base<TcpServerSocket<T>>
{
 protected:
	using tcp_server_socket_t = TcpServerSocket<T>;

 public:
	static constexpr std::string_view param_prefix() { return "tcp"; }

	int _init(const tll::UrlView &url, tll::Channel *master);

	int _open(const tll::PropsView &props);
	int _close();

	int _process(long timeout, int flags);

	void bind(int fd) { this->_update_fd(fd); }
};

template <typename T, typename C>
class TcpServer : public tll::channel::Base<T>
{
 protected:
	int _af = 0;
	std::string _host;
	unsigned short _port;

	using tcp_server_t = TcpServer<T, C>;
	using tcp_server_socket_t = TcpServerSocket<T>;
	using tcp_socket_t = TcpSocket<C>;
	std::list<std::unique_ptr<Channel>> _sockets;
	std::list<tcp_socket_t *> _clients;
	bool _cleanup_flag = false;

 public:
	static constexpr std::string_view param_prefix() { return "tcp"; }
	static constexpr auto child_policy() { return tll::channel::Base<T>::ChildPolicy::Many; }

	int _init(const tll::UrlView &url, tll::Channel *master);

	int _open(const tll::PropsView &props);
	int _close();

	int _post(const tll_msg_t *msg, int flags);

 protected:
	// Hooks
	int _on_accept(tll_channel_t * c) { return 0; }

	static int _cb_state(const tll_channel_t *c, const tll_msg_t *msg, void * user)
	{
		return static_cast<TcpServer<T, C> *>(user)->_cb_state(c, msg);
	}

	int _cb_state(const tll_channel_t *c, const tll_msg_t *msg);

	static int _cb_data(const tll_channel_t *c, const tll_msg_t *msg, void * user)
	{
		return static_cast<TcpServer<T, C> *>(user)->_cb_data(c, msg);
	}

	int _cb_data(const tll_channel_t *c, const tll_msg_t *msg);

	static int _cb_socket(const tll_channel_t *c, const tll_msg_t *msg, void * user)
	{
		return static_cast<TcpServer<T, C> *>(user)->_cb_socket(c, msg);
	}

	int _cb_socket(const tll_channel_t *c, const tll_msg_t *msg);

	int _bind(const tll::network::sockaddr_any &addr);
	void _cleanup();
};

} // namespace tll::channel

#endif//_TLL_CHANNEL_TCP_H
