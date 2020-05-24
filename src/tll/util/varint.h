/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_VARINT_H
#define _TLL_UTIL_VARINT_H

#include <algorithm>

namespace tll::varint {

using u8 = unsigned char;

template <typename T, typename Buf>
size_t encode_uint(T value, Buf & buf)
{
	static_assert(std::is_unsigned<T>::value, "Unsigned type only");
	buf.resize(sizeof(value) * 8 / 7);
	auto ptr = (u8 *) buf.data();
	do {
		u8 b = value & 0x7fu;
		value >>= 7;
		*(ptr++) = 0x80 | b;
	} while (value);
	*(ptr - 1) ^= 0x80;
	buf.resize(ptr - (u8 *) buf.data());
	return buf.size();
}

template <typename T>
int decode_uint(T& value, const void * data, size_t size)
{
	static_assert(std::is_unsigned<T>::value, "Unsigned type only");
	auto ptr = (const u8 *) data;
	unsigned limit = std::min<unsigned>(size, std::min<unsigned>(9, sizeof(T) * 8 / 7));
	value = 0;
	for (unsigned i = 0; i < limit; i++) {
		u8 b = ptr[i];
		value |= (b & 0x7f) << (7 * i);
		if ((b & 0x80) == 0)
			return i + 1; // Bytes consumed, max 9 bytes
	}
	return -1;
}

template <typename T, typename Buf>
int decode_uint(T& value, const Buf & buf)
{
	return decode_uint<T>(value, buf.data(), buf.size());
}

} // namespace tll::util

#endif//_TLL_UTIL_VARINT_H
