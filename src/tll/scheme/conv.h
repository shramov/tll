/*
 * Copyright (c) 2022 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_SCHEME_CONV_H
#define _TLL_SCHEME_CONV_H

#include "tll/scheme.h"

namespace tll::conv {

template <>
struct dump<tll_scheme_field_type_t> : public to_string_from_string_buf<tll_scheme_field_type_t>
{
	template <typename Buf>
	static std::string_view to_string_buf(tll_scheme_field_type_t v, Buf &buf)
	{
		using namespace tll::scheme;
		switch (v) {
		case Field::Int8: return "int8";
		case Field::Int16: return "int16";
		case Field::Int32: return "int32";
		case Field::Int64: return "int64";
		case Field::UInt8: return "uint8";
		case Field::UInt16: return "uint16";
		case Field::UInt32: return "uint32";
		case Field::Double: return "double";
		case Field::Decimal128: return "decimal128";
		case Field::Bytes: return "bytes";
		case Field::Message: return "message";
		case Field::Array: return "array";
		case Field::Pointer: return "pointer";
		case Field::Union: return "union";
		}
		return "unknown";
	}
};

template <>
struct dump<tll_scheme_sub_type_t> : public to_string_from_string_buf<tll_scheme_sub_type_t>
{
	template <typename Buf>
	static std::string_view to_string_buf(tll_scheme_sub_type_t v, Buf &buf)
	{
		using namespace tll::scheme;
		switch (v) {
		case Field::SubNone: return "none";
		case Field::Enum: return "enum";
		case Field::ByteString: return "string";
		case Field::TimePoint: return "time_point";
		case Field::Duration: return "duration";
		case Field::Fixed: return "fixed";
		case Field::Bits: return "bits";
		}
		return "unknown";
	}
};

} // namespace tll::conv

#endif//_TLL_SCHEME_CONV_H
