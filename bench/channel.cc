#include <tll/channel.h>
#include <tll/logger.h>
#include <tll/util/argparse.h>

#include "timeit.h"

using namespace std::chrono;
int post(tll::Channel * c, tll_msg_t * msg)
{
	return c->post(msg);
}

int main(int argc, char *argv[])
{
	constexpr size_t count = 100000;
	tll::Logger::set("tll", tll::Logger::Warning, true);

	tll::util::ArgumentParser parser("url [--module=module]");

	std::string url = "null://;name=null";
	std::vector<std::string> modules;
	bool callback = false;

	parser.add_argument({"URL"}, "channel url", &url);
	parser.add_argument({"-m", "--module"}, "load channel modules", &modules);
	parser.add_argument({"-c", "--callback"}, "add callback", &callback);
	auto pr = parser.parse(argc, argv);
	if (!pr) {
		fmt::print("Invalid arguments: {}\nRun '{} --help' for more information\n", pr.error(), argv[0]);
		return 1;
	} else if (parser.help) {
		fmt::print("Usage {} {}\n", argv[0], parser.format_help());
		return 1;
	}

	auto ctx = tll::channel::Context(tll::Config());

	for (auto &m : modules) {
		if (ctx.load(m, "channel_module")) {
			fmt::print("Failed to load module {}\n", m);
			return 1;
		}
	}

	auto c = ctx.channel(url);
	if (!c)
		return -1;
	size_t counter = 0;
	if (callback) {
		fmt::print("Add callback\n");
		c->callback_add([](auto *c, auto *m, void *user) { ++*(size_t *)user; return 0; }, &counter);
	}

	if (c->open())
		return -1;

	for (auto i = 0; i < 10; i++) {
		if (c->state() != tll::state::Opening)
			break;
		c->process();
	}

	char data[1024] = {};
	tll_msg_t msg = {};
	msg.data = data;
	msg.size = 128;

	timeit(count, "post", post, c.get(), &msg);
	if (callback && !counter)
		fmt::print("Callback was added but not called\n");

	return 0;
}

