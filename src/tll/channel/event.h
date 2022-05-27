/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_EVENT_H
#define _TLL_CHANNEL_EVENT_H

#include "tll/channel/base.h"

namespace tll::channel {

/// Detached notification structure
struct EventNotify
{
	int fd = -1;

	int notify();
	void close();
	void swap(EventNotify &n) { std::swap(fd, n.fd); }
};

template <typename T>
class Event : public Base<T>
{
 protected:
	int _event_notify_nocheck();
	int _event_clear_nocheck();

 public:
	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();

	EventNotify event_detached();
	int event_notify() { this->_log.debug("Try notify on {}", this->fd()); if (this->fd() != -1) return _event_notify_nocheck(); return 0; }
	int event_clear() { if (this->fd() != -1) return _event_clear_nocheck(); return 0; }

	template <typename F>
	int event_clear_race(F rearm)
	{
		if (this->fd() == -1) return 0;
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
