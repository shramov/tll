/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_BROWSE_H
#define _TLL_UTIL_BROWSE_H

#include "tll/util/string.h"

#include <string_view>
#include <vector>

namespace tll {

inline bool match(const std::vector<std::string_view> &mask, std::string_view path)
{
	auto ps = split<'.'>(path);
	auto mi = mask.begin();
	auto pi = ps.begin();
	{
		bool dstar = false;
		for (auto & i : mask) {
			if (i == "") return false;
			if (i == "**") {
				if (dstar) return false;
				dstar = true;
			}
		}
	}
	for (; pi != ps.end() && mi != mask.end(); pi++, mi++)
	{
		if (*mi == "*")
			continue;
		if (*mi == "**") {
			if (mi + 1 == mask.end()) // Trailing .**
				return true;
		} else if (*mi != *pi)
			return false;
	}
	if (mi != mask.end())
		return false;
	if (pi != ps.end())
		return false;
	return true;
}

inline bool match(std::string_view mask, std::string_view path)
{
	if (!mask.size()) return path.size() == 0;
	if (!path.size()) return false;
	if (mask == "**")
		return true;
	return match(splitv<'.', false>(mask), path);
}

} // namespace tll

#endif//_TLL_UTIL_BROWSE_H
