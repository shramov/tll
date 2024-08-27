/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
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

/// Type is well known, fmtlib has it's own formatter
template <typename T, typename Tag = void>
struct _well_known : public std::false_type {};

template <> struct _well_known<std::string> : public std::true_type {};
template <> struct _well_known<std::string_view> : public std::true_type {};

template <typename T>
struct _well_known<T, typename std::enable_if<std::is_integral_v<T>>::type> : public std::true_type {};
template <typename T>
struct _well_known<T, typename std::enable_if<std::is_floating_point_v<T>>::type> : public std::true_type {};
template <typename T>
struct _well_known<T, typename std::enable_if<tll::conv::dump<T>::fmt_has_formatter>::type> : public std::true_type {};

template <typename T>
constexpr auto _well_known_v = _well_known<T>::value;

/// Type has user defined tll::conv::to_string
template <typename T, typename Tag = void>
struct _custom_conv : public std::true_type {};

template <typename T>
struct _custom_conv<T, typename std::enable_if<tll::conv::dump<T>::std_to_string>::type> : public std::false_type {};

template <typename T>
constexpr auto _custom_conv_v = _custom_conv<T>::value;
}

template <typename T>
struct fmt::formatter<T, char, typename std::enable_if<!_well_known_v<T> && _custom_conv_v<T>>::type>
{
	template <typename ParseContext>
	constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

	template <typename FormatContext>
	auto format(const T &v, FormatContext &ctx) const {
		return fmt::format_to(ctx.out(), "{}", tll::conv::to_string<T>(v));
	}
};

#endif//_TLL_UTIL_CONV_FMT_H_
