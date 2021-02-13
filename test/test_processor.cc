#include "tll/channel/base.h"
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
	virtual int prepare() { return 0; }

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

		ASSERT_EQ(prepare(), 0);

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

class Long : public tll::channel::Base<Long>
{
 public:
	static constexpr std::string_view param_prefix() { return "long"; }
	static constexpr auto open_policy() { return OpenPolicy::Manual; }
	static constexpr auto close_policy() { return ClosePolicy::Long; }

	int _open(const tll::PropsView &)
	{
		_dcaps_pending(true);
		return 0;
	}
	int _close(bool force) { return 0; }

	int _process(long timeout, int flags)
	{
		if (state() == tll::state::Opening) {
			_log.info("Long open finished");
			state(tll::state::Active);
			return 0;
		}
		if (state() == tll::state::Closing) {
			_log.info("Long close finished");
			return tll::channel::Base<Long>::_close();
		}
		return EAGAIN;
	}
};

TLL_DEFINE_IMPL(Long);

class ProcessorLong : public Processor
{
public:
	virtual std::string_view config_data()
	{
		return R"(yamls://
processor.objects:
  base:
    url: long://
  null:
    url: null://
    depends: base
)";
	}

	virtual int prepare()
	{
		return context.reg(&Long::impl);
	}
};

TEST_F(ProcessorLong, Test)
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

class ProcessorOrder : public Processor
{
public:
	virtual std::string_view config_data()
	{
		return R"(yamls://
processor.objects:
  a:
    url: mem://;master=z
  b:
    url: null://
    channels.input: z
  z:
    url: mem://
)";
	}
};

TEST_F(ProcessorOrder, Test)
{
}
