/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_EVENT_H
#define _TLL_CHANNEL_EVENT_H

#include "tll/channel/base.h"

namespace tll::channel {

template <typename T>
class Event : public Base<T>
{
 protected:
	bool _with_fd = true;

	int _event_notify_nocheck();
	int _event_clear_nocheck();

 public:
	static constexpr std::string_view param_prefix() { return "event"; }

	/// Detached notification structure
	struct Notify {
		int fd = -1;

		int notify();
		void close();
		void swap(Notify &n) { std::swap(fd, n.fd); }
	};

	int _init(const tll::UrlView &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();

	Notify event_detached();
	int event_notify() { this->_log.debug("Try notify on {}", this->fd()); if (this->fd() != -1) return _event_notify_nocheck(); return 0; }
	int event_clear() { if (this->fd() != -1) return _event_clear_nocheck(); return 0; }

	template <typename F>
	int event_clear_race_nocheck(F rearm)
	{
		if (!_with_fd) return 0;
		return event_clear_race<F>(rearm);
	}

	template <typename F>
	int event_clear_race(F rearm)
	{
		if (!_with_fd) return 0;
		if (rearm())
			return 0;
		if (_event_clear_nocheck())
			return this->_log.fail(EINVAL, "Failed to clear event");
		if (!rearm())
			return 0;
		this->_log.debug("Rearm event");
		if (_event_notify_nocheck())
			return this->_log.fail(EINVAL, "Failed to rearm event");
		return 0;
	}
};

} // namespace tll::channel

#endif//_TLL_CHANNEL_EVENT_H
