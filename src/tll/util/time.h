/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_TIME_H
#define _TLL_UTIL_TIME_H

#include "tll/util/conv.h"

#include <chrono>
#include <cstdint>

namespace tll {

using std::chrono::duration_cast;
using duration = std::chrono::duration<long long, std::nano>;
using time_point = std::chrono::time_point<std::chrono::system_clock, duration>;

namespace time {

static inline time_point now() { return std::chrono::system_clock::now(); }
static constexpr time_point epoch = {};

} // namespace time

template <typename T, typename R>
struct conv::parse<std::chrono::duration<T, R>>
{
	typedef std::chrono::duration<T, R> value_type;
	static result_t<value_type> to_any(std::string_view s)
	{
		using namespace std::chrono;
		if (s.size() == 0)
			return error("Empty value");
		auto sep = s.find_last_of("0123456789");
		if (sep == s.npos)
			return error("No digits found");
		auto ve = tll::conv::to_any<T>(s.substr(0, sep + 1));
		if (!ve) return error(ve.error());
		auto v = *ve;
		auto suffix = s.substr(sep + 1);
		if (suffix == "s") return duration_cast<value_type>(std::chrono::duration<T, std::ratio<1>>(v));
		else if (suffix == "ms") return duration_cast<value_type>(std::chrono::duration<T, std::milli>(v));
		else if (suffix == "us") return duration_cast<value_type>(std::chrono::duration<T, std::micro>(v));
		else if (suffix == "ns") return duration_cast<value_type>(std::chrono::duration<T, std::nano>(v));
		else if (suffix == "m") return duration_cast<value_type>(std::chrono::duration<T, std::ratio<60>>(v));
		else if (suffix == "h") return duration_cast<value_type>(std::chrono::duration<T, std::ratio<3600>>(v));
		else if (suffix == "d") return duration_cast<value_type>(std::chrono::duration<T, std::ratio<86400>>(v));
		else
			return error("Invalid suffix " + std::string(suffix));
	}
};

namespace {
template <typename T> static constexpr std::string_view _ratio_str();
template <> constexpr std::string_view _ratio_str<std::nano>() { return "ns"; }
template <> constexpr std::string_view _ratio_str<std::micro>() { return "us"; }
template <> constexpr std::string_view _ratio_str<std::milli>() { return "ms"; }
template <> constexpr std::string_view _ratio_str<std::ratio<1, 1>>() { return "s"; }
template <> constexpr std::string_view _ratio_str<std::ratio<60, 1>>() { return "m"; }
template <> constexpr std::string_view _ratio_str<std::ratio<3600, 1>>() { return "h"; }
template <> constexpr std::string_view _ratio_str<std::ratio<86400, 1>>() { return "d"; }
}

template <typename T, typename R>
struct conv::dump<std::chrono::duration<T, R>> : public conv::to_string_from_string_buf<std::chrono::duration<T, R>>
{
	typedef std::chrono::duration<T, R> value_type;

	template <typename Buf>
	static std::string_view to_string_buf(const value_type &value, Buf &buf)
	{
		auto r = tll::conv::to_string_buf<T>(value.count(), buf);
		return tll::conv::append(buf, r, _ratio_str<R>());
	}
};

} // namespace tll

#endif//_TLL_UTIL_TIME_H
