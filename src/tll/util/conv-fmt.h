/*
 * Copyright (c) 2018-2019 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_CONV_FMT_H_
#define _TLL_UTIL_CONV_FMT_H_

#include "tll/util/conv.h"

#include <type_traits>
#include <fmt/format.h>

namespace {
template <typename T, typename Tag = void>
struct _custom : public std::true_type {};

template <> struct _custom<std::string> : public std::false_type {};
template <> struct _custom<std::string_view> : public std::false_type {};

template <typename T>
struct _custom<T, typename std::enable_if<std::is_integral_v<T>>::type> : public std::false_type {};
template <typename T>
struct _custom<T, typename std::enable_if<std::is_floating_point_v<T>>::type> : public std::false_type {};

template <typename T>
struct _custom<T, typename std::enable_if<tll::conv::dump<T>::std_to_string>::type> : public std::false_type {};
}

template <typename T>
struct fmt::formatter<T, char, typename std::enable_if<_custom<T>::value>::type>
{
	template <typename ParseContext>
	constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

	template <typename FormatContext>
	auto format(const T &v, FormatContext &ctx) {
		return format_to(ctx.out(), "{}", tll::conv::to_string<T>(v));
	}
};

#endif//_TLL_UTIL_CONV_FMT_H_
