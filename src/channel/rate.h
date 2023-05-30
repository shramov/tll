/*
 * Copyright (c) 2023 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _CHANNEL_RATE_H
#define _CHANNEL_RATE_H

#include "tll/channel/prefix.h"
#include "tll/channel/rate.h"

namespace tll::channel {

class Rate : public tll::channel::Prefix<Rate>
{
	std::unique_ptr<tll::Channel> _timer;

	rate::Settings _conf;
	rate::Bucket _bucket;

 public:
	using Base = tll::channel::Prefix<Rate>;
	static constexpr std::string_view channel_protocol() { return "rate+"; }

	const Scheme * scheme(int type) const
	{
		if (type == TLL_MESSAGE_CONTROL)
			return _scheme_control.get();
		return Base::scheme(type);
	}

	int _init(const Channel::Url &url, tll::Channel *master);
	int _open(const tll::ConstConfig &cfg)
	{
		_bucket.reset();
		_timer->open();
		return Base::_open(cfg);
	}

	int _on_closed()
	{
		_timer->close(true);
		if (!(internal.caps & tll::caps::Output) && (_child->dcaps() & tll::dcaps::SuspendPermanent)) {
			if (internal.dcaps & tll::dcaps::Suspend) {
				_child->internal->dcaps ^= tll::dcaps::SuspendPermanent; // Remove suspend lock
			} else
				_child->resume();
		}
		return Base::_on_closed();
	}

	int _on_data(const tll_msg_t *);

	int _post(const tll_msg_t *msg, int flags);

 private:
	int _on_timer(const tll_msg_t *msg);
	int _rearm(const tll::duration &dt);

	void _callback_control(int msgid)
	{
		tll_msg_t msg = { TLL_MESSAGE_CONTROL };
		msg.msgid = msgid;
		_callback(&msg);
	}
};

} // namespace tll::channel

#endif//_CHANNEL_RATE_H
