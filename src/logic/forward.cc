/*
 * Copyright (c)2020-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/channel/module.h"
#include "tll/channel/tagged.h"

using tll::channel::Input;
using tll::channel::Output;
using tll::channel::TaggedChannel;

template <>
struct tll::channel::TaggedChannel<tll::channel::Input>
{
	tll::Channel * channel;
	TaggedChannel<Output> * output;
};

class Forward : public tll::channel::Tagged<Forward, Input, Output>
{
 public:
	static constexpr std::string_view channel_protocol() { return "forward"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);

	int callback_tag(TaggedChannel<Input> * c, const tll_msg_t *msg);
	int callback_tag(TaggedChannel<Output> * c, const tll_msg_t *msg) { return 0; }
};

int Forward::_init(const tll::Channel::Url &url, tll::Channel *)
{
	auto & inputs = _channels.get<Input>();
	auto & outputs = _channels.get<Output>();
	if (inputs.size() != outputs.size())
		return _log.fail(EINVAL, "Input size {} differs from output size {}", inputs.size(), outputs.size());
	auto i = inputs.begin();
	auto o = outputs.begin();
	for (; i != inputs.end(); i++, o++) {
		_log.info("Forward {} -> {}", i->first.channel->name(), o->first->name());
		i->first.output = o->first;
	}
	return 0;
}

int Forward::callback_tag(TaggedChannel<Input> * c, const tll_msg_t *msg)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	c->output->post(msg);
	return 0;
}

TLL_DEFINE_IMPL(Forward);

TLL_DEFINE_MODULE(Forward);
