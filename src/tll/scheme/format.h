/*
 * Copyright (c)2020-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_SCHEME_FORMAT_H
#define _TLL_SCHEME_FORMAT_H

#include "tll/conv/decimal128.h"
#include "tll/conv/float.h"
#include "tll/scheme.h"
#include "tll/scheme/types.h"
#include "tll/scheme/util.h"
#include "tll/util/decimal128.h"
#include "tll/util/result.h"
#include "tll/util/string.h"
#include "tll/util/time.h"

#include <list>
#include <string>
#include <utility>

namespace tll::scheme {

namespace {
inline bool scalar_field(const tll::scheme::Field * field)
{
	using tll::scheme::Field;
	return field->type != Field::Message && field->type != Field::Array && field->type != Field::Pointer;
}
using path_error_t = std::pair<std::string, std::string>;

inline path_error_t append_path(const path_error_t &e, std::string_view path)
{
	path_error_t r = {"", e.second};
	if (!e.first.size())
		r.first = path;
	else if (e.first[0] == '[')
		r.first = std::string(path) + e.first;
	else
		r.first = std::string(path) + '.' + e.first;
	return r;
}

using format_result_t = tll::expected<std::list<std::string>, path_error_t>;
}

template <typename View>
format_result_t to_strings(const tll::scheme::Message * msg, const View &data);

template <typename View>
format_result_t to_strings(const tll::scheme::Field * msg, const View &data);

template <typename Int, typename Period>
format_result_t to_strings_time_point(Int v)
{
	using duration = std::chrono::duration<Int, Period>;
	using time_point = std::chrono::time_point<std::chrono::system_clock, duration>;
	return std::list<std::string> { tll::conv::to_string(time_point(duration(v))) };
}

template <typename Int>
format_result_t to_strings_number(const tll::scheme::Field * field, Int v, bool secret)
{
	if (secret)
		v = 0;
	if constexpr (!std::is_floating_point_v<Int>) {
		if (field->sub_type == field->Fixed) {
			tll::conv::unpacked_float<Int> u(v, -field->fixed_precision);
			return std::list<std::string> { conv::to_string(u) };
		} else if (field->sub_type == field->Bits) {
			std::string r;
			for (auto b = field->bitfields; b; b = b->next) {
				if (tll_scheme_bit_field_get(v, b->offset, b->size)) {
					if (r.size())
						r += " | ";
					r += std::string(b->name);
				}
			}
			return std::list<std::string> { r };
		} else if (field->sub_type == field->Enum) {
			for (auto i = field->type_enum->values; i; i = i->next) {
				if ((Int) i->value == v)
					return std::list<std::string> { i->name };
			}
			return std::list<std::string> { tll::conv::to_string(v) };
		}
	}
	if (field->sub_type == field->TimePoint) {
		switch (field->time_resolution) {
		case TLL_SCHEME_TIME_NS: return to_strings_time_point<Int, std::nano>(v); break;
		case TLL_SCHEME_TIME_US: return to_strings_time_point<Int, std::micro>(v); break;
		case TLL_SCHEME_TIME_MS: return to_strings_time_point<Int, std::milli>(v); break;
		case TLL_SCHEME_TIME_SECOND: return to_strings_time_point<Int, std::ratio<1, 1>>(v); break;
		case TLL_SCHEME_TIME_MINUTE: return to_strings_time_point<Int, std::ratio<60, 1>>(v); break;
		case TLL_SCHEME_TIME_HOUR: return to_strings_time_point<Int, std::ratio<3600, 1>>(v); break;
		case TLL_SCHEME_TIME_DAY: return to_strings_time_point<Int, std::ratio<86400, 1>>(v); break;
		}
		return std::list<std::string> { "Unknown resolution" };
	}

	auto r = std::list<std::string> {tll::conv::to_string(v)};
	if (field->sub_type == field->Duration)
		r.front() += time_resolution_str(field->time_resolution);
	return r;
}

template <typename View>
format_result_t to_strings_list(const tll::scheme::Field * field, const View &data, size_t size, size_t entity)
{
	std::list<std::string> result;
	bool scalar = scalar_field(field);
	if (scalar)
		result.push_back("");
	for (auto i = 0u; i < size; i++) {
		auto r = to_strings(field, data.view(i * entity));
		if (!r)
			return unexpected(append_path(r.error(), fmt::format("[{}]", i)));
		if (scalar) {
			if (result.front().size())
				result.front() += ", ";
			result.front() += r->front();
		} else if (!result.size() && r->size() == 1) {
			result = *r;
		} else {
			std::string prefix = "- ";
			if (result.size() == 1)
				result.front() = prefix + result.front();
			for (auto & i : *r) {
				result.push_back(prefix + i);
				prefix = "  ";
			}
		}
	}

	if (result.size() == 1)
		result.front() = "[" + result.front() + "]";
	else if (!result.size())
		result.push_back("[]");
	return result;
}

template <typename View>
format_result_t to_strings(const tll::scheme::Field * field, const View &data)
{
	using tll::scheme::Field;
	if (data.size() < field->size)
		return unexpected(path_error_t {"", fmt::format("Data size too small: {} < {}", data.size(), field->size)});

	auto secret = tll::getter::getT(field->options, "tll.secret", false).value_or(false);

	switch (field->type) {
	case Field::Int8:  return to_strings_number(field, *data.template dataT<int8_t>(), secret);
	case Field::Int16: return to_strings_number(field, *data.template dataT<int16_t>(), secret);
	case Field::Int32: return to_strings_number(field, *data.template dataT<int32_t>(), secret);
	case Field::Int64: return to_strings_number(field, *data.template dataT<int64_t>(), secret);
	case Field::UInt8:  return to_strings_number(field, *data.template dataT<uint8_t>(), secret);
	case Field::UInt16: return to_strings_number(field, *data.template dataT<uint16_t>(), secret);
	case Field::UInt32: return to_strings_number(field, *data.template dataT<uint32_t>(), secret);
	case Field::UInt64: return to_strings_number(field, *data.template dataT<uint64_t>(), secret);
	case Field::Double: return to_strings_number(field, *data.template dataT<double>(), secret);
	case Field::Decimal128:
		return std::list<std::string> { tll::conv::to_string(*data.template dataT<tll::util::Decimal128>()) };
	case Field::Bytes: {
		auto ptr = data.template dataT<const char>();
		if (secret)
			return std::list<std::string> {'"' + std::string(field->size, '*') + '"'};
		if (field->sub_type == Field::ByteString) {
			return std::list<std::string> {'"' + std::string(ptr, strnlen(ptr, field->size)) + '"'};
		}
		std::string result;
		result.reserve(field->size * 4);
		for (auto c = ptr; c < ptr + field->size; c++) {
			if (tll::util::printable(*c) && *c != '"')
				result.push_back(*c);
			else
				result += fmt::format("\\x{:02x}", (unsigned char) *c);
		}
		return std::list<std::string> {'"' + result + '"'};
	}
	case Field::Array: {
		auto size = read_size(field->count_ptr, data.view(field->count_ptr->offset));
		if (size < 0)
			return unexpected(path_error_t {"", fmt::format("Array size {} is invalid", size)});
		if ((size_t) size > field->count)
			return unexpected(path_error_t {"", fmt::format("Array size {} > max count {}", size, field->count)});
		return to_strings_list(field->type_array, data.view(field->type_array->offset), size, field->type_array->size);
	}
	case Field::Pointer: {
		auto ptr = read_pointer(field, data);
		if (!ptr)
			return unexpected(path_error_t {"", fmt::format("Unknown offset ptr version: {}", field->offset_ptr_version)});

		if (ptr->offset > data.size())
			return unexpected(path_error_t {"", fmt::format("Offset out of bounds: offset {} > data size {}", ptr->offset, data.size())});
		else if (ptr->offset + ptr->size * ptr->entity > data.size())
			return unexpected(path_error_t {"", fmt::format("Offset data out of bounds: offset {} + data {} * entity {} > data size {}", ptr->offset, (unsigned) ptr->size, ptr->entity, data.size())});
		if (field->sub_type == Field::ByteString) {
			secret = tll::getter::getT(field->type_ptr->options, "tll.secret", false).value_or(false);
			if (secret)
				return std::list<std::string>{'"' + std::string(std::max(ptr->size, 1u) - 1, '*') + '"'};

			std::string_view r;
			if (ptr->size)
				r = {data.view(ptr->offset).template dataT<const char>(), ptr->size - 1};
			return std::list<std::string> { '"' + std::string(r)  + '"' };
		}

		return to_strings_list(field->type_ptr, data.view(ptr->offset), ptr->size, ptr->entity);
	}
	case Field::Message:
		return to_strings(field->type_msg, data);
	case Field::Union: {
		auto type = read_size(field->type_union->type_ptr, data.view(field->type_union->type_ptr->offset));
		if (type < 0 || (size_t) type > field->type_union->fields_size)
			return unexpected(path_error_t {"", fmt::format("Union type out of bounds: {}", type)});
		auto uf = field->type_union->fields + type;
		auto r = to_strings(uf, data.view(uf->offset));
		if (!r)
			unexpected(append_path(r.error(), uf->name));
		if (r->size() == 1)
			return std::list<std::string> { fmt::format("{{{}: {}}}", uf->name, r->front()) };
		for (auto & l : *r)
			l = "  " + l;
		r->push_front(uf->name + std::string(":"));
		return r;
	}
	}
	return unexpected(path_error_t {"", fmt::format("unknown field type: {}", field->type)});
}

template <typename View>
format_result_t to_strings(const tll::scheme::Message * msg, const View &data)
{
	auto pmap = msg->pmap ? data.view(msg->pmap->offset) : data;
	if (data.size() < msg->size)
		return unexpected(path_error_t {"", fmt::format("Message size too small: {} < {}", data.size(), msg->size)});
	std::list<std::string> result;
	for (auto f = msg->fields; f; f = f->next) {
		if (msg->pmap) {
			if (!tll::scheme::pmap_get(pmap.data(), f->index))
				continue;
		}
		auto r = to_strings(f, data.view(f->offset));
		if (!r)
			return unexpected(append_path(r.error(), f->name));
		if (r->size() != 1) {
			result.push_back(fmt::format("{}:", f->name));
			for (auto & i : *r)
				result.push_back("  " + i);
		} else
			result.push_back(fmt::format("{}: {}", f->name, r->front()));
	}
	if (result.size() == 1)
		result.front() = "{" + result.front() + "}";
	return result;
}

template <typename View>
tll::result_t<std::string> to_string(const tll::scheme::Message * msg, const View &data)
{
	auto r = to_strings(msg, data);
	if (!r) {
		auto & e = r.error();
		if (!e.first.size())
			return error(e.second);
		return error(fmt::format("Failed to format field {}: {}", e.first, e.second));
	}
	std::string result;
	for (auto & i : *r) {
		if (result.size())
			result += '\n';
		result += i;
	}
	return result;
}

} // namespace tll::scheme

#endif//_TLL_SCHEME_FORMAT_H
