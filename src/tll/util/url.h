/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_URL_H
#define _TLL_UTIL_URL_H

#include <optional>
#include <map>
#include <string_view>
#include <type_traits>

#include "tll/util/browse.h"
#include "tll/util/conv.h"
#include "tll/util/props.h"
#include "tll/util/result.h"
#include "tll/util/string.h"

namespace tll {

template <typename Str>
class PropsT : public std::map<Str, Str, std::less<>>, public PropsGetter<PropsT<Str>>
{
 public:
	typedef Str string_type;
	typedef std::map<Str, Str, std::less<>> map_t;

	static tll::result_t<PropsT<Str>> parse(std::string_view s)
	{
		PropsT<Str> r;
		for (const auto & i : split<';'>(s)) {
			if (i.empty()) continue;
			auto sep = i.find_first_of('=');
			if (sep == i.npos)
				return tll::error("Missing '='");
			auto key = i.substr(0, sep);
			auto value = i.substr(sep + 1);
			if (!r.insert(std::make_pair(Str(key), Str(value))).second)
				return tll::error("Duplicate key: " + std::string(key));
		}
		return std::move(r);
	}

	bool has(std::string_view key) const { return map_t::find(key) != map_t::end(); }

	std::optional<std::string_view> get(std::string_view key) const
	{
		auto v = map_t::find(key);
		if (v == map_t::end())
			return std::nullopt;
		return v->second;
	}

	std::vector<std::pair<Str, Str>> browse(std::string_view mask) const
	{
		std::vector<std::pair<Str, Str>> r;
		auto mv = splitv<'.', false>(mask);
		for (auto & i : *this) {
			if (match(mv, i.first))
				r.push_back(i);
		}
		return r;
	}
};

typedef PropsT<std::string_view> PropsView;
typedef PropsT<std::string> Props;

typedef PropsReaderT<const PropsView &> PropsViewReader;
typedef PropsReaderT<const Props &> PropsReader;

template <typename Str>
class UrlT : public PropsT<Str>
{
public:
	Str proto;
	Str host;

	static tll::result_t<UrlT<Str>> parse(std::string_view s)
	{
		UrlT<Str> r;
		auto sep = s.find("://");
		if (sep == s.npos)
			return error("No :// found in url");
		r.proto = s.substr(0, sep);
		if (r.proto.size() == 0)
			return error("Empty protocol in url");
		auto hsep = s.find_first_of(';', sep + 3);
		r.host = s.substr(sep + 3, hsep - (sep + 3));
		if (hsep == s.npos)
			return r;
		auto props = PropsT<Str>::parse(s.substr(hsep + 1));
		if (!props)
			return error(props.error());
		std::swap(r, *props);
		return std::move(r);
	}
};


typedef UrlT<std::string_view> UrlView;
typedef UrlT<std::string> Url;

typedef PropsReaderT<const UrlView &> UrlViewReader;
typedef PropsReaderT<const Url &> UrlReader;

template <typename Str>
struct conv::dump<PropsT<Str>> : public conv::to_string_buf_from_string<PropsT<Str>>
{
	static std::string to_string(const PropsT<Str> &props)
	{
		std::string r;
		for (auto & p : props) {
			if (r.size()) r += ";";
			r += std::string(p.first) + "=" + std::string(p.second);
		}
		return r;
	}
};

template <typename Str>
struct conv::dump<UrlT<Str>> : public conv::to_string_buf_from_string<UrlT<Str>>
{
	static std::string to_string(const UrlT<Str> &url)
	{
		std::string r = std::string(url.proto) + "://" + std::string(url.host);
		auto p = ::tll::conv::to_string<PropsT<Str>>(url);
		if (!p.size()) return r;
		return r + ";" + p;
	}
};

} // namespace tll

#endif//_TLL_UTIL_URL_H
