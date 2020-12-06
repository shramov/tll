/*
 * Copyright (c) 2019 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_BUFFER_H
#define _TLL_UTIL_BUFFER_H

#include <vector>

namespace tll::util {

template <typename T>
class buffer_t
{
	std::vector<T> _buf;
	size_t _size = 0;
 public:
	typedef typename std::vector<T>::value_type value_type;
	typedef typename std::vector<T>::iterator iterator;
	typedef typename std::vector<T>::const_iterator const_iterator;
	typedef typename std::vector<T>::reference reference;
	typedef typename std::vector<T>::pointer pointer;

	size_t size() const { return _size; }

	void resize(size_t size)
	{
		if (size > _buf.size())
			_buf.resize(size);
		_size = size;
	}

	void push_back(const value_type &v)
	{
		resize(_size + 1);
		_buf[_size - 1] = v;
	}

	T * data() { return _buf.data(); }
	const T * data() const { return _buf.data(); }

	iterator begin() { return _buf.begin(); }
	const_iterator begin() const { return _buf.begin(); }

	iterator end() { return begin() + _size; }
	const_iterator end() const { return begin() + _size; }
};

typedef buffer_t<char> buffer;

} // namespace tll::util

#endif//_TLL_UTIL_BUFFER_H
