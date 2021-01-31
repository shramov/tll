/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/timer.h"
#include "channel/timer-scheme.h"

#ifdef __linux__
#include <sys/timerfd.h>
#include <unistd.h>
#endif

using namespace tll;
using namespace std::chrono_literals;

TLL_DEFINE_IMPL(ChTimer);

namespace {
constexpr std::string_view clock2str(int clock)
{
	switch (clock) {
	case CLOCK_MONOTONIC: return "monotonic";
	case CLOCK_REALTIME: return "realtime";
	}
	return "unknown";
}

constexpr tll::duration ts2tll(timespec ts)
{
	using namespace std::chrono;
	return seconds(ts.tv_sec) + nanoseconds(ts.tv_nsec);
}

#ifdef __linux__
constexpr timespec tll2ts(tll::duration ts)
{
	using namespace std::chrono;
	auto s = duration_cast<seconds>(ts);
	return { (time_t) s.count(), (long) nanoseconds(ts - s).count() };
}
#endif
} // namespace

int ChTimer::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	_with_fd = reader.getT("fd", true);
	_interval_init = reader.getT<tll::duration>("interval", 0ns);
	_initial_init = reader.getT<tll::duration>("initial", 0ns);
	_clock_type = reader.getT("clock", CLOCK_MONOTONIC, {{"monotonic", CLOCK_MONOTONIC}, {"realtime", CLOCK_REALTIME}});
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	struct timespec ts = {};
	if (clock_gettime(_clock_type, &ts))
		return _log.fail(EINVAL, "Clock {} is not supported", clock2str(_clock_type));
	
	_scheme.reset(context().scheme_load(timer_scheme::scheme_absolute));
	if (!_scheme.get())
		return _log.fail(EINVAL, "Failed to load timer scheme");

	_log.info("Initializing with {} clock", clock2str(_clock_type));

	return 0;
}

int ChTimer::_open(const PropsView &url)
{
	_next = {};
	auto reader = channel_props_reader(url);
	_interval = reader.getT<tll::duration>("interval", _interval_init);
	_initial = reader.getT<tll::duration>("initial", _initial_init);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

#ifdef __linux__
	if (_with_fd) {
		auto fd = timerfd_create(_clock_type, TFD_NONBLOCK | TFD_CLOEXEC);
		if (fd == -1)
			return _log.fail(EINVAL, "Failed to create timer fd: {}", strerror(errno));
		_update_fd(fd);
	}
#endif

	if (_interval.count() || _initial.count()) {
		auto initial = (_initial.count() ? _initial : _interval);
		_next = _now() + initial;
		_log.debug("Next wakeup: {}", std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(initial));
#ifdef __linux__
		struct itimerspec its = {};
		its.it_interval = tll2ts(_interval);
		its.it_value = tll2ts(initial);
		if (timerfd_settime(fd(), 0, &its, nullptr))
			return _log.fail(EINVAL, "Failed to rearm timerfd: {}", strerror(errno));
		_update_dcaps(dcaps::Process | dcaps::CPOLLIN);
#else
		_update_dcaps(dcaps::Process);
#endif
	} else
		_next = {};

	return 0;
}

int ChTimer::_close()
{
#ifdef __linux__
	auto fd = this->_update_fd(-1);
	if (fd != -1)
		::close(fd);
#endif
	return 0;
}

timer_clock::time_point ChTimer::_now() const
{
	// clock_gettime can not fail here
	struct timespec ts = {};
	clock_gettime(_clock_type, &ts);
	return time_point(ts2tll(ts));
}

int ChTimer::_rearm(tll::duration ts)
{
	auto now = _now();
	_next = now + ts;
	_interval = {};

	_log.debug("Rearm relative: {}", ts);
	unsigned caps = dcaps::Process;
#ifdef __linux__
	struct itimerspec its = {};
	its.it_value = tll2ts(ts);
	if (timerfd_settime(fd(), 0, &its, nullptr))
		return _log.fail(EINVAL, "Failed to rearm timerfd: {}", strerror(errno));
	caps |= dcaps::CPOLLIN;
#endif
	_update_dcaps(caps);
	return 0;
}

int ChTimer::_rearm(tll::time_point ts)
{
	_next = time_point(ts.time_since_epoch());
	_interval = {};

	unsigned caps = dcaps::Process;
#ifdef __linux__
	struct itimerspec its = {};
	its.it_value = tll2ts(ts.time_since_epoch());
	if (timerfd_settime(fd(), TFD_TIMER_ABSTIME, &its, nullptr))
		return _log.fail(EINVAL, "Failed to rearm timerfd: {}", strerror(errno));
	caps |= dcaps::CPOLLIN;
#endif
	_update_dcaps(caps);
	return 0;
}

int ChTimer::_rearm_clear()
{
	_next = {};
	_interval = {};

#ifdef __linux__
	struct itimerspec its = {};
	if (timerfd_settime(fd(), TFD_TIMER_ABSTIME, &its, nullptr))
		return _log.fail(EINVAL, "Failed to clear timerfd: {}", strerror(errno));
#endif
	_update_dcaps(0, dcaps::Process | dcaps::CPOLLIN);
	return 0;
}

int ChTimer::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	switch (msg->msgid) {
	case timer_scheme::relative::id: {
		auto ts = (const timer_scheme::relative *) msg->data;
		if (msg->size != sizeof(*ts))
			return _log.fail(EMSGSIZE, "Invalid message size: {} != {}", msg->size, sizeof(*ts));
		if (ts->ts == tll::duration {})
			return _rearm_clear();
		return _rearm(ts->ts);
	}
	case timer_scheme::absolute::id: {
		if (_clock_type == CLOCK_MONOTONIC)
			return _log.fail(EINVAL, "Absolute timestamps not supported with monotonic timer");
		auto ts = (const timer_scheme::absolute *) msg->data;
		if (msg->size != sizeof(*ts))
			return _log.fail(EMSGSIZE, "Invalid message size: {} != {}", msg->size, sizeof(*ts));
		if (ts->ts == tll::time_point {})
			return _rearm_clear();
		return _rearm(ts->ts);
	}
	default:
		return _log.fail(EINVAL, "Invalid message {}", msg->msgid);
	}
	return 0;
}

int ChTimer::_process(long timeout, int flags)
{
	if (_next == time_point())
		return EAGAIN;
	auto now = _now();
	if (now < _next)
		return EAGAIN;

	tll_msg_t msg = {};
	msg.type = TLL_MESSAGE_DATA;
	msg.msgid = timer_scheme::absolute::id;
	msg.data = &now;
	msg.size = sizeof(now);

	if (_interval.count())
		_next += _interval;
	else
		_next = {};

#ifdef __linux__
	if (_next < now)
		_log.debug("Pending notification, not clearing fd");
	if (_with_fd && (_next == time_point() || _next > now)) {
		uint64_t w;
		if (read(fd(), &w, sizeof(w)) != sizeof(w))
			return _log.fail(EINVAL, "Failed to read from timerfd: {}", strerror(errno));
		_log.debug("Clear {} notifications from fd", w);
	}
#endif

	if (_clock_type == CLOCK_MONOTONIC)
		now = {};

	_callback_data(&msg);

	if (_next == time_point())
		_update_dcaps(0, dcaps::Process | dcaps::CPOLLMASK);
	return 0;
}
