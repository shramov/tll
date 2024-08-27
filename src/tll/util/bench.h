/*
 * Copyright (c) 2021-2022 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 *
 */

/*
 * Helper function for microbenchmarking
 */

#ifndef _TLL_UTIL_BENCH_H
#define _TLL_UTIL_BENCH_H

#include <chrono>
#include <fmt/format.h>
#include <fmt/chrono.h>

namespace tll::bench {

template <typename R, typename P>
void prewarm(const std::chrono::duration<R, P> &timeout)
{
	auto end = std::chrono::steady_clock::now() + timeout;
	while (end > std::chrono::steady_clock::now()) {}
}

template <typename T>
struct reduce
{
	static constexpr unsigned long long call(const T &v) { return static_cast<unsigned long long>(v); }
};

template <>
struct reduce<std::string_view>
{
	static constexpr auto call(std::string_view v) { return v.size(); }
};

template <typename T>
struct reduce<T *>
{
	static constexpr auto call(T *v) { return (ptrdiff_t) v; }
};

template <typename Rep, typename Res>
struct reduce<std::chrono::duration<Rep, Res>>
{
	static constexpr auto call(const std::chrono::duration<Rep, Res>& v) { return v.count(); }
};

template <typename Clock, typename Dur>
struct reduce<std::chrono::time_point<Clock, Dur>>
{
	static constexpr auto call(const std::chrono::time_point<Clock, Dur>& v) { return v.time_since_epoch().count(); }
};

template <typename T>
unsigned long long _reduce(const T &v) { return reduce<T>::call(v); }

template <typename F, typename... Args>
[[gnu::noinline]] auto timeit(size_t count, std::string_view name, F f, Args... args)
{
	using namespace std::chrono;

	volatile unsigned long long accum = 0;
	auto start = steady_clock::now();
	asm volatile("": : :"memory");
	for (auto i = 0u; i < count; i++) {
		accum = accum ^ _reduce(f(args...));
		asm volatile("": : :"memory");
	}
	nanoseconds dt = steady_clock::now() - start;
	(void) accum;
	fmt::print("Time {}: {:.3f}/{}: {}\n", name, std::chrono::duration<double, std::milli>(dt), count, dt / count);
}

} // namespace tll::bench

#endif//_TLL_UTIL_BENCH_H
