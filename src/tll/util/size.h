/*
 * Copyright (c) 2019-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_SIZE_H
#define _TLL_UTIL_SIZE_H

#include "tll/util/conv.h"

namespace tll::util {

template <typename T>
struct SizeT
{
	T value;

	SizeT(T s = T()) : value(s) {}

	operator T () const { return value; }
};

using Size = SizeT<size_t>;

} // namespace tll::util

namespace tll::conv {

template <typename T>
struct dump<tll::util::SizeT<T>> : public to_string_from_string_buf<tll::util::SizeT<T>>
{
	static const std::pair<unsigned, std::string_view> suffix(T v)
	{
		if (v % (1024 * 1024 * 1024) == 0)
			return {1024 * 1024 * 1024, "gb"};
		else if (v % (1024 * 1024) == 0)
			return {1024 * 1024, "mb"};
		else if (v % (1024) == 0)
			return {1024, "kb"};
		else
			return {1, "b"};
	}

	template <typename Buf>
	static std::string_view to_string_buf(const tll::util::Size v, Buf &buf)
	{
		auto [div, s] = suffix(v.value);
		auto r = tll::conv::to_string_buf<T>(v.value / div, buf);
		return tll::conv::append(buf, r, s);
	}
};

template <typename T>
struct parse<tll::util::SizeT<T>>
{
	static result_t<tll::util::SizeT<T>> to_any(std::string_view s)
	{
		if (s.size() == 0)
			return error("Empty value");
		auto sep = s.find_last_not_of("kmgbit");
		if (sep == s.npos)
			return error("No digits found");
		auto ve = tll::conv::to_any<T>(s.substr(0, sep + 1));
		if (!ve) return error(ve.error());
		auto v = *ve;
		auto suffix = s.substr(sep + 1);
		if (suffix == "b") v *= 1;
		else if (suffix == "kb") v *= 1024;
		else if (suffix == "mb") v *= 1024 * 1024;
		else if (suffix == "gb") v *= 1024 * 1024 * 1024;
		else if (suffix == "bit") v /= 8;
		else if (suffix == "kbit") v *= 1024 / 8;
		else if (suffix == "mbit") v *= 1024 * 1024 / 8;
		else if (suffix == "gbit") v *= 1024 * 1024 * 1024 / 8;
		else
			return error("Invalid suffix " + std::string(suffix));
		return tll::util::SizeT<T> {v};
	}
};
} // namespace tll::conv

#endif//_TLL_UTIL_SIZE_H
