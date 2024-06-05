#include <tll/util/bench.h>
#include <tll/util/refptr.h>

#include <thread>
#include <unistd.h>

using namespace tll::bench;
using namespace std::chrono_literals;

struct Call : public tll::util::refbase_t<Call>{
	volatile unsigned value = 0;
	unsigned call() { return ++value; }
};

template <typename Ptr> unsigned copy(Ptr &ptr)
{
	Ptr copy(ptr);
	return copy->call();
}

template <typename Ptr> unsigned ref(Ptr *ptr)
{
	auto r = ptr->ref()->call();
	ptr->unref();
	return r;
}

int main(int argc, char *argv[])
{
	unsigned count = 100000000;

	std::shared_ptr<Call> scall(new Call);
	tll::util::refptr_t<Call> rcall(new Call);
	auto ptr = new Call;
	Call call;

	prewarm(100ms);
	timeit(count, "shared_ptr (nothread)", copy<decltype(scall)>, scall);
	bool run = false;
	std::thread thread([](bool * flag){ while (*flag) usleep(100000); }, &run);

	prewarm(100ms);
	timeit(count, "shared_ptr", copy<decltype(scall)>, scall);
	timeit(count, "refcnt", copy<decltype(rcall)>, rcall);
	timeit(count, "ref", ref<Call>, ptr);
	timeit(count, "raw", copy<Call *>, &call);

	ptr->unref();

	thread.join();

	return 0;
}
