// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_UTIL_ERRNO_H
#define _TLL_UTIL_ERRNO_H

#include <string.h>

#include <string_view>

namespace tll::util {

struct Errno
{
	int value = 0;

	Errno(int v) : value(v) {}

	std::string_view error() const { return strerror(value); }
	constexpr operator bool () const { return value; }
};

inline auto format_as(const Errno &v) noexcept { return v.error(); }

} // namespace tll::util

#endif//_TLL_UTIL_ERRNO_H
