#ifndef _TLL_UTIL_RUSAGE_H
#define _TLL_UTIL_RUSAGE_H

#include <errno.h>
#include <sys/resource.h>

#include <chrono>

#ifndef RUSAGE_THREAD
#define RUSAGE_THREAD RUSAGE_SELF // TODO: Use clock_gettime(CLOCK_THREAD_CPUTIME_ID)
#endif

namespace tll::util {

class RUsage
{
	using Clock = std::chrono::steady_clock;
	using nanoseconds = std::chrono::nanoseconds;
	using time_point = typename Clock::time_point;

	time_point _last;
	struct rusage _rusage = {};
	int _who = RUSAGE_SELF;

 public:
	nanoseconds cpu;
	double cpu_ratio = 0;
	size_t memory = 0;

	enum Type { Process, Thread };
	RUsage(Type type = Process)
	{
		if (type == Thread)
			_who = RUSAGE_THREAD;
		else
			_who = RUSAGE_SELF;
	}

	void reset()
	{
		_last = {};
		cpu = {};
		cpu_ratio = {};
		memory = {};
	}

	int update()
	{
		return update(Clock::now());
	}

	int update(time_point now)
	{
		if (now == _last)
			return 0;
		if (auto r = getrusage(_who, &_rusage); r)
			return errno;
		auto cnew = c2ns(_rusage.ru_utime) + c2ns(_rusage.ru_stime);
		if (_last != time_point {} && now != _last)
			cpu_ratio = (1. * (cnew - cpu)) / (now - _last);
		_last = now;
		cpu = cnew;
		memory = _rusage.ru_maxrss * 1024; // Kilobytes
		return 0;
	}

	static constexpr nanoseconds c2ns(const timeval &ts)
	{
		return std::chrono::seconds(ts.tv_sec) + std::chrono::microseconds(ts.tv_usec);
	}
};

} // namespace tll::util

#endif//_TLL_UTIL_RUSAGE_H
