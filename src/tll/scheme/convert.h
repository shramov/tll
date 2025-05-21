// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_SCHEME_CONVERT_H
#define _TLL_SCHEME_CONVERT_H

#include "tll/logger.h"
#include "tll/scheme.h"
#include "tll/scheme/error-stack.h"
#include "tll/scheme/format.h"
#include "tll/util/decimal128.h"
#include "tll/util/memoryview.h"

namespace tll::scheme {

struct Convert : public ErrorStack
{
	std::map<int, const Message *> map_from;
	SchemePtr scheme_from;
	SchemePtr scheme_into;

	struct Ratio
	{
		unsigned long mul = 1;
		unsigned long div = 1;

		void simplify()
		{
			if (mul == div) {
				mul = div = 1;
			} else if (mul > div) {
				mul /= div;
				div = 1;
			} else {
				div /= mul;
				mul = 1;
			}
		}

		void flip()
		{
			std::swap(mul, div);
		}
	};

	tll::Logger log = { "tll.scheme.convert" };

	struct MessageInto
	{
		const Message * into = nullptr;
	};

	struct FieldFrom
	{
		const Field * from = nullptr;
		enum Mode {
			Trivial,
			Copy,
			Complex,
		} mode = Complex;
		std::map<long long, long long> enum_map;
	};

	void reset()
	{
		scheme_from.reset();
		scheme_into.reset();
		map_from.clear();
	}

	int init(const tll::Logger &log, const Scheme * from, const Scheme * into)
	{
		map_from.clear();
		if (!from || !into)
			return EINVAL;
		this->log = log;
		scheme_from.reset(from->copy());
		scheme_into.reset(into->copy());
		for (auto m = scheme_from->messages; m; m = m->next) {
			if (m->msgid)
				map_from[m->msgid] = m;
			if (m->user)
				continue;
			auto into = scheme_into->lookup(m->name);
			if (!into)
				continue;
			if (!convertible(into, m))
				return log.fail(EINVAL, "Message {} can not be converted", m->name);
		}
		return 0;
	}

	bool convertible(Message * into, Message * from)
	{
		log.debug("Bind message {} to {}", from->name, into->name);
		if (from->user)
			return true;
		from->user = new MessageInto { .into = into };
		from->user_free = [](void * ptr) { delete static_cast<MessageInto *>(ptr); };

		for (auto f = into->fields; f; f = f->next) {
			auto ffrom = tll::scheme::lookup_name(from->fields, f->name);
			if (!ffrom)
				continue;
			if (!convertible(f, ffrom))
				return log.fail(false, "Message {} field {} can not be converted", into->name, f->name);
		}
		return true;
	}

	bool convertible(Field * into, Field * from);
	bool convertible_numeric(Field * into, const Field * from);

	template <typename View, typename ViewIn>
	int convert(View view, const tll::scheme::Message * msg, ViewIn from)
	{
		auto user = static_cast<const MessageInto *>(msg->user);
		if (!user)
			return fail(EINVAL, "Message {} not found in destination scheme", msg->name);
		if (view.size() < user->into->size)
			view.resize(user->into->size);
		auto ipmap = user->into->pmap ? view.view(user->into->pmap->offset) : view;
		const void * fpmap = msg->pmap ? from.view(msg->pmap->offset).data() : nullptr;
		for (auto finto = user->into->fields; finto; finto = finto->next) {
			auto fuser = static_cast<const FieldFrom *>(finto->user);
			if (!fuser)
				continue;
			auto ffrom = fuser->from;
			if (fpmap && !pmap_get(fpmap, ffrom->index))
				continue;
			if (user->into->pmap) {
				if (finto == user->into->pmap)
					continue;
				pmap_set(ipmap.data(), finto->index);
			}
			if (auto r = convert(view.view(finto->offset), finto, from.view(ffrom->offset), ffrom); r)
				return fail_field(r, ffrom);
		}
		return 0;
	}

	template <typename View, typename ViewIn>
	int convert(View into, const Field * finto, ViewIn from, const Field * ffrom);

	template <typename View, typename ViewIn>
	int convert_array(View into, const Field * finto, ViewIn from, const Field * ffrom);

	template <typename View, typename ViewIn>
	int convert_pointer(View into, const Field * finto, ViewIn from, const Field * ffrom);

	template <typename View, typename ViewIn>
	int convert_vstring(View into, const Field * finto, ViewIn from, const Field * ffrom);

	template <typename ViewIn>
	int convert_bytes(tll::memory into, ViewIn from, const Field * ffrom);

	template <typename ViewIn>
	format_result_t to_string(ViewIn from, const Field * ffrom);

	template <typename ViewIn>
	int convert_string(tll::memory into, ViewIn from, const Field * ffrom);

	template <typename T, typename ViewIn>
	int convert_numeric(T * into, const Field * finto, ViewIn from, const Field * ffrom);

	template <typename ViewIn>
	int convert_decimal128(tll::util::Decimal128 * into, ViewIn from, const Field * ffrom);

	template <typename T, typename ViewIn>
	int convert_fixed(T * into, const Field * finto, ViewIn from, const Field * ffrom);

	template <typename T, typename From>
	int convert_fixed_numeric(T * into, int prec, From from, const Field * ffrom);

	template <typename T, typename From>
	int convert_time_numeric(T * into, Ratio prec, From from, const Field * ffrom);

	template <typename T, typename ViewIn>
	int convert_time_point(T * into, const Field * finto, ViewIn from, const Field * ffrom);

	template <typename T, typename From>
	int convert_numeric_numeric(T * into, const Field * finto, From from, const Field * ffrom);

	template <typename T>
	int convert_raw_decimal128(T * into, const tll::util::Decimal128 * from, const Field * ffrom);

	template <typename T, typename From>
	int check_overflow(T * into, From from, T mul = 1);
};

template <typename View, typename ViewIn>
int Convert::convert(View into, const Field * finto, ViewIn from, const Field * ffrom)
{
	auto user = static_cast<FieldFrom *>(finto->user);
	if (user->mode == FieldFrom::Trivial || user->mode == FieldFrom::Copy) {
		// If buffer is not zeroed - memset reset in Copy mode
		memcpy(into.data(), from.data(), ffrom->size);
		return 0;
	}
	switch (finto->type)
	{
	case Field::Int8: return convert_numeric(into.template dataT<int8_t>(), finto, from, ffrom);
	case Field::Int16: return convert_numeric(into.template dataT<int16_t>(), finto, from, ffrom);
	case Field::Int32: return convert_numeric(into.template dataT<int32_t>(), finto, from, ffrom);
	case Field::Int64: return convert_numeric(into.template dataT<int64_t>(), finto, from, ffrom);
	case Field::UInt8: return convert_numeric(into.template dataT<uint8_t>(), finto, from, ffrom);
	case Field::UInt16: return convert_numeric(into.template dataT<uint16_t>(), finto, from, ffrom);
	case Field::UInt32: return convert_numeric(into.template dataT<uint32_t>(), finto, from, ffrom);
	case Field::UInt64: return convert_numeric(into.template dataT<uint64_t>(), finto, from, ffrom);
	case Field::Double: return convert_numeric(into.template dataT<double>(), finto, from, ffrom);
	case Field::Decimal128: return convert_decimal128(into.template dataT<tll::util::Decimal128>(), from, ffrom);
	case Field::Bytes:
		if (finto->sub_type == Field::ByteString)
			return convert_string({ into.data(), finto->size }, from, ffrom);
		return convert_bytes({ into.data(), finto->size }, from, ffrom);
	case Field::Array: return convert_array(into, finto, from, ffrom);
	case Field::Pointer:
		if (finto->sub_type == Field::ByteString)
			return convert_vstring(into, finto, from, ffrom);
		return convert_pointer(into, finto, from, ffrom);
	case Field::Message:
		if (ffrom->type != Field::Message)
			return fail(EINVAL, "Can not convert non-message field {} to message", ffrom->type);
		return convert(into, ffrom->type_msg, from);
	default:
		break;
	}
	return fail(EINVAL, "Unsupported field type {}", finto->type);
}

template <typename View, typename ViewIn>
int Convert::convert_array(View into, const Field * finto, ViewIn from, const Field * ffrom)
{
	if (ffrom->type == Field::Array) {
		auto size = read_size(ffrom->count_ptr, from.view(ffrom->count_ptr->offset));

		if (size == 0)
			return 0;
		if (size > (ssize_t) finto->count)
			return fail(ERANGE, "Source list size too large: {} > maximum {}", size, finto->count);

		write_size(finto->count_ptr, into.view(finto->count_ptr->offset), size);

		auto iuser = static_cast<const FieldFrom *>(finto->type_array->user);
		if (iuser->mode == FieldFrom::Trivial) {
			memcpy(into.view(finto->type_array->offset).data(), from.view(ffrom->type_array->offset).data(), size * ffrom->type_array->size);
			return 0;
		}

		auto pinto = into.view(finto->type_array->offset);
		auto pfrom = from.view(ffrom->type_array->offset);
		for (auto i = 0u; i < size; i++) {
			if (auto r = convert(pinto.view(finto->type_array->size * i), finto->type_array, pfrom.view(ffrom->type_array->size * i), ffrom->type_ptr); r)
				return fail_index(r, i);
		}
		return 0;
	} else if (ffrom->type == Field::Pointer) {
		auto ptr = read_pointer(ffrom, from);
		if (!ptr)
			return fail(EINVAL, "Unknown offset ptr version: {}", ffrom->offset_ptr_version);
		if (ptr->offset > from.size())
			return fail(EINVAL, "Offset out of bounds: offset {} > data size {}", ptr->offset, from.size());
		else if (ptr->offset + ptr->size * ptr->entity > from.size())
			return fail(EINVAL, "Offset data out of bounds: offset {} + data {} * entity {} > data size {}", ptr->offset, ptr->size, ptr->entity, from.size());

		if (ptr->size == 0)
			return 0;
		if (ptr->size > finto->count)
			return fail(ERANGE, "Source list size too large: {} > maximum {}", ptr->size, finto->count);

		write_size(finto->count_ptr, into.view(finto->count_ptr->offset), ptr->size);

		auto iuser = static_cast<const FieldFrom *>(finto->type_array->user);
		if (iuser->mode == FieldFrom::Trivial) {
			memcpy(into.view(finto->type_array->offset).data(), from.view(ptr->offset).data(), ptr->size * ffrom->type_ptr->size);
			return 0;
		}

		auto pinto = into.view(finto->type_array->offset);
		auto pfrom = from.view(ptr->offset);
		for (auto i = 0u; i < ptr->size; i++) {
			if (auto r = convert(pinto.view(finto->type_array->size * i), finto->type_array, pfrom.view(ptr->entity * i), ffrom->type_ptr); r)
				return fail_index(r, i);
		}
		return 0;
	}
	return fail(EINVAL, "Array conversion not implemented");
}

template <typename View, typename ViewIn>
int Convert::convert_pointer(View into, const Field * finto, ViewIn from, const Field * ffrom)
{
	if (ffrom->type == Field::Array) {
		auto size = read_size(ffrom->count_ptr, from.view(ffrom->count_ptr->offset));

		if (size == 0)
			return 0;

		generic_offset_ptr_t wptr;
		wptr.size = size;
		wptr.entity = finto->type_ptr->size;

		if (tll::scheme::alloc_pointer(finto, into, wptr))
			return fail(ERANGE, "Offset pointer out of range");

		auto pinto = into.view(wptr.offset);
		auto pfrom = from.view(ffrom->type_array->offset);
		for (auto i = 0u; i < size; i++) {
			if (auto r = convert(pinto.view(wptr.entity * i), finto->type_ptr, pfrom.view(ffrom->type_array->size * i), ffrom->type_ptr); r)
				return fail_index(r, i);
		}
		return 0;
	} else if (ffrom->type == Field::Pointer) {
		auto ptr = read_pointer(ffrom, from);
		if (!ptr)
			return fail(EINVAL, "Unknown offset ptr version: {}", ffrom->offset_ptr_version);
		if (ptr->offset > from.size())
			return fail(EINVAL, "Offset out of bounds: offset {} > data size {}", ptr->offset, from.size());
		else if (ptr->offset + ptr->size * ptr->entity > from.size())
			return fail(EINVAL, "Offset data out of bounds: offset {} + data {} * entity {} > data size {}", ptr->offset, ptr->size, ptr->entity, from.size());

		if (ptr->size == 0)
			return 0;

		generic_offset_ptr_t wptr;
		wptr.size = ptr->size;
		wptr.entity = finto->type_ptr->size;

		if (tll::scheme::alloc_pointer(finto, into, wptr))
			return fail(ERANGE, "Offset pointer out of range");

		auto pinto = into.view(wptr.offset);
		auto pfrom = from.view(ptr->offset);
		for (auto i = 0u; i < wptr.size; i++) {
			if (auto r = convert(pinto.view(wptr.entity * i), finto->type_ptr, pfrom.view(ptr->entity * i), ffrom->type_ptr); r)
				return fail_index(r, i);
		}
		return 0;
	}
	return fail(EINVAL, "Can not convert Pointer from {}", ffrom->type);
}

template <typename View, typename ViewIn>
int Convert::convert_vstring(View into, const Field * finto, ViewIn from, const Field * ffrom)
{
	std::string storage;
	std::string_view v;
	if (ffrom->type == Field::Bytes) {
		if (ffrom->sub_type != Field::ByteString)
			return fail(EINVAL, "Can not convert Bytes to string");
		v = { from.template dataT<char>(), strnlen(from.template dataT<char>(), ffrom->size) };
	} else if (ffrom->type == Field::Pointer) {
		if (ffrom->sub_type != Field::ByteString)
			return fail(EINVAL, "Can not convert Pointer to string");
		auto ptr = read_pointer(ffrom, from);
		if (!ptr)
			return fail(EINVAL, "Unknown offset ptr version: {}", ffrom->offset_ptr_version);
		if (ptr->offset > from.size())
			return fail(EINVAL, "Offset out of bounds: offset {} > data size {}", ptr->offset, from.size());
		else if (ptr->offset + ptr->size * ptr->entity > from.size())
			return fail(EINVAL, "Offset data out of bounds: offset {} + data {} * entity {} > data size {}", ptr->offset, ptr->size, ptr->entity, from.size());
		if (ptr->size)
			v = {from.view(ptr->offset).template dataT<const char>(), ptr->size - 1};
	} else {
		auto r = to_string(from, ffrom);
		if (!r)
			return fail(EINVAL, "Failed to convert field to string: {}", r.error().second);
		std::swap(storage, r->front());
		v = storage;
	}

	tll::scheme::generic_offset_ptr_t ptr;
	ptr.size = v.size() + 1;
	ptr.entity = 1;

	if (tll::scheme::alloc_pointer(finto, into, ptr))
		return fail(ERANGE, "Offset string out of range");
	auto data = into.view(ptr.offset);
	memcpy(data.data(), v.data(), v.size());
	*data.view(v.size()).template dataT<char>() = 0;
	return 0;
}

template <typename ViewIn>
int Convert::convert_string(tll::memory into, ViewIn from, const Field * ffrom)
{
	std::string storage;
	std::string_view v;
	if (ffrom->type == Field::Bytes) {
		if (ffrom->sub_type != Field::ByteString)
			return fail(EINVAL, "Can not convert Bytes to string");
		v = { from.template dataT<char>(), strnlen(from.template dataT<char>(), ffrom->size) };
	} else if (ffrom->type == Field::Pointer) {
		if (ffrom->sub_type != Field::ByteString)
			return fail(EINVAL, "Can not convert Pointer to string");
		auto ptr = read_pointer(ffrom, from);
		if (!ptr)
			return fail(EINVAL, "Unknown offset ptr version: {}", ffrom->offset_ptr_version);
		if (ptr->offset > from.size())
			return fail(EINVAL, "Offset out of bounds: offset {} > data size {}", ptr->offset, from.size());
		else if (ptr->offset + ptr->size * ptr->entity > from.size())
			return fail(EINVAL, "Offset data out of bounds: offset {} + data {} * entity {} > data size {}", ptr->offset, ptr->size, ptr->entity, from.size());
		if (ptr->size)
			v = {from.view(ptr->offset).template dataT<const char>(), ptr->size - 1};
	} else {
		auto r = to_string(from, ffrom);
		if (!r)
			return fail(EINVAL, "Failed to convert field to string: {}", r.error().second);
		std::swap(storage, r->front());
		v = storage;
	}

	if (v.size() > into.size)
		return fail(EINVAL, "String result '{}' is too long: {} > max {}", v, v.size(), into.size);
	memcpy(into.data, v.data(), v.size());
	return 0;
}

template <typename ViewIn>
format_result_t Convert::to_string(ViewIn from, const Field * ffrom)
{
	switch (ffrom->type) {
	case Field::Int8:  return to_strings_number(ffrom, *from.template dataT<int8_t>());
	case Field::Int16: return to_strings_number(ffrom, *from.template dataT<int16_t>());
	case Field::Int32: return to_strings_number(ffrom, *from.template dataT<int32_t>());
	case Field::Int64: return to_strings_number(ffrom, *from.template dataT<int64_t>());
	case Field::UInt8:  return to_strings_number(ffrom, *from.template dataT<uint8_t>());
	case Field::UInt16: return to_strings_number(ffrom, *from.template dataT<uint16_t>());
	case Field::UInt32: return to_strings_number(ffrom, *from.template dataT<uint32_t>());
	case Field::UInt64: return to_strings_number(ffrom, *from.template dataT<uint64_t>());
	case Field::Double: return to_strings_number(ffrom, *from.template dataT<double>());
	case Field::Decimal128:
		return std::list<std::string> { tll::conv::to_string(*from.template dataT<tll::util::Decimal128>()) };
	case Field::Bytes:
	case Field::Pointer:
		// Should be handled in fast path
		break;
	case Field::Array:
	case Field::Message:
	case Field::Union:
		break;
	}
	return unexpected(path_error_t {"", fmt::format("Can not convert {} to string", ffrom->type) });
}

template <typename ViewIn>
int Convert::convert_bytes(tll::memory into, ViewIn from, const Field * ffrom)
{
	if (ffrom->type != Field::Bytes)
		return fail(EINVAL, "Can not convert bytes from {}", ffrom->type);
	memcpy(into.data, from.data(), std::min(into.size, ffrom->size));
	return 0;
}

template <typename T, typename ViewIn>
int Convert::convert_numeric(T * into, const Field * finto, ViewIn from, const Field * ffrom)
{
	switch (ffrom->type) {
	case Field::Int8: return convert_numeric_numeric(into, finto, *from.template dataT<int8_t>(), ffrom);
	case Field::Int16: return convert_numeric_numeric(into, finto, *from.template dataT<int16_t>(), ffrom);
	case Field::Int32: return convert_numeric_numeric(into, finto, *from.template dataT<int32_t>(), ffrom);
	case Field::Int64: return convert_numeric_numeric(into, finto, *from.template dataT<int64_t>(), ffrom);
	case Field::UInt8: return convert_numeric_numeric(into, finto, *from.template dataT<uint8_t>(), ffrom);
	case Field::UInt16: return convert_numeric_numeric(into, finto, *from.template dataT<uint16_t>(), ffrom);
	case Field::UInt32: return convert_numeric_numeric(into, finto, *from.template dataT<uint32_t>(), ffrom);
	case Field::UInt64: return convert_numeric_numeric(into, finto, *from.template dataT<uint64_t>(), ffrom);
	case Field::Double: return convert_numeric_numeric(into, finto, *from.template dataT<double>(), ffrom);
	//case Field::Decimal128: return convert_raw_decimal128(into, *from.template dataT<tll::util::Decimal128>(), ffrom);
	default:
		return fail(EINVAL, "Can not convert {} into {}", ffrom->type, finto->type);
	}
	return 0;
}

template <typename T, typename From>
int Convert::check_overflow(T * into, From from, T mul)
{
	if constexpr (std::is_floating_point_v<T>) {
		if (from < std::numeric_limits<T>::min() / mul)
			return -1;
		if (from > std::numeric_limits<T>::max() / mul)
			return 1;
	} else if constexpr (std::is_unsigned_v<T>) {
		if constexpr (std::is_floating_point_v<From>) {
			if (from < std::numeric_limits<T>::min() / mul)
				return -1;
			if (from > std::numeric_limits<T>::max() / mul)
				return 1;
		} else if constexpr (std::is_unsigned_v<From>) {
			if (from > std::numeric_limits<T>::max() / mul)
				return -1;
		} else {
			if (from < 0)
				return -1;
			if (static_cast<std::make_unsigned_t<From>>(from) > std::numeric_limits<T>::max() / mul)
				return 1;
		}
	} else {
		if constexpr (std::is_unsigned_v<From>) {
			if (from > static_cast<std::make_unsigned_t<T>>(std::numeric_limits<T>::max() / mul))
				return 1;
		} else {
			if (from < std::numeric_limits<T>::min() / mul)
				return -1;
			if (from > std::numeric_limits<T>::max() / mul)
				return 1;
		}
	}
	return 0;
}

namespace {
unsigned long pow10(unsigned exp)
{
	unsigned long r = 1;
	while (exp-- > 0)
		r *= 10;
	return r;
}

constexpr Convert::Ratio resolution(tll_scheme_time_resolution_t v)
{
	switch (v) {
	case TLL_SCHEME_TIME_NS: return {1, 1000000000};
	case TLL_SCHEME_TIME_US: return {1, 1000000};
	case TLL_SCHEME_TIME_MS: return {1, 1000};
	case TLL_SCHEME_TIME_SECOND: return {1, 1};
	case TLL_SCHEME_TIME_MINUTE: return {60, 1};
	case TLL_SCHEME_TIME_HOUR: return {3600, 1};
	case TLL_SCHEME_TIME_DAY: return {86400, 1};
	}
	return {0, 0};
}

bool movable(const tll::scheme::Field * into, const tll::scheme::Field * from)
{
	using tll::scheme::Field;
	const auto ft = from->type;
	switch (into->type) {
	case Field::Int8:  return ft == Field::Int8;
	case Field::Int16: return ft == Field::Int8 || ft == Field::Int16;
	case Field::Int32: return ft == Field::Int8 || ft == Field::Int16 || ft == Field::Int32;
	case Field::Int64: return ft == Field::Int8 || ft == Field::Int16 || ft == Field::Int32 || ft == Field::Int64;
	case Field::UInt8:  return ft == Field::UInt8;
	case Field::UInt16: return ft == Field::UInt8 || ft == Field::UInt16;
	case Field::UInt32: return ft == Field::UInt8 || ft == Field::UInt16 || ft == Field::UInt32;
	case Field::UInt64: return ft == Field::UInt8 || ft == Field::UInt16 || ft == Field::UInt32 || ft == Field::UInt64;
	case Field::Double: return ft == Field::Double;
	case Field::Decimal128: return ft == Field::Decimal128;
	case Field::Bytes: return ft == Field::Bytes && from->size <= into->size;
	default:
		return false;
	}
}

Convert::FieldFrom::Mode copy_mode(const tll::scheme::Field * into, const tll::scheme::Field * from)
{
	if (!movable(into, from))
		return Convert::FieldFrom::Complex;
	if (into->type == from->type && into->size == from->size)
		return Convert::FieldFrom::Trivial;
	return Convert::FieldFrom::Copy;
}

constexpr Convert::FieldFrom::Mode get_field_mode(const tll::scheme::Field * f)
{
	return static_cast<const Convert::FieldFrom *>(f->user)->mode;
}
}

inline bool Convert::convertible(Field * into, Field * from)
{
	if (!into->user) {
		into->user = new FieldFrom { .from = from };
		into->user_free = [](void * ptr) { delete static_cast<FieldFrom *>(ptr); };
	}
	auto user = static_cast<FieldFrom *>(into->user);

	switch (into->type) {
	case Field::Int8: return convertible_numeric(into, from);
	case Field::Int16: return convertible_numeric(into, from);
	case Field::Int32: return convertible_numeric(into, from);
	case Field::Int64: return convertible_numeric(into, from);
	case Field::UInt8: return convertible_numeric(into, from);
	case Field::UInt16: return convertible_numeric(into, from);
	case Field::UInt32: return convertible_numeric(into, from);
	case Field::UInt64: return convertible_numeric(into, from);
	case Field::Double: return convertible_numeric(into, from);
	case Field::Decimal128: return from->type == Field::Decimal128;
	case Field::Bytes:
		if (into->sub_type == Field::ByteString) {
			switch (from->type) {
			case Field::Array:
			case Field::Message:
			case Field::Union:
				return false;
			case Field::Pointer:
				return from->sub_type == Field::ByteString;
			default:
				return true;
			}
		} else
			return from->type == Field::Bytes;
	case Field::Message:
		if (from->type != Field::Message)
			return false;
		return convertible(into->type_msg, from->type_msg);
	case Field::Array:
		switch (from->type) {
		case Field::Array:
			if (!convertible(into->count_ptr, from->count_ptr)) // Fill user data
				return false;
			if (!convertible(into->type_array, from->type_array))
				return false;
			if (get_field_mode(into->count_ptr) == FieldFrom::Trivial && get_field_mode(into->type_array) == FieldFrom::Trivial) {
				if (into->count == from->count)
					user->mode = FieldFrom::Trivial;
				else if (into->count > from->count)
					user->mode = FieldFrom::Copy;
			}

		case Field::Pointer:
			if (from->sub_type == Field::ByteString)
				return false;
			return convertible(into->type_array, from->type_ptr);
		default:
			return false;
		}
	case Field::Pointer:
		if (into->sub_type == Field::ByteString) {
			switch (from->type) {
			case Field::Array:
			case Field::Message:
			case Field::Union:
				return false;
			case Field::Pointer:
				return from->sub_type == Field::ByteString;
			default:
				return true;
			}
		} else {
			switch (from->type) {
			case Field::Array:
				return convertible(into->type_ptr, from->type_array);
			case Field::Pointer:
				if (from->sub_type == Field::ByteString)
					return false;
				return convertible(into->type_ptr, from->type_ptr);
			default:
				return false;
			}
		}
	default:
		return false;
	}
}

inline bool Convert::convertible_numeric(Field * into, const Field * from)
{
	auto user = static_cast<FieldFrom *>(into->user);
	switch (from->type) {
	case Field::Int8:
	case Field::Int16:
	case Field::Int32:
	case Field::Int64:
	case Field::UInt8:
	case Field::UInt16:
	case Field::UInt32:
	case Field::UInt64:
		break;
	case Field::Double:
		if (into->sub_type == Field::Enum)
			return false;
		break;
	default:
		return false;
	}

	if (into->sub_type == Field::Enum) {
		auto user = static_cast<FieldFrom *>(into->user);
		if (from->sub_type == Field::Enum) {
			bool trivial = true;
			for (auto v = from->type_enum->values; v; v = v->next) {
				auto vi = lookup_name(into->type_enum->values, v->name);
				trivial = trivial && (vi && vi->value == v->value);
			}
			if (trivial) { // Enum is same or extended
				user->mode = copy_mode(into, from);
				return true;
			}
			// Conversion map
			for (auto v = from->type_enum->values; v; v = v->next) {
				if (auto vi = lookup_name(into->type_enum->values, v->name); vi)
					user->enum_map.emplace(v->value, vi->value);
			}
		} else {
			// Validation map
			for (auto v = into->type_enum->values; v; v = v->next)
				user->enum_map.emplace(v->value, v->value);
		}
	} else if (into->sub_type == Field::Duration || into->sub_type == Field::TimePoint) {
		if (from->sub_type != Field::SubNone) {
			if (into->sub_type != from->sub_type)
				return false;
			if (into->time_resolution == from->time_resolution)
				user->mode = copy_mode(into, from);
		} else
			user->mode = copy_mode(into, from);
	} else if (into->sub_type == Field::Fixed) {
		if (from->sub_type == Field::Fixed) {
			if (into->fixed_precision == from->fixed_precision)
				user->mode = copy_mode(into, from);
		} else if (from->sub_type != Field::SubNone)
			return false;
	} else if (into->sub_type == Field::SubNone) {
		if (from->sub_type != Field::Fixed) {
			user->mode = copy_mode(into, from);
			return true;
		}
	}
	return true;
}

template <typename T, typename From>
int Convert::convert_fixed_numeric(T * into, int prec, From from, const Field * ffrom)
{
	T mul = 1;
	if (ffrom->sub_type == Field::Fixed) {
		if (auto delta = prec - (int) ffrom->fixed_precision; delta == 0) {
			// Same precision
		} else if (delta > 0) {
			mul = pow10(delta);
		} else {
			from /= pow10(-delta);
		}
	} else if (ffrom->sub_type == Field::SubNone) {
		if constexpr (std::is_floating_point_v<From>)
			from *= powl(10, prec);
		else
			mul = pow10(prec);
	} else
		return fail(EINVAL, "Can not convert non-fixed {}", ffrom->sub_type);

	if (auto r = check_overflow(into, from, mul); r) {
		if (r < 0)
			return fail(ERANGE, "Source value out of range: min {}, got {}", std::numeric_limits<T>::min(), from);
		else
			return fail(ERANGE, "Source value out of range: max {}, got {}", std::numeric_limits<T>::max(), from);
	}
	*into = from;
	*into *= mul;
	return 0;
}

template <typename T, typename From>
int Convert::convert_time_numeric(T * into, Convert::Ratio prec, From from, const Field * ffrom)
{
	if (ffrom->sub_type == Field::TimePoint || ffrom->sub_type == Field::Duration) {
		auto fprec = resolution(ffrom->time_resolution);
		prec.mul *= fprec.div;
		prec.div *= fprec.mul;
		prec.simplify();
		from /= prec.mul;
	} else if (ffrom->sub_type == Field::SubNone) {
		prec = {1, 1};
	} else
		return fail(EINVAL, "Can not convert from non-time {}", ffrom->sub_type);

	if (auto r = check_overflow(into, from, (T) prec.div); r) {
		if (r < 0)
			return fail(ERANGE, "Source value out of range: min {}, got {}", std::numeric_limits<T>::min(), from);
		else
			return fail(ERANGE, "Source value out of range: max {}, got {}", std::numeric_limits<T>::max(), from);
	}
	*into = from;
	*into *= prec.div;
	return 0;
}

template <typename T, typename From>
int Convert::convert_numeric_numeric(T * into, const Field * finto, From from, const Field * ffrom)
{
	auto user = static_cast<const FieldFrom *>(finto->user);
	if constexpr (!std::is_floating_point_v<T>) {
		if (finto->sub_type == Field::Fixed) {
			return convert_fixed_numeric(into, finto->fixed_precision, from, ffrom);
		} else if (finto->sub_type == Field::TimePoint || finto->sub_type == Field::Duration) {
			return convert_time_numeric(into, resolution(finto->time_resolution), from, ffrom);
		} else if (finto->sub_type == Field::Enum) {
			if (user->enum_map.empty()) {
				// Fast path, trivial copy
				*into = from;
				return 0;
			}

			auto it = user->enum_map.find(from);
			if (it == user->enum_map.end())
				return fail(EINVAL, "Unkonwn enum value {}", from);
			*into = it->second;
			return 0;
		}
	}

	if (ffrom->sub_type == Field::Fixed) {
		if (finto->sub_type != Field::SubNone)
			return fail(EINVAL, "Can not convert fixed to {}", finto->sub_type);
		if constexpr (std::is_floating_point_v<T>) {
			*into = from / powl(10, ffrom->fixed_precision);
			return 0;
		} else
			from /= pow10(ffrom->fixed_precision);
	}

	if (auto r = check_overflow(into, from); r) {
		if (r < 0)
			return fail(ERANGE, "Source value out of range: min {}, got {}", std::numeric_limits<T>::min(), from);
		else
			return fail(ERANGE, "Source value out of range: max {}, got {}", std::numeric_limits<T>::max(), from);
	}
	*into = from;
	return 0;
}

template <typename T>
int Convert::convert_raw_decimal128(T * into, const tll::util::Decimal128 * from, const Field * ffrom)
{
	tll::util::Decimal128::Unpacked u;
	from->unpack(u);
	if constexpr (!std::is_floating_point_v<T>) {
		if (u.isnan())
			*into = NAN;
		else if (u.isinf()) {
			if (u.sign)
				*into = -INFINITY;
			else
				*into = INFINITY;
		}
	} else {
		if (u.isnan())
			return fail(EINVAL, "Source value is NaN");
		else if (u.isinf())
			return fail(EINVAL, "Source value is infinity");
	}
	if (u.mantissa.hi == u.mantissa.lo == 0) {
		*into = 0;
		return 0;
	}

	return fail(EINVAL, "Can not convert decimal128");
}

template <typename ViewIn>
int Convert::convert_decimal128(tll::util::Decimal128 * into, ViewIn from, const Field * ffrom)
{
	if (ffrom->type != Field::Decimal128)
		return fail(EINVAL, "Can not convert non-decimal128: {}", ffrom->type);
	*into = *from.template dataT<tll::util::Decimal128>();
	return 0;
}

template <typename T, typename ViewIn>
int Convert::convert_fixed(T * into, const Field * finto, ViewIn from, const Field * ffrom)
{
	//const auto exponent = -finto->fixed_precision;

	switch (ffrom->type) {
	case Field::Int8: return convert_fixed_numeric(into, finto, *from.template dataT<int8_t>(), ffrom);
	case Field::Int16: return convert_fixed_numeric(into, finto, *from.template dataT<int16_t>(), ffrom);
	case Field::Int32: return convert_fixed_numeric(into, finto, *from.template dataT<int32_t>(), ffrom);
	case Field::Int64: return convert_fixed_numeric(into, finto, *from.template dataT<int64_t>(), ffrom);
	case Field::UInt8: return convert_fixed_numeric(into, finto, *from.template dataT<uint8_t>(), ffrom);
	case Field::UInt16: return convert_fixed_numeric(into, finto, *from.template dataT<uint16_t>(), ffrom);
	case Field::UInt32: return convert_fixed_numeric(into, finto, *from.template dataT<uint32_t>(), ffrom);
	case Field::UInt64: return convert_fixed_numeric(into, finto, *from.template dataT<uint64_t>(), ffrom);
	case Field::Double: return convert_fixed_numeric(into, finto, *from.template dataT<double>(), ffrom);
	default:
		return fail(EINVAL, "Can not convert {} into fixed point {}", ffrom->type, finto->type);
	}
}

} // namespace tll::scheme

#endif//_TLL_SCHEME_CONVERT_H
