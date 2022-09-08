/*
 * Copyright (c) 2022 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _CHANNEL_FRAMED_H
#define _CHANNEL_FRAMED_H

#include <tll/channel/prefix.h>

class Framed : public tll::channel::Base<Framed>
{
 public:
	static constexpr std::string_view channel_protocol() { return "frame+"; }

	std::optional<const tll_channel_impl_t *> _init_replace(const tll::Channel::Url &url, tll::Channel *master);

	int _init(const tll::Channel::Url &url, tll::Channel * master) { return this->_log.fail(EINVAL, "Failed to choose proper frame"); }
};

#endif//_CHANNEL_FRAMED_H
