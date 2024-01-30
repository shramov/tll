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

template <typename... Args>
class DelayedFormat
{
	format_string<Args...> _fmt;
	std::tuple<std::decay_t<Args>...> _args;

public:
	DelayedFormat(format_string<Args...> fmt, Args && ... args) : _fmt(fmt), _args({std::forward<std::decay_t<Args>>(args)... }) {}

	std::string operator () () const
	{
		try {
			using Index = std::make_index_sequence<sizeof...(Args)>;
			return apply(Index());
		} catch (fmt::format_error &e) {
			return fmt::format("Invalid format '{}': {};", static_cast<fmt::string_view>(_fmt), e.what());
		}
	}

	template <size_t... Idx>
	std::string apply(std::index_sequence<Idx...>) const
	{
#if FMT_VERSION >= 90000
		return fmt::vformat(_fmt, fmt::make_format_args(std::get<Idx>(_args)...));
#else
		return fmt::format(_fmt, std::get<Idx>(_args)...);
#endif
	}
};

template <typename Log, typename Func>
class Prefix : public logger::Methods<Prefix<Log, Func>>
{
	const Log &_log;

	mutable std::variant<std::string, Func> _prefix;

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

 public:
	Prefix(const Log &logger, Func && func) : _log(logger), _prefix(std::in_place_index<1>, std::move(func)) {}

	template <typename F, typename... A>
	void log(Logger::level_t level, F format, A && ... args) const
	{
#if FMT_VERSION < 80000
		if (this->level() > level) return;
		auto buf = Logger::tls_buf();
		buf->resize(0);
		auto l = fill_prefix(*buf);
		try {
			fmt::format_to(std::back_inserter(*buf), format, std::forward<A>(args)...);
		} catch (fmt::format_error &e) {
			fmt::format_to(std::back_inserter(*buf), "Invalid format {}: {}", static_cast<fmt::string_view>(format), e.what());
		}
		buf->push_back('\0');
		l->log_buf(level, std::string_view(buf->data(), buf->size() - 1));
#else
		vlog(level, format, fmt::make_format_args(args...));
#endif
	}

#if FMT_VERSION >= 80000
	void vlog(Logger::level_t level, fmt::string_view format, fmt::format_args args) const
	{
		if (this->level() > level) return;
		auto buf = Logger::tls_buf();
		buf->resize(0);
		auto l = fill_prefix(*buf);
		try {
			fmt::vformat_to(std::back_inserter(*buf), format, args);
		} catch (fmt::format_error &e) {
			fmt::format_to(std::back_inserter(*buf), "Invalid format {}: {}", format, e.what());
		}
		buf->push_back('\0');
		l->log_buf(level, std::string_view(buf->data(), buf->size() - 1));
	}
#endif

	Logger::level_t level() const { return _log.level(); }

	template <typename Buf>
	const Logger * fill_prefix(Buf &buf) const
	{
		const Logger * l = nullptr;
		if constexpr (!std::is_same_v<Log, Logger>)
			l = _log.fill_prefix(buf);
		else
			l = &_log;
		auto p = _format_prefix();
		auto size = buf.size();
		buf.resize(size + p.size());
		memcpy(buf.data() + size, p.data(), p.size());
		buf.push_back(' ');
		return l;
	}
};

} // namespace tll::logger

#endif//_TLL_LOGGER_CONTEXT_H
