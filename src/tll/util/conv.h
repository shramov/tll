/*
 * Copyright (c) 2018-2019 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_CONV_H
#define _TLL_UTIL_CONV_H

#include <cstring>
#include <list>
#include <map>
#include <string_view>
#include <type_traits>
#include <vector>

#include "tll/util/result.h"
#include "tll/util/string.h"

namespace tll::conv {

template <typename T> inline std::string to_string(const T &value);
template <typename T, typename Buf> inline std::string_view to_string_buf(const T &value, Buf &buf);

/**
 * Helper class prividing to_string_buf implementation from to_string
 */
template <typename T>
struct to_string_buf_from_string
{
	template <typename Buf>
	static std::string_view to_string_buf(const T &value, Buf &buf)
	{
		auto r = to_string(value);
		if (buf.size() < r.size())
			buf.resize(r.size());
		memcpy(buf.data(), r.data(), r.size());
		return std::string_view(buf.data(), r.size());
	}
};

/**
 * Helper class prividing to_string implementation from to_string_buf
 */
template <typename T>
struct to_string_from_string_buf
{
	static std::string to_string(const T &value)
	{
		std::string buf;
		auto r = to_string_buf<T>(value, buf);
		if (r.data() == buf.data() && r.size() == buf.size())
			return buf;
		return std::string(r);
	}
};

/**
 * Serialization for custom types
 *
 * Provides 2 way to convert type to string - simple with std::string result (and memory allocation)
 * and more effective (if implemented) with std::string_view and user supplied buffer.
 *
 * When specializing you need to provide only one function and use either to_string_from_string_buf
 * or to_string_buf_from_string base for another.
 */
template <typename T, typename Tag = void>
struct dump : public to_string_buf_from_string<T>
{
	static constexpr bool std_to_string = true;

	static std::string to_string(const T &value)
	{
		return std::to_string(value);
	}
};

/**
 * Convert type T to string
 */
template <typename T>
inline std::string to_string(const T &value)
{
	return dump<T>::to_string(value);
}

/**
 * Convert type T to string using user supplied buffer buf
 */
template <typename T, typename Buf>
inline std::string_view to_string_buf(const T &value, Buf &buf)
{
	return dump<T>::to_string_buf(value, buf);
}

template <typename T, typename Tag = void>
struct _parse
{
	/*
	static result_t<T> to_any(std::string_view s)
	{
		return error("Unknown conversion");
	}
	*/
};

template <typename T, typename Tag = void>
struct parse
{
	static result_t<T> to_any(std::string_view s) { return _parse<T>::to_any(s); }
};

/*
template <typename T, typename Tag> template <typename Buf>
std::string_view _to_string_buf<T, Tag>::to_string_buf(const T &value, Buf &buf)
{
	auto r = to_string(value);
	if (buf.size() < r.size())
		buf.resize(r.size());
	memcpy(buf.data(), r.data(), r.size());
	return std::string_view(buf.data(), buf.size());
}
*/

template <typename T>
inline result_t<T> to_any(std::string_view s)
{
	return parse<T>::to_any(s);
}

// Utility functions

template <typename Buf>
std::string_view append(Buf &buf, std::string_view l, std::string_view r)
{
	if (r.size() == 0) return l;
	if (l.size() == 0) return r;
	auto base = (char *)(buf.data());
	if (l.data() < base || l.data() >= base + buf.size()) {
		buf.resize(l.size() + r.size());
		base = (char *)(buf.data());
		memcpy(base, l.data(), l.size());
		memcpy(base + l.size(), r.data(), r.size());
		return std::string_view(base, l.size() + r.size());
	}
	auto off = l.data() - base;
	if (buf.size() < off + l.size() + r.size()) {
		buf.resize(off + l.size() + r.size());
		base = (char *)(buf.data());
	}
	memcpy(base + off + l.size(), r.data(), r.size());
	return std::string_view(base + off, l.size() + r.size());
}

// Specialization for basic types

namespace {
template <unsigned Base>
struct Digits {};

template <>
struct Digits<10> { static char decode(char c) { if (c < '0' || c > '9') return -1; return c - '0'; } };

template <>
struct Digits<16>
{
	static char decode(char c)
	{
		static const char lookup[] = ""
			"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x00
			"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x10
			"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x20
			"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x80\x80\x80\x80\x80\x80" // 0x30
			"\x80\x0a\x0b\x0c\x0d\x0e\x0f\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x40
			"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x50
			"\x80\x0a\x0b\x0c\x0d\x0e\x0f\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x60
			"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x70
			"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x80
			"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x90
			"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xa0
			"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xb0
			"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xc0
			"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xd0
			"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xe0
			"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xf0
			"";
		return lookup[(unsigned char) c];
	}
};

template <typename I, I Limit = std::numeric_limits<I>::max(), int Base = 10>
inline result_t<I> to_any_uint_base(std::string_view s)
{
	if (!s.size()) return error("Empty string");
	I r = 0;
	auto ptr = s.begin();
	for (; ptr != s.end(); ptr++) {
		auto x = Digits<Base>::decode(*ptr);
		if (x < 0)
			return error("Invalid digit: " + std::string(ptr, 1));
		if constexpr (Limit == std::numeric_limits<I>::max()) {
			auto old = r;
			r = Base * r + x;
			if (old > r)
				return error("Overflow");
		} else {
			r = Base * r + x;
			if (r > Limit)
				return error("Overflow");
		}
	}
	return r;
}

template <typename I, I Limit = std::numeric_limits<I>::max()>
inline result_t<I> to_any_uint(std::string_view s)
{
	if (s.size() > 2) {
		if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
			return to_any_uint_base<I, Limit, 16>(s.substr(2));
	}
	return to_any_uint_base<I, Limit, 10>(s);
}

template <typename I>
inline result_t<I> to_any_sint(std::string_view s)
{
	I mul = 1;
	if (s.size() == 0)
		return error("Empty string");
	auto ptr = s.begin();
	if (*ptr == '-') {
		mul = -1;
		ptr++;
	} else if (*ptr == '+') {
		mul = 1;
		ptr++;
	}

	using uI = typename std::make_unsigned_t<I>;
	if (mul > 0) {
		auto r = to_any_uint<uI, std::numeric_limits<I>::max()>(s.substr(ptr - s.begin()));
		if (!r) return error(r.error());
		return *r;
	} else {
		auto r = to_any_uint<uI, static_cast<uI>(std::numeric_limits<I>::max()) + 1>(s.substr(ptr - s.begin()));
		if (!r) return error(r.error());
		return -*r;
	}
}

template <typename I>
inline result_t<I> to_any_int(std::string_view s)
{
	if constexpr (std::is_unsigned_v<I>)
		return to_any_uint<I>(s);
	else if constexpr (!std::is_unsigned_v<I>) //XXX: clang needs this
		return to_any_sint<I>(s);
	return error("Not integer type");
}

template <int Base>
struct to_string_buf_uint {
	template <typename I, typename Buf>
	static inline std::string_view to_string_buf(const I &v, Buf &buf);
};

template <>
struct to_string_buf_uint<16> {
	template <typename I, typename Buf>
	static inline std::string_view to_string_buf(I v, Buf &buf)
	{
		static const char lookup[] = "0123456789abcdef";
		buf.resize(3 + sizeof(I) * 2); // One for possible sign
		auto end = ((char *) buf.data()) + buf.size();
		auto ptr = end;
		for (unsigned i = 0; i < sizeof(I); i++) { // For unroll
			unsigned char lo = v & 0xfu;
			unsigned char hi = (v & 0xffu) >> 4;
			v >>= 8;
			*--ptr = lookup[lo];
			*--ptr = lookup[hi];
			if (!v) break;
		}
		return std::string_view(ptr, end - ptr);
	}
};

template <>
struct to_string_buf_uint<10> {
	template <typename I, typename Buf>
	static inline std::string_view to_string_buf(I v, Buf &buf)
	{
		buf.resize(1 + sizeof(I) * 3); // One for possible sign
		auto end = ((char *) buf.data()) + buf.size();
		auto ptr = end;
		do {
			I r = v % 10;
			v /= 10;
			*--ptr = '0' + r;
		} while (v);
		return std::string_view(ptr, end - ptr);
	}
};

template <typename I, typename Buf, int Base = 10>
inline std::string_view to_string_buf_int(I v, Buf &buf)
{
	if constexpr (std::is_unsigned_v<I>)
		return to_string_buf_uint<Base>::template to_string_buf<I, Buf>(v, buf);
	if (v >= 0)
		return to_string_buf_uint<Base>::template to_string_buf<std::make_unsigned_t<I>, Buf>(v, buf);
	auto r = to_string_buf_uint<Base>::template to_string_buf<std::make_unsigned_t<I>, Buf>(-v, buf);
	auto ptr = (char *) r.data();
	*--ptr = '-';
	return std::string_view(ptr, r.size() + 1);
}

} // namespace

template <typename T>
struct dump<T, typename std::enable_if<std::is_integral_v<T>>::type> : public to_string_from_string_buf<T>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(T v, Buf &buf) { return to_string_buf_int<T, Buf, 10>(v, buf); }
};

template <typename T>
struct parse<T, typename std::enable_if<std::is_integral_v<T>>::type>
{
	static result_t<T> to_any(std::string_view s) { return to_any_int<T>(s); }
};

template <typename T>
struct dump<T, typename std::enable_if<std::is_floating_point_v<T>>::type> : public to_string_from_string_buf<T>
{
	static constexpr std::string_view _snprintf_format()
	{
		if constexpr (sizeof(T) == sizeof(long double))
			return "%.*Lg";
		return "%.*g";
	}

	static constexpr int _snprintf_precision()
	{
		if constexpr (std::is_same_v<T, double>)
			return std::numeric_limits<T>::max_digits10 - 1;
		return std::numeric_limits<T>::max_digits10 - 1;
	}

	template <typename Buf>
	static std::string_view to_string_buf(const T &value, Buf &buf)
	{
		constexpr auto prec = _snprintf_precision();
		buf.resize(prec + 16);
		auto l = snprintf(buf.data(), buf.size(), _snprintf_format().data(), prec, value);
		auto r = std::string_view(buf.data(), std::min<int>(l, buf.size()));
		return r;
		//if (r.find('.') != r.npos) return r;
		//return append(buf, r, ".0");
	}
};

template <typename T>
struct parse<T, typename std::enable_if<std::is_floating_point_v<T>>::type>
{
	static result_t<double> to_any(std::string_view s)
	{
		if (!s.size()) return error("Empty value");
		std::string tmp(s);
		char * end = nullptr;
		auto r = strtold(tmp.c_str(), &end);
		if (!end || *end)
			return error("Invalid double string");
		return r;
	}
};

template <>
struct dump<bool>
{
	template <typename Buf>
	static std::string_view to_string_buf(bool value, Buf &buf) { if (value) return "true"; else return "false"; }
	static std::string to_string(bool value) { if (value) return "true"; else return "false"; }
};

template <>
struct parse<bool>
{
	static result_t<bool> to_any(std::string_view s)
	{
		// TODO: case insensitive compare
		if (s == "true" || s == "yes" || s == "1")
			return true;
		else if (s == "false" || s == "no" || s == "0")
			return false;
		return error("Invalid bool string");
	}
};

namespace {
template <typename T>
struct is_sequential : public std::false_type {};

template <typename T>
struct is_sequential<std::list<T>> : public std::true_type {};

template <typename T>
struct is_sequential<std::vector<T>> : public std::true_type {};

template <typename T>
inline constexpr bool is_sequential_v = is_sequential<T>::value;
}

template <typename T>
struct dump<T, typename std::enable_if<is_sequential_v<T>>::type> : public to_string_buf_from_string<T>
{
	static std::string to_string(const T &value)
	{
		std::string r;
		for (auto &i: value) {
			if (r.size()) r += ",";
			r += tll::conv::to_string<typename T::value_type>(i);
		}
		return r;
	}
};

template <typename T>
struct parse<T, typename std::enable_if<is_sequential_v<T>>::type> : public to_string_buf_from_string<T>
{
	static result_t<T> to_any(std::string_view s)
	{
		T r;
		for (auto i: split<','>(s)) {
			if (!i.size())
				return error("Empty value in the list");
			auto v = parse<typename T::value_type>::to_any(i);
			if (!v)
				return error(v.error());
			r.push_back(*v);
		}
		return r;
	}
};

template <>
struct dump<std::string>
{
	template <typename Buf>
	static std::string_view to_string_buf(const std::string &value, Buf &buf) { return value; } // XXX: Copy?

	static std::string to_string(const std::string &value) { return value; }
};

template <>
struct parse<std::string>
{
	static result_t<std::string> to_any(std::string_view s) { return {s}; }
};

template <>
struct dump<std::string_view>
{
	template <typename Buf>
	static std::string_view to_string_buf(std::string_view value, Buf &buf) { return value; } // XXX: Copy?

	static std::string to_string(std::string_view value) { return std::string(value); }
};

template <>
struct parse<std::string_view>
{
	static result_t<std::string_view> to_any(std::string_view s) { return {s}; }
};

template <typename T>
inline result_t<T> select(std::string_view s, const std::map<std::string_view, T> m)
{
	auto i = m.find(s);
	if (i == m.end()) return error("No matches");
	return i->second;
}

} // namespace tll::conv

#endif//_TLL_UTIL_CONV_H
