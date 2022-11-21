/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_TCP_H
#define _TLL_CHANNEL_TCP_H

#include "tll/channel/base.h"
#include "tll/util/sockaddr.h"

#include <array>
#include <chrono>
#include <list>
#include <vector>

struct iovec;

namespace tll::channel {

struct tcp_socket_addr_t
{
	int fd = -1;
	int seq = 0;

	static const tcp_socket_addr_t * cast(const tll_addr_t * ptr)
	{
		static_assert(sizeof(tcp_socket_addr_t) <= sizeof(tll_addr_t), "tll_addr_t can not be casted to tcp_socket_addr_t");
		return (const tcp_socket_addr_t *) ptr;
	}

	operator tll_addr_t () const
	{
		static_assert(sizeof(tcp_socket_addr_t) <= sizeof(tll_addr_t), "tcp_socket_addr_t can not be casted to tll_addr_t");
		return *(tll_addr_t *) this;
	}
};

struct tcp_settings_t {
	size_t sndbuf = 0;
	size_t rcvbuf = 0;
	size_t buffer_size = 1024;
	bool timestamping = false;
	bool keepalive = true;
};

struct tcp_connect_t {
	int fd;
	socklen_t  addrlen;
	sockaddr * addr;
};

template <typename T>
class TcpSocket : public Base<T>
{
 protected:
	size_t _size = 1024;
	std::vector<char> _rbuf;
	std::vector<char> _wbuf;
	std::vector<char> _cbuf;

	size_t _roff = 0; ///< Unprocessed data offset
	size_t _rsize = 0; ///< Received data size
	size_t _woff = 0; ///< Pending output data offset
	size_t _wsize = 0; ///< Pending output data size

	tcp_socket_addr_t _msg_addr;

	using tcp_socket_t = TcpSocket<T>;

 public:
	static constexpr std::string_view channel_protocol() { return "tcp"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();

	int _post(const tll_msg_t *msg, int flags)
	{
		switch (msg->type) {
		case TLL_MESSAGE_DATA:
			return this->channelT()->_post_data(msg, flags);
		case TLL_MESSAGE_CONTROL:
			return this->channelT()->_post_control(msg, flags);
		default:
			break;
		}
		return 0;
	}
	int _post_data(const tll_msg_t *msg, int flags);
	int _post_control(const tll_msg_t *msg, int flags);

	int _process(long timeout, int flags);
	int _process_output();

	void bind(int fd, int seq = 0) { this->_update_fd(fd); _msg_addr = { fd, seq }; }
	const tcp_socket_addr_t & msg_addr() const { return _msg_addr; }

	int setup(const tcp_settings_t &settings);

	void _on_close()
	{
		this->close();
	}

	/// Hook called when output buffer is fully sent
	void _on_output_sent() {}

 protected:
	std::chrono::nanoseconds _timestamp;
	size_t rsize() const { return _rsize - _roff; }

	template <typename D>
	const D * rdataT(size_t off = 0, size_t size = sizeof(D)) const
	{
		off += _roff;
		if (off + size > _rsize)
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

	template <typename ... Args>
	int _sendv(const Args & ... args);

	int _sendmsg(const iovec * iov, size_t N);

	void _store_output(const void * base, size_t size, size_t offset = 0);

	std::chrono::nanoseconds _cmsg_timestamp(msghdr * msg);
};

template <typename T, typename S = TcpSocket<T>>
class TcpClient : public S
{
 protected:
	std::optional<tll::network::hostport> _peer;
	bool _timestamping = false;

	using addr_list_t = std::vector<tll::network::sockaddr_any>;
	addr_list_t _addr_list;
	addr_list_t::iterator _addr = _addr_list.end();

	tcp_settings_t _settings = {};

	using tcp_client_t = TcpClient<T>;
 public:
	static constexpr auto open_policy() { return Base<T>::OpenPolicy::Manual; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);

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
	static constexpr std::string_view channel_protocol() { return "tcp"; }

	int _init(const tll::Channel::Url &url, tll::Channel *master);

	int _open(const tll::ConstConfig &props);
	int _close();

	int _process(long timeout, int flags);

	void bind(int fd) { this->_update_fd(fd); }
};

template <typename T, typename C>
class TcpServer : public Base<T>
{
 protected:
	network::AddressFamily _af = network::AddressFamily::UNSPEC;
	std::string _host;
	unsigned short _port;
	int _addr_seq = 0;

	using tcp_server_t = TcpServer<T, C>;
	using tcp_server_socket_t = TcpServerSocket<T>;
	using tcp_socket_t = TcpSocket<C>;
	std::list<std::unique_ptr<Channel>> _sockets;
	std::map<int, tcp_socket_t *> _clients;
	bool _cleanup_flag = false;
	tcp_settings_t _settings = {};

 public:
	static constexpr std::string_view channel_protocol() { return "tcp"; }
	static constexpr auto child_policy() { return Base<T>::ChildPolicy::Many; }
	static constexpr auto open_policy() { return Base<T>::OpenPolicy::Manual; }
	static constexpr auto process_policy() { return Base<T>::ProcessPolicy::Never; }

	int _init(const tll::Channel::Url &url, tll::Channel *master);

	int _open(const tll::ConstConfig &props);
	int _close();

	int _post(const tll_msg_t *msg, int flags);

	void _on_child_connect(tcp_socket_t *, const tcp_connect_t *);
	void _on_child_error(tcp_socket_t *) {}
	void _on_child_closing(tcp_socket_t *);

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
	void _cleanup(tcp_socket_t *);
	tcp_socket_t * _lookup(const tll_addr_t &addr);
};

} // namespace tll::channel

#endif//_TLL_CHANNEL_TCP_H
