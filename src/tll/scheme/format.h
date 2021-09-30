/*
 * Copyright (c)2020-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_SCHEME_FORMAT_H
#define _TLL_SCHEME_FORMAT_H

#include "tll/scheme.h"
#include "tll/scheme/types.h"
#include "tll/scheme/util.h"
#include "tll/util/result.h"
#include "tll/util/string.h"

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

template <typename Int>
std::string to_string_fixed(unsigned precision, Int v)
{
	if (precision == 0)
		return tll::conv::to_string(v);

	bool minus = v < (Int) 0;
	if (minus)
		v = -v;
	std::string r;
	r.resize(1 + 1 + std::max<unsigned>(precision, 20u)); // sign, dot, 20 digits for 2^64 or precision
	auto ptr = r.end();

	unsigned prec = precision;
	for (; prec > 0; prec--) {
		if (v == 0) break;
		auto tmp = v / 10;
		auto digit = v % 10;
		v = tmp;
		*--ptr = '0' + digit;
	}
	for (; prec > 0; prec--)
		*--ptr = '0';
	if (prec == 0)
		*--ptr = '.';
	while (v != 0) {
		auto tmp = v / 10;
		auto digit = v % 10;
		v = tmp;
		*--ptr = '0' + digit;
	}
	if (minus)
		*--ptr = '-';
	r.erase(0, ptr - r.begin());
	return r;
}

template <typename Int>
format_result_t to_strings_number(const tll::scheme::Field * field, Int v)
{
	if constexpr (!std::is_floating_point_v<Int>) {
		if (field->sub_type == field->Fixed)
			return std::list<std::string> { to_string_fixed(field->fixed_precision, v) };
	}
	auto r = std::list<std::string> {tll::conv::to_string(v)};
	if (field->sub_type == field->Duration)
		r.front() += time_resolution_str(field->time_resolution);
	else if (field->sub_type == field->TimePoint)
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

	switch (field->type) {
	case Field::Int8:  return to_strings_number(field, *data.template dataT<int8_t>());
	case Field::Int16: return to_strings_number(field, *data.template dataT<int16_t>());
	case Field::Int32: return to_strings_number(field, *data.template dataT<int32_t>());
	case Field::Int64: return to_strings_number(field, *data.template dataT<int64_t>());
	case Field::UInt8:  return to_strings_number(field, *data.template dataT<uint8_t>());
	case Field::UInt16: return to_strings_number(field, *data.template dataT<uint16_t>());
	case Field::UInt32: return to_strings_number(field, *data.template dataT<uint32_t>());
	case Field::Double: return to_strings_number(field, *data.template dataT<double>());
	case Field::Decimal128:
		return std::list<std::string> {fmt::format("0x{:016x}{:016x}", *data.template dataT<uint64_t>(), *data.view(8).template dataT<uint64_t>())};
	case Field::Bytes: {
		auto ptr = data.template dataT<const char>();
		if (field->sub_type == Field::ByteString) {
			return std::list<std::string> {'"' + std::string(ptr, strnlen(ptr, field->size)) + '"'};
		}
		std::string result;
		result.reserve(field->size * 4);
		for (auto c = ptr; c < ptr + field->size; c++) {
			if (tll::util::printable(*c) && *c != '"')
				result.push_back(*c);
			else
				result += fmt::format("\\x{:02x}", *c);
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
			std::string_view r;
			if (ptr->size)
				r = {data.view(ptr->offset).template dataT<const char>(), ptr->size - 1};
			return std::list<std::string> { '"' + std::string(r)  + '"' };
		}

		return to_strings_list(field->type_ptr, data.view(ptr->offset), ptr->size, ptr->entity);
	}
	case Field::Message:
		return to_strings(field->type_msg, data);
	}
	return unexpected(path_error_t {"", fmt::format("unknown field type: {}", field->type)});
}

template <typename View>
format_result_t to_strings(const tll::scheme::Message * msg, const View &data)
{
	if (data.size() < msg->size)
		return unexpected(path_error_t {"", fmt::format("Message size too small: {} < {}", data.size(), msg->size)});
	std::list<std::string> result;
	for (auto f = msg->fields; f; f = f->next) {
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
