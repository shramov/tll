/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_ZERO_H
#define _TLL_CHANNEL_ZERO_H

#include "tll/channel/event.h"

class ChZero : public tll::channel::Event<ChZero>
{
	bool _with_pending = true;
	size_t _size = 1024;
	std::vector<char> _buf;
	tll_msg_t _msg = { TLL_MESSAGE_DATA };

 public:
	static constexpr std::string_view channel_protocol() { return "zero"; }

	int _init(const tll::Channel::Url &url, tll::Channel *master);
	int _open(const tll::ConstConfig &url);

	int _process(long timeout, int flags)
	{
		_msg.seq++;
		_callback(&_msg);
		return 0;
	}

	int _post(const tll_msg_t *msg, int flags) { return 0; }
};

#endif//_TLL_CHANNEL_ZERO_H
