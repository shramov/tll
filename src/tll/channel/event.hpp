/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
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
int Event<T>::_init(const UrlView &url, tll::Channel *master)
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
	_fd() = -1;
	return 0;
}

template <typename T>
int Event<T>::_open(const PropsView &url)
{
	if (!_with_fd) return 0;
#ifdef __linux__
	_fd() = eventfd(0, EFD_NONBLOCK);
	if (_fd() == -1)
		return this->_log.fail(EINVAL, "Failed to create eventfd: {}", strerror(errno));
	this->_dcaps_poll(dcaps::CPOLLIN);
#endif
	return 0;
}

template <typename T>
int Event<T>::_close()
{
#ifdef __linux__
	if (_fd() != -1)
		::close(_fd());
#endif
	_fd() = -1;
	return 0;
}

template <typename T>
int Event<T>::_event_notify_nocheck()
{
#ifdef __linux__
	int64_t w = 1;
	//_log.debug("Write to {}", *_qout->fd);
	if (write(_fd(), &w, sizeof(w)) != sizeof(w))
		return this->_log.fail(EINVAL, "Failed to write to eventfd: {}", strerror(errno));
#endif
	return 0;
}

template <typename T>
int Event<T>::_event_clear_nocheck()
{
#ifdef __linux__
	int64_t w = 1;
	//_log.debug("Write to {}", *_qout->fd);
	if (read(_fd(), &w, sizeof(w)) != sizeof(w))
		return this->_log.fail(EINVAL, "Failed to read from eventfd: {}", strerror(errno));
#endif
	return 0;
}

} // namespace tll::channel

#endif
