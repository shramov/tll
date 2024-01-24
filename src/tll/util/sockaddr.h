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

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <net/ethernet.h>

#include <fmt/format.h>

namespace tll::network {

static inline int nonblock(int fd)
{
	auto f = fcntl(fd, F_GETFL);
	if (f == -1) return errno;
	return fcntl(fd, F_SETFL, f | O_NONBLOCK);
}

template <typename T>
static inline int setsockoptT(int fd, int level, int optname, T v)
{
	return setsockopt(fd, level, optname, &v, sizeof(v));
}

namespace _ { // Hide enum so tll::network is not polluted
enum AddressFamily {
	UNSPEC = AF_UNSPEC,
	INET = AF_INET,
	INET6 = AF_INET6,
	UNIX = AF_UNIX,
};
}

using AddressFamily = _::AddressFamily;

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

	constexpr operator int () const noexcept { return _fd; }
	constexpr int get() const noexcept { return _fd; }

	int release() { int fd = _fd; _fd = -1; return fd; }
};

constexpr auto format_as(const scoped_socket &fd) noexcept { return fd.get(); }

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

	bool operator != (const sockaddr_in *rhs) const { return !(*this == rhs); }
	bool operator == (const sockaddr_in *rhs) const
	{
		auto lhs = in();
		return (lhs->sin_family == rhs->sin_family) &&
			(lhs->sin_port == rhs->sin_port) &&
			(lhs->sin_addr.s_addr == rhs->sin_addr.s_addr);
		return true;
	}

	bool operator != (const sockaddr_in6 *rhs) const { return !(*this == rhs); }
	bool operator == (const sockaddr_in6 *rhs) const
	{
		auto lhs = in6();
		return (lhs->sin6_family == rhs->sin6_family) &&
			(lhs->sin6_port == rhs->sin6_port) &&
			(lhs->sin6_flowinfo == rhs->sin6_flowinfo) &&
			(lhs->sin6_scope_id == rhs->sin6_scope_id) &&
			(memcmp(lhs->sin6_addr.s6_addr, rhs->sin6_addr.s6_addr, sizeof(in6_addr)) == 0);
		return true;
	}

	bool operator != (const sockaddr_any &rhs) const { return !(*this == rhs); }
	bool operator == (const sockaddr_any &rhs) const
	{
		if ((*this)->sa_family != rhs->sa_family)
			return false;
		if (rhs->sa_family == AF_UNIX) {
			if (size != rhs.size)
				return false;
			// See unix(7) for description of sun_path
			return memcmp(un(), rhs.un(), size) == 0;
		} else if (rhs->sa_family == AF_INET)
			return (*this) == rhs.in();
		else if (rhs->sa_family == AF_INET6)
			return (*this) == rhs.in6();
		return false;
	}
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
		l.back().size = offsetof(struct sockaddr_un, sun_path) + host.size() + 1;
		auto addr = l.back().un();
		addr->sun_family = AF_UNIX;
		if (host.size() >= sizeof(addr->sun_path))
			return error("Filename for Unix socket too long");
		memcpy(addr->sun_path, host.data(), host.size());
		/*
		 * Normal paths have trailing zero, abstract paths do not
		 * See unix(7) for sun_path description
		 */
		if (host[0] == '@') {
			l.back().size--;
			addr->sun_path[0] = '\0';
		} else
			addr->sun_path[host.size()] = 0;
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
	AddressFamily af = AddressFamily::UNSPEC;
	std::string host;
	unsigned short port = 0;
};

static inline tll::result_t<hostport> parse_hostport(std::string_view host, AddressFamily af = AddressFamily::UNSPEC)
{
	hostport r = { af };
	if (r.af == AddressFamily::UNSPEC && host.find('/') != host.npos)
		r.af = AddressFamily::UNIX;

	if (r.af == AddressFamily::UNIX) {
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
struct parse<tll::network::AddressFamily>
{
        static result_t<tll::network::AddressFamily> to_any(std::string_view s)
        {
		using tll::network::AddressFamily;
                return tll::conv::select(s, std::map<std::string_view, AddressFamily> {
			{"any", AddressFamily::UNSPEC},
			{"ipv4", AddressFamily::INET},
			{"ipv6", AddressFamily::INET6},
			{"unix", AddressFamily::UNIX}
		});
        }
};

template <>
struct dump<tll::network::AddressFamily> : public to_string_from_string_buf<tll::network::AddressFamily>
{
	template <typename Buf>
	static std::string_view to_string_buf(const tll::network::AddressFamily &v, Buf &buf)
        {
		using tll::network::AddressFamily;
		switch (v) {
		case AddressFamily::UNSPEC: return "any";
		case AddressFamily::INET: return "ipv4";
		case AddressFamily::INET6: return "ipv6";
		case AddressFamily::UNIX: return "unix";
		}
		return "UNKNOWN";
        }
};

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

template <>
struct dump<in_addr> : public to_string_from_string_buf<in_addr>
{
	template <typename Buf>
	static std::string_view to_string_buf(const in_addr &v, Buf &buf)
	{
		buf.resize(INET_ADDRSTRLEN);
		if (inet_ntop(AF_INET, &v, (char *) buf.data(), INET_ADDRSTRLEN) == nullptr)
			return "INVALID-IPV4";
		return std::string_view((const char *) buf.data());
	}
};

template <>
struct parse<in_addr>
{
	static result_t<in_addr> to_any(std::string_view s)
	{
		in_addr r;
		std::string str(s);
		if (!inet_pton(AF_INET, str.c_str(), &r))
			return error("Invalid IPv4 address");
		return r;
	}
};

template <>
struct dump<in6_addr> : public to_string_from_string_buf<in6_addr>
{
	template <typename Buf>
	static std::string_view to_string_buf(const in6_addr &v, Buf &buf)
	{
		buf.resize(INET6_ADDRSTRLEN);
		if (inet_ntop(AF_INET6, &v, (char *) buf.data(), INET6_ADDRSTRLEN) == nullptr)
			return "INVALID-IPV6";
		return std::string_view((const char *) buf.data());
	}
};

template <>
struct parse<in6_addr>
{
	static result_t<in6_addr> to_any(std::string_view s)
	{
		in6_addr r;
		std::string str(s);
		if (!inet_pton(AF_INET6, str.c_str(), &r))
			return error("Invalid IPv6 address");
		return r;
	}
};

template <>
struct dump<ether_addr> : public to_string_from_string_buf<ether_addr>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const ether_addr &v, Buf &buf)
	{
		buf.resize(2 * 6 + 5 + 1);
		static const char lookup[] = "0123456789abcdef";
		auto begin = (char *) buf.data();
		auto ptr = begin;
		auto octet = (const uint8_t *) &v;
		for (auto i = 0u; i < sizeof(v); i++) {
			unsigned char lo = octet[i] & 0xfu;
			unsigned char hi = (octet[i] & 0xffu) >> 4;
			if (i != 0)
				*ptr++ = ':';
			*ptr++ = lookup[hi];
			*ptr++ = lookup[lo];
		}
		*ptr++ = '\0';
		return { (const char *) buf.data(), 2 * 6 + 5 };
	}
};

template <>
struct parse<ether_addr>
{
	using value_type = ether_addr;
	static result_t<value_type> to_any(std::string_view s)
	{
		if (s.size() != 2 * 6 + 5)
			return error("Invalid string length");
		auto end = s.data() + 2 * 6 + 5;

		ether_addr r = {};
		auto rptr = (uint8_t *) &r;
		for (auto ptr = s.data(); ptr != end;) {
			if (ptr != s.data()) {
				if (*ptr++ != ':')
					return error("Invalid separator");
			}
			auto hi = Digits<16>::decode(*ptr++);
			auto lo = Digits<16>::decode(*ptr++);
			if (hi == 16 || lo == 16)
				return error("Invalid digits");
			*rptr++ = (hi << 4) | lo;
		}
		return r;
	}
};
} // namespace tll::conv

#endif//_TLL_UTIL_SOCKADDR_H
