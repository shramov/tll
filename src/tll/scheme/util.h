/*
 * Copyright (c) 2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_SCHEME_UTIL_H
#define _TLL_SCHEME_UTIL_H

#include "tll/scheme/types.h"
#include "tll/util/memoryview.h"

namespace tll::scheme {

template <typename View>
long long read_size(const tll::scheme::Field * field, const View &data)
{
	using tll::scheme::Field;

	switch (field->type) {
	case Field::Int8:  return *data.template dataT<int8_t>();
	case Field::Int16: return *data.template dataT<int16_t>();
	case Field::Int32: return *data.template dataT<int32_t>();
	case Field::Int64: return *data.template dataT<int64_t>();
	case Field::UInt8:  return *data.template dataT<uint8_t>();
	case Field::UInt16: return *data.template dataT<uint16_t>();
	case Field::UInt32: return *data.template dataT<uint32_t>();
	default: return -1;
	}
}

template <typename View>
int write_size(const tll::scheme::Field * field, View &data, size_t size)
{
	using tll::scheme::Field;

	switch (field->type) {
	case Field::Int8:  *data.template dataT<int8_t>() = size; break;
	case Field::Int16: *data.template dataT<int16_t>() = size; break;
	case Field::Int32: *data.template dataT<int32_t>() = size; break;
	case Field::Int64: *data.template dataT<int64_t>() = size; break;
	case Field::UInt8:  *data.template dataT<uint8_t>() = size; break;
	case Field::UInt16: *data.template dataT<uint16_t>() = size; break;
	case Field::UInt32: *data.template dataT<uint32_t>() = size; break;
	default: break;
	}
	return 0;
}

struct generic_offset_ptr_t {
	unsigned offset;
	unsigned size;
	unsigned entity;
};

template <typename View>
std::optional<generic_offset_ptr_t> read_pointer(const tll::scheme::Field * field, View data)
{
	generic_offset_ptr_t r = {};
	switch (field->offset_ptr_version) {
	case TLL_SCHEME_OFFSET_PTR_DEFAULT: {
		auto ptr = data.template dataT<const tll_scheme_offset_ptr_t>();
		r.size = ptr->size;
		r.offset = ptr->offset;
		r.entity = ptr->entity;
		break;
	}
	case TLL_SCHEME_OFFSET_PTR_LEGACY_LONG: {
		auto ptr = data.template dataT<const tll_scheme_offset_ptr_legacy_long_t>();
		r.size = ptr->size;
		r.offset = ptr->offset;
		r.entity = ptr->entity;
		break;
	}
	case TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT: {
		auto ptr = data.template dataT<const tll_scheme_offset_ptr_legacy_short_t>();
		r.size = ptr->size;
		r.offset = ptr->offset;
		r.entity = field->type_ptr->size;
		break;
	}
	default:
		return std::nullopt;
	}
	return r;
}

template <typename View>
int write_pointer(const tll::scheme::Field * field, View data, const generic_offset_ptr_t &ptr)
{
	switch (field->offset_ptr_version) {
	case TLL_SCHEME_OFFSET_PTR_DEFAULT: {
		auto p = data.template dataT<tll_scheme_offset_ptr_t>();
		p->size = ptr.size;
		p->offset = ptr.offset;
		p->entity = ptr.entity;
		break;
	}
	case TLL_SCHEME_OFFSET_PTR_LEGACY_LONG: {
		auto p = data.template dataT<tll_scheme_offset_ptr_legacy_long_t>();
		p->size = ptr.size;
		p->offset = ptr.offset;
		p->entity = ptr.entity;
		break;
	}
	case TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT: {
		auto p = data.template dataT<tll_scheme_offset_ptr_legacy_short_t>();
		p->size = ptr.size;
		p->offset = ptr.offset;
		break;
	}
	}
	return 0;
}
} // namespace tll::scheme

#endif//_TLL_SCHEME_UTIL_H
