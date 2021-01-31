/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_IMPL_CHANNEL_EVENT_HPP

#include "tll/channel/event.h"

#ifdef __linux__
#include <sys/eventfd.h>
#include <unistd.h>
#endif

namespace tll::channel {

template <typename T>
int Event<T>::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = this->channelT()->channel_props_reader(url);
	_with_fd = reader.getT("fd", _with_fd);
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());
	if (!_with_fd)
		this->_log.debug("Event notification disabled");
#ifndef __linux__
	this->_log.debug("Event polling supported only on Linux");
	_with_fd = false;
#endif
	return Base<T>::_init(url, master);
}

template <typename T>
int Event<T>::_open(const PropsView &url)
{
	if (!_with_fd) return 0;
#ifdef __linux__
	auto fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (fd == -1)
		return this->_log.fail(EINVAL, "Failed to create eventfd: {}", strerror(errno));
	this->_update_fd(fd);
	this->_dcaps_poll(dcaps::CPOLLIN);
#endif
	return Base<T>::_open(url);
}

template <typename T>
int Event<T>::_close()
{
#ifdef __linux__
	auto fd = this->_update_fd(-1);
	if (fd != -1)
		::close(fd);
#endif
	return 0;
}

#ifdef __linux__
namespace {
int _notify(int fd) {
	int64_t w = 1;
	if (write(fd, &w, sizeof(w)) != sizeof(w))
		return errno ? errno : EMSGSIZE;
	return 0;
}
}
#endif

template <typename T>
int Event<T>::_event_notify_nocheck()
{
#ifdef __linux__
	if (auto r = _notify(this->fd()))
		return this->_log.fail(EINVAL, "Failed to write to eventfd: {}", strerror(r));
#endif
	return 0;
}

inline int EventNotify::notify()
{
#ifdef __linux__
	if (fd == -1) return 0;
	return _notify(fd);
#else
	return 0;
#endif
}

inline void EventNotify::close()
{
#ifdef __linux__
	if (fd != -1)
		::close(fd);
	fd = -1;
#endif
}

template <typename T>
int Event<T>::_event_clear_nocheck()
{
#ifdef __linux__
	int64_t w = 1;
	if (read(this->fd(), &w, sizeof(w)) != sizeof(w)) {
		//if (errno == EAGAIN) // Data landed before event notification received?
		//	return 0;
		return this->_log.fail(EINVAL, "Failed to read from eventfd: {}", strerror(errno));
	}
#endif
	return 0;
}

template <typename T>
EventNotify Event<T>::event_detached()
{
#ifdef __linux__
	return { dup(this->fd()) };
#else
	return { -1 };
#endif
}

} // namespace tll::channel

#endif
