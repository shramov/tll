/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_DIRECT_H
#define _TLL_CHANNEL_DIRECT_H

#include "tll/channel/base.h"

class ChDirect : public tll::channel::Base<ChDirect>
{
	ChDirect * _sibling = nullptr;
	bool _sub = false;

 public:
	static constexpr std::string_view param_prefix() { return "direct"; }
	static constexpr auto process_policy() { return ProcessPolicy::Never; }

	int _init(const tll::Channel::Url &url, tll::Channel * master)
	{
		if (!master)
			return 0;

		_sub = true;
		_sibling = tll::channel_cast<ChDirect>(master);
		if (!_sibling)
			return _log.fail(EINVAL, "Parent {} must be direct:// channel", master->name());
		_log.debug("Init child of master {}", _sibling->name);
		return 0;
	}

	int _open(const tll::PropsView &)
	{
		if (_sub)
			_sibling->_sibling = this;
		state(TLL_STATE_ACTIVE);
		return 0;
	}

	int _close()
	{
		if (_sub)
			_sibling->_sibling = nullptr;
		return 0;
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		if (_sibling)
			_sibling->_callback(msg);
		return 0;
	}

	int _process(long timeout, int flags) { return EAGAIN; }
};

#endif//_TLL_CHANNEL_DIRECT_H
