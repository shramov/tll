/*
 * Copyright (c) 2019-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/channel.h"
#include "tll/logger.h"
#include "tll/processor.h"

#include <signal.h>
#include <stdio.h>

#include <atomic>
#include <thread>
#include <list>

static std::atomic<int> counter = {};

static void handler(int, siginfo_t *sig, void *)
{
	counter += 1;
}

int main(int argc, char *argv[])
{
	if (argc == 1) {
		printf("Usage %s config\n", argv[0]);
		return 1;
	}

	std::string curl(argv[1]);
	if (curl.find("://") == curl.npos)
		curl = "yaml://" + curl;
	auto cfg = tll::Config::load(curl);
	if (!cfg) {
		printf("Failed to load config %s\n", argv[1]);
		return 1;
	}

	if (cfg->process_imports("processor.include")) {
		printf("Failed to process imports of %s\n", argv[1]);
		return 1;
	}

	auto levels = cfg->sub("logger.levels");
	if (levels)
		tll_logger_config(*levels);

	tll::channel::Context context(cfg->sub("processor.defaults").value_or(tll::Config()));

	std::unique_ptr<tll::Channel> loader;
	if (auto mcfg = cfg->sub("processor.module")) {
		tll::Channel::Url lurl;
		lurl.set("tll.proto", "loader");
		lurl.set("tll.internal", "yes");
		lurl.set("name", fmt::format("{}/loader", "processor"));
		lurl.set("module", mcfg->copy());
		loader = context.channel(lurl);
		if (!loader) {
			printf("Failed to load channel modules");
			return 1;
		}
	}

	cfg->set("name", "processor");
	if (auto ppp = cfg->get("processor.format"))
		cfg->set("tll.proto", fmt::format("{}+{}", *ppp, "processor"));
	else
		cfg->set("tll.proto", "processor");

	auto proc = tll::Processor::init(*cfg, context);
	if (!proc) {
		printf("Failed to init processor\n");
		return 1;
	}

	std::list<std::thread> threads;

	if (proc->open()) {
		printf("Failed to open processor\n");
		return 1;
	}

	struct sigaction sa = {};
	sa.sa_sigaction = handler;
	sa.sa_flags = SA_SIGINFO;

	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);

	for (auto & w : proc->workers())
		threads.push_back(
			std::thread([](auto ptr) {
				if (ptr->open()) return;
				ptr->loop()->run();
			}, w));

	auto loop = proc->loop();
	while (!loop->stop) {
		using namespace std::chrono_literals;
		loop->step(100ms);
		if (counter) {
			counter = 0;
			if (proc->state() == tll::state::Opening || proc->state() == tll::state::Active)
				proc->close();
		}
	}

	for (auto & t : threads)
		t.join();

	return 0;
}
