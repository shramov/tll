/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "processor/chains.h"
#include "processor/processor.h"
#include "processor/worker.h"
#include "tll/channel/impl.h"

using tll::processor::_::Processor;

TLL_DEFINE_IMPL(tll::processor::_::Processor);
TLL_DEFINE_IMPL(tll::processor::_::Worker);
TLL_DEFINE_IMPL(tll::processor::_::Chains);

int tll_processor_init(tll_channel_context_t *ctx)
{
	tll_channel_impl_register(ctx, &Processor::impl, 0);
	tll_channel_impl_register(ctx, &tll::processor::_::Chains::impl, 0);
	return 0;
}

tll_channel_list_t * tll_processor_workers(tll_processor_t *p)
{
	return tll::channel_cast<Processor>(p)->self()->children()->next->next;
}

tll_processor_loop_t * tll_processor_loop(tll_processor_t *p)
{
	return &tll::channel_cast<Processor>(p)->loop;
}

tll_processor_loop_t * tll_processor_worker_loop(tll_processor_worker_t *w)
{
	return &tll::channel_cast<tll::processor::_::Worker>(w)->loop;
}

int tll_processor_run(tll_processor_t *p)
{
	return tll_processor_loop(p)->run();
}

int tll_processor_step(tll_processor_t *p, long timeout)
{
	return tll_processor_loop(p)->step(std::chrono::milliseconds(timeout));
}

int tll_processor_worker_run(tll_processor_worker_t *w)
{
	return tll_processor_worker_loop(w)->run();
}

int tll_processor_worker_step(tll_processor_worker_t *w, long timeout)
{
	return tll_processor_worker_loop(w)->step(std::chrono::milliseconds(timeout));
}
