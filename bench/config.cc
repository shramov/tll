#include <tll/config.h>
#include <tll/logger.h>
#include <tll/util/bench.h>

using namespace tll::bench;
using namespace std::chrono_literals;

int main(int argc, char *argv[])
{
	tll::Logger::set("tll", tll::Logger::Warning, true);

	tll::Config cfg;
	cfg.set("root", "value");
	cfg.set("ff.a", "yes");
	cfg.set("ff.b", "no");
	cfg.set("ff.int", "100");

	unsigned count = 100000;

	prewarm(100ms);
	timeit(count, "sub", [](tll::Config &cfg) { return cfg.sub("ff") ? 0 : 1; }, std::ref(cfg));
	timeit(count, "sub(miss)", [](tll::Config &cfg) { return cfg.sub("miss") ? 0 : 1; }, std::ref(cfg));
	timeit(count, "get(root)", [](tll::Config &cfg) { return cfg.get("root") ? 0 : 1; }, std::ref(cfg));
	timeit(count, "get(ff.a)", [](tll::Config &cfg) { return cfg.get("ff.a") ? 0 : 1; }, std::ref(cfg));
	timeit(count, "get<bool>", [](tll::Config &cfg) { return cfg.getT("ff.a", true).value_or(false) ? 0 : 1; }, std::ref(cfg));
	timeit(count, "get<int>", [](tll::Config &cfg) { return cfg.getT("ff.int", 0).value_or(0); }, std::ref(cfg));

	return 0;
}

