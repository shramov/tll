/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_REFPTR_H
#define _TLL_UTIL_REFPTR_H

#include <atomic>
#include <stdio.h>

namespace tll::util {

template <typename T, int Initial = 1, bool Debug = false>
struct refbase_t
{
	const T * ref() const { return const_cast<refbase_t<T, Initial, Debug> *>(this)->ref(); }
	void unref() const { return const_cast<refbase_t<T, Initial, Debug> *>(this)->unref(); }

	T * ref() { if (Debug) printf("+ ref %p %d++\n", this, _ref.load()); _ref += 1; return static_cast<T *>(this); }
	void unref() { if (Debug) printf("- ref %p %d--\n", this, _ref.load()); if ((_ref -= 1) == 0) static_cast<T *>(this)->destroy(); }
	int refcnt() const { return _ref.load(); }
	void destroy() { delete static_cast<T *>(this); }

 protected:
	mutable std::atomic<int> _ref = {Initial};
};

template <typename T>
class refptr_t
{
	T * _data = nullptr;
public:

	refptr_t(const refptr_t& ptr) = delete;
	refptr_t(refptr_t& ptr) : _data(nullptr) { reset(ptr.get()); }
	refptr_t(refptr_t&& ptr) : _data(nullptr) { std::swap(_data, ptr._data); }

	refptr_t & operator = (const refptr_t &ptr) = delete;
	refptr_t & operator = (refptr_t &  ptr) { reset(ptr._data); return *this; }
	refptr_t & operator = (refptr_t && ptr) { std::swap(_data, ptr._data); return *this; }

	refptr_t(T * ptr) : _data(nullptr) { reset(ptr); }
	refptr_t() {}
	~refptr_t() { reset(nullptr); }

	T * operator -> () { return _data; }
	const T * operator -> () const { return _data; }

	T & operator * () { return *_data; }
	const T & operator * () const { return *_data; }

	void reset(T * ptr) { if (_data) _data->unref(); _data = ptr; if (_data) _data->ref(); }

	T * get() { return _data; }
	const T * get() const { return _data; }

	T * release() { auto ptr = _data; _data = 0; return ptr; }

	operator bool () const { return _data != nullptr; }

	bool operator == (const refptr_t<T> & rhs) const { return _data == rhs._data; }
	bool operator != (const refptr_t<T> & rhs) const { return _data != rhs._data; }
};

//template <typename T>
//refptr_t<T> make_ref(T * ptr) { return refptr_t<T>(ptr); }

} // namespace tll::util

#endif//_TLL_UTIL_REFPTR_H
