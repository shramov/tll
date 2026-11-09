#include <tll/channel/base.h>
#include <tll/logger/prefix.h>

class Echo : public tll::channel::Base<Echo>
{
 public:
	static constexpr std::string_view channel_protocol() { return "echo"; }
	static constexpr auto process_policy() { return tll::channel::Base<Echo>::ProcessPolicy::Never; }

	int _post(const tll_msg_t *msg, int flags) { return _callback_data(msg); }
};

TLL_DEFINE_IMPL(Echo);

int main()
{
	{
		tll::Logger l { "l0" };
		auto p0 = l.prefix("f0");
		l.info("Prefix");
		p0.info("Prefix");
	}

	auto ctx = tll::channel::Context(tll::Config());
	ctx.reg(&Echo::impl);

	auto c = ctx.channel("echo://;name=echo;dump=frame");
	if (auto r = c->open(); r)
		return r;
	tll_msg_t m = {.msgid = 10, .data = "data", .size = 4};
	c->post(&m);
	return 0;
}
