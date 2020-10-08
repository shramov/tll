/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_TIMER_H
#define _TLL_CHANNEL_TIMER_H

#include "tll/channel/base.h"
#include "tll/util/time.h"

#include <time.h>

/// Fake clock to forbid conversion from tll::time_point to local time_point type.
struct timer_clock
{
    using duration = tll::duration;
    using rep = duration::rep;
    using period = duration::period;
    using time_point = std::chrono::time_point<timer_clock>;
    static constexpr bool is_steady = false;
};

class ChTimer : public tll::channel::Base<ChTimer>
{
	using time_point = timer_clock::time_point;

	bool _with_fd = true;
	tll::duration _initial, _initial_init;
	tll::duration _interval, _interval_init;

	clockid_t _clock_type;

	time_point _next;

	time_point _now() const;

	int _rearm(tll::duration);
	int _rearm(tll::time_point);
 public:
	static constexpr std::string_view param_prefix() { return "timer"; }
	static constexpr auto process_policy() { return ProcessPolicy::Custom; }

	int _init(const tll::UrlView &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags);
};

//extern template class tll::channel::Base<ChTimer>;

#endif//_TLL_CHANNEL_TIMER_H
