/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_SCHEME_TYPES_H
#define _TLL_SCHEME_TYPES_H

#include <stdint.h>

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

#define tll_scheme_bit_field_mask(width, value) ((0xffffffffu >> (32 - (width))) & (value))
#define tll_scheme_bit_field_get(data, offset, width) tll_scheme_bit_field_mask(width, ((data) >> offset))
#define tll_scheme_bit_field_set(data, offset, width, value) ((data) ^= ((tll_scheme_bit_field_get(data, offset, width) ^ tll_scheme_bit_field_mask(width, (value))) << (offset)))

#ifdef __cplusplus

#include "tll/util/bits.h"
#include "tll/util/decimal128.h"
#include "tll/util/fixed_point.h"
#include "tll/util/offset_iterator.h"

#include <array>
#include <chrono>
#include <string_view>
#include <type_traits>

namespace tll::scheme {

using tll::util::Bits;
using tll::util::Decimal128;
using tll::util::FixedPoint;

template <typename T = void, typename Ptr = tll_scheme_offset_ptr_t>
struct __attribute__((packed)) offset_ptr_t : public Ptr
{
	using iterator = tll::util::offset_iterator<T>;
	using const_iterator = tll::util::offset_iterator<const T>;

	T * data() { return (T *) (((char *) this) + data_offset()); }
	const T * data() const { return (const T *) (((const char *) this) + data_offset()); }

	const void * data_raw() const { return (((const char *) this) + this->offset); }

	size_t data_offset() const
	{
		if (std::is_same_v<Ptr, tll_scheme_offset_ptr_t> && this->offset && entity_size() >= 0xff)
			return this->offset + sizeof(uint32_t);
		else
			return this->offset;
	}

	size_t entity_size() const
	{
		if constexpr (std::is_same_v<Ptr, tll_scheme_offset_ptr_t>) {
			if (this->offset && this->entity == 0xff)
				return *(const uint32_t *) data_raw();
			else
				return this->entity;
		} else if constexpr (std::is_same_v<Ptr, tll_scheme_offset_ptr_legacy_short_t>) {
			if constexpr (!std::is_same_v<T, void>)
				return sizeof(T);
			else
				return 1;
		} else if constexpr (std::is_same_v<Ptr, tll_scheme_offset_ptr_legacy_long_t>)
			return this->entity;
	}

	iterator begin() { return iterator(data(), entity_size()); }
	const_iterator begin() const { return const_iterator(data(), entity_size()); }
	iterator end() { return begin() + this->size; }
	const_iterator end() const { return begin() + this->size; }

	auto & operator [] (size_t idx) { return *(begin() + idx); }
	auto & operator [] (size_t idx) const { return *(begin() + idx); }
};

template <typename T = void> using offset_ptr_legacy_short_t = offset_ptr_t<T, tll_scheme_offset_ptr_legacy_short_t>;
template <typename T = void> using offset_ptr_legacy_long_t = offset_ptr_t<T, tll_scheme_offset_ptr_legacy_long_t>;

template <typename Ptr = tll_scheme_offset_ptr_t>
struct __attribute__((packed)) String : public offset_ptr_t<char, Ptr>
{
	operator std::string_view () const
	{
		if (this->size == 0)
			return std::string_view("");
		return std::string_view(this->data(), this->size - 1);
	}

	std::string_view operator * () const { return static_cast<std::string_view>(*this); }
};

template <size_t Size>
struct Bytes : public std::array<unsigned char, Size>
{
	static_assert(Size > 0, "Empty Bytes are not allowed");
};

template <size_t Size>
struct ByteString : public std::array<char, Size>
{
	static_assert(Size > 0, "Empty Chars are not allowed");
	operator std::string_view () const { return {this->data(), strnlen(this->data(), Size)}; }
	ByteString operator = (std::string_view s) { memcpy(this->data(), s.data(), std::min(Size, s.size())); return *this; }
};

template <typename T, size_t Size, typename CountT>
struct __attribute__((packed)) Array
{
	using count_type = CountT;
	using value_type = T;
	static constexpr size_t max_count = Size;

	count_type count;
	std::array<value_type, Size> array;
};

template <typename Type, size_t Size>
class __attribute__((packed)) UnionBase
{
	Type _type = 0 ;
	unsigned char _data[Size] = {};

 public:
	Type type() const { return _type; }

	template <typename T> T & uncheckedT() { return * (T *) _data; }
	template <typename T> const T & uncheckedT() const { return * (const T *) _data; }

	template <typename T> T * getT(Type t) { if (_type != t) return nullptr; return &uncheckedT<T>(); }
	template <typename T> const T * getT(Type t) const { if (_type != t) return nullptr; return &uncheckedT<T>(); }

	template <typename T> T & setT(Type t) { _type = t; auto & v = uncheckedT<T>(); v = {}; return v; }
	template <typename T> void setT(Type t, const T &v) { _type = t; uncheckedT<T>() = v; }
};

} // namespace tll::scheme
#endif

#endif//_TLL_SCHEME_TYPES_H
