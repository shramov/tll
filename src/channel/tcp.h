/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_IMPL_CHANNEL_TCP_H
#define _TLL_IMPL_CHANNEL_TCP_H

#include "tll/channel/base.h"

class ChTcp : public tll::channel::Base<ChTcp>
{
 public:
	static constexpr std::string_view param_prefix() { return "tcp"; }

	tll_channel_impl_t * _init_replace(const tll::Channel::Url &url);

	int _init(const tll::Channel::Url &url, tll::Channel * master) { return _log.fail(EINVAL, "Failed to choose proper tcp channel"); }
};

#endif//_TLL_IMPL_CHANNEL_TCP_H
