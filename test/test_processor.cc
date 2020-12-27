#include "tll/config.h"
#include "tll/processor.h"

#include <gtest/gtest.h>
#include <thread>

using namespace std::chrono_literals;
using namespace tll::state;

class Processor : public ::testing::Test
{
protected:
	virtual std::string_view config_data()
	{
		return R"(yamls://
processor.objects:
  base:
    url: null://
  null:
    url: null://
    depends: base
)";
	}

	virtual std::vector<std::string> worker_list() { return { "test/worker/default" }; }
	virtual tll::duration timeout() { return 1000ms; }

	tll::Logger log = { "test" };
	tll::Config config;
	tll::channel::Context context { tll::Config() };

	std::unique_ptr<tll::Processor> proc;

public:
	virtual void SetUp() override
	{
		auto cfg = tll::Config::load(config_data());

		ASSERT_TRUE(cfg);

		cfg->set("tll.proto", "processor");
		cfg->set("name", "test");

		config = *cfg;

		proc = tll::Processor::init(config, context);

		ASSERT_TRUE(proc);

		ASSERT_EQ(proc->open(), 0);

		auto wl = worker_list();
		ASSERT_EQ(proc->workers().size(), wl.size());
		for (auto i = 0u; i < wl.size(); i++)
			ASSERT_STREQ(proc->workers()[i]->name(), wl[i].c_str());

		for (auto & w : proc->workers())
			ASSERT_EQ(w->open(), 0);
	}

	virtual void TearDown() override
	{
		proc.reset();
	}

	template <typename F>
	void run(F f)
	{
		auto end = tll::time::now() + timeout();
		auto loop = proc->loop();
		while (!loop->stop) {
			loop->step(1us);
			for (auto & w : proc->workers()) {
				w->loop()->step(1us);
			}

			f();

			ASSERT_LE(tll::time::now(), end);
		}
	}
};

TEST_F(Processor, Basic)
{

	auto null = context.get("null");
	ASSERT_TRUE(null);
	ASSERT_EQ(null->state(), Closed);

	run([&null, this](){
		if (null->state() == Active && this->proc->state() == Active) {
			this->log.info("Close processor");
			this->proc->close();
		}

	});

	ASSERT_EQ(proc->state(), Closed);
}

TEST_F(Processor, Reopen)
{
	auto null = context.get("null");
	auto base = context.get("base");
	ASSERT_TRUE(null);
	ASSERT_TRUE(base);
	bool reopen = true;

	run([&null, &base, &reopen, this](){
		if (null->state() == Active && base->state() == Active && this->proc->state() == Active) {
			if (reopen) {
				this->log.info("Close {}", base->name());
				reopen = false;
				base->close();
			} else {
				this->log.info("Close processor");
				this->proc->close();
			}
		}
	});

	ASSERT_EQ(proc->state(), Closed);
}
