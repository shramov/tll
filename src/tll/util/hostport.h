// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_UTIL_HOSTPORT_H
#define _TLL_UTIL_HOSTPORT_H

#include <tll/util/props.h>
#include <tll/util/result.h>
#include <tll/util/sockaddr.h>

namespace tll {
namespace network {

struct hostport
{
	AddressFamily af = AddressFamily::UNSPEC;
	std::string host;
	unsigned short port = 0;

	auto resolve(int socktype) const { return tll::network::resolve(af, socktype, host, port); }

	int set_af(AddressFamily v)
	{
		if (v == AddressFamily::UNSPEC)
			return 0;
		if (af != AddressFamily::UNSPEC && af != v)
			return EINVAL;
		af = v;
		return 0;
	}
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

} // namespace network

template <>
struct getter::GetT<network::hostport>
{
	template <typename Cfg>
	static result_t<std::optional<network::hostport>> get(const Cfg &cfg, std::string_view key)
	{
		auto v = cfg.get(key);
		if (!v || !v->size()) {
			auto host = getter::get(cfg, std::string(key) + ".host");
			if (!host || !host->size()) {
				auto path = getter::get(cfg, std::string(key) + ".path");
				if (!path || !path->size())
					return std::nullopt;
				return network::hostport { .af = network::AddressFamily::UNIX, .host = std::string(*path) };
			}
			auto port = getter::getT<Cfg, uint16_t>(cfg, std::string(key) + ".port");
			if (!port)
				return error(fmt::format("Invalid port: {}", port.error()));
			auto af = network::AddressFamily::UNSPEC;
			if (host->find(':') != host->npos)
				af = network::AddressFamily::INET6;
			return network::hostport { .af = af, .host = std::string(*host), .port = *port };
		} else if (!v->size())
			return std::nullopt;
		if (auto r = network::parse_hostport(*v); r)
			return std::make_optional(*r);
		else
			return error(r.error());
	}
};

} // namespace tll

#endif//_TLL_UTIL_HOSTPORT_H
