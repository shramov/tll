/*
 * Copyright (c) 2019-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/channel.h"
#include "tll/logger.h"
#include "tll/processor.h"
#include "tll/util/argparse.h"

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
	tll::util::ArgumentParser parser("config [-Dkey=value]");
	std::string curl;
	std::vector<std::string> defs;
	parser.add_argument({"CONFIG"}, "configuration file", &curl);
	parser.add_argument({"-D"}, "extra configuration variables", &defs);
	auto pr = parser.parse(argc, argv);
	if (!pr) {
		printf("Invalid arguments: %s\nRun '%s --help' for more information\n", pr.error().c_str(), argv[0]);
		return 1;
	} else if (parser.help) {
		printf("Usage %s %s\n", argv[0], parser.format_help().c_str());
		return 1;
	}

	//std::string curl(argv[1]);
	if (curl.find("://") == curl.npos)
		curl = "yaml://" + curl;
	auto cfg = tll::Config::load(curl);
	if (!cfg) {
		printf("Failed to load config %s\n", argv[1]);
		return 1;
	}

	for (auto &d : defs) {
		auto sep = d.find('=');
		if (sep == d.npos) {
			printf("Invalid -D value: '%s'\n", d.c_str());
			return 1;
		}
		cfg->set(d.substr(0, sep), d.substr(sep + 1));
	}

	if (cfg->process_imports("processor.include")) {
		printf("Failed to process imports of %s\n", argv[1]);
		return 1;
	}

	auto logger = cfg->sub("logger");
	if (logger && tll_logger_config(*logger)) {
		printf("Failed to configure logger\n");
		return 1;
	}

	tll::channel::Context context(cfg->sub("processor.defaults").value_or(tll::Config()));

	std::unique_ptr<tll::Channel> loader;
	{
		tll::Channel::Url lurl;
		lurl.set("tll.proto", "loader");
		lurl.set("tll.internal", "yes");
		lurl.set("name", fmt::format("{}/loader", "processor"));
		if (auto mcfg = cfg->sub("processor.module"))
			lurl.set("module", mcfg->copy());
		if (auto acfg = cfg->sub("processor.alias"))
			lurl.set("alias", acfg->copy());
		loader = context.channel(lurl);
		if (!loader) {
			printf("Failed to load channel modules\n");
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
				proc->close(false);
		}
	}

	for (auto & t : threads)
		t.join();

	loader.reset();

	return 0;
}
