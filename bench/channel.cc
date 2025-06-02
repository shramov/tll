#include <tll/channel.h>
#include <tll/channel/prefix.h>
#include <tll/conv/base.h>
#include <tll/logger.h>
#include <tll/processor/loop.h>
#include <tll/util/argparse.h>
#include <tll/util/bench.h>
#include <tll/util/ownedmsg.h>

template <typename R, typename... Args>
[[nodiscard]]
R fail(R err, tll::logger::format_string<Args...> format, Args && ... args)
{
	fmt::print(format, std::forward<Args>(args)...);
	return err;
}

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

std::string bench_name(const tll::Channel::Url &cfg)
{
	auto name = cfg.get("bench-name");
	if (name && name->size())
		return std::string(*name);
	return tll::conv::to_string(cfg);
}

std::unique_ptr<tll::Channel> prepare(tll::channel::Context &ctx, const tll::Channel::Url &url, bool callback, size_t &counter)
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

int timeit_post(tll::channel::Context &ctx, const tll::Channel::Url &url, bool callback, unsigned count, const tll_msg_t *msg)
{
	size_t counter = 0;
	auto c = prepare(ctx, url, callback, counter);

	if (!c.get())
		return -1;

	timeit(count, bench_name(url), post, c.get(), msg);
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

int timeit_process(tll::channel::Context &ctx, const tll::Channel::Url &url, bool callback, unsigned count)
{
	size_t counter = 0;
	auto c = prepare(ctx, url, callback, counter);

	if (!c.get())
		return -1;

	auto list = process_list(c.get());
	if (list.size() == 0)
		return fail(0, "No channels to process for {}\n", tll::conv::to_string(url));

	if (list.size() != 1)
		return fail(-1, "Channel with several active childs\n");

	timeit(count, bench_name(url), tll_channel_process, list[0], 0, 0);
	if (callback && !counter)
		fmt::print("Callback was added but not called\n");
	return 0;
}

std::optional<tll::util::OwnedMessage> payload_read(tll::channel::Context &ctx, const tll::Channel::Url &url, std::string_view open)
{
	auto c = ctx.channel(url);
	if (!c)
		return fail(std::nullopt, "Failed to create payload channel {}\n", tll::conv::to_string(url));

	auto loop = tll::processor::Loop();
	if (auto r = loop.init(tll::Config()); r)
		return fail(std::nullopt, "Failed to init processor loop\n");

	loop.add(c.get());

	struct Callback
	{
		tll::util::OwnedMessage msg;

		int callback(const tll::Channel *, const tll_msg_t *m)
		{
			msg = m;
			return 0;
		}
	};
	Callback cb;
	c->callback_add(&cb, TLL_MESSAGE_MASK_DATA);
	c->open(open);
	for (auto i = 0; i < 10; i++) {
		loop.step(100us);
		if (cb.msg.size)
			break;
	}
	if (!cb.msg.size)
		return fail(std::nullopt, "Failed to read data from payload channel\n");

	return std::move(cb.msg);
}

int main(int argc, char *argv[])
{
	tll::util::ArgumentParser parser("url [--module=module]");

	std::vector<std::string> url;
	std::vector<tll::Channel::Url> curl;
	std::vector<std::string> modules;
	std::string payload_channel;
	std::string payload_open;
	std::string config_file;
	bool callback = false;
	bool process = false;
	unsigned count = 10000000;
	unsigned msgsize = 0;
	int msgid = 0;
	std::string loglevel = "warning";

	parser.add_argument({"URL"}, "channel url", &url);
	parser.add_argument({"--config"}, "read benchmark configuration from file", &config_file);
	parser.add_argument({"-m", "--module"}, "load channel modules", &modules);
	parser.add_argument({"-c", "--callback"}, "add callback", &callback);
	parser.add_argument({"--process"}, "run process benchmark", &process);
	parser.add_argument({"-C", "--count"}, "number of iterations", &count);
	parser.add_argument({"--msgid"}, "message id", &msgid);
	parser.add_argument({"--msgsize"}, "message size", &msgsize);
	parser.add_argument({"--payload"}, "read payload from channel", &payload_channel);
	parser.add_argument({"--payload-open"}, "open parameters for payload channel", &payload_open);
	parser.add_argument({"--loglevel"}, "set logging level: debug, info, warning", &loglevel);
	auto pr = parser.parse(argc, argv);
	if (!pr) {
		fmt::print("Invalid arguments: {}\nRun '{} --help' for more information\n", pr.error(), argv[0]);
		return 1;
	} else if (parser.help) {
		fmt::print("Usage {} {}\n", argv[0], parser.format_help());
		return 1;
	}

	{
		auto level = tll::conv::select<tll::Logger::level_t>(loglevel, {{"debug", tll::Logger::Debug}, {"info", tll::Logger::Info}, {"warning", tll::Logger::Warning}});
		tll::Logger::set("tll", level.value_or(tll::Logger::Warning), true);
	}

	auto ctx = tll::channel::Context(tll::Config());

	tll::util::OwnedMessage msg;

	for (auto &m : modules) {
		if (ctx.load(m))
			return fail(1, "Failed to load module {}\n", m);
	}

	if (config_file.size()) {
		auto cfg = tll::Config::load("yaml://" + config_file);
		if (!cfg)
			return fail(1, "Failed to load config {}\n", config_file);

		tll::Channel::Url lurl;
		lurl.set("tll.proto", "loader");
		lurl.set("tll.internal", "yes");
		lurl.set("name", "loader");
		if (auto mcfg = cfg->sub("module"))
			lurl.set("module", mcfg->copy());
		if (auto acfg = cfg->sub("alias"))
			lurl.set("alias", acfg->copy());
		if (!ctx.channel(lurl))
			return fail(1, "Failed to load channel modules\n");

		auto reader = tll::make_props_reader(*cfg);
		if (msgsize == 0)
			msgsize = reader.getT("msgsize", msgsize);
		if (msgid == 0)
			msgid = reader.getT("msgid", msgid);

		if (!reader)
			return fail(1, "Invalid config parameters: {}", reader.error());

		if (auto url = cfg->getT("payload", tll::Channel::Url()); url) {
			if (url->proto() != "") {
				auto open = *cfg->getT("payload-open", std::string(""));
				if (payload_open.size())
					open = payload_open;
				if (auto r = payload_read(ctx, *url, open); !r)
					return 1;
				else
					std::swap(msg, *r);
			}
		} else
			return fail(1, "Invalid payload url in config: {}", url.error());

		for (auto &[p, _] : cfg->browse("channel.*", true)) {
			auto r = cfg->getT<tll::Channel::Url>(p);
			if (!r)
				return fail(1, "Failed to load channel url from config: {}", r.error());
			curl.push_back(*r);
		}
	}

	if (payload_channel.size()) {
		auto url = tll::Channel::Url::parse(payload_channel);
		if (!url)
			return fail(1, "Failed to parse payload url {}\n", payload_channel);

		if (auto r = payload_read(ctx, *url, payload_open); !r)
			return 1;
		else
			std::swap(msg, *r);
	}

	if (!msgsize)
		msgsize = 1024;
	if (!msg.data)
		msg.resize(msgsize);
	if (msgid)
		msg.msgid = msgid;

	ctx.reg(&Echo::impl);
	ctx.reg(&Prefix::impl);

	if (url.empty() && curl.empty())
		url = {"null://", "prefix+null://", "echo://", "prefix+echo://"};

	for (auto & u : url) {
		auto r = tll::Channel::Url::parse(u);
		if (!r)
			return fail(1, "Invalid url '{}': {}\n", u, r.error());
		curl.push_back(*r);
	}
	tll::bench::prewarm(100ms);
	for (auto & u : curl) {
		if (process)
			timeit_process(ctx, u, callback, count);
		else
			timeit_post(ctx, u, callback, count, &msg);
	}

	return 0;
}
