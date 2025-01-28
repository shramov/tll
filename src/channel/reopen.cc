#include "tll/channel/reopen.h"

struct tll_channel_reopen_t : tll::channel::ReopenData {};

tll_channel_reopen_t * tll_channel_reopen_new(const tll_config_t *cptr)
{
	std::unique_ptr<tll_channel_reopen_t> r {new tll_channel_reopen_t};
	if (cptr) {
		tll::ConstConfig cfg(cptr);
		auto reader = tll::make_props_reader(cfg);
		if (r->init(reader))
			return nullptr;
	}
	return r.release();
}

void tll_channel_reopen_free(tll_channel_reopen_t *ptr)
{
	if (!ptr)
		return;
	delete ptr;
}

long long tll_channel_reopen_next(tll_channel_reopen_t *ptr)
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(ptr->next.time_since_epoch()).count();
}

tll_channel_reopen_action_t tll_channel_reopen_on_timer(tll_channel_reopen_t *ptr, tll_logger_t * log, long long now)
{
	tll::Logger logger(log);
	return (tll_channel_reopen_action_t) ptr->on_timer(logger, tll::time_point { std::chrono::nanoseconds { now } });
}

void tll_channel_reopen_on_state(tll_channel_reopen_t *ptr, tll_state_t state)
{
	ptr->on_state(state);
}

tll_channel_t * tll_channel_reopen_set_channel(tll_channel_reopen_t * ptr, tll_channel_t * channel)
{
	auto tmp = ptr->channel;
	ptr->channel = static_cast<tll::Channel *>(channel);
	return tmp;
}

void tll_channel_reopen_set_open_config(tll_channel_reopen_t * ptr, const tll_config_t * cfg)
{
	if (cfg)
		ptr->open_params = tll::ConstConfig(cfg);
	else
		ptr->open_params = tll::ConstConfig();
}

int tll_channel_reopen_open(tll_channel_reopen_t * ptr)
{
	return ptr->open();
}

void tll_channel_reopen_close(tll_channel_reopen_t * ptr)
{
	if (ptr && ptr->channel)
		ptr->channel->close();
}
