// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_SCHEME_ENCODER_H
#define _TLL_SCHEME_ENCODER_H

#include <tll/config.h>
#include <tll/scheme/error-stack.h>
#include <tll/scheme/util.h>
#include <tll/conv/numeric.h>
#include <tll/conv/decimal128.h>
#include <tll/util/memoryview.h>
#include <tll/util/time.h>

#include <optional>
#include <vector>

namespace tll::scheme {

struct ConfigEncoder : public ErrorStack
{
	struct Settings {
		bool strict = true;
	};

	Settings settings = {};

	template <typename View>
	int encode(View view, const tll::scheme::Message * msg, const tll::ConstConfig &cfg)
	{
		View pmap = msg->pmap ? view.view(msg->pmap->offset) : view;
		for (auto & [p, c] : cfg.browse("*", true)) {
			auto f = tll::scheme::lookup_name(msg->fields, p);
			if (!f) {
				if (!settings.strict)
					continue;
				return fail(ENOENT, "Field {} not found in message {}", p, msg->name);
			}
			if (msg->pmap && f->index >= 0)
				tll::scheme::pmap_set(pmap.data(), f->index);
			if (auto r = encode(view.view(f->offset), f, c); r)
				return fail_field(r, f);
		}
		return 0;
	}

	template <typename View>
	int encode(View data, const tll::scheme::Field * field, const tll::ConstConfig &cfg);

	template <typename T>
	int _fill_numeric(T * ptr, const tll::scheme::Field * field, std::string_view s);

	template <typename T>
	int _fill_conv(void * ptr, std::string_view s)
	{
		auto v = tll::conv::to_any<T>(s);
		if (v) {
			*static_cast<T *>(ptr) = *v;
			return 0;
		}
		return fail(EINVAL, "Invalid string '{}': {}", s, v.error());
	}
};

template <typename View>
int ConfigEncoder::encode(View view, const tll::scheme::Field * field, const tll::ConstConfig &cfg)
{
	using tll::scheme::Field;
	if (field->type == Field::Message) {
		return encode(view, field->type_msg, cfg);
	} else if (field->type == Field::Array) {
		auto l = cfg.browse("*", true);
		if (l.size() > field->count)
			return fail(ERANGE, "List size {} larger then {}", l.size(), field->count);
		tll::scheme::write_size(field->count_ptr, view.view(field->count_ptr->offset), l.size());
		auto data = view.view(field->type_array->offset);
		unsigned i = 0;
		for (auto & [_, c] : l) {
			if (auto r = encode(data.view(i++ * field->type_array->size), field->type_array, c); r)
				return fail_index(r, i - 1);
		}
		return 0;
	} else if (field->type == Field::Pointer) {
		if (field->sub_type == field->ByteString) {
			auto v = cfg.get();
			if (!v)
				return 0;

			tll::scheme::generic_offset_ptr_t ptr;
			ptr.size = v->size() + 1;
			ptr.entity = 1;

			if (tll::scheme::alloc_pointer(field, view, ptr))
				return fail(ERANGE, "Offset string out of range");
			auto data = view.view(ptr.offset);
			memmove(data.data(), v->data(), v->size());
			*data.view(v->size()).template dataT<char>() = 0;
			return 0;
		}

		auto l = cfg.browse("*", true);

		tll::scheme::generic_offset_ptr_t ptr;
		ptr.size = l.size();
		ptr.entity = field->type_ptr->size;

		if (tll::scheme::alloc_pointer(field, view, ptr))
			return fail(ERANGE, "Offset list out of range");
		auto data = view.view(ptr.offset);
		unsigned i = 0;
		for (auto & [_, c] : l) {
			if (auto r = encode(data.view(i++ * field->type_ptr->size), field->type_ptr, c); r)
				return fail_index(r, i - 1);
		}
		return 0;
	} else if (field->type == Field::Union) {
		auto l = cfg.browse("*", true);
		if (l.size() == 0)
			return 0;
		if (l.size() > 1)
			return fail(EINVAL, "Failed to fill union: too many keys");
		auto key = l.begin()->first;
		for (auto i = 0u; i < field->type_union->fields_size; i++) {
			auto uf = field->type_union->fields + i;
			if (key == uf->name) {
				tll::scheme::write_size(field->type_union->type_ptr, view, i);
				if (auto r = encode(view.view(uf->offset), uf, l.begin()->second); r)
					return fail_field(r, uf);
				return 0;
			}
		}
		return fail(EINVAL, "Unknown union type: {}", key);
	}

	auto v = cfg.get();
	if (!v)
		return 0;

	switch (field->type) {
	case Field::Int8: return _fill_numeric(view.template dataT<int8_t>(), field, *v);
	case Field::Int16: return _fill_numeric(view.template dataT<int16_t>(), field, *v);
	case Field::Int32: return _fill_numeric(view.template dataT<int32_t>(), field, *v);
	case Field::Int64: return _fill_numeric(view.template dataT<int64_t>(), field, *v);
	case Field::UInt8: return _fill_numeric(view.template dataT<uint8_t>(), field, *v);
	case Field::UInt16: return _fill_numeric(view.template dataT<uint16_t>(), field, *v);
	case Field::UInt32: return _fill_numeric(view.template dataT<uint32_t>(), field, *v);
	case Field::UInt64: return _fill_numeric(view.template dataT<uint64_t>(), field, *v);
	case Field::Double: return _fill_numeric(view.template dataT<double>(), field, *v);

	case Field::Decimal128: {
		auto r = tll::conv::to_any<tll::util::Decimal128>(*v);
		if (!r)
			return fail(EINVAL, "Invalid decimal128 string '{}': {}", *v, r.error());
		*view.template dataT<tll::util::Decimal128>() = *r;
		return 0;
	}

	case Field::Bytes: {
		if (v->size() > field->size)
			return fail(ERANGE, "Value '{}' is longer then field size {}", *v, field->size);
		memmove(view.data(), v->data(), v->size());
		memset(view.view(v->size()).data(), 0, field->size - v->size());
		return 0;
	}

	case Field::Message:
	case Field::Array:
	case Field::Pointer:
	case Field::Union:
		// Already handled, allows compiler to check completeness
		break;
	}
	return 0;
}

template <typename T>
int ConfigEncoder::_fill_numeric(T * ptr, const tll::scheme::Field * field, std::string_view s)
{
	if constexpr (!std::is_same_v<T, double>) {
		if (field->sub_type == field->Bits) {
			*ptr = 0;
			for (auto v : tll::split<'|', ','>(s)) {
				v = tll::util::strip(v);

				auto b = field->type_bits->values;
				for (; b; b = b->next) {
					if (v == b->name)
						break;
				}

				if (b) {
					*ptr |= 1ull << b->offset;
					continue;
				}

				auto ri = tll::conv::to_any<T>(v);
				if (!ri)
					return fail(EINVAL, "Invalid component value: {}", v);
				*ptr |= *ri;
			}
			return 0;
		}
	}
	if (field->sub_type == field->Enum) {
		auto num = tll::conv::to_any<T>(s);
		if (num) {
			*ptr = *num;
			return 0;
		}
		for (auto e = field->type_enum->values; e; e = e->next) {
			if (e->name == s) {
				*ptr = static_cast<T>(e->value);
				return 0;
			}
		}
		return fail(EINVAL, "String '{}' does not match any enum {} values", s, field->type_enum->name);
	} else if (field->sub_type == field->Fixed) {
		if constexpr (!std::is_floating_point_v<T>) {
			int prec = -(int) field->fixed_precision;

			auto u = conv::to_any<tll::conv::unpacked_float<T>>(s);
			if (!u)
				return fail(EINVAL, "Invalid number '{}': {}", s, u.error());
			auto m = u->mantissa;
			if (u->sign) {
				if constexpr (std::is_unsigned_v<T>)
					return fail(EINVAL, "Invalid number '{}': negative value", s);
				m = -m;
			}
			auto r = tll::util::FixedPoint<T, 0>::normalize_mantissa(m, u->exponent, prec);
			if (!std::holds_alternative<T>(r))
				return fail(EINVAL, "Invalid number '{}': {}", s, std::get<1>(r));
			*ptr = std::get<0>(r);
			return 0;
		}
	} else if (field->sub_type == field->TimePoint) {
		using std::chrono::duration;
		using std::chrono::time_point;
		using std::chrono::system_clock;
		switch (field->time_resolution) {
		case TLL_SCHEME_TIME_NS: return _fill_conv<time_point<system_clock, duration<T, std::nano>>>(ptr, s);
		case TLL_SCHEME_TIME_US: return _fill_conv<time_point<system_clock, duration<T, std::micro>>>(ptr, s);
		case TLL_SCHEME_TIME_MS: return _fill_conv<time_point<system_clock, duration<T, std::milli>>>(ptr, s);
		case TLL_SCHEME_TIME_SECOND: return _fill_conv<time_point<system_clock, duration<T, std::ratio<1, 1>>>>(ptr, s);
		case TLL_SCHEME_TIME_MINUTE: return _fill_conv<time_point<system_clock, duration<T, std::ratio<60, 1>>>>(ptr, s);
		case TLL_SCHEME_TIME_HOUR: return _fill_conv<time_point<system_clock, duration<T, std::ratio<3600, 1>>>>(ptr, s);
		case TLL_SCHEME_TIME_DAY: return _fill_conv<time_point<system_clock, duration<T, std::ratio<86400, 1>>>>(ptr, s);
		}
		return fail(EINVAL, "Unknown resolution: {}", field->time_resolution);
	} else if (field->sub_type == field->Duration) {
		using std::chrono::duration;
		switch (field->time_resolution) {
		case TLL_SCHEME_TIME_NS: return _fill_conv<duration<T, std::nano>>(ptr, s);
		case TLL_SCHEME_TIME_US: return _fill_conv<duration<T, std::micro>>(ptr, s);
		case TLL_SCHEME_TIME_MS: return _fill_conv<duration<T, std::milli>>(ptr, s);
		case TLL_SCHEME_TIME_SECOND: return _fill_conv<duration<T, std::ratio<1, 1>>>(ptr, s);
		case TLL_SCHEME_TIME_MINUTE: return _fill_conv<duration<T, std::ratio<60, 1>>>(ptr, s);
		case TLL_SCHEME_TIME_HOUR: return _fill_conv<duration<T, std::ratio<3600, 1>>>(ptr, s);
		case TLL_SCHEME_TIME_DAY: return _fill_conv<duration<T, std::ratio<86400, 1>>>(ptr, s);
		}
		return fail(EINVAL, "Unknown resolution: {}", field->time_resolution);
	}
	return _fill_conv<T>(ptr, s);
}

} // namespace tll::scheme

#endif//_TLL_SCHEME_ENCODER_H
