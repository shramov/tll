/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
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
	bool _oneshot_init = false;
	tll::duration _interval, _interval_init;

	clockid_t _clock_type;

	time_point _next;

	time_point _now() const;

	int _rearm(tll::duration);
	int _rearm(tll::time_point);
	int _rearm_clear();
#ifdef __linux__
	int _rearm_timerfd(tll::duration interval, bool oneshot, int flags);
#endif
 public:
	static constexpr std::string_view channel_protocol() { return "timer"; }
	static constexpr auto process_policy() { return ProcessPolicy::Custom; }
	static constexpr auto scheme_policy() { return SchemePolicy::Manual; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags);
};

//extern template class tll::channel::Base<ChTimer>;

#endif//_TLL_CHANNEL_TIMER_H
