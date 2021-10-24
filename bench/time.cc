#include <tll/util/time.h>
#include <tll/util/conv-fmt.h>

#include <fmt/format.h>

#include <time.h>
#include <sys/time.h>

using namespace std::chrono;

constexpr size_t count = 100000;

template <typename F, typename... Args>
void timeit(std::string_view name, F f, Args... args)
{
	auto accum = f(args...);
	auto start = system_clock::now();
	for (auto i = 0u; i < count; i++)
		accum = f(args...);
	nanoseconds dt = system_clock::now() - start;
	fmt::print("Time {}: {:.3f}ms/{}: {}\n", name, duration<double, std::milli>(dt).count(), count, dt / count);
}

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
	timeit("system_clock::now", system_clock::now);
	timeit("steady_clock::now", steady_clock::now);
	timeit("hrt::now", high_resolution_clock::now);
	timeit("clock_gettime(REALTIME)", cgt, CLOCK_REALTIME);
	timeit("clock_gettime(MONOTONIC)", cgt, CLOCK_MONOTONIC);
	timeit("gettimeofday", gtod);
	timeit("time", ftime);
	timeit("tll::time::now", tll::time::now);

	return 0;
}
