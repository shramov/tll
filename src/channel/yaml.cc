/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/yaml.h"

#include "tll/conv/decimal128.h"
#include "tll/util/memoryview.h"
#include "tll/util/time.h"
#include "tll/scheme/util.h"

using namespace tll;

TLL_DEFINE_IMPL(ChYaml);

template <typename T>
const T * lookup(const T * data, std::string_view name)
{
	for (auto i = data; i; i = i->next) {
		if (i->name == name)
			return i;
	}
	return nullptr;
}

int ChYaml::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	_filename = url.host();
	auto cfg = url.sub("config");
	if (cfg)
		_url_config = *cfg;
	else if (!_filename.size())
		return _log.fail(EINVAL, "Need either filename in host or 'config' subtree");

	auto reader = channel_props_reader(url);
	_autoclose = reader.getT("autoclose", true);
	_autoseq = reader.getT("autoseq", false);
	_strict = reader.getT("strict", true);
	auto control = reader.get("scheme-control");

	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (control) {
		_log.debug("Loading control scheme from {}...", control->substr(0, 64));
		_scheme_control.reset(context().scheme_load(*control));
		if (!_scheme_control)
			return _log.fail(EINVAL, "Failed to load control scheme from {}...", control->substr(0, 64));
	}

	if (!_scheme_url)
		_log.info("Working with raw data without scheme");
	return 0;
}

int ChYaml::_open(const ConstConfig &url)
{
	_seq = -1;
	if (!_url_config) {
		auto cfg = tll::Config::load(std::string("yaml://") + _filename);
		if (!cfg)
			return _log.fail(EINVAL, "Failed to load config from '{}'", _filename);
		_config = std::move(*cfg);
	} else
		_config = *_url_config;

	for (auto & [k,v] : _config.browse("*", true))
		_messages.push_back(v);
	_log.debug("{} messages in file {}", _messages.size(), _filename);

	_dcaps_pending(true);
	return 0;
}

template <typename T>
int ChYaml::_fill_numeric(T * ptr, const tll::scheme::Field * field, std::string_view s)
{
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
		return _log.fail(EINVAL, "String '{}' does not match any enum {} values", s, field->type_enum->name);
	} else if (field->sub_type == field->Fixed) {
		if constexpr (!std::is_floating_point_v<T>) {
			int prec = -(int) field->fixed_precision;

			auto u = conv::to_any<tll::conv::unpacked_float<T>>(s);
			if (!u)
				return _log.fail(EINVAL, "Invalid number '{}': {}", s, u.error());
			auto m = u->mantissa;
			if (u->sign) {
				if constexpr (std::is_unsigned_v<T>)
					return _log.fail(EINVAL, "Invalid number '{}': negative value", s);
				m = -m;
			}
			auto r = tll::util::FixedPoint<T, 0>::normalize_mantissa(m, u->exponent, prec);
			if (!std::holds_alternative<T>(r))
				return _log.fail(EINVAL, "Invalid number '{}': {}", s, std::get<1>(r));
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
		return _log.fail(EINVAL, "Unknown resolution: {}", field->time_resolution);
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
		return _log.fail(EINVAL, "Unknown resolution: {}", field->time_resolution);
	}
	return _fill_conv<T>(ptr, s);
}

template <typename T>
int ChYaml::_fill_conv(void * ptr, std::string_view s)
{
	auto v = tll::conv::to_any<T>(s);
	if (v) {
		*static_cast<T *>(ptr) = *v;
		return 0;
	}
	return _log.fail(EINVAL, "Invalid string '{}': {}", s, v.error());
}

template <typename View>
int ChYaml::_fill(View view, const tll::scheme::Field * field, tll::ConstConfig &cfg)
{
	using tll::scheme::Field;
	if (field->type == Field::Message) {
		return _fill(view, field->type_msg, cfg);
	} else if (field->type == Field::Array) {
		auto l = cfg.browse("*", true);
		if (l.size() > field->count)
			return _log.fail(EINVAL, "List size {} larger then {}", l.size(), field->count);
		_log.trace("Write size: {}", l.size());
		tll::scheme::write_size(field->count_ptr, view.view(field->count_ptr->offset), l.size());
		auto data = view.view(field->type_array->offset);
		unsigned i = 0;
		for (auto & [_, c] : l) {
			if (_fill(data.view(i++ * field->type_array->size), field->type_array, c))
				return _log.fail(EINVAL, "Failed to fill offset list element {}", i - 1);
		}
		return 0;
	} else if (field->type == Field::Pointer) {
		if (field->sub_type == field->ByteString) {
			auto v = cfg.get();
			if (!v)
				return 0;

			tll::scheme::generic_offset_ptr_t ptr;
			ptr.offset = view.size();
			ptr.size = v->size() + 1;
			ptr.entity = 1;

			tll::scheme::write_pointer(field, view, ptr);
			auto data = view.view(view.size());
			data.resize(v->size() + 1);
			memmove(data.data(), v->data(), v->size());
			*data.view(v->size()).template dataT<char>() = 0;
			return 0;
		}

		auto l = cfg.browse("*", true);

		tll::scheme::generic_offset_ptr_t ptr;
		ptr.offset = view.size();
		ptr.size = l.size();
		ptr.entity = field->type_ptr->size;

		_log.trace("Write ptr: off {}, size {}, entity {}", ptr.offset, ptr.size, ptr.entity);

		tll::scheme::write_pointer(field, view, ptr);
		auto data = view.view(view.size());
		data.resize(ptr.entity * ptr.size);
		unsigned i = 0;
		for (auto & [_, c] : l) {
			if (_fill(data.view(i++ * field->type_ptr->size), field->type_ptr, c))
				return _log.fail(EINVAL, "Failed to fill offset list element {}", i - 1);
		}
		return 0;
	} else if (field->type == Field::Union) {
		auto l = cfg.browse("*", true);
		if (l.size() == 0)
			return 0;
		if (l.size() > 1)
			return _log.fail(EINVAL, "Failed to fill union: too many keys");
		auto key = l.begin()->first;
		for (auto i = 0u; i < field->type_union->fields_size; i++) {
			auto uf = field->type_union->fields + i;
			_log.trace("Check union type {}", uf->name);
			if (key == uf->name) {
				_log.trace("Write union type {}: {}", key, i);
				tll::scheme::write_size(field->type_union->type_ptr, view, i);
				if (_fill(view.view(uf->offset), uf, l.begin()->second))
					return _log.fail(EINVAL, "Failed to fill union body type {}", key);
				return 0;
			}
		}
		return _log.fail(EINVAL, "Unknown union type: {}", key);
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
			return _log.fail(EINVAL, "Invalid decimal128 string '{}': {}", *v, r.error());
		*view.template dataT<tll::util::Decimal128>() = *r;
		return 0;
	}

	case Field::Bytes: {
		if (v->size() > field->size)
			return _log.fail(EINVAL, "Value '{}' is longer then field size {}", *v, field->name, field->size);
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

template <typename View>
int ChYaml::_fill(View view, const tll::scheme::Message * msg, tll::ConstConfig &cfg)
{
	View pmap = msg->pmap ? view.view(msg->pmap->offset) : view;
	_log.trace("Fill message {}", msg->name);
	for (auto & [p, c] : cfg.browse("*", true)) {
		auto f = lookup(msg->fields, p);
		if (!f) {
			if (!_strict)
				continue;
			return _log.fail(EINVAL, "Field {} not found in message {}", p, msg->name);
		}
		_log.trace("Fill field {}", f->name);
		if (msg->pmap && f->index >= 0)
			tll::scheme::pmap_set(pmap.data(), f->index);
		if (_fill(view.view(f->offset), f, c))
			return _log.fail(EINVAL, "Failed to fill field {}", f->name);
	}
	return 0;
}

int ChYaml::_fill(const tll::Scheme * scheme, tll_msg_t * msg, tll::ConstConfig &cfg)
{
	auto data = cfg.sub("data");
	if (!data)
		return _log.fail(EINVAL, "No 'data' field for message {}", _idx);

	auto name = cfg.get("name");
	if (!name)
		return _log.fail(EINVAL, "No 'name' field for message {}", _idx);
	auto m = scheme->lookup(*name);
	if (!m)
		return _log.fail(EINVAL, "Message '{}' not found in scheme for {}", *name, _idx);
	_buf.clear();
	_buf.resize(m->size);
	auto view = tll::make_view(_buf);
	msg->msgid = m->msgid;
	auto r = _fill(view, m, *data);
	if (r)
		return r;
	msg->data = _buf.data();
	msg->size = _buf.size();
	return 0;
}

int ChYaml::_process(long timeout, int flags)
{
	if (_idx == _messages.size()) {
		if (_autoclose) {
			_log.info("All messages processed. Closing");
			close();
			return 0;
		}
		return EAGAIN;
	}

	tll_msg_t msg = { TLL_MESSAGE_DATA };
	auto cfg = _messages[_idx];

	auto reader = tll::make_props_reader(cfg);
	msg.seq = reader.getT<long long>("seq", 0);
	msg.addr.i64 = reader.getT<int64_t>("addr", 0);
	msg.type = reader.getT("type", TLL_MESSAGE_DATA, {{"data", TLL_MESSAGE_DATA}, {"control", TLL_MESSAGE_CONTROL}});

	if (_autoseq) {
		if (_seq == -1)
			_seq = msg.seq;
		else
			msg.seq = ++_seq;
	}

	auto data = cfg.get("data");

	auto scheme = _scheme.get();
	if (msg.type == TLL_MESSAGE_CONTROL)
		scheme = _scheme_control.get();

	if (!scheme) {
		msg.msgid = reader.getT<int>("msgid", 0);
		if (!data)
			return _log.fail(EINVAL, "No 'data' field for message {}", _idx);
		msg.size = data->size();
		msg.data = data->data();
	} else {
		if (_fill(scheme, &msg, cfg))
			return _log.fail(EINVAL, "Failed to fill message {}", _idx);
	}

	if (!reader)
		return _log.fail(EINVAL, "Invalid parameters in message {}: {}", _idx, reader.error());

	_idx++;

	_callback_data(&msg);
	return 0;
}
