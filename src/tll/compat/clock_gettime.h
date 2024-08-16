// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_COMPAT_CLOCK_GETTIME_H
#define _TLL_COMPAT_CLOCK_GETTIME_H

#include <time.h>

#ifdef _WIN32
#include <cerrno>
#include <chrono>

enum clockid_t {
	CLOCK_REALTIME,
	CLOCK_MONOTONIC,
};

static inline int clock_gettime(clockid_t clockid, struct timespec *tp)
{
	using namespace std::chrono;
	nanoseconds ns;

	if (!tp)
		return EINVAL;

	switch (clockid) {
	case CLOCK_REALTIME:
		ns = system_clock::now().time_since_epoch();
		break;
	case CLOCK_MONOTONIC:
		ns = steady_clock::now().time_since_epoch();
		break;
	default:
		return EINVAL;
	}
	tp->tv_sec = duration_cast<seconds>(ns).count();
	tp->tv_nsec = ns.count() % 1000000000;
	return 0;
}
#endif

#endif//_TLL_COMPAT_CLOCK_GETTIME_H
