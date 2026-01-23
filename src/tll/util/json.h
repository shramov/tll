/*
 * Copyright (c) 2018-2019 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_JSON_H
#define _TLL_UTIL_JSON_H

#include <deque>

#include <rapidjson/reader.h>
#include <rapidjson/writer.h>
#include <rapidjson/memorybuffer.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/error/en.h>

#include "tll/channel.h"
#include "tll/conv/decimal128.h"
#include "tll/conv/float.h"
#include "tll/logger.h"
#include "tll/scheme.h"
#include "tll/scheme/types.h"
#include "tll/scheme/util.h"
#include "tll/scheme/optr-util.h"
#include "tll/util/decimal128.h"
#include "tll/util/fixed_point.h"
#include "tll/util/memoryview.h"
#include "tll/util/listiter.h"
#include "tll/util/buffer.h"
#include "tll/util/time.h"

namespace tll::json {

using tll::scheme::Field;
using tll::scheme::Message;

using rapidjson::SizeType;

struct field_meta_t
{
	bool message_inline = false;
	bool enum_number = false;
	//bool bytes_string = false;
	bool skip = false;
	size_t list_size;
	std::map<long long, std::string_view> enum_values;
};

struct message_meta_t
{
	std::map<std::string_view, const Field *> index;
	//const Message * remap = nullptr;
	bool as_list = false;

	/// List of Pointer fields directly in this message (and non-pointer submessages)
	std::vector<const Field *> pointers;
};

struct scheme_meta_t
{
	std::map<std::string_view, const Message *> index;
	std::map<int, const Message *> index_id;
};

template <typename Buf>
struct DataHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, DataHandler<Buf>>
{
	Logger & _log;
	DataHandler(Logger &log, Buf& buf, const Message *msg) : _log(log)
	{
		state.view = make_view(buf);
		state.msg = msg;
	}

	std::string_view _name_field;
	std::string_view _seq_field;

	//using variant_t = std::variant<bool, size_t, Field *>;
	struct state_t {
		std::optional<memoryview<Buf>> view;
		std::optional<size_t> index;
		size_t index_max = 0;
		const Message * msg = nullptr;
		const Field * field = nullptr;

		memoryview<Buf> fview()
		{
			if (index)
				return view->view(field->size * *index);
			return view->view(field->offset);
		}
	};

	std::deque<state_t> stack;
	state_t state;

	bool check_overflow()
	{
		if (!state.index) return true;
		if (*state.index < state.index_max)
			return true;
		if (stack.size()) {
			auto parent = stack.back();
			if (parent.field && parent.field->type == scheme::Field::Pointer) {
				_log.debug("Resize offset ptr: +{}", state.index_max);
				scheme::optr_resize(parent.field, parent.fview(), state.index_max);
				state.index_max *= 2;
				return true;
			}
		}
		return _log.fail(false, "List {} overflow: {}", state.field->name, *state.index);
	}

	bool Default() { return _log.fail(false, "Default handler called"); }
	bool Null()
	{
		if (!state.field) return true;
		if (!check_overflow()) return false;
		if (state.index)
			++*state.index;
		return true;
	}

	bool Bool(bool b)
	{
		if (!state.field) return true;
		if (!check_overflow()) return false;
		auto r = decode(b?"true":"false");
		if (state.index)
			++*state.index;
		return r;
	}

	bool RawNumber(const char* string, SizeType length, bool copy)
	{
		return String(string, length, copy);
	}

	template <typename T>
	bool decode_scalar(memoryview<Buf> view, std::string_view str)
	{
		auto r = conv::to_any<T>(str);
		if (!r)
			return _log.fail(false, "Failed to decode {}: {}", str, r.error());
		*view.template dataT<T>() = *r;
		return true;
	}

	template <typename T, typename Res>
	bool decode_duration(memoryview<Buf>& view, std::string_view str)
	{
		return decode_scalar<std::chrono::duration<T, Res>>(view, str);
	}

	template <typename T, typename Res>
	bool decode_time(memoryview<Buf>& view, std::string_view str)
	{
		return decode_scalar<std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<T, Res>>>(view, str);
	}

	template <typename T>
	bool decode_number(std::string_view str)
	{
		auto f = state.field;
		auto fview = state.fview();
		if (f->sub_type == Field::SubNone) {
			return decode_scalar<T>(fview, str);
		} else if (f->sub_type == Field::Duration) {
			switch (f->time_resolution) {
			case TLL_SCHEME_TIME_NS: return decode_duration<T, std::nano>(fview, str); break;
			case TLL_SCHEME_TIME_US: return decode_duration<T, std::micro>(fview, str); break;
			case TLL_SCHEME_TIME_MS: return decode_duration<T, std::milli>(fview, str); break;
			case TLL_SCHEME_TIME_SECOND: return decode_duration<T, std::ratio<1, 1>>(fview, str); break;
			case TLL_SCHEME_TIME_MINUTE: return decode_duration<T, std::ratio<60, 1>>(fview, str); break;
			case TLL_SCHEME_TIME_HOUR: return decode_duration<T, std::ratio<3600, 1>>(fview, str); break;
			case TLL_SCHEME_TIME_DAY: return decode_duration<T, std::ratio<86400, 1>>(fview, str); break;
			}
			return decode_scalar<T>(fview, str);
		} else if (f->sub_type == Field::TimePoint) {
			switch (f->time_resolution) {
			case TLL_SCHEME_TIME_NS: return decode_time<T, std::nano>(fview, str); break;
			case TLL_SCHEME_TIME_US: return decode_time<T, std::micro>(fview, str); break;
			case TLL_SCHEME_TIME_MS: return decode_time<T, std::milli>(fview, str); break;
			case TLL_SCHEME_TIME_SECOND: return decode_time<T, std::ratio<1, 1>>(fview, str); break;
			case TLL_SCHEME_TIME_MINUTE: return decode_time<T, std::ratio<60, 1>>(fview, str); break;
			case TLL_SCHEME_TIME_HOUR: return decode_time<T, std::ratio<3600, 1>>(fview, str); break;
			case TLL_SCHEME_TIME_DAY: return decode_time<T, std::ratio<86400, 1>>(fview, str); break;
			}
			return decode_scalar<T>(fview, str);
		} else if (f->sub_type == Field::Enum) {
			for (auto e = f->type_enum->values; e; e = e->next) {
				if (e->name == str) {
					*fview.template dataT<T>() = (T) e->value;
					return true;
				}
			}
			return decode_scalar<T>(fview, str);
		} else if (!std::is_same_v<T, double> && f->sub_type == Field::Fixed) {
			if constexpr (!std::is_same_v<double, T>) {
				auto r = conv::to_any<conv::unpacked_float<T>>(str);
				if (!r)
					return _log.fail(false, "Failed to decode fixed numeric field {}: {}", str, r.error());
				if (std::is_unsigned_v<T> && r->sign)
					return _log.fail(false, "Failed to decode fixed numeric field {}: negative value for unsigned field", str);
				if (r->sign)
					r->mantissa = -r->mantissa;
				auto m = util::fixed_point::convert_mantissa(r->mantissa, r->exponent, -f->fixed_precision);
				if (!m)
					return _log.fail(false, "Failed to convert numeric value '{}' to exponent {}: {}", str, -f->fixed_precision, m.error());
				*fview.template dataT<T>() = (T) *m;
			}
		} else
			return decode_scalar<T>(fview, str);
		return true;
	}

	bool String(const char* string, SizeType length, bool copy)
	{
		if (!check_overflow()) return false;
		auto str = std::string_view(string, length);
		auto r = decode(str);
		if (state.index)
			++*state.index;
		return r;
	}
	bool decode(std::string_view str)
	{
		auto f = state.field;
		if (!f) return true;
		_log.debug("Decode field {}: {}", f->name, str);
		switch (f->type) {
		case Field::Int8: return decode_number<int8_t>(str);
		case Field::Int16: return decode_number<int16_t>(str);
		case Field::Int32: return decode_number<int32_t>(str);
		case Field::Int64: return decode_number<int64_t>(str);
		case Field::UInt8: return decode_number<uint8_t>(str);
		case Field::UInt16: return decode_number<uint16_t>(str);
		case Field::UInt32: return decode_number<uint32_t>(str);
		case Field::UInt64: return decode_number<uint64_t>(str);
		case Field::Double: return decode_number<double>(str);
		case Field::Bytes: {
			auto fview = state.fview();
			if (f->sub_type == Field::ByteString) {
				if (str.size() > f->size)
					return _log.fail(false, "String too long: {} > {}", str.size(), f->size);
				memcpy(fview.data(), str.data(), str.size());
				memset(fview.template dataT<char>() + str.size(), 0, f->size - str.size());
				return true;
			}
			return _log.fail(false, "Bytes not supported");
		}
		case Field::Array:
			return _log.fail(false, "Got scalar value for array field {}: {}", f->name, str);
		case Field::Pointer: {
			auto fview = state.fview();
			if (f->sub_type == Field::ByteString) {
				scheme::generic_offset_ptr_t ptr = {};
				ptr.size = str.size() + 1;
				ptr.entity = 1;
				if (scheme::alloc_pointer(f, fview, ptr))
					return _log.fail(false, "Failed to allocate pointer for {}", f->name);
				auto sview = fview.view(ptr.offset);
				sview.resize(ptr.size);
				memcpy(sview.data(), str.data(), str.size());
				*sview.view(str.size()).template dataT<char>() = '\0';
				return true;
			}
			return _log.fail(false, "Got scalar value for pointer field {}: {}", f->name, str);
		}
		case Field::Message: return _log.fail(false, "Got scalar value for Message field: {}", str);
		case Field::Decimal128:
			return decode_scalar<tll::util::Decimal128>(state.fview(), str);

		case Field::Union: return _log.fail(false, "Union not supported");
		}
		return true;
	}

	bool Key(const char* string, SizeType length, bool copy)
	{
		if (!state.msg)
			return true;
		auto str = std::string_view(string, length);
		if (stack.size() == 1) {
			if (_seq_field.size() && _seq_field == str) {
				state.field = nullptr;
				return true;
			} else if (_name_field.size() && _name_field == str) {
				state.field = nullptr;
				return true;
			}
		}
		auto meta = static_cast<const message_meta_t *>(state.msg->user);

		auto next = state.field?state.field->next:state.msg->fields;
		if (next && str == next->name) {
			state.field = next;
			return true;
		}

		auto k = meta->index.find(str);
		if (k != meta->index.end())
			state.field = k->second;
		else
			state.field = nullptr;

		return true;
	}

	bool StartObject()
	{
		if (!check_overflow()) return false;
		stack.push_back(state);
		if (!state.field) {
			if (stack.size() == 1)
				return true;
			state = {};
			return true;
		}

		if (state.field->type != Field::Message)
			return _log.fail(false, "Got Object for non-message field {}", state.field->name);

		state.view = state.fview();
		state.msg = state.field->type_msg;
		state.index = {};
		state.field = nullptr;
		return true;
	}

	bool EndObject(SizeType memberCount)
	{
		state = stack.back();
		stack.pop_back();
		if (!state.field) return true;
		if (state.index)
			++*state.index;
		return true;
	}

	bool StartArray()
	{
		if (!check_overflow()) return false;
		if (!stack.size())
			return _log.fail(false, "List messages not supported");
		stack.push_back(state);
		if (!state.field) {
			state = {};
			return true;
		}
		if (state.field->type == Field::Array) {
			_log.debug("Array {}, shift {}", state.field->name, state.field->type_array->offset);
			state.view = state.fview().view(state.field->type_array->offset);
			state.index_max = state.field->count;
			state.field = state.field->type_array;
		} else if (state.field->type == Field::Pointer) {
			auto meta = static_cast<const field_meta_t *>(state.field->user);
			auto fview = state.fview();
			_log.debug("Prealloc list {}: {} elements", state.field->name, meta->list_size);
			scheme::generic_offset_ptr_t ptr = {};
			ptr.size = meta->list_size;
			ptr.entity = state.field->type_ptr->size;
			if (scheme::alloc_pointer(state.field, fview, ptr))
				return _log.fail(false, "Failed to preallocate pointer for {}: size {}", state.field->name, ptr.size);
			state.view = fview.view(ptr.offset);
			state.index_max = meta->list_size;
			state.field = state.field->type_ptr;
		} else if (state.field->type == Field::Message)
			return _log.fail(false, "Message arrays not supported");
		else
			return _log.fail(false, "Got array for scalar field {}", state.field->name);

		state.index = 0;
		return true;
	}

	bool EndArray(SizeType elementCount)
	{
		auto size = state.index.value_or(0); //XXX: GCC 7.3 bug, can not save std::optional
		state = stack.back();
		stack.pop_back();
		if (!state.field) return true;
		auto f = state.field;
		auto fview = state.fview();
		_log.debug("Write array size for {}: {}", f->name, size);
		if (f->type == Field::Array) {
			auto cf = state.field->count_ptr;
			if (scheme::write_size(cf, fview, size))
				return _log.fail(false, "Invalid count field type for {}: {}", cf->name, cf->type);
		} else if (f->type == Field::Pointer) {
			scheme::write_pointer_size(f, fview, size);
		} else if (f->type == Field::Message) {
		}
		if (state.index)
			++*state.index;
		return true;
	}
};

struct LookupHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, LookupHandler>
{
	Logger & _log;
	LookupHandler(Logger &log, const Scheme * s) : _log(log), scheme(s) {}
	enum { None, Name, Seq, Other } _key = None;
	std::string_view _name_field;
	std::string_view _seq_field;
	const Scheme * scheme = nullptr;
	const Message * msg = nullptr;
	std::optional<long long> seq;
	size_t depth = 0;

	bool Default() { return _log.fail(false, "Default handler called"); }

	bool Null()
	{
		if (depth != 1) return true;
		if (_key == Seq) {
			seq = 0;
			if (msg || !_name_field.size()) return false;
		} else if (_key == Name)
			return _log.fail(false, "Invalid name '{}': null value", _name_field);
		return true;
	}

	bool Bool(bool b) {
		if (depth != 1) return true;
		if (_key == Seq) {
			return _log.fail(false, "Invalid seq '{}': bool value", _seq_field);
		} else if (_key == Name)
			return _log.fail(false, "Invalid name '{}': bool value", _name_field);
		return true;
	}

	bool RawNumber(const char* string, SizeType length, bool copy)
	{
		return String(string, length, copy);
	}

	bool String(const char* string, SizeType length, bool copy)
	{
		if (depth != 1) return true;
		const auto str = std::string_view(string, length);
		if (_key == Seq) {
			auto r = conv::to_any<long long>(str);
			if (!r)
				return _log.fail(false, "Invalid seq '{}': {}", str, r.error());
			seq = *r;
			if (msg || !_name_field.size()) return false;
		} else if (_key == Name) {
			auto meta = static_cast<const scheme_meta_t *>(scheme->user);
			auto it = meta->index.find(str);
			if (it == meta->index.end())
				return _log.fail(false, "Invalid name '{}': '{}' not found", _name_field, str);
			msg = it->second;
			if (seq || !_seq_field.size()) return false;
		}
		return true;
	}

	bool Key(const char* string, SizeType length, bool copy)
	{
		if (depth != 1) return true;
		const auto str = std::string_view(string, length);
		if (_seq_field.size() && str == _seq_field)
			_key = Seq;
		else if (_name_field.size() && str == _name_field)
			_key = Name;
		else
			_key = Other;
		return true;
	}

	bool StartObject()
	{
		if (depth++ != 1)
			return true;
		if (_key == Seq) {
			return _log.fail(false, "Invalid seq '{}': got object", _seq_field);
		} else if (_key == Name)
			return _log.fail(false, "Invalid name '{}': got object", _name_field);
		return true;
	}

	bool EndObject(SizeType memberCount) { depth--; return true; }

	bool StartArray()
	{
		if (depth == 0)
			return _log.fail(false, "Top level list not supported");
		if (depth++ != 1)
			return true;
		if (_key == Seq) {
			return _log.fail(false, "Invalid seq '{}': got array", _seq_field);
		} else if (_key == Name)
			return _log.fail(false, "Invalid name '{}': got array", _name_field);
		return true;
	}

	bool EndArray(SizeType elementCount) { depth--; return true; }
};

class JSON
{
	Logger &_log;
	util::buffer _buf;
	scheme::SchemePtr _scheme;
	rapidjson::MemoryBuffer _buffer_out;
	util::buffer _buffer_in;

	std::string _name_field;
	std::string _seq_field;
	std::optional<std::string> _default_name;
	const Message * _default_message = nullptr;

	template <typename T>
	static void meta_free(void * ptr) { delete static_cast<T *>(ptr); }

 public:
	JSON(Logger &log) : _log(log) {}

	const scheme::Message * lookup(int msgid) const
	{
		auto meta = static_cast<const scheme_meta_t *>(_scheme->user);
		auto it = meta->index_id.find(msgid);
		if (it == meta->index_id.end()) return nullptr;
		return it->second;
	}

	const scheme::Message * lookup(std::string_view name) const
	{
		auto meta = static_cast<const scheme_meta_t *>(_scheme->user);
		auto it = meta->index.find(name);
		if (it == meta->index.end()) return nullptr;
		return it->second;
	}

	template <typename T>
	int init(PropsReaderT<T> &props)
	{
		_name_field = props.template getT<std::string>("name-field", "_tll_name");
		_seq_field = props.template getT<std::string>("seq-field", "_tll_seq");
		_default_name = props.get("default-message");
		if (!props)
			return _log.fail(EINVAL, "Failed to init JSON parameters: {}", props.error());
		return 0;
	}

	int init_field(Field *f)
	{
		if (!f) return _log.fail(EINVAL, "Null poiner");

		auto fmeta = new field_meta_t;
		f->user = fmeta;
		f->user_free = &meta_free<field_meta_t>;

		auto oprops = scheme::options_map(f->options);
		auto reader = make_props_reader(oprops);

		if (f->type == Field::Message) {
			fmeta->message_inline = reader.getT("json.inline", false);
		} else if (f->type == Field::Array) {
			init_field(f->count_ptr);
			if (init_field(f->type_array))
				return _log.fail(EINVAL, "Failed to init sub field");
			if (static_cast<field_meta_t *>(f->type_array->user)->skip)
				fmeta->skip = true;
		} else if (f->type == Field::Pointer) {
			fmeta->list_size = reader.getT("json.expected-list-size", 64u);
			_log.debug("Expected list size {}: {}", f->name, fmeta->list_size);
			if (init_field(f->type_ptr))
				return _log.fail(EINVAL, "Failed to init sub field");
			if (static_cast<field_meta_t *>(f->type_ptr->user)->skip)
				fmeta->skip = true;
		} else if (f->sub_type == Field::Enum)
			fmeta->enum_number = reader.getT("json.enum-as-int", false);
		fmeta->skip = reader.getT("json.skip", false);
		if (!reader)
			return _log.fail(EINVAL, "Invalid JSON options: {}", reader.error());

		if (oprops.has("_auto"))
			fmeta->skip = true;

		if (f->sub_type == Field::Enum) {
			for (auto & e : util::list_wrap(f->type_enum->values))
				fmeta->enum_values.emplace(e.value, e.name);
		}
		return 0;
	}

	int init_scheme(const Scheme *s)
	{
		if (!s) return _log.fail(EINVAL, "Null scheme");
		_default_message = nullptr;
		scheme::SchemePtr scheme {s->copy()};
		if (!scheme) return _log.fail(EINVAL, "Failed to copy scheme");
		auto meta = new scheme_meta_t;
		scheme->user = meta;
		scheme->user_free = &meta_free<scheme_meta_t>;
		for (auto & m : util::list_wrap(scheme->messages)) {
			meta->index.emplace(m.name, &m);
			if (m.msgid)
				meta->index_id.emplace(m.msgid, &m);
			auto mmeta = new message_meta_t;
			m.user = mmeta;
			m.user_free = &meta_free<message_meta_t>;

			auto mprops = scheme::options_map(m.options);

			auto as_list = mprops.getT("json.message-as-list", false);
			if (!as_list)
				return _log.fail(EINVAL, "Invalid json.message-as-list option for {}: {}", m.name, as_list.error());
			mmeta->as_list = *as_list;
			if (mmeta->as_list)
				_log.debug("Encode message {} as list", m.name);

			for (auto & f : util::list_wrap(m.fields)) {
				if (init_field(&f))
					return _log.fail(EINVAL, "Failed to init field {}.{}", m.name, f.name);
				mmeta->index.emplace(f.name, &f);
				auto ptr = &f;
				for (; ptr->type == Field::Array; ptr = ptr->type_array) {}
				if (ptr->type == Field::Message) {
					auto m = static_cast<message_meta_t *>(ptr->type_msg->user);
					if (m && m->pointers.size())
						mmeta->pointers.push_back(&f);
				} else if (ptr->type == Field::Pointer)
					mmeta->pointers.push_back(&f);
			}
		}
		if (_default_name) {
			_default_message = scheme->lookup(*_default_name);
			if (!_default_message)
				return _log.fail(EINVAL, "Default message '{}' not found in scheme", *_default_name);
		}
		std::swap(_scheme, scheme);
		return 0;
	}

	template <typename W, typename Buf>
	int encode_field(W &writer, const memoryview<Buf> &data, const Field *field);

	template <typename W, typename Buf>
	int encode_list(W &writer, const memoryview<Buf> &data, const Field *field, size_t size, size_t entity);

	template <typename W, typename Buf>
	int encode_message(W &writer, const memoryview<Buf> &data, const Message *message, bool borders = true);

	template <typename W, typename T>
	int encode_int(W &writer, const T& v, const Field *field)
	{
		auto meta = static_cast<const field_meta_t *>(field->user);
		if (field->sub_type == Field::SubNone)
			return encode_number(writer, v);
		else if (field->sub_type == Field::Duration)
			return encode_duration(writer, v, field->time_resolution);
		else if (field->sub_type == Field::TimePoint) {
			switch (field->time_resolution) {
			case TLL_SCHEME_TIME_NS: return encode_time<W, T, std::nano>(writer, v); break;
			case TLL_SCHEME_TIME_US: return encode_time<W, T, std::micro>(writer, v); break;
			case TLL_SCHEME_TIME_MS: return encode_time<W, T, std::milli>(writer, v); break;
			case TLL_SCHEME_TIME_SECOND: return encode_time<W, T, std::ratio<1, 1>>(writer, v); break;
			case TLL_SCHEME_TIME_MINUTE: return encode_time<W, T, std::ratio<60, 1>>(writer, v); break;
			case TLL_SCHEME_TIME_HOUR: return encode_time<W, T, std::ratio<3600, 1>>(writer, v); break;
			case TLL_SCHEME_TIME_DAY: return encode_time<W, T, std::ratio<86400, 1>>(writer, v); break;
			}
			return encode_number(writer, v);
		} else if (field->sub_type == Field::Enum && !meta->enum_number) {
			auto it = meta->enum_values.find(v);
			if (it == meta->enum_values.end())
				return encode_number(writer, v);
			writer.String(it->second.data(), it->second.size());
			return 0;
		} else if (field->sub_type == Field::Fixed) {
			if constexpr (!std::is_same_v<T, double>) {
				tll::conv::unpacked_float<T> uf { v, -static_cast<int>(field->fixed_precision) };

				auto r = uf.to_string_buf(_buf, uf.ZeroAfterDot | uf.ZeroBeforeDot | uf.LowerCaseE);
				writer.RawValue(r.data(), r.size(), rapidjson::kNumberType);
				return 0;
			}
		}
		return encode_number(writer, v);
	}

	template <typename W, typename T>
	int encode_number(W &writer, const T& v)
	{
		auto r = conv::to_string_buf<T>(v, _buf);
		writer.RawValue(r.data(), r.size(), rapidjson::kNumberType);
		return 0;
	}

	template <typename W, typename T>
	int encode_duration(W &writer, const T& v, const tll_scheme_time_resolution_t &res)
	{
		auto r = conv::to_string_buf(v, _buf);
		r = tll::conv::append(_buf, r, scheme::time_resolution_str(res));
		writer.String(r.data(), r.size());
		return 0;
	}

	template <typename W, typename T, typename Res>
	int encode_time(W &writer, const T& v)
	{
		using duration = std::chrono::duration<T, Res>;
		using time_point = std::chrono::time_point<std::chrono::system_clock, duration>;
		auto r = conv::to_string_buf(time_point(duration(v)), _buf);
		writer.String(r.data(), r.size());
		return 0;
	}

	std::optional<tll::const_memory> encode(const tll_msg_t *msg, tll_msg_t *out);
	std::optional<tll::const_memory> encode(const Message *scheme, const tll_msg_t *msg, tll_msg_t *out);
	std::optional<tll::const_memory> decode(const tll_msg_t *msg, tll_msg_t *out);

	template <typename Buf>
	std::optional<tll::const_memory> decode(const Message *scheme, const Buf &msg, tll_msg_t *out);
};

template <typename W, typename Buf>
inline int JSON::encode_field(W &writer, const memoryview<Buf> &data, const Field *field)
{
	auto meta = static_cast<const field_meta_t *>(field->user);
	if (!meta) return _log.fail(EINVAL, "No user data on field {}", field->name);
	if (meta->skip)
		return 0;

	using namespace rapidjson;
	switch (field->type) {
	case Field::Int8: return encode_int(writer, *data.template dataT<int8_t>(), field);
	case Field::Int16: return encode_int(writer, *data.template dataT<int16_t>(), field);
	case Field::Int32: return encode_int(writer, *data.template dataT<int32_t>(), field);
	case Field::Int64: return encode_int(writer, *data.template dataT<int64_t>(), field);
	case Field::UInt8: return encode_int(writer, *data.template dataT<uint8_t>(), field);
	case Field::UInt16: return encode_int(writer, *data.template dataT<uint16_t>(), field);
	case Field::UInt32: return encode_int(writer, *data.template dataT<uint32_t>(), field);
	case Field::UInt64: return encode_int(writer, *data.template dataT<uint64_t>(), field);
	case Field::Double: return encode_int(writer, *data.template dataT<double>(), field);
	case Field::Bytes: {
		if (field->sub_type == Field::ByteString) {
			writer.String(data.template dataT<char>(), strnlen(data.template dataT<char>(), field->size));
			return 0;
		}
		return _log.fail(EINVAL, "Bytes not implemented");
	}
	case Field::Decimal128: {
		tll::util::Decimal128::Unpacked u;
		data.template dataT<tll::util::Decimal128>()->unpack(u);
		tll::conv::unpacked_float<decltype(u.mantissa.value)> uf { u.sign != 0, u.mantissa.value, u.exponent };

		auto r = uf.to_string_buf(_buf, uf.ZeroAfterDot | uf.ZeroBeforeDot | uf.LowerCaseE);
		writer.RawValue(r.data(), r.size(), rapidjson::kNumberType);
		return 0;
	}
	case Field::Message:
		if (encode_message(writer, data, field->type_msg, !meta->message_inline))
			return _log.fail(EINVAL, "Failed to encode sub-message {}", field->type_msg->name);
		return 0;
	case Field::Array: {
		auto size = tll::scheme::read_size(field->count_ptr, data);
		if (size < 0)
			return _log.fail(EINVAL, "Negative count for field {}: {}", field->name, size);
		auto af = field->type_array;
		return encode_list(writer, data.view(af->offset), af, size, af->size);
	}
	case Field::Pointer: {
		auto ptr = scheme::read_pointer(field, data);
		if (!ptr)
			return _log.fail(EINVAL, "Invalid offset ptr version: {}", field->offset_ptr_version);
		if (data.size() < ptr->offset)
			return _log.fail(EINVAL, "Offset pointer {} out of bounds: +{} < {}", field->name, ptr->offset, data.size());
		if (field->sub_type == Field::ByteString) {
			if (ptr->size != 0) {
				writer.String(data.view(ptr->offset).template dataT<const char>(), ptr->size - 1);
			} else
				writer.String("", 0);
			return 0;
		}
		return encode_list(writer, data.view(ptr->offset), field->type_ptr, ptr->size, ptr->entity ? ptr->entity : field->type_ptr->size);
	}
	case Field::Union:
		return _log.fail(EINVAL, "Unions not supported");
	}
	return 0;
}

template <typename W, typename Buf>
inline int JSON::encode_list(W &writer, const memoryview<Buf> &data, const Field *field, size_t size, size_t entity)
{
	_log.trace("Encode list {} with {} values", field->name, size);
	writer.StartArray();
	for (auto i = 0u; i < size; i++) {
		if (encode_field(writer, data.view(i * entity), field))
			return _log.fail(EINVAL, "Faield to encode element {}[{}]", field->name, size);
	}
	writer.EndArray();
	return 0;
}

template <typename W, typename Buf>
inline int JSON::encode_message(W &writer, const memoryview<Buf> &data, const Message *msg, bool borders)
{
	_log.trace("Encode message {}", msg->name);
	if (data.size() < msg->size)
		return _log.fail(EMSGSIZE, "Data size less then message {} size: {} < {}", msg->name, data.size(), msg->size);
	auto meta = static_cast<const message_meta_t *>(msg->user);
	if (!meta) return _log.fail(EINVAL, "No user data on message {}", msg->name);
	if (borders) {
		if (meta->as_list)
			writer.StartArray();
		else
			writer.StartObject();
	}
	for (auto & f : util::list_wrap(msg->fields)) {
		auto fmeta = static_cast<const field_meta_t *>(f.user);
		if (!fmeta || fmeta->skip) continue;
		if (!meta->as_list)
			writer.Key(f.name);
		_log.trace("Encode field {}", f.name);
		if (encode_field(writer, data.view(f.offset), &f))
			return _log.fail(EINVAL, "Failed to encode field {}", f.name);
	}
	if (borders) {
		if (meta->as_list)
			writer.EndArray();
		else
			writer.EndObject();
	}
	return 0;
}

inline std::optional<const_memory> JSON::encode(const tll_msg_t *msg, tll_msg_t *out)
{
	if (!_scheme)
		return _log.fail(std::nullopt, "Scheme not initialized");

	auto message = lookup(msg->msgid);
	if (!message)
		return _log.fail(std::nullopt, "Message {} ({}) not found", msg->msgid, "null"); //msg->name?msg->name:"null");
	return encode(message, msg, out);
}

inline std::optional<const_memory> JSON::encode(const Message * message, const tll_msg_t *msg, tll_msg_t *out)
{
	auto meta = static_cast<const message_meta_t *>(message->user);
	if (!meta) return _log.fail(std::nullopt, "No user data on message {}", message->name);

	_buffer_out.Clear();
	rapidjson::Writer<rapidjson::MemoryBuffer> writer(_buffer_out);
	if (meta->as_list)
		writer.StartArray();
	else
		writer.StartObject();
	if (_name_field.size()) {
		writer.Key(_name_field.data(), _name_field.size());
		writer.String(message->name); //, strlen(message->name), rapidjson::kStringType);
	}
	if (_seq_field.size()) {
		auto r = conv::to_string_buf(msg->seq, _buf);
		writer.Key(_seq_field.data(), _seq_field.size());
		writer.RawValue(r.data(), r.size(), rapidjson::kNumberType);
	}
	if (encode_message(writer, make_view(*msg), message, false))
		return _log.fail(std::nullopt, "Failed to encode message {}", message->name);
	if (meta->as_list)
		writer.EndArray();
	else
		writer.EndObject();
	auto s = std::string_view(_buffer_out.GetBuffer(), _buffer_out.GetSize());
	_log.trace("Encoded json ({}): {}", s.size(), s);
	return const_memory { _buffer_out.GetBuffer(), _buffer_out.GetSize() };
}

inline std::optional<const_memory> JSON::decode(const tll_msg_t *msg, tll_msg_t *out)
{
	using namespace rapidjson;
	if (!_scheme)
		return _log.fail(std::nullopt, "Scheme not initialized");

	const Message * message = nullptr;

	MemoryStream stream((const char *) msg->data, msg->size);
	Reader reader;
	LookupHandler handler(_log, _scheme.get());
	handler._name_field = _name_field;
	handler._seq_field = _seq_field;
	if (!reader.Parse<kParseNumbersAsStringsFlag>(stream, handler)) {
		auto e = reader.GetParseErrorCode();
		if (e != kParseErrorTermination) {
			auto off = reader.GetErrorOffset();
			return _log.fail(std::nullopt, "Failed to parse json at {}: {}", off, GetParseError_En(e));
			//string(json).substr(o, 10) << "...'" << endl;
		}
	}
	if (!handler.msg) {
		if (!_default_message)
			return _log.fail(std::nullopt, "Failed to lookup message name");
		else
			_log.debug("Use default decode message '{}'", _default_message->name);
		handler.msg = _default_message;
	}
	message = handler.msg;
	out->seq = handler.seq.value_or(0);

	_log.debug("Lookup message {}, seq {}", message->name, out->seq);
	return decode(message, *msg, out);
}

template <typename Buf>
inline std::optional<const_memory> JSON::decode(const Message * message, const Buf &msg, tll_msg_t *out)
{
	using namespace rapidjson;
	out->msgid = message->msgid;

	MemoryStream stream((const char *) tll::memoryview_api<Buf>::data(msg), tll::memoryview_api<Buf>::size(msg));
	Reader reader;
	_buffer_in.resize(message->size);
	memset(_buffer_in.data(), 0, _buffer_in.size());
	DataHandler handler(_log, _buffer_in, message);

	if (!reader.Parse<kParseNumbersAsStringsFlag>(stream, handler)) {
		auto off = reader.GetErrorOffset();
		auto e = reader.GetParseErrorCode();
		return _log.fail(std::nullopt, "Failed to parse json at {}: {}", off, GetParseError_En(e));
		//string(json).substr(o, 10) << "...'" << endl;
	}

	/*
	reader.IterativeParseInit();
	while (!reader.IterativeParseComplete()) {
		reader.IterativeParseNext<kParseDefaultFlags>(is, lhandler);
		// Your handler has been called once.
	}
	*/

	//_log.trace("Encoded json ({}): {}", s.size(), s);
	return const_memory { _buffer_in.data(), _buffer_in.size() };
}

} // namespace tll::json

#endif//_TLL_UTIL_JSON_H
