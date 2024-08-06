// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _CHANNEL_TIMELINE_H
#define _CHANNEL_TIMELINE_H

#include "tll/channel/prefix.h"
#include "tll/util/time.h"

namespace tll::channel {

class TimeLine : public tll::channel::Prefix<TimeLine>
{
	std::unique_ptr<tll::Channel> _timer;

	double _speed = 1.0;
	tll::time_point _next = {};
	tll_msg_t _msg = {};
	std::vector<char> _buf;

 public:
	using Base = tll::channel::Prefix<TimeLine>;
	static constexpr std::string_view channel_protocol() { return "timeline+"; }

	int _init(const Channel::Url &url, tll::Channel *master);
	int _open(const tll::ConstConfig &cfg)
	{
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

 private:
	int _on_timer(const tll::Channel *, const tll_msg_t *msg);
	int _rearm(const tll::duration &dt);
};

} // namespace tll::channel

#endif//_CHANNEL_TIMELINE_H
