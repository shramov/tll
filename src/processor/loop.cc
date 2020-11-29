/*
 * Copyright (c) 2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/processor/loop.h"
#include "tll/util/string.h"

tll_processor_loop_t * tll_processor_loop_new(const char * name, int len)
{
	return new tll_processor_loop_t(tll::string_view_from_c(name, len));
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

int tll_processor_loop_pending(tll_processor_loop_t *loop)
{
	return loop->pending();
}

int tll_processor_loop_step(tll_processor_loop_t *loop, long timeout)
{
	return loop->step(std::chrono::milliseconds(timeout));
}

int tll_processor_loop_run(tll_processor_loop_t *loop, long timeout)
{
	return loop->run(std::chrono::milliseconds(timeout));
}

int tll_processor_loop_stop_get(const tll_processor_loop_t *loop)
{
	return loop->stop;
}

int tll_processor_loop_stop_set(tll_processor_loop_t *loop, int flag)
{
	std::swap(flag, loop->stop);
	return flag;
}
