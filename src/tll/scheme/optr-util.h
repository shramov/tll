/*
 * Copyright (c) 2018-2019 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_SCHEME_OPTR_UTIL_H
#define _TLL_SCHEME_OPTR_UTIL_H

#include "tll/scheme.h"
#include "tll/scheme/types.h"
#include "tll/scheme/util.h"
#include "tll/util/listiter.h"
#include "tll/util/memoryview.h"

namespace tll::scheme {

template <typename Buf>
size_t optr_offset(const tll::scheme::Field * field, const Buf &buf)
{
	auto ptr = read_pointer(field, buf);
	if (!ptr)
		return 0;
	return ptr->offset;
}

template <typename Buf>
int optr_shift(const tll::scheme::Field * field, memoryview<Buf> buf, size_t offset)
{
	if (field->type == Field::Message) {
		for (auto & f : tll::util::list_wrap(field->type_msg->fields)) {
			auto r = optr_shift(&f, buf.view(f.offset), offset);
			if (r)
				return r;
		}
	} else if (field->type == Field::Array) {
		auto size = read_size(field->count_ptr, buf.view(field->count_ptr->offset));
		if (size < 0)
			return EINVAL;
		if ((size_t) size > field->count)
			return EINVAL;
		auto fview = buf.view(field->type_array->offset);
		auto entity = field->type_array->size;
		for (auto i = 0u; i < size; i++) {
			auto r = optr_shift(field->type_array, fview.view(i * entity), offset);
			if (r)
				return r;
		}
	} else if (field->type == Field::Pointer) {
		switch (field->offset_ptr_version) {
		case TLL_SCHEME_OFFSET_PTR_DEFAULT:
			buf.template dataT<offset_ptr_t<>>()->offset += offset;
			return 0;
		case TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT:
			buf.template dataT<offset_ptr_legacy_short_t<>>()->offset += offset;
			return 0;
		case TLL_SCHEME_OFFSET_PTR_LEGACY_LONG:
			buf.template dataT<offset_ptr_legacy_long_t<>>()->offset += offset;
			return 0;
		default:
			return EINVAL;
		}
	}
	return 0;
}

namespace {
template <typename OPtr>
struct optr_helper { static void set_entity(OPtr * ptr, size_t entity) { ptr->entity = entity; } };

template <typename T>
struct optr_helper<offset_ptr_legacy_short_t<T>> { static void set_entity(offset_ptr_legacy_short_t<T> * ptr, size_t entity) {} };
}

template <typename Buf, typename OPtr = offset_ptr_t<void>>
int optr_resizeT(const tll::scheme::Field * field, memoryview<Buf> buf, size_t size)
{
	auto buf_size = buf.size();
	const auto entity = field->type_ptr->size;
	buf.resize(buf.size() + entity * size);

	auto ptr = buf.template dataT<OPtr>();
	if (ptr->size == 0) {
		ptr->size = size;
		ptr->offset = buf_size;
		optr_helper<OPtr>::set_entity(ptr, entity);
		return 0;
	}

	if (size < ptr->size) {
		ptr->size = size;
		return 0;
	}

	auto end = ((char *) ptr->data()) + entity * ptr->size;
	memmove(end + entity * size, end, buf_size - (ptr->offset + entity * ptr->size));
	memset(end, 0, entity * size);

	auto dview = buf.view(ptr->offset);
	for (auto i = 0u; i < ptr->size; i++)
		optr_shift(field->type_ptr, dview.view(entity * i), entity * size);
	ptr->size += size;
	return 0;
}

template <typename Buf>
int optr_resize(const tll::scheme::Field * field, memoryview<Buf> buf, size_t size)
{
	if (field->type != Field::Pointer)
		return 0;
	switch (field->offset_ptr_version) {
	case TLL_SCHEME_OFFSET_PTR_DEFAULT:
		return optr_resizeT<Buf, offset_ptr_t<void>>(field, buf, size);
	case TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT:
		return optr_resizeT<Buf, offset_ptr_legacy_short_t<void>>(field, buf, size);
	case TLL_SCHEME_OFFSET_PTR_LEGACY_LONG:
		return optr_resizeT<Buf, offset_ptr_legacy_long_t<void>>(field, buf, size);
	default:
		return EINVAL;
	}
}

} // namespace tll::scheme

#endif//_TLL_SCHEME_OPTR_UTIL_H
