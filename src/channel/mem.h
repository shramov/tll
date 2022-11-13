/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_MEM_H
#define _TLL_CHANNEL_MEM_H

#include "tll/channel/base.h"

class ChMem : public tll::channel::Base<ChMem>
{
 public:
	static constexpr std::string_view channel_protocol() { return "mem"; }

	std::optional<const tll_channel_impl_t *> _init_replace(const tll::Channel::Url &url, tll::Channel *master);

	int _init(const tll::Channel::Url &url, tll::Channel * master) { return this->_log.fail(EINVAL, "Failed to choose proper mem channel"); }
};

#endif//_TLL_CHANNEL_MEM_H
