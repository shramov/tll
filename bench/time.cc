#include <tll/util/bench.h>
#include <tll/util/time.h>

#include <chrono>

#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

using namespace tll::bench;

using namespace std::chrono;

nanoseconds struct2ns(const struct timespec &ts)
{
	return seconds(ts.tv_sec) + nanoseconds(ts.tv_nsec);
}

nanoseconds struct2ns(const struct timeval &ts)
{
	return seconds(ts.tv_sec) + microseconds(ts.tv_usec);
}

nanoseconds cgt(clockid_t clock)
{
	timespec ts = {};
	clock_gettime(clock, &ts);
	return struct2ns(ts);
}

nanoseconds ftime() { return seconds(time(0)); }

int main()
{
	constexpr size_t count = 100000;
	prewarm(100ms);
	timeit(count, "system_clock::now", system_clock::now);
	timeit(count, "steady_clock::now", steady_clock::now);
	timeit(count, "hrt::now", high_resolution_clock::now);
	timeit(count, "clock_gettime(REALTIME)", cgt, CLOCK_REALTIME);
	timeit(count, "clock_gettime(MONOTONIC)", cgt, CLOCK_MONOTONIC);
	timeit(count, "clock_gettime(PROCESS_CPUTIME)", cgt, CLOCK_PROCESS_CPUTIME_ID);
	timeit(count, "clock_gettime(THREAD_CPUTIME)", cgt, CLOCK_THREAD_CPUTIME_ID);
	timeit(count, "gettimeofday", [] { timeval v = {}; gettimeofday(&v, 0); return struct2ns(v); });
	timeit(count, "getrusage", [] { rusage v = {}; getrusage(RUSAGE_SELF, &v); return struct2ns(v.ru_utime); });
	timeit(count, "time", [] { return nanoseconds(seconds(time(0))); } );
	timeit(count, "tll::time::now", tll::time::now);

	return 0;
}
