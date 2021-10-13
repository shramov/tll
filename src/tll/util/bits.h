/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_BITS_H
#define _TLL_UTIL_BITS_H

namespace tll::util {

template <typename T>
struct Bits
{
	T _bits = 0;

	Bits() = default;
	Bits(const Bits &) = default;
	Bits(Bits &&) = default;

	Bits & operator = (const Bits &) = default;
	Bits & operator = (Bits &&) = default;

	constexpr Bits(T value) : _bits(value) {}
	constexpr Bits(T value, unsigned offset) : _bits(value << offset) {}

	operator T () const { return _bits; }
	void clear() { _bits = T(); }

	constexpr bool get(size_t offset) const { return get(offset, 1); }
	constexpr void set(size_t offset, bool v) { set(offset, 1, v); }

	static constexpr unsigned mask(size_t width)
	{
		return 0xffffffffu >> (sizeof(unsigned) * 8 - width);
	}

	constexpr unsigned get(size_t offset, size_t width) const
	{
		return mask(width) & (_bits >> offset);
	}

	constexpr Bits & set(size_t offset, size_t width, unsigned v)
	{
		_bits ^= (get(offset, width) ^ (mask(width) & v)) << offset;
		return *this;
	}

	bool operator == (const Bits &rhs) const { return _bits == rhs._bits; }
	bool operator != (const Bits &rhs) const { return _bits != rhs._bits; }

	Bits & operator |= (const Bits &rhs) { _bits |= rhs._bits; return *this; }
	Bits & operator &= (const Bits &rhs) { _bits &= rhs._bits; return *this; }
	Bits & operator ^= (const Bits &rhs) { _bits ^= rhs._bits; return *this; }

	Bits & operator += (const Bits &rhs) { _bits |= rhs._bits; return *this; }
	Bits & operator -= (const Bits &rhs) { _bits ^= (_bits & rhs._bits); return *this; }
};

} // namespace tll::util

#endif//_TLL_UTIL_BITS_H
