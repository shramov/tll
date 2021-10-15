/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_PROPS_H
#define _TLL_UTIL_PROPS_H

#include <fmt/format.h>
#include <map>
#include <optional>
#include <string_view>
#include <type_traits>

#include "tll/util/conv.h"
#include "tll/util/getter.h"
#include "tll/util/result.h"
#include "tll/util/string.h"

namespace tll {

namespace getter {
template <typename Klass, typename T>
inline result_t<T> getT(const Klass &obj, std::string_view key)
{
	auto v = tll::getter::getter_api<Klass>::get(obj, key);
	if (!v || !v->size())
		return error("Missing value");
	auto r = conv::to_any<T>(*v);
	if (!r)
		return error(fmt::format("Invalid value '{}': {}", *v, r.error()));
	return r;
}

template <typename Klass, typename T>
inline result_t<T> getT(const Klass &obj, std::string_view key, const T &def)
{
	auto v = tll::getter::getter_api<Klass>::get(obj, key);
	if (!v || !v->size())
		return def;
	auto r = conv::to_any<T>(*v);
	if (!r)
		return error(fmt::format("Invalid value '{}': {}", *v, r.error()));
	return r;
}

template <typename Klass, typename T>
inline result_t<T> getT(const Klass &obj, std::string_view key, const T & def, const std::map<std::string_view, T> m)
{
	auto v = tll::getter::getter_api<Klass>::get(obj, key);
	if (!v || !v->size())
		return def;
	auto r = conv::select(*v, m);
	if (!r)
		return error(fmt::format("Invalid value '{}': {}", *v, r.error()));
	return r;
}

} // namespace getter

template <typename Klass>
struct PropsGetter
{
	template <typename T>
	result_t<T> getT(std::string_view key) const
	{
		auto self = static_cast<const Klass *>(this);
		return tll::getter::getT<Klass, T>(*self, key);
	}

	template <typename T>
	result_t<T> getT(std::string_view key, const T& def) const
	{
		auto self = static_cast<const Klass *>(this);
		return tll::getter::getT<Klass, T>(*self, key, def);
	}

	template <typename T>
	result_t<T> getT(std::string_view key, const T& def, const std::map<std::string_view, T> m) const
	{
		auto self = static_cast<const Klass *>(this);
		return tll::getter::getT<Klass, T>(*self, key, def, m);
	}
};

template <typename T>
struct PropsPrefix
{
	using string_type = typename tll::getter::getter_api<T>::string_type;

	T props;
	std::string prefix;

	std::string make_key(std::string_view key) const { return prefix + "." + std::string(key); }

	bool has(std::string_view key) const { return tll::getter::has(props, make_key(key)); }

	decltype(auto) get(std::string_view key) const { return tll::getter::get(props, make_key(key)); }
};

template <typename T>
PropsPrefix<T> make_props_prefix(const T &props, std::string_view prefix)
{
	return PropsPrefix<T> { props, std::string(prefix) };
};

template <typename ... Chain>
struct PropsChainT;

template <>
struct PropsChainT<> : public PropsGetter<PropsChainT<>>
{
	using string_type = std::string;

	bool has(std::string_view key) const { return false; }

	std::optional<std::string> get(std::string_view key) const { return std::nullopt; }
};

template <typename U, typename ... Chain>
struct PropsChainT<U, Chain...> : public PropsGetter<PropsChainT<U, Chain...>>
{
	using string_type = std::string;
	U props;
	PropsChainT<Chain...> chain;

	PropsChainT(const U &u, const Chain & ... c) : props(u), chain(c...) {}

	bool has(std::string_view key) const { return getter::has(props, key) || chain.has(key); }

	std::optional<string_type> get(std::string_view key) const
	{
		auto v = getter::get(props, key);
		if (v && v->size())
			return string_type(*v);
		return chain.get(key);
	}
};

template <typename ... Chain>
PropsChainT<Chain...> make_props_chain(const Chain & ... chain)
{
	return PropsChainT<Chain...>(chain...);
};

template <typename U>
class PropsReaderT
{
	U _props;
	//std::string_view _prefix;
	std::optional<std::string> _error;
	using getter_api = typename tll::getter::getter_api<std::remove_cv_t<std::remove_reference_t<U>>>;
	using string_type = typename getter_api::string_type;

	std::string format(std::string_view key, std::string_view e)
	{
		return fmt::format("Failed to load '{}': {}", key, e);
	}

	template <typename T>
	const T _getT(std::string_view key, const T * def)
	{
		if (_error)
			return def?*def:T();
		auto v = get(key);
		if (!v) {
			if (!def)
				_error = format(key, "Missing value");
			return def?*def:T();
		}
		auto r = conv::to_any<T>(*v);
		if (!r) {
			_error = format(key, fmt::format("Invalid value '{}': {}", *v, r.error()));
			return def?*def:T();
		}
		return std::move(*r);
	}

 public:
	PropsReaderT(const U url) : _props(url) {}

	bool has(std::string_view key) const { return getter_api::has(_props, key); }
	decltype(auto) get(std::string_view key) { return getter_api::get(_props, key); }

	template <typename T> T getT(std::string_view key) { return _getT<T>(key, nullptr); }
	template <typename T> T getT(std::string_view key, const T& def) { return _getT<T>(key, &def); }
	//
	//template <typename T> T getT(std::string_view key) { return update(key, _props.template getT<T>(key), T()); }
	//template <typename T> T getT(std::string_view key, const T& def) { return update(key, _props.template getT<T>(key, def), def); }
	template <typename T> T getT(std::string_view key, const T& def, const std::map<std::string_view, T> &m)
	{
		if (_error) return def;
		auto v = _props.get(key);
		if (!v) return def;
		auto r = conv::select(*v, m);
		if (r)
			return std::move(*r);
		_error = format(key, fmt::format("Invalid value '{}': {}", *v, r.error()));
		return def;
	}

	operator bool () const { return !_error; }
	std::string_view error() { return *_error; }
};

template <typename T>
PropsReaderT<T> make_props_reader(const T &p) { return { p }; }

} // namespace tll

#endif//_TLL_UTIL_PROPS_H
