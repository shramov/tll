/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/yaml.h"
#include "channel/emulate-control.hpp"

#include "tll/util/memoryview.h"

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
	_encoder.settings.strict = reader.getT("strict", true);

	if (auto r = _init_emulate_control(reader); r)
		return r;

	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

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
	if (_encoder.encode(tll::make_view(_buf), m, *data))
		return _log.fail(EINVAL, "Failed to encode message {} at {}: {}", m->name, _encoder.format_stack(), _encoder.error);
	msg->msgid = m->msgid;
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
		_update_dcaps(0, dcaps::Process | dcaps::Pending);
		return EAGAIN;
	}

	tll_msg_t msg = { TLL_MESSAGE_DATA };
	auto cfg = _messages[_idx];

	auto reader = tll::make_props_reader(cfg);
	msg.seq = reader.getT<long long>("seq", 0);
	msg.addr.i64 = reader.getT<int64_t>("addr", 0);
	msg.type = reader.getT("type", TLL_MESSAGE_DATA, {{"data", TLL_MESSAGE_DATA}, {"control", TLL_MESSAGE_CONTROL}});
	auto time = reader.getT<std::string>("time", "");
	if (time.empty()) {
	} else if (time == "now") {
		_last_ts = tll::time::now();
	} else if (time[0] == '+') {
		auto r = tll::conv::to_any<tll::duration>(time.substr(1));
		if (!r)
			return _log.fail(EINVAL, "Message {}: invalid time delta '{}': {}", _idx, time.substr(1), r.error());
		_last_ts += *r;
	} else {
		auto r = tll::conv::to_any<tll::time_point>(time);
		if (!r)
			return _log.fail(EINVAL, "Message {}: invalid time point '{}': {}", _idx, time, r.error());
		_last_ts = *r;
	}

	msg.time = _last_ts.time_since_epoch().count();

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
			return _log.fail(EINVAL, "No 'data' field for message without scheme {}", _idx);
		msg.size = data->size();
		msg.data = data->data();
	} else {
		if (_fill(scheme, &msg, cfg))
			return _log.fail(EINVAL, "Failed to fill message {}", _idx);
	}

	if (!reader)
		return _log.fail(EINVAL, "Invalid parameters in message {}: {}", _idx, reader.error());

	_idx++;

	_callback(&msg);
	return 0;
}
