/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 *
 * memoryview - class to wrap arbitray memory location with safe (if you check borders)
 * to its subregions
 */

/** @file
 * memoryview objects
 *
 * Lightweight view over any object with data() and size() functions (or properties) inspired by
 * python memoryview but without own size
 */

#ifndef _TLL_UTIL_MEMORYVIEW_H
#define _TLL_UTIL_MEMORYVIEW_H

#include <type_traits>

namespace tll {

struct memory
{
	void * data;
	size_t size;
};

struct const_memory
{
	const void * data;
	size_t size;
};

namespace {
template <typename T>
struct is_function_data
{
	template <typename U> static bool check(decltype(std::declval<U>().data) *x);
	template <typename U> static void check(...);
	static constexpr bool value = std::is_void_v<decltype(check<T>(nullptr))>;
};

template <typename T>
struct is_function_size
{
	template <typename U> static bool check(decltype(std::declval<U>().size) *x);
	template <typename U> static void check(...);
	static constexpr bool value = std::is_void_v<decltype(check<T>(nullptr))>;
};
}

/**
 * Helper class that can specialized for custom types that don't have data()/size() functions
 * or properties
 */
template <typename T>
struct memoryview_api
{
	static constexpr bool is_const_data()
	{
		if constexpr (is_function_data<T>::value)
			return std::is_const_v<decltype(std::declval<T>().data())>;
		else
			return std::is_const_v<decltype(std::declval<T>().data)>;
	}

	using pointer = typename std::conditional<is_const_data(), const void *, void *>::type;

	static pointer data(T &obj)
	{
		if constexpr (is_function_data<T>::value)
			return obj.data();
		else
			return obj.data;
	}
	static const void * data(const T &obj)
	{
		if constexpr (is_function_data<T>::value)
			return obj.data();
		else
			return obj.data;
	}
	static size_t size(const T &obj)
	{
		if constexpr (is_function_size<T>::value)
			return obj.size();
		else
			return obj.size;
	}
	static void   resize(T &obj, size_t size) { obj.resize(size); }
};

template <typename T>
class memoryview
{
	T * _memory;
	size_t _offset = 0;
 public:
	memoryview(T &memory, size_t offset = 0) : _memory(&memory), _offset(offset) {}

	T & memory() { return *_memory; }
	const T & memory() const { return *_memory; }

	size_t offset() const { return _offset; }

	memoryview<T> view(size_t offset) { return memoryview(*_memory, _offset + offset); }
	memoryview<const T> view(size_t offset) const { return memoryview(*_memory, _offset + offset); }

	void resize(size_t size) { memoryview_api<T>::resize(*_memory, size + _offset); }

	size_t size() const { return std::max<ssize_t>(memoryview_api<T>::size(*_memory) - (ssize_t) _offset, 0); }

	void * data() { return static_cast<char *>(memoryview_api<T>::data(*_memory)) + _offset; }
	const void * data() const { return static_cast<const char *>(memoryview_api<T>::data(*_memory)) + _offset; }

	template <typename R> R * dataT() { return static_cast<R *>(data()); }
	template <typename R> const R * dataT() const { return static_cast<const R *>(data()); }
};

template <typename T>
class memoryview<const T>
{
	const T * _memory;
	size_t _offset = 0;
 public:
	memoryview(const T &memory, size_t offset = 0) : _memory(&memory), _offset(offset) {}
	memoryview(const memoryview<T> &rhs, size_t offset = 0) : _memory(&rhs.memory()), _offset(rhs.offset() + offset) {}
	memoryview(memoryview<T> &&rhs) : _memory(&rhs.memory()), _offset(rhs.offset()) {}

	const T & memory() const { return *_memory; }
	size_t offset() const { return _offset; }

	memoryview<const T> view(size_t offset) const { return memoryview(*_memory, _offset + offset); }

	size_t size() const { return std::max<ssize_t>(memoryview_api<T>::size(*_memory) - (ssize_t) _offset, 0); }

	const void * data() const { return static_cast<const char *>(memoryview_api<T>::data(*_memory)) + _offset; }

	template <typename R> const R * dataT() const { return static_cast<const R *>(data()); }
};

template <typename T>
memoryview<T> make_view(T & data, size_t offset = 0) { return memoryview<T>(data, offset); }

} // namespace tll

#endif//_TLL_UTIL_MEMORYVIEW_H
