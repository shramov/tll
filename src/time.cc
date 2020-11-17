#include "tll/util/time.h"

#include <sys/time.h>
#include <time.h>

struct cached_clock_t
{
	long long last = 0;
	int enabled = 0;
	timespec ts = {};

	void enable(bool v)
	{
		if (v) {
			last = now();
			enabled++;
		} else
			enabled--;
	}

	long long now()
	{
		clock_gettime(CLOCK_REALTIME, &ts);
		last = 1000000000ll * ts.tv_sec + ts.tv_nsec;
		return last;
	}

	long long now_cached()
	{
		if (enabled)
			return last;
		return now();
	}
};

namespace { thread_local cached_clock_t cached_clock = {}; }

long long tll_time_now()
{
	return cached_clock.now();
}

long long tll_time_now_cached()
{
	return cached_clock.now_cached();
}

void tll_time_cache_enable(int enable)
{
	cached_clock.enable(enable);
}
