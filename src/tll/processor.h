/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_PROCESSOR_H
#define _TLL_PROCESSOR_H

#include "tll/channel.h"

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

typedef struct tll_channel_t tll_processor_worker_t;
typedef struct tll_channel_t tll_processor_t;

struct tll_processor_loop_t;

int tll_processor_init(tll_channel_context_t * cctx);

tll_channel_list_t * tll_processor_workers(tll_processor_t *p);

struct tll_processor_loop_t * tll_processor_loop(tll_processor_t *p);
struct tll_processor_loop_t * tll_processor_worker_loop(tll_processor_worker_t *w);

#ifdef __cplusplus
} // extern "C"
#endif//__cplusplus


#ifdef __cplusplus

#include <vector>
#include <memory>

#include "tll/processor/loop.h"

namespace tll {
namespace processor {
class Worker : public tll::Channel
{
	Worker() = delete;

 public:
	tll::processor::Loop * loop() { return tll_processor_worker_loop(this); }
};
}

class Processor : public tll::Channel
{
 public:
	static std::unique_ptr<Processor> init(Config cfg, tll::channel::Context &ctx)
	{
		if (tll_processor_init(ctx))
			return {};

		auto impl = ctx.impl_get("processor");
		if (cfg.has("tll.proto"))
			impl = nullptr;

		auto ptr = ctx.channel(cfg, nullptr, impl);
		if (!ptr) return {};
		return std::unique_ptr<Processor>(static_cast<Processor *>(ptr.release()));
	}

	tll::processor::Loop * loop() { return tll_processor_loop(this); }

	std::vector<processor::Worker *> workers()
	{
		std::vector<processor::Worker *> r;
		for (auto i = tll_processor_workers(this); i; i = i->next) // Skip first child
			r.push_back(static_cast<processor::Worker *>(i->channel));
		return r;
	}
};

} // namespasce tll
#endif

#endif//_TLL_PROCESSOR_H
