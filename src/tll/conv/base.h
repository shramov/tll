/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CONV_BASE_H
#define _TLL_CONV_BASE_H

#include <cstring>
#include <map>
#include <optional>
#include <string_view>
#include <type_traits>

#include <tll/util/result.h>

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

template <typename T>
struct parse<std::optional<T>>
{
	static result_t<std::optional<T>> to_any(std::string_view s)
	{
		if (!s.size()) return std::nullopt;
		auto r = parse<T>::to_any(s);
		if (!r)
			return error(r.error());
		return *r;
	}
};

} // namespace tll::conv

#endif//_TLL_CONV_BASE_H
