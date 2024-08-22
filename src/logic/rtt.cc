/*
 * Copyright (c) 2022 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <tll/channel/tagged.h>
#include <tll/channel/module.h>
#include <tll/util/size.h>

#include <tll/scheme/logic/quantile.h>

using namespace tll::channel;

struct Timer : public Tag<TLL_MESSAGE_MASK_DATA> { static constexpr std::string_view name() { return "timer"; } };

class RTT : public Tagged<RTT, Timer, Input, Output>
{
	tll::Channel * _output = nullptr;

	tll_msg_t _msg = {};
	std::vector<unsigned char> _data;

	tll::time_point _last_timer = {};
	bool _chained = false;

	tll_msg_t _msg_time = {};
	struct TimeData {
		char name[8];
		uint64_t value;
	} _time_data = {};
 public:
	using Base = Tagged<RTT, Timer, Input, Output>;
	static constexpr std::string_view channel_protocol() { return "rtt"; }
	static constexpr auto scheme_policy() { return SchemePolicy::Manual; }

	struct StatType : public Base::StatType
	{
		tll::stat::IntegerGroup<tll::stat::Ns, 'r', 't', 't'> rtt;
	};

	tll::stat::BlockT<StatType> * stat() { return static_cast<tll::stat::BlockT<StatType> *>(this->internal.stat); }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &cfg)
	{
		_msg.seq = -1;
		_last_timer = tll::time::now();
		return Base::_open(cfg);
	}

	int callback_tag(TaggedChannel<Input> * c, const tll_msg_t *msg);
	int callback_tag(TaggedChannel<Output> * c, const tll_msg_t *msg) { return 0; }
	int callback_tag(TaggedChannel<Timer> * c, const tll_msg_t *msg);

	int _send();
};

int RTT::_init(const tll::Channel::Url &url, tll::Channel *)
{
	if (_channels.get<Timer>().size()) {
		if (check_channels_size<Output>(1, 1))
			return EINVAL;
		_output = _channels.get<Output>().front().first;
	}

	auto reader = channel_props_reader(url);
	auto size = reader.getT("payload", tll::util::Size { 128 });
	_chained = reader.getT("chained", false);

	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (_chained && !_output)
		return _log.fail(EINVAL, "Chained mode is available only with timer/output channels");

	_scheme.reset(context().scheme_load(quantile_scheme::scheme_string));
	if (!_scheme.get())
		return _log.fail(EINVAL, "Failed to load quantile scheme");

	_data.resize(sizeof(tll::time_point) + size);
	_msg.data = _data.data();
	_msg.size = _data.size();

	_msg_time.msgid = quantile_scheme::Data::meta_id();
	_msg_time.data = &_time_data;
	_msg_time.size = sizeof(_time_data);

	return 0;
}

int RTT::callback_tag(TaggedChannel<Input> * c, const tll_msg_t *msg)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	if (msg->size < sizeof(tll::time_point))
		return _log.fail(EMSGSIZE, "Message from '{}' too small: {} < minimal {}", c->name(), msg->size, sizeof(tll::time_point));
	auto dt = tll::time::now() - *static_cast<const tll::time_point *>(msg->data);

	if (this->channelT()->stat()) {
		auto page = this->channelT()->stat()->acquire();
		if (page) {
			page->rtt = dt.count();
//			page->rx = 1;
//			page->rxb = msg->size;
			this->channelT()->stat()->release(page);
		}
	}

	if (_chained) {
		_send();
		_last_timer = {};
	}

	_time_data.value = dt.count();
	_callback_data(&_msg_time);

	return 0;
}

int RTT::callback_tag(TaggedChannel<Timer> * c, const tll_msg_t *msg)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	if (_last_timer == tll::time::epoch)
		return 0;
	return _send();
}

int RTT::_send()
{
	_last_timer = *(tll::time_point *)_data.data() = tll::time::now();

	_msg.seq++;
	_output->post(&_msg);

	return 0;
}

TLL_DEFINE_IMPL(RTT);

TLL_DEFINE_MODULE(RTT);
