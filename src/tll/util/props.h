/*
 * Copyright (c) 2018-2019 Pavel Shramov <shramov@mexmat.net>
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
#include "tll/util/result.h"
#include "tll/util/string.h"

namespace tll {

template <typename Klass>
struct PropsGetter
{
	template <typename T>
	result_t<T> getT(const std::string_view &key) const
	{
		auto self = static_cast<const Klass *>(this);
		auto v = self->get(key);
		if (!v || !v->size())
			return error("Missing value");
		auto r = conv::to_any<T>(*v);
		if (!r)
			return error(fmt::format("Invalid value '{}': {}", *v, r.error()));
		return r;
	}

	template <typename T>
	result_t<T> getT(const std::string_view &key, const T& def) const
	{
		auto self = static_cast<const Klass *>(this);
		auto v = self->get(key);
		if (!v || !v->size())
			return def;
		auto r = conv::to_any<T>(*v);
		if (!r)
			return error(fmt::format("Invalid value '{}': {}", *v, r.error()));
		return r;
	}

	template <typename T>
	result_t<T> getT(const std::string_view &key, const T& def, const std::map<std::string_view, T> m) const
	{
		auto self = static_cast<const Klass *>(this);
		auto v = self->get(key);
		if (!v || !v->size())
			return def;
		auto r = conv::select(*v, m);
		if (!r)
			return error(fmt::format("Invalid value '{}': {}", *v, r.error()));
		return r;
	}
};

template <typename ... Chain>
struct PropsChainT;

template <>
struct PropsChainT<>
{
	using string_type = std::string;
	bool has(const std::string_view &key) { return false; }

	std::optional<std::string> get(const std::string_view &key) { return std::nullopt; }
};

template <typename U, typename ... Chain>
struct PropsChainT<U, std::string_view, Chain...>
{
	using string_type = std::string;
	using pair_type = std::pair<const U *, std::string_view>;
	const U & props;
	std::string prefix;
	PropsChainT<Chain...> chain;

	PropsChainT(const U &u, const std::string_view &p, const Chain & ... c) : props(u), prefix(p), chain(c...)
	{
		if (prefix.size() && prefix.back() != '.')
			prefix += '.';
	}

	bool has(const std::string_view &key) { return props.has(prefix + std::string(key)) || chain.has(key); }

	std::optional<string_type> get(const std::string_view &key)
	{
		auto k = prefix + std::string(key);
		auto v = props.get(k);
		if (v && v->size())
			return string_type(*v);
		return chain.get(key);
	}
};

template <typename U, typename ... Chain>
struct PropsChainT<U, Chain...>
{
	using string_type = std::string;
	const U & props;
	PropsChainT<Chain...> chain;

	PropsChainT(const U &u, const Chain & ... c) : props(u), chain(c...) {}

	bool has(const std::string_view &key) { return props.has(key) || chain.has(key); }

	std::optional<string_type> get(const std::string_view &key)
	{
		auto v = props.get(key);
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
	using string_type = typename std::remove_cv_t<std::remove_reference_t<U>>::string_type;

	std::string format(const std::string_view &key, const std::string_view &e)
	{
		return fmt::format("Failed to load '{}': {}", key, e);
	}

	template <typename T>
	const T _getT(const std::string_view &key, const T * def)
	{
		if (_error)
			return def?*def:T();
		auto v = _props.get(key);
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
	PropsReaderT(const U url, const std::string_view prefix = "") : _props(url) {} //, _prefix(prefix) {}

	bool has(const std::string_view &key) const { return _props.has(key); }
	decltype(auto) get(const std::string_view &key) { return _props.get(key); }

	template <typename T> T getT(const std::string_view &key) { return _getT<T>(key, nullptr); }
	template <typename T> T getT(const std::string_view &key, const T& def) { return _getT<T>(key, &def); }
	//
	//template <typename T> T getT(const std::string_view &key) { return update(key, _props.template getT<T>(key), T()); }
	//template <typename T> T getT(const std::string_view &key, const T& def) { return update(key, _props.template getT<T>(key, def), def); }
	template <typename T> T getT(const std::string_view &key, const T& def, const std::map<std::string_view, T> &m)
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
	const std::string_view error() { return *_error; }
};

template <typename T>
PropsReaderT<const T &> make_props_reader(const T &p, const std::string_view prefix = "") { return { p, prefix }; }

} // namespace tll

#endif//_TLL_UTIL_PROPS_H
