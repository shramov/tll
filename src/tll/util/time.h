/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_TIME_H
#define _TLL_UTIL_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

/// Get nanoseconds from epoch and update cached value
long long tll_time_now();

/** Get cached time (if enabled) else same as @ref tll_time_now
 *
 * Time cache is thread local and is controlled by @ref tll_time_cache_enable function
 */
long long tll_time_now_cached();

/** Enable or disable thread local time cache
 * Cache is disabled initially and it's enabled when usage counter is non-zero.
 *
 * @param enable 0 for decrease, non-zero for increase of usage counter
 */
void tll_time_cache_enable(int enable);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#include <tll/conv/numeric.h>

#include <chrono>
#include <cstdint>

namespace tll {

using std::chrono::duration_cast;
using duration = std::chrono::duration<long long, std::nano>;
using time_point = std::chrono::time_point<std::chrono::system_clock, duration>;

namespace time {

//static inline time_point now() { return std::chrono::system_clock::now(); }
static inline time_point now() { return time_point(duration(tll_time_now())); }
static inline time_point now_cached() { return time_point(duration(tll_time_now_cached())); }
static constexpr time_point epoch = {};

static inline void cache_enable(bool enable) { tll_time_cache_enable(enable); }

} // namespace time

template <typename To, typename R, typename P>
result_t<To> duration_cast_exact(const std::chrono::duration<R, P> &d)
{
	using From = std::chrono::duration<R, P>;
	if constexpr (std::is_convertible_v<From, To>) // Implicitly convertible
		return To(d);
	else if constexpr (std::is_floating_point_v<typename From::rep>) {
		auto r = duration_cast<To>(d);
		auto dt = d - duration_cast<From>(r);
		if (dt.count() == 0) // epsilon checks?
			return r;
		return error("Inexact conversion");
	} else {
		auto r = duration_cast<To>(d);
		auto dt = d - duration_cast<From>(r);
		if (dt.count() == 0)
			return r;
		return error("Inexact conversion");

	}
}

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
		else if (suffix == "ms") return duration_cast_exact<value_type>(std::chrono::duration<T, std::milli>(v));
		else if (suffix == "us") return duration_cast_exact<value_type>(std::chrono::duration<T, std::micro>(v));
		else if (suffix == "ns") return duration_cast_exact<value_type>(std::chrono::duration<T, std::nano>(v));
		else if (suffix == "m") return duration_cast_exact<value_type>(std::chrono::duration<T, std::ratio<60>>(v));
		else if (suffix == "h") return duration_cast_exact<value_type>(std::chrono::duration<T, std::ratio<3600>>(v));
		else if (suffix == "d") return duration_cast_exact<value_type>(std::chrono::duration<T, std::ratio<86400>>(v));
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

	static constexpr bool fmt_has_formatter = true;

	template <typename Buf>
	static std::string_view to_string_buf(const value_type &value, Buf &buf)
	{
		auto r = tll::conv::to_string_buf<T>(value.count(), buf);
		return tll::conv::append(buf, r, _ratio_str<R>());
	}
};

template <typename D>
struct conv::dump<std::chrono::time_point<std::chrono::system_clock, D>> :
		public to_string_from_string_buf<std::chrono::time_point<std::chrono::system_clock, D>>
{
	using value_type = typename std::chrono::time_point<std::chrono::system_clock, D>;

	template <typename Buf>
	static std::string_view to_string_buf(const value_type &v, Buf &buf)
	{
		using namespace std::chrono;

		auto ts = v.time_since_epoch();
		auto sec = duration_cast<seconds>(ts);
		auto ns = duration_cast<nanoseconds>(ts - sec);

		time_t csec = sec.count();
		struct tm parts = {};

		if (!gmtime_r(&csec, &parts))
			return conv::to_string_buf(std::string_view("Overflow"), buf);

		if constexpr (std::is_same_v<typename value_type::period, std::ratio<86400, 1>> &&
				!std::is_floating_point_v<typename value_type::rep>) {
			buf.resize(11);
			auto r = strftime((char *) buf.data(), buf.size(), "%Y-%m-%d", &parts);
			return std::string_view { buf.data(), r };
		}

		buf.resize(10 + 1 + 8 + 1 + 9 + 1); // 2000-01-02T03:04:05.0123456789Z
		strftime((char *) buf.data(), buf.size(), "%Y-%m-%dT%H:%M:%S", &parts);
		std::string_view r(buf.data(), 10 + 1 + 8);
		if (ns.count() == 0)
			return r;

		auto d = ns.count();
		auto end = const_cast<char *>(r.end() + 9);
		while (end != r.end()) {
			auto digit = d % 10;
			d = d / 10;
			*(end--) = '0' + digit;
		}
		*end = '.';
		end += 9;
		while (*end == '0')
			end--;

		return std::string_view(r.data(), end - r.data() + 1);
	}
};

template <typename D>
struct conv::parse<std::chrono::time_point<std::chrono::system_clock, D>> :
		public to_string_from_string_buf<std::chrono::time_point<std::chrono::system_clock, D>>
{
	using value_type = typename std::chrono::time_point<std::chrono::system_clock, D>;

	static result_t<value_type> to_any(std::string_view str)
	{
		using namespace std::chrono;

		if (str.size() < 10)
			return error("Time string too short");
		if (str.back() == 'Z')
			str = str.substr(0, str.size() - 1);

		auto tail = str;
		struct tm date = {};

		{
			auto r = strptime(str.data(), "%Y-%m-%d", &date);
			if (r == nullptr)
				return error("Failed to parse date part");
			tail = str.substr(r - str.begin());
		}

		if (tail.size() >= 1) {
			if (tail[0] != 'T' && str[10] != ' ')
				return error("Invalid date-time separator: '" + std::string(tail.substr(0, 1)) + "'");
			tail = tail.substr(1);
		}

		if (tail.size() >= 8) {
			struct tm time = {};
			auto r = strptime(tail.data(), "%H:%M:%S", &time);
			if (r == nullptr)
				return error("Failed to parse time part");
			tail = tail.substr(r - tail.data());

			date.tm_hour = time.tm_hour;
			date.tm_min = time.tm_min;
			date.tm_sec = time.tm_sec;
		} else if (tail.size())
			return error("Time part of string too short: '" + std::string(tail) + "'");

		typename value_type::duration subsecond = {};
		if (tail.size() && tail[0] == '.') {
			tail = tail.substr(1);

			auto r = conv::to_any_uint_base<unsigned, 999999999, 10>(tail);
			if (!r)
				return error("Invalid subsecond part: " + std::string(r.error()));
			auto sub = *r;
			for (int i = tail.size(); i < 9; i++)
				sub *= 10;
			auto rs = duration_cast_exact<typename value_type::duration>(nanoseconds(sub));
			if (!rs)
				return error("Inexact conversion from subsecond part ." + std::string(tail));
			subsecond = *rs;
		} else if (tail.size())
			return error("Trailing data: '" + std::string(tail) + "'");

		auto ts = timegm(&date);

		auto dt = duration_cast_exact<typename value_type::duration>(seconds(ts));
		if (!dt)
			return error("Inexact conversion from seconds");
		value_type r(*dt);
		r += subsecond;
		return r;
	}
};


} // namespace tll

#endif//__cplusplus

#endif//_TLL_UTIL_TIME_H
