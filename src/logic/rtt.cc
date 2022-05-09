/*
 * Copyright (c) 2022 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <tll/channel/tagged.h>
#include <tll/channel/module.h>
#include <tll/util/size.h>

using namespace tll::channel;

struct Timer : public Tag<TLL_MESSAGE_MASK_DATA> { static constexpr std::string_view name() { return "timer"; } };

class RTT : public Tagged<RTT, Timer, Input, Output>
{
	tll::Channel * _output = nullptr;

	tll_msg_t _msg = {};
	std::vector<unsigned char> _data;
 public:
	using Base = Tagged<RTT, Timer, Input, Output>;
	static constexpr std::string_view channel_protocol() { return "rtt"; }

	struct StatType : public Base::StatType
	{
		tll::stat::IntegerGroup<tll::stat::Ns, 'r', 't', 't'> rtt;
	};

	tll::stat::BlockT<StatType> * stat() { return static_cast<tll::stat::BlockT<StatType> *>(this->internal.stat); }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &cfg)
	{
		_msg.seq = -1;
		return Base::_open(cfg);
	}

	int callback_tag(TaggedChannel<Input> * c, const tll_msg_t *msg);
	int callback_tag(TaggedChannel<Output> * c, const tll_msg_t *msg) { return 0; }
	int callback_tag(TaggedChannel<Timer> * c, const tll_msg_t *msg);
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

	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	_data.resize(sizeof(tll::time_point) + size);
	_msg.data = _data.data();
	_msg.size = _data.size();

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
	return 0;
}

int RTT::callback_tag(TaggedChannel<Timer> * c, const tll_msg_t *msg)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	*(tll::time_point *)_data.data() = tll::time::now();

	_msg.seq++;
	_output->post(&_msg);

	return 0;
}

TLL_DEFINE_IMPL(RTT);

TLL_DEFINE_MODULE(RTT);
