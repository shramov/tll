// SPDX-License-Identifier: MIT

#ifndef _TLL_UTIL_CSTRING_H
#define _TLL_UTIL_CSTRING_H

#include <cstdlib>
#include <cstring>
#include <string_view>

namespace tll::util {

/**
 * Helper class for externally allocated C strings
 */
class cstring
{
	std::string_view _data; // C++ 24.4.2.1 Default constructor sets data to nullptr

	static std::string_view memdup(const char * data, size_t size)
	{
		if (!data)
			return {};
		auto ptr = (char *) malloc(size + 1);
		memcpy(ptr, data, size);
		ptr[size] = 0;
		return { ptr, size };
	}

 public:
	struct consume_tag {};
	cstring(const char * data, size_t size, consume_tag tag) : _data(data, size) {}
	cstring(const char * data) : _data(memdup(data, data ? std::strlen(data) : 0)) {}
	cstring(const char * data, size_t size) : _data(memdup(data, size)) {}
	cstring() {}
	cstring(cstring && rhs) { std::swap(_data, rhs._data); }
	cstring(const cstring &rhs) : _data(memdup(rhs._data.data(), rhs._data.size())) {}
	cstring(std::string_view rhs) : _data(memdup(rhs.data(), rhs.size())) {}

	~cstring() { if (_data.data()) free((void *) _data.data()); _data = {}; }

	cstring & operator = (cstring rhs) { std::swap(_data, rhs._data); return *this; }

	static cstring consume(const char * data) { return cstring(data, data ? strlen(data) : 0, consume_tag {}); }
	static cstring consume(const char * data, size_t size) { return cstring(data, size, consume_tag {}); }

	std::string_view operator * () const { return _data; }
	const std::string_view * operator -> () const { return &_data; }
	operator bool () const { return _data.data() != nullptr; }

	std::string_view value_or(std::string_view s) const { if (*this) return _data; return s; }

	const char * release() { std::string_view tmp; tmp.swap(_data); return tmp.data(); }
};

} // namespace tll::util

#endif//_TLL_UTIL_CSTRING_H
