#ifndef _TIMEIT_H
#define _TIMEIT_H

#include <chrono>
#include <fmt/format.h>

#include <tll/util/time.h>
#include <tll/util/conv-fmt.h>

template <typename F, typename... Args>
void timeit(size_t count, std::string_view name, F f, Args... args)
{
	using namespace std::chrono;

	auto accum = f(args...);
	auto start = system_clock::now();
	for (auto i = 0u; i < count; i++)
		accum = f(args...);
	(void) (accum == accum);
	nanoseconds dt = system_clock::now() - start;
	fmt::print("Time {}: {:.3f}ms/{}: {}\n", name, duration<double, std::milli>(dt).count(), count, dt / count);
}

#endif//_TIMEIT_H
