// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_UTIL_SECRET_H
#define _TLL_UTIL_SECRET_H

#include "tll/util/cstring.h"

namespace tll::util {

/// C-string wrapper that zeroes memory before deallocation
class Secret : public cstring
{
 public:
	Secret(char * data, size_t size, consume_tag tag) : cstring(data, size, tag) {}
	Secret(const char * data) : cstring(data) {}
	Secret(const char * data, size_t size) : cstring(data, size) {}
	Secret(std::string_view data) : cstring(data.data(), data.size()) {}

	Secret() {}
	Secret(Secret &&rhs) : cstring(std::move(rhs)) {}
	Secret(const Secret &rhs) : cstring(rhs) {}

	~Secret()
	{
		if (_data.data()) {
			// std::memset_explicit will be available in C++26
			volatile char * ptr = (char *) _data.data();
			while (ptr != _data.data() + _data.size())
				*ptr++ = 0;
		}
	}

	Secret &operator = (Secret rhs) { std::swap(_data, rhs._data); return *this; }

	static Secret consume(char * data) { return Secret(data, data ? strlen(data) : 0, consume_tag {}); }
	static Secret consume(char * data, size_t size) { return Secret(data, size, consume_tag {}); }

	constexpr const char * data() const { return _data.data(); }
	constexpr size_t size() const { return _data.size(); }

	constexpr std::string_view str() const { return _data; };
};

// Prevent accidental leaks into logs
constexpr auto format_as(const Secret &) noexcept { return std::string_view("********"); }

} // namespace tll::util

#endif//_TLL_UTIL_SECRET_H
