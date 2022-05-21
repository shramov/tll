/*
 * Copyright (c)2020-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_LOGGER_CONTEXT_H
#define _TLL_LOGGER_CONTEXT_H

#include <type_traits>
#include <variant>

#include "tll/logger.h"

namespace tll::logger {

namespace _ {

template <typename... Args>
struct arg_store_t : public fmt::format_arg_store<fmt::format_context, Args...>
{
	using fmt::format_arg_store<fmt::format_context, Args...>::format_arg_store;
	std::string operator () (fmt::string_view format) const { return fmt::vformat(format, fmt::format_args(*this)); }
};

template <>
struct arg_store_t<>
{
	std::string operator () (fmt::string_view format) const { return std::string(format.data(), format.size()); }
};

template <typename A0>
struct arg_store_t<A0> : public std::tuple<A0>
{
	using std::tuple<A0>::tuple;
	std::string operator () (fmt::string_view format) const { return fmt::format(format, std::get<0>(*this)); }
};

template <typename A0, typename A1>
struct arg_store_t<A0, A1> : public std::tuple<A0, A1>
{
	using std::tuple<A0, A1>::tuple;
	std::string operator () (fmt::string_view format) const { return fmt::format(format, std::get<0>(*this), std::get<1>(*this)); }
};

template <typename A0, typename A1, typename A2>
struct arg_store_t<A0, A1, A2> : public std::tuple<A0, A1, A2>
{
	using std::tuple<A0, A1, A2>::tuple;
	std::string operator () (fmt::string_view format) const { return fmt::format(format, std::get<0>(*this), std::get<1>(*this), std::get<2>(*this)); }
};

template <typename A0, typename A1, typename A2, typename A3>
struct arg_store_t<A0, A1, A2, A3> : public std::tuple<A0, A1, A2, A3>
{
	using std::tuple<A0, A1, A2, A3>::tuple;
	std::string operator () (fmt::string_view format) const { return fmt::format(format, std::get<0>(*this), std::get<1>(*this), std::get<2>(*this), std::get<3>(*this)); }
};

template <typename Fmt, bool Func, typename... Args>
struct _delayed_format_t {};

template <typename Fmt, typename... Args>
struct _delayed_format_t<Fmt, false, Args...>
{
	std::pair<Fmt, arg_store_t<std::decay_t<Args>...>> pair;
	_delayed_format_t(Fmt && fmt, Args && ... args) : pair(std::forward<Fmt>(fmt), {std::forward<std::decay_t<Args>>(args)... }) {}

	std::string operator () () const
	{
		try {
			return pair.second(pair.first);
		} catch (fmt::format_error &e) {
			return fmt::format("Invalid format '{}': {};", pair.first, e.what());
		}
	}
};

template <typename Func>
struct _delayed_format_t<Func, true>
{
	Func f;
	_delayed_format_t(Func && func) : f(std::move(func)) {}

	std::string operator () () const
	{
		try {
			return f();
		} catch (fmt::format_error &e) {
			return fmt::format("Invalid format: {};", e.what());
		}
	}
};
} // namespace _

template <typename Fmt, typename... Args>
using delayed_format_t = _::_delayed_format_t<Fmt, std::is_invocable_v<Fmt>, Args...>;

template <typename Log = Logger, typename Fmt = std::string_view, typename... Args>
class Prefix : public logger::Methods<Prefix<Log, Fmt, Args...>>
{
	template <typename L, typename F, typename... A> friend class Prefix;

	const Log &_log;

	mutable std::variant<std::string, delayed_format_t<Fmt, Args...>> _prefix;

	std::string_view _format_prefix() const
	{
		if (_prefix.index() == 0)
			return std::get<0>(_prefix);
		auto & f = std::get<1>(_prefix);
		try {
			return _prefix.template emplace<0>(std::string(f()));
		} catch (fmt::format_error &e) {
			return _prefix.template emplace<0>(fmt::format("Invalid format: {};", e.what()));
		}
	}

	template <typename Buf>
	const Logger * _fill_prefix(Buf &buf) const
	{
		const Logger * l = nullptr;
		if constexpr (!std::is_same_v<Log, Logger>)
			l = _log._fill_prefix(buf);
		else
			l = &_log;
		auto p = _format_prefix();
		auto size = buf.size();
		buf.resize(size + p.size());
		memcpy(buf.data() + size, p.data(), p.size());
		buf.push_back(' ');
		return l;
	}

 public:
	Prefix(const Log &logger, Fmt && fmt, Args && ... args) : _log(logger)
	{
		if constexpr (!std::is_invocable_v<Fmt> && sizeof...(Args) == 0) {
			_prefix.template emplace<0>(std::forward<Fmt>(fmt));
		} else {
			_prefix.template emplace<1>(std::forward<Fmt>(fmt), std::forward<Args>(args)...);
		}
	}

	template <typename F, typename... A>
	void log(Logger::level_t level, F format, A && ... args) const
	{
		if (this->level() > level) return;
		auto buf = Logger::tls_buf();
		buf->resize(0);
		auto l = _fill_prefix(*buf);
		try {
			fmt::format_to(std::back_inserter(*buf), format, std::forward<A>(args)...);
		} catch (fmt::format_error &e) {
			fmt::format_to(std::back_inserter(*buf), "Invalid format {}: {}", format, e.what());
		}
		buf->push_back('\0');
		l->log_buf(level, std::string_view(buf->data(), buf->size() - 1));
	}

	Logger::level_t level() const { return _log.level(); }
};

} // namespace tll::logger

#endif//_TLL_LOGGER_CONTEXT_H
