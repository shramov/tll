/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_STRING_H
#define _TLL_UTIL_STRING_H

#include <cstring>
#include <string_view>
#include <vector>

namespace tll {

inline std::string_view string_view_from_c(const char *base, int len)
{
	return { base, (len < 0)?strlen(base):len };
}

template <char Sep>
struct split_helperT
{
	std::string_view data;
	struct iterator {
		typedef std::string_view::const_iterator ptr_t;
		ptr_t data_begin;
		ptr_t data_end;
		ptr_t begin;
		ptr_t end;

		bool operator == (const iterator &rhs) const { return begin == rhs.begin; }
		bool operator != (const iterator &rhs) const { return begin != rhs.begin; }

		iterator(ptr_t dbegin, ptr_t dend, ptr_t it)
			: data_begin(dbegin)
			, data_end(dend)
			, begin(it)
			, end(next(it))
		{
		}

		ptr_t next(ptr_t ptr) const
		{
			if (ptr >= data_end)
				return data_end;
			for (; ptr + 1 < data_end && *ptr != Sep; ptr++) {}
			return ptr;
		}

		ptr_t prev(ptr_t ptr) const
		{
			if (ptr <= data_begin)
				return data_begin;
			for (; ptr > data_begin && *(ptr - 1) != Sep; ptr--) {}
			return ptr;
		}

		iterator & operator ++ ()
		{
			begin = end;
			if (begin == data_end)
				return *this;
			begin++;
			end = next(begin);
			return *this;
		}

		iterator & operator -- ()
		{
			if (begin == data_begin)
				return *this;
			end = begin - 1;
			begin = prev(end);
			return *this;
		}

		iterator operator ++ (int) { auto tmp = *this; ++*this; return tmp; }
		iterator operator -- (int) { auto tmp = *this; --*this; return tmp; }

		std::string_view operator * () const
		{
			if (begin == data_end)
				return "";
			return std::string_view(begin, end - begin);
		}
	};

	iterator begin() const { return iterator(data.begin(), data.end() + 1, data.begin()); }
	iterator end() const { return iterator(data.begin(), data.end() + 1, data.end() + 1); }
};

template <char Sep>
inline split_helperT<Sep> split(std::string_view s)
{
	return { s };
}

template <char Sep, bool Skip = false, typename T>
inline T & splitl(T & r, std::string_view s)
{
	using string_type = typename T::value_type;
	for (auto i : split<Sep>(s)) {
		if (Skip && i.empty())
			continue;
		r.push_back(string_type(i)); // Explicit constructor for std::string containers
	}
	return r;
}

template <char Sep, bool Skip = false>
inline std::vector<std::string_view> splitv(std::string_view s)
{
	std::vector<std::string_view> r;
	return std::move(splitl<Sep, Skip>(r, s));
}

} // namespace tll

#endif//_TLL_UTIL_STRING_H
