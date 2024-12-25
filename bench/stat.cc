#include <tll/stat.h>
#include <tll/util/bench.h>

#include <thread>

template <typename T>
struct StatT
{
	tll::stat::FieldT<T, tll::stat::Sum, tll::stat::Unknown, 'f', '0'> f0;
	tll::stat::FieldT<T, tll::stat::Min, tll::stat::Unknown, 'f', '1'> f1;
	tll::stat::FieldT<T, tll::stat::Max, tll::stat::Unknown, 'f', '2'> f2;
	tll::stat::GroupT<T, tll::stat::Unknown, 'f', '3'> grp;
};

using Stat = StatT<tll_stat_int_t>;

int acquire(tll::stat::Block<Stat> * block, int value)
{
	if (auto p = block->acquire(); p) {
		p->f0 = value;
		block->release(p);
	}
	return value;
}

int acquire_wait(tll::stat::Block<Stat> * block, int value)
{
	if (auto p = block->acquire_wait(); p) {
		p->f0 = 1;
		block->release(p);
	}
	return value;
}

template <typename F, typename ... Args>
inline int apply_func(tll::stat::Block<Stat> * block, F func, Args... args)
{
	if (auto p = block->acquire(); p) {
		func(p, std::forward<Args>(args)...);
		block->release(p);
	}
	return 1;
}

int apply(tll::stat::Block<Stat> * block, int value)
{
	apply_func(block, [value](auto * p) { p->f0 = value; });
	return value;
}

int main()
{
	using namespace std::chrono_literals;
	constexpr unsigned count = 10000000;
	tll::stat::Block<Stat> block { "integer" };
	tll::bench::prewarm(100ms);
	tll::bench::timeit(count, "acquire", acquire, &block, 1);
	apply_func(&block, [](auto * p) { fmt::print("f0: {}\n", p->f0.value()); p->f0.reset(); });
	tll::bench::timeit(count, "acquire loop", acquire_wait, &block, 1);
	apply_func(&block, [](auto * p) { fmt::print("f0: {}\n", p->f0.value()); p->f0.reset(); });
	tll::bench::timeit(count, "apply", apply, &block, 1);
	apply_func(&block, [](auto * p) { fmt::print("f0: {}\n", p->f0.value()); p->f0.reset(); });
	bool stop = false;
	auto thread = std::thread([](auto * stop, auto * block) {
		while (!*stop) {
			apply_func(block, [](auto p) { p->f1 = 1; });
			std::this_thread::yield();
		}
	}, &stop, &block);
	tll::bench::prewarm(1ms);
	tll::bench::timeit(count, "thread + acquire", acquire, &block, 1);
	apply_func(&block, [](auto * p) { fmt::print("f0: {}\n", p->f0.value()); p->f0.reset(); });
	tll::bench::timeit(count, "thread + acquire loop", acquire_wait, &block, 1);
	apply_func(&block, [](auto * p) { fmt::print("f0: {}\n", p->f0.value()); p->f0.reset(); });
	tll::bench::timeit(count, "thread + apply", apply, &block, 1);
	apply_func(&block, [](auto * p) { fmt::print("f0: {}\n", p->f0.value()); p->f0.reset(); });
	stop = true;
	thread.join();
	return 0;
}
