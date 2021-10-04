/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_OFFSETITER_H
#define _TLL_UTIL_OFFSETITER_H

namespace tll::util {

template <typename T>
class offset_iterator
{
	union {
		T * _data = nullptr;
		const unsigned char * _raw;
	};
	size_t _step = sizeof(T); 

 public:
	using poiner = T *;
	using value_type = T;
	using reference = T &;

	offset_iterator(T * data, size_t step = sizeof(T)) : _data(data), _step(step) {}

	offset_iterator & operator ++ () { _raw += _step; return *this; }
	offset_iterator operator ++ (int) { auto tmp = *this; ++*this; return tmp; }

	offset_iterator & operator += (size_t i) { _raw += i * _step; return *this; }
	offset_iterator operator + (size_t i) const { auto tmp = *this;tmp += i; return tmp; }

	bool operator == (const offset_iterator &rhs) const { return _data == rhs._data; }
	bool operator != (const offset_iterator &rhs) const { return _data != rhs._data; }

	T& operator * () { return *_data; }
	const T& operator * () const { return *_data; }

	T * operator -> () { return _data; }
	const T * operator -> () const { return _data; }
};

template <typename T>
using const_offset_iterator = offset_iterator<const T>;

} // namespace tll::util

#endif//_TLL_UTIL_OFFSETITER_H
