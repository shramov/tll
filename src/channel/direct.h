/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
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
	bool _notify_state = false;

 public:
	static constexpr std::string_view channel_protocol() { return "direct"; }
	static constexpr auto process_policy() { return ProcessPolicy::Never; }
	static constexpr auto open_policy() { return OpenPolicy::Manual; }

	int _init(const tll::Channel::Url &url, tll::Channel * master);
	int _open(const tll::ConstConfig &)
	{
		if (!_sub) {
			state(tll::state::Active);
			return 0;
		}
		_update_state(tll::state::Opening);
		_sibling->_sibling = this;
		_update_state(tll::state::Active);
		return 0;
	}

	int _close()
	{
		if (!_sub)
			return 0;
		_update_state(tll::state::Closing);
		_sibling->_sibling = nullptr;
		_update_state(tll::state::Closed);
		return 0;
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		if (_sibling)
			_sibling->_callback(msg);
		return 0;
	}

	int _process(long timeout, int flags) { return EAGAIN; }

	void _update_state(tll_state_t state);
};

#endif//_TLL_CHANNEL_DIRECT_H
