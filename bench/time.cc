#include <tll/util/bench.h>
#include <tll/util/time.h>

#include <chrono>

#include <time.h>
#include <sys/time.h>

using namespace tll::bench;

using namespace std::chrono;
nanoseconds cgt(clockid_t clock)
{
	timespec ts = {};
	clock_gettime(clock, &ts);
	return seconds(ts.tv_sec) + nanoseconds(ts.tv_nsec);
}

nanoseconds gtod()
{
	timeval ts = {};
	gettimeofday(&ts, 0);
	return seconds(ts.tv_sec) + microseconds(ts.tv_usec);
}

nanoseconds ftime() { return seconds(time(0)); }

int main()
{
	constexpr size_t count = 100000;
	timeit(count, "system_clock::now", system_clock::now);
	timeit(count, "steady_clock::now", steady_clock::now);
	timeit(count, "hrt::now", high_resolution_clock::now);
	timeit(count, "clock_gettime(REALTIME)", cgt, CLOCK_REALTIME);
	timeit(count, "clock_gettime(MONOTONIC)", cgt, CLOCK_MONOTONIC);
	timeit(count, "gettimeofday", gtod);
	timeit(count, "time", ftime);
	timeit(count, "tll::time::now", tll::time::now);

	return 0;
}
