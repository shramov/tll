/*
 * Copyright (c)2020-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/channel/logic.h"

class Forward : public tll::LogicBase<Forward>
{
	tll::Channel * _input = nullptr;
	tll::Channel * _output = nullptr;
 public:
	static constexpr std::string_view channel_protocol() { return "forward"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);

	int logic(const tll::Channel * c, const tll_msg_t *msg);
};

int Forward::_init(const tll::Channel::Url &url, tll::Channel *)
{
	auto i = _channels.find("input");
	auto o = _channels.find("output");
	if (i == _channels.end()) return _log.fail(EINVAL, "No input channels");
	if (o == _channels.end()) return _log.fail(EINVAL, "No output channels");
	if (i->second.size() != 1) return _log.fail(EINVAL, "Need exactly one input, got {}", i->second.size());
	if (o->second.size() != 1) return _log.fail(EINVAL, "Need exactly one output, got {}", i->second.size());
	_input = i->second.front();
	_output = o->second.front();
	return 0;
}

int Forward::logic(const tll::Channel * c, const tll_msg_t *msg)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	if (c == _input)
		_output->post(msg);
	return 0;
}

TLL_DEFINE_IMPL(Forward);

auto channel_module = tll::make_channel_module<Forward>();
