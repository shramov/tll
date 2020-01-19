#include "tll/processor/loop.h"

struct tll_processor_loop_t : public tll::processor::Loop {};

tll_processor_loop_t * tll_processor_loop_new()
{
	return new tll_processor_loop_t();
}

void tll_processor_loop_free(tll_processor_loop_t *loop)
{
	delete loop;
}

int tll_processor_loop_add(tll_processor_loop_t *loop, tll_channel_t *c)
{
	return loop->add((tll::Channel *) c);
}

int tll_processor_loop_del(tll_processor_loop_t *loop, const tll_channel_t *c)
{
	return loop->del((const tll::Channel *) c);
}

tll_channel_t * tll_processor_loop_poll(tll_processor_loop_t *loop, long timeout)
{
	return loop->poll(std::chrono::milliseconds(timeout));
}

int tll_processor_loop_process(tll_processor_loop_t *loop)
{
	return loop->process();
}
