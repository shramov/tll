/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/zero.h"
#include "tll/channel/event.hpp"

#include "tll/util/size.h"

using namespace tll;

TLL_DEFINE_IMPL(ChZero);

int ChZero::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	_size = reader.getT<tll::util::Size>("size", 1024);
	_with_pending = reader.getT("pending", true);
	_msg.msgid = reader.getT("msgid", 0);
	char fill = reader.getT("fill", '\0');
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	_buf.resize(_size);
	_msg.data = _buf.data();
	_msg.size = _buf.size();
	memset(_buf.data(), fill, _buf.size());
	return Event<ChZero>::_init(url, master);
}

int ChZero::_open(const ConstConfig &url)
{
	int r = Event<ChZero>::_open(url);
	if (r)
		return r;
	event_notify();
	if (_with_pending)
		_dcaps_pending(true);
	return 0;
}
