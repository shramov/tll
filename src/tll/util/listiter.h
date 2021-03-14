/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_LISTITER_H
#define _TLL_UTIL_LISTITER_H

namespace tll::util {

template <typename T>
struct list_iterator
{
	T * data;

	using poiner = T *;
	using value_type = T;
	using reference = T &;

	struct iterator
	{
		T * data;

		iterator & operator ++ () { data = data->next; return *this; }
		iterator operator ++ (int) { auto tmp = *this; ++*this; return tmp; }

		bool operator == (const iterator &rhs) const { return data == rhs.data; }
		bool operator != (const iterator &rhs) const { return data != rhs.data; }

		T& operator * () { return *data; }
		const T& operator * () const { return *data; }

		T * operator -> () { return data; }
		const T * operator -> () const { return data; }
	};

	iterator begin() { return { data }; }
	iterator end() { return { nullptr }; }
};

template <typename T>
list_iterator<T> list_wrap(T * l) { return list_iterator<T> { l }; }

} // namespace tll::util

#endif//_TLL_UTIL_LISTITER_H
