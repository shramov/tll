#include <tll/channel.h>
#include <tll/logger.h>
#include <tll/util/argparse.h>
#include <tll/util/bench.h>

#include <tll/channel/prefix.h>

using namespace tll::bench;

class Echo : public tll::channel::Base<Echo>
{
 public:
	static constexpr std::string_view channel_protocol() { return "echo"; }
	static constexpr auto process_policy() { return tll::channel::Base<Echo>::ProcessPolicy::Never; }

	int _post(const tll_msg_t *msg, int flags) { return _callback_data(msg); }
};

TLL_DEFINE_IMPL(Echo);
class Prefix : public tll::channel::Prefix<Prefix>
{
 public:
	static constexpr std::string_view channel_protocol() { return "prefix+"; }
};
TLL_DEFINE_IMPL(Prefix);

using namespace std::chrono;
int post(tll::Channel * c, const tll_msg_t * msg)
{
	return c->post(msg);
}

std::unique_ptr<tll::Channel> prepare(tll::channel::Context &ctx, std::string_view url, bool callback, size_t &counter)
{
	auto c = ctx.channel(url);
	if (!c)
		return nullptr;

	if (callback) {
		//fmt::print("Add callback\n");
		c->callback_add([](auto *c, auto *m, void *user) { ++*(size_t *)user; return 0; }, &counter);
	}

	if (c->open()) {
		fmt::print("Failed to open channel\n");
		return nullptr;
	}

	for (auto i = 0; i < 10; i++) {
		if (c->state() != tll::state::Opening)
			break;
		c->process();
	}

	if (c->state() != tll::state::Active) {
		fmt::print("Failed to open channel\n");
		return nullptr;
	}
	return c;
}

int timeit_post(tll::channel::Context &ctx, std::string_view url, bool callback, unsigned count, const tll_msg_t *msg)
{
	size_t counter = 0;
	auto c = prepare(ctx, url, callback, counter);

	if (!c.get())
		return -1;

	timeit(count, url, post, c.get(), msg);
	if (callback && !counter)
		fmt::print("Callback was added but not called\n");
	return 0;
}

std::vector<tll::Channel *> process_list(tll::Channel * c)
{
	std::vector<tll::Channel *> r;
	if (c->dcaps() & tll::dcaps::Process)
		r.push_back(c);
	for (auto ptr = c->children(); ptr; ptr = ptr->next) {
		for (auto & i : process_list(static_cast<tll::Channel *>(ptr->channel)))
			r.push_back(i);
	}
	return r;
}

int timeit_process(tll::channel::Context &ctx, std::string_view url, bool callback, unsigned count)
{
	size_t counter = 0;
	auto c = prepare(ctx, url, callback, counter);

	if (!c.get())
		return -1;

	auto list = process_list(c.get());
	if (list.size() == 0) {
		fmt::print("No channels to process for {}\n", url);
		return 0;
	}
	if (list.size() != 1) {
		fmt::print("Channel with several active childs\n");
		return -1;
	}

	timeit(count, url, tll_channel_process, list[0], 0, 0);
	if (callback && !counter)
		fmt::print("Callback was added but not called\n");
	return 0;
}

int main(int argc, char *argv[])
{
	tll::Logger::set("tll", tll::Logger::Warning, true);

	tll::util::ArgumentParser parser("url [--module=module]");

	std::vector<std::string> url;
	std::vector<std::string> modules;
	bool callback = false;
	bool process = false;
	unsigned count = 10000000;
	unsigned msgsize = 1024;
	tll_msg_t msg = { TLL_MESSAGE_DATA };

	parser.add_argument({"URL"}, "channel url", &url);
	parser.add_argument({"-m", "--module"}, "load channel modules", &modules);
	parser.add_argument({"-c", "--callback"}, "add callback", &callback);
	parser.add_argument({"--process"}, "run process benchmark", &process);
	parser.add_argument({"-C", "--count"}, "number of iterations", &count);
	parser.add_argument({"--msgid"}, "message id", &msg.msgid);
	parser.add_argument({"--msgsize"}, "message size", &msgsize);
	auto pr = parser.parse(argc, argv);
	if (!pr) {
		fmt::print("Invalid arguments: {}\nRun '{} --help' for more information\n", pr.error(), argv[0]);
		return 1;
	} else if (parser.help) {
		fmt::print("Usage {} {}\n", argv[0], parser.format_help());
		return 1;
	}

	std::vector<char> buf;
	buf.resize(msgsize);
	msg.data = buf.data();
	msg.size = msgsize;

	auto ctx = tll::channel::Context(tll::Config());

	for (auto &m : modules) {
		if (ctx.load(m, "channel_module")) {
			fmt::print("Failed to load module {}\n", m);
			return 1;
		}
	}
	ctx.reg(&Echo::impl);
	ctx.reg(&Prefix::impl);

	if (url.empty())
		url = {"null://", "prefix+null://", "echo://", "prefix+echo://"};

	timeit_post(ctx, "null://;name=prewarm", true, count, &msg);
	for (auto & u : url) {
		if (process)
			timeit_process(ctx, u, callback, count);
		else
			timeit_post(ctx, u, callback, count, &msg);
	}

	return 0;
}
