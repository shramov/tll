/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_SOCKADDR_H
#define _TLL_UTIL_SOCKADDR_H

#include "tll/util/conv.h"
#include "tll/util/result.h"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace tll::network {

class scoped_socket
{
	int _fd = -1;

 public:
	explicit scoped_socket(int fd) : _fd(fd) {}
	~scoped_socket() { reset(); }

	void reset(int fd = -1)
	{
		if (_fd != -1)
			::close(_fd);
		_fd = fd;
	}

	operator int () const { return _fd; }
	int get() const { return _fd; }

	int release() { int fd = _fd; _fd = -1; return fd; }
};

struct sockaddr_any
{
	socklen_t size = 0;
	char buf[128] = { 0 }; // Space for unix, ip, ip6, ...

	struct sockaddr * operator -> () { return (struct sockaddr *) buf; }
	const struct sockaddr * operator -> () const { return (struct sockaddr *) buf; }

	operator struct sockaddr * () { return (struct sockaddr *) buf; }
	operator const struct sockaddr * () const { return (const struct sockaddr *) buf; }

	struct sockaddr_in * in() { return (struct sockaddr_in *) buf; }
	const struct sockaddr_in * in() const { return (const struct sockaddr_in *) buf; }

	struct sockaddr_in6 * in6() { return (struct sockaddr_in6 *) buf; }
	const struct sockaddr_in6 * in6() const { return (const struct sockaddr_in6 *) buf; }

	struct sockaddr_un * un() { return (struct sockaddr_un *) buf; }
	const struct sockaddr_un * un() const { return (const struct sockaddr_un *) buf; }
};

static inline tll::result_t<std::vector<tll::network::sockaddr_any>> resolve(int af, int socktype, std::string_view host, unsigned short port)
{
	struct addrinfo hints = {};
	struct addrinfo *result;

	auto nport = htons(port);

	hints.ai_family = af;
	hints.ai_socktype = socktype;

	std::vector<tll::network::sockaddr_any> l;

	if (af == AF_UNIX)
	{
		l.resize(1);
		l.back().size = sizeof(sockaddr_un);
		auto addr = l.back().un();
		addr->sun_family = AF_UNIX;
		if (host.size() > sizeof(addr->sun_path))
			return error("Filename for Unix socket too long");
		memcpy(addr->sun_path, host.data(), host.size());
		if (host[0] == '@')
			addr->sun_path[0] = '\0';
		return l;
	}

	if (host == "*") {
		switch (af) {
		case AF_UNSPEC:
		case AF_INET6:
			l.push_back({sizeof(sockaddr_in6)});
			l.back().in6()->sin6_family = AF_INET6;
			l.back().in6()->sin6_port = htons(port);
			l.back().in6()->sin6_addr = IN6ADDR_ANY_INIT;
			break;
		case AF_INET:
			l.push_back({sizeof(sockaddr_in)});
			l.back().in()->sin_family = AF_INET;
			l.back().in()->sin_port = htons(port);
			l.back().in()->sin_addr.s_addr = INADDR_ANY;
			break;
		default:
			return error("Can not fill * for unknown af " + conv::to_string(af));
		}
		return l;
	}

	std::string h(host); // Ensure trailing 0
	if (int r = getaddrinfo(h.c_str(), 0, &hints, &result))
		return error(gai_strerror(r));

	if (!result)
		return error("No matches found");
	for (auto ai = result; ai; ai = ai->ai_next) {
		l.push_back({ai->ai_addrlen});
		memcpy(l.back().buf, ai->ai_addr, ai->ai_addrlen);
		l.back().in()->sin_port = nport;
	}
	freeaddrinfo(result);
	return l;
}

struct hostport
{
	int af = AF_UNSPEC;
	std::string host;
	unsigned short port = 0;
};

static inline tll::result_t<hostport> parse_hostport(std::string_view host, int af = AF_UNSPEC)
{
	hostport r = { af };
	if (r.af == AF_UNSPEC && host.find('/') != host.npos)
		r.af = AF_UNIX;

	if (r.af == AF_UNIX) {
		r.host = host;
		return r;
	}

	auto sep = host.find_last_of(':');
	if (sep == host.npos)
		error("Invalid host:port pair, no ':' separator found");
	auto p = conv::to_any<unsigned short>(host.substr(sep + 1));
	if (!p)
		return error(fmt::format("Invalid port '{}': {}", host.substr(sep + 1), p.error()));

	r.port = *p;
	r.host = host.substr(0, sep);
	return r;
}

} // tll::network

namespace tll::conv {

template <>
struct dump<sockaddr_un> : public to_string_from_string_buf<sockaddr_un>
{
	template <typename Buf>
	static std::string_view to_string_buf(const sockaddr_un &v, Buf &buf)
	{
		auto path = v.sun_path;
		if (*path != '\0') {
			const auto len = strnlen(path, sizeof(v.sun_path));
			return tll::conv::append(buf, "", std::string_view(path, len));
		}
		path++;
		const auto len = strnlen(path + 1, sizeof(v.sun_path) - 1);
		return tll::conv::append(buf, "@", std::string_view(path, len));
	}
};

namespace {
template <typename Buf>
static std::string_view _sockaddr_to_str(const sockaddr * v, size_t size, Buf &buf)
{
	if (v->sa_family == AF_UNIX) {
		if (size < sizeof(sockaddr_un))
			return tll::conv::append(buf, "Invalid sockaddr for AF_UNIX", "");
		return tll::conv::to_string_buf(*(const sockaddr_un *)v, buf);
	}
	buf.resize(64); // Space for IPv6, separator and port
	char port[6] = {0};
	auto r = getnameinfo(v, size, (char *) buf.data(), buf.size(), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
	if (r)
		return tll::conv::append(buf, "getnameinfo failed: ", gai_strerror(r));
	auto s = tll::conv::append(buf, std::string_view((const char *) buf.data()), ":");
	return tll::conv::append(buf, s, port);
}
}

template <>
struct dump<sockaddr_in> : public to_string_from_string_buf<sockaddr_in>
{
	template <typename Buf>
	static std::string_view to_string_buf(const sockaddr_in &v, Buf &buf)
	{
		return _sockaddr_to_str((const sockaddr *) &v, sizeof(v), buf);
	}
};

template <>
struct dump<sockaddr_in6> : public to_string_from_string_buf<sockaddr_in6>
{
	template <typename Buf>
	static std::string_view to_string_buf(const sockaddr_in6 &v, Buf &buf)
	{
		return _sockaddr_to_str((const sockaddr *) &v, sizeof(v), buf);
	}
};

template <>
struct dump<tll::network::sockaddr_any> : public to_string_from_string_buf<tll::network::sockaddr_any>
{
	template <typename Buf>
	static std::string_view to_string_buf(const tll::network::sockaddr_any &v, Buf &buf)
	{
		if (v->sa_family == AF_UNIX)
			return tll::conv::to_string_buf(*v.un(), buf);
		return _sockaddr_to_str(v, v.size, buf);
	}
};
} // namespace tll::conv

#endif//_TLL_UTIL_SOCKADDR_H
