#include <tll/channel.h>
#include <tll/logger.h>
#include <tll/util/bench.h>

using namespace tll::bench;
using namespace std::chrono_literals;

template <typename T, int (T::*F)(const tll::Channel *, const tll_msg_t *)>
static int _proxy_member(const tll_channel_t * c, const tll_msg_t * msg, void * data)
{
	return (static_cast<T *>(data)->*F)(static_cast<const tll::Channel *>(c), msg);
}

template <typename T, int (T::*F)(const tll::Channel *, const tll_msg_t *)>
static int _proxy_invoke(const tll_channel_t * c, const tll_msg_t * msg, void * data)
{
	return std::invoke(F, static_cast<T *>(data), static_cast<const tll::Channel *>(c), msg);
}

struct Counter
{
	unsigned count = 0;

	int callback(const tll_channel_t *, const tll_msg_t * m)
	{
		count++;
		return 0;
	}

	int method(const tll::Channel *, const tll_msg_t * m)
	{
		count++;
		return 0;
	}

	int cmethod(const tll::Channel *, const tll_msg_t * m) const
	{
		return 0;
	}

	static int function(Counter * self, const tll::Channel *, const tll_msg_t * m)
	{
		self->count++;
		return 0;
	}

	static int cfunction(const tll_channel_t *, const tll_msg_t * m, void * data)
	{
		auto self = static_cast<Counter *>(data);
		self->count++;
		return 0;
	}
};

int main(int argc, char *argv[])
{
	tll::Logger::set("tll", tll::Logger::Warning, true);
	auto ctx = tll::channel::Context(tll::Config());

	Counter c0, c1, c2, c3;

	auto null = ctx.channel("null://");
	null->callback_add(_proxy_member<Counter, &Counter::method>, &c0, TLL_MESSAGE_MASK_DATA);
	null->callback_add(_proxy_invoke<Counter, &Counter::method>, &c0, TLL_MESSAGE_MASK_DATA);
	null->callback_add<Counter, &Counter::cmethod>(&c0, TLL_MESSAGE_MASK_DATA);

	auto z0 = ctx.channel("zero://");
	auto z1 = ctx.channel("zero://");
	auto z2 = ctx.channel("zero://");
	auto z3 = ctx.channel("zero://");

	z0->callback_add(Counter::cfunction, &c0, TLL_MESSAGE_MASK_DATA);
	z2->callback_add<Counter, &Counter::function>(&c1, TLL_MESSAGE_MASK_DATA);
	z2->callback_add<Counter, &Counter::method>(&c2, TLL_MESSAGE_MASK_DATA);
	z3->callback_add(&c3, TLL_MESSAGE_MASK_DATA);

	unsigned count = 100000000;

	prewarm(100ms);
	timeit(count, "plain-c", tll_channel_process, z0.get(), 0, 0);
	timeit(count, "c++-func", tll_channel_process, z1.get(), 0, 0);
	timeit(count, "c++-method", tll_channel_process, z2.get(), 0, 0);
	timeit(count, "c++-object", tll_channel_process, z3.get(), 0, 0);

	return 0;
}
