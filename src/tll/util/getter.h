/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef __TLL_UTIL_GETTER_H
#define __TLL_UTIL_GETTER_H

#include <optional>
#include <string_view>
#include <type_traits>

namespace tll::getter {

template <typename T>
struct has_function_has
{
	template <typename U> static bool check(decltype(std::declval<U>().has) *x);
	template <typename U> static void check(...);
	static constexpr bool value = std::is_same_v<bool, decltype(check<T>(nullptr))>;
};

template <typename T>
struct getter_api
{
	using string_type = std::string_view;

	static decltype(auto) get(const T &obj, std::string_view key)
	{
		return obj.get(key);
	}

	static bool has(const T &obj, std::string_view key)
	{
		if constexpr (has_function_has<T>::value)
			return obj.has(key);
		else
			return !!getter_api<T>::get(obj, key);
	}
};

template <typename T>
bool has(const T &obj, std::string_view key)
{
	return getter_api<T>::has(obj, key);
}

template <typename T>
auto get(const T &obj, std::string_view key)
{
	return getter_api<T>::get(obj, key);
}

} // namespace tll::getter

#endif//__TLL_UTIL_GETTER_H
