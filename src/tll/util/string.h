/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
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

namespace util {

inline bool printable(char c) { return c >= 0x20 && c < 0x7f; }

inline std::string_view strip(std::string_view s, std::string_view chars = " ")
{
	auto p = s.find_first_not_of(chars);
	if (p == s.npos)
		return "";
	if (p != 0)
		s.remove_prefix(p);
	p = s.find_last_not_of(chars);
	if (p != s.size())
		s.remove_suffix(s.size() - p - 1);
	return s;
}

template <char ... Chars>
struct sep_helper;

template <char C, char ... Chars>
struct sep_helper<C, Chars...>
{
	static constexpr bool match(char c)
	{
		return c == C || sep_helper<Chars...>::match(c);
	}
};

template <>
struct sep_helper<>
{
	static constexpr bool match(char c) { return false; }
};

template <char ... Chars>
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
			for (; ptr + 1 < data_end && !sep_helper<Chars...>::match(*ptr); ptr++) {}
			return ptr;
		}

		ptr_t prev(ptr_t ptr) const
		{
			if (ptr <= data_begin)
				return data_begin;
			for (; ptr > data_begin && !sep_helper<Chars...>::match(*(ptr - 1)); ptr--) {}
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

template <char ... Chars>
inline split_helperT<Chars...> split(std::string_view s)
{
	return { s };
}

template <typename T, char ... Chars>
inline auto & split_append(T & r, std::string_view s, bool Skip = false)
{
	using string_type = typename T::value_type;
	for (auto i : split<Chars...>(s)) {
		if (Skip && i.empty())
			continue;
		r.push_back(string_type(i)); // Explicit constructor for std::string containers
	}
	return r;
}

template <char ... Chars>
inline std::vector<std::string_view> splitv(std::string_view s, bool skip = false)
{
	std::vector<std::string_view> r;
	return std::move(split_append<decltype(r), Chars...>(r, s, skip));
}

} // namespace util

template <char ... Chars>
using split_helperT = util::split_helperT<Chars...>;

// Old functions for backward compatibility
template <char ... Chars>
inline auto split(std::string_view s) { return util::split<Chars...>(s); }

template <char Sep, bool Skip = false, typename T>
inline T & splitl(T & r, std::string_view s) { return util::split_append<T, Sep>(r, s, Skip); }

template <char Sep, bool Skip = false>
inline auto splitv(std::string_view s) { return util::splitv<Sep>(s, Skip); }

} // namespace tll

#endif//_TLL_UTIL_STRING_H
