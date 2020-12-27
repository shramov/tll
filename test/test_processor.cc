#include "tll/config.h"
#include "tll/processor.h"

#include <gtest/gtest.h>
#include <thread>

using namespace std::chrono_literals;
using namespace tll::state;

TEST(Processor, Basic)
{
	tll::Logger log = { "test" };
	auto cfg = tll::Config::load(R"(yamls://
processor.objects:
  base:
    url: null://
  null:
    url: null://
    depends: base
)");
	ASSERT_TRUE(cfg);

	cfg->set("tll.proto", "processor");
	cfg->set("name", "test");

	std::list<std::thread> threads;

	auto context = tll::channel::Context(tll::Config());
	auto proc = tll::Processor::init(*cfg, context);

	ASSERT_TRUE(proc);

	ASSERT_EQ(proc->open(), 0);

	ASSERT_EQ(proc->workers().size(), 1u);
	ASSERT_STREQ(proc->workers()[0]->name(), "test/worker/default"); 

	auto null = context.get("null");
	ASSERT_TRUE(null);
	ASSERT_EQ(null->state(), Closed);

	for (auto & w : proc->workers()) {
		ASSERT_EQ(w->open(), 0);
	}

	auto start = tll::time::now();
	auto loop = proc->loop();
	while (!loop->stop) {
		loop->step(1us);
		for (auto & w : proc->workers()) {
			w->loop()->step(1us);
		}

		if (null->state() == Active && proc->state() == Active) {
			log.info("Close processor");
			proc->close();
		}

		ASSERT_LE(tll::time::now() - start, 10ms);
	}

	ASSERT_EQ(proc->state(), Closed);
}

TEST(Processor, Reopen)
{
	tll::Logger log = { "test" };
	auto cfg = tll::Config::load(R"(yamls://
processor.objects:
  base:
    url: null://
  null:
    url: null://
    depends: base
)");
	ASSERT_TRUE(cfg);

	cfg->set("tll.proto", "processor");
	cfg->set("name", "test");

	std::list<std::thread> threads;

	auto context = tll::channel::Context(tll::Config());
	auto proc = tll::Processor::init(*cfg, context);

	ASSERT_TRUE(proc);

	ASSERT_EQ(proc->open(), 0);

	ASSERT_EQ(proc->workers().size(), 1u);
	ASSERT_STREQ(proc->workers()[0]->name(), "test/worker/default"); 

	auto null = context.get("null");
	auto base = context.get("base");
	ASSERT_TRUE(null);
	ASSERT_TRUE(base);

	for (auto & w : proc->workers()) {
		ASSERT_EQ(w->open(), 0);
	}

	auto start = tll::time::now();
	bool reopen = true;

	auto loop = proc->loop();
	while (!loop->stop) {
		loop->step(1us);
		for (auto & w : proc->workers()) {
			w->loop()->step(1us);
		}

		if (null->state() == Active && base->state() == Active && proc->state() == Active) {
			if (reopen) {
				log.info("Close {}", base->name());
				reopen = false;
				base->close();
			} else {
				log.info("Close processor");
				proc->close();
			}
		}

		ASSERT_LE(tll::time::now() - start, 10ms);
	}

	ASSERT_EQ(proc->state(), Closed);
}
