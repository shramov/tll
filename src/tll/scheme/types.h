/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_SCHEME_TYPES_H
#define _TLL_SCHEME_TYPES_H

typedef struct __attribute__((packed)) tll_scheme_offset_ptr_legacy_short_t
{
	uint16_t offset;
	uint16_t size;
} tll_scheme_offset_ptr_legacy_short_t;

typedef struct __attribute__((packed)) tll_scheme_offset_ptr_legacy_long_t
{
	uint32_t offset;
	uint16_t size;
	uint16_t entity;
} tll_scheme_offset_ptr_legacy_long_t;

typedef struct __attribute__((packed)) tll_scheme_offset_ptr_t
{
	uint32_t offset;
	uint32_t size : 24;
	uint8_t  entity;
} tll_scheme_offset_ptr_t;

#define tll_scheme_offset_ptr_data(ptr) (((char *) (ptr)) + (ptr)->offset)

#ifdef __cplusplus
#include <string_view>

namespace tll::scheme {

template <typename T = void, typename Ptr = tll_scheme_offset_ptr_t>
struct __attribute__((packed)) offset_ptr_t : public Ptr
{
	T * data() { return (T *) (((char *) this) + this->offset); }
	const T * data() const { return (const T *) (((const char *) this) + this->offset); }
};

template <typename T = void> using offset_ptr_legacy_short_t = offset_ptr_t<T, tll_scheme_offset_ptr_legacy_short_t>;
template <typename T = void> using offset_ptr_legacy_long_t = offset_ptr_t<T, tll_scheme_offset_ptr_legacy_long_t>;

template <unsigned Size>
struct Bytes : public std::array<unsigned char, Size>
{
	static_assert(Size > 0, "Empty Bytes are not allowed");
};

template <unsigned Size>
struct ByteString : public std::array<char, Size>
{
	static_assert(Size > 0, "Empty Chars are not allowed");
	operator std::string_view () { return {this->data(), strnlen(this->data(), Size)}; }
};
} // namespace tll::scheme
#endif

#endif//_TLL_SCHEME_TYPES_H
