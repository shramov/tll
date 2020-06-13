/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_NULL_H
#define _TLL_CHANNEL_NULL_H

#include "tll/channel/base.h"

class ChNull : public tll::channel::Base<ChNull>
{
 public:
	static constexpr std::string_view param_prefix() { return "null"; }
	static constexpr auto process_policy() { return ProcessPolicy::Never; }

	int _init(const tll::UrlView &, tll::Channel *master) { return 0; }

	int _process(long timeout, int flags) { return 0; }
	int _post(const tll_msg_t *msg, int flags) { return 0; }
};

#endif//_TLL_CHANNEL_NULL_H
