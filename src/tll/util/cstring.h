// SPDX-License-Identifier: MIT

#ifndef _TLL_UTIL_CSTRING_H
#define _TLL_UTIL_CSTRING_H

#include <cstring>
#include <string_view>

namespace tll::util {

/**
 * Helper class for externally allocated C strings
 */
class cstring
{
	std::string_view _data; // C++ 24.4.2.1 Default constructor sets data to nullptr
 public:
	explicit cstring(const char * data) : _data(data, data ? std::strlen(data) : 0) {}
	explicit cstring(const char * data, size_t size) : _data(data, size) {}
	cstring() {}
	cstring(const std::nullopt_t &) {}
	cstring(cstring && rhs) { std::swap(_data, rhs._data); }

	~cstring() { if (_data.data()) free((void *) _data.data()); _data = {}; }

	std::string_view operator * () const { return _data; }
	const std::string_view * operator -> () const { return &_data; }
	operator bool () const { return _data.data() != nullptr; }

	std::string_view value_or(std::string_view s) const { if (*this) return _data; return s; }

	const char * release() { std::string_view tmp; tmp.swap(_data); return tmp.data(); }
};

} // namespace tll::util

#endif//_TLL_UTIL_CSTRING_H
