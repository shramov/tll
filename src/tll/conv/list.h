// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_CONV_LIST_H
#define _TLL_CONV_LIST_H

#include <tll/conv/base.h>
#include <tll/util/string.h>

#include <list>
#include <vector>

namespace tll::conv {
template <typename T>
struct is_sequential : public std::false_type {};

template <typename T>
struct is_sequential<std::list<T>> : public std::true_type {};

template <typename T>
struct is_sequential<std::vector<T>> : public std::true_type {};

template <typename T>
inline constexpr bool is_sequential_v = is_sequential<T>::value;

template <typename T>
struct dump<T, typename std::enable_if<is_sequential_v<T>>::type> : public to_string_buf_from_string<T>
{
	static std::string to_string(const T &value)
	{
		std::string r;
		for (auto &i: value) {
			if (r.size()) r += ",";
			r += tll::conv::to_string<typename T::value_type>(i);
		}
		return r;
	}
};

template <typename T>
struct parse<T, typename std::enable_if<is_sequential_v<T>>::type> : public to_string_buf_from_string<T>
{
	static result_t<T> to_any(std::string_view s)
	{
		T r;
		for (auto i: split<','>(s)) {
			if (!i.size())
				return error("Empty value in the list");
			auto v = parse<typename T::value_type>::to_any(i);
			if (!v)
				return error(v.error());
			r.push_back(*v);
		}
		return r;
	}
};

} // namespace tll::conv

#endif//_TLL_CONV_LIST_H
