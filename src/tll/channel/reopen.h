/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_REOPEN_H
#define _TLL_CHANNEL_REOPEN_H

#ifdef __cplusplus

#include "tll/channel/base.h"
#include "tll/scheme/channel/timer.h"
#include "tll/util/time.h"

namespace tll::channel {

struct ReopenData
{
	enum class Action { None, Open, Close };

	Channel * channel = nullptr;
	tll_state_t state = state::Closed;
	time_point next = {};
	time_point active_ts = {};

	duration timeout_open = std::chrono::seconds(300);
	duration timeout_min = std::chrono::seconds(1);
	duration timeout_max = std::chrono::seconds(30);
	duration timeout_tremble = std::chrono::milliseconds(1);

	bool reopen_wait = false;
	unsigned count = 0;
	ConstConfig open_params;

	bool pending() const { return next != time_point {}; }

	template <typename Reader>
	int init(Reader &reader)
	{
		timeout_open = reader.getT("open-timeout", timeout_open);
		timeout_min = reader.getT("reopen-timeout-min", timeout_min);
		timeout_max = reader.getT("reopen-timeout-max", timeout_max);
		timeout_tremble = reader.getT("reopen-active-min", timeout_tremble);
		return reader ? 0 : 1;
	}

	void reset()
	{
		count = 0;
		next = {};
		state = state::Closed;

		if (channel)
			state = channel->state();
	}

	duration timeout()
	{
		if (count == 0)
			return {};
		auto r = timeout_min * (1ll << (count - 1));
		return std::min(timeout_max, r);
	}

	void on_state(tll_state_t s)
	{
		return on_state(s, tll::time::now());
	}

	void on_state(tll_state_t s, tll::time_point now)
	{
		next = {};
		switch (s) {
		case state::Opening:
			next = {};
			active_ts = {};
			if (timeout() < timeout_max)
				count++;
			if (timeout_open > duration {0})
				next = now + timeout_open;
			break;
		case state::Active:
			active_ts = now;
			break;
		case state::Error:
			if (state == state::Opening) {
				reopen_wait = true;
			} else if (state == state::Active) {
				if (now - active_ts < timeout_tremble)
					reopen_wait = true;
			}
			next = now;
			break;
		case state::Closing:
			if (state == state::Active) {
				if (now - active_ts < timeout_tremble)
					reopen_wait = true;
			}
			break;
		case state::Closed:
			if (reopen_wait) {
				next = now + timeout();
				reopen_wait = false;
			} else {
				next = now;
				count = 0;
			}
			break;
		case state::Destroy:
			channel = nullptr;
			reset();
			break;
		default:
			break;
		}
		state = s;
	}

	Action on_timer(tll::Logger &log, tll::time_point now) const
	{
		if (state == state::Error)
			return Action::Close;
		else if (state == state::Closed && now >= next)
			return Action::Open;
		else if (state == state::Opening && now >= next) {
			log.warning("Open timeout for channel {}", channel->name());
			return Action::Close;
		}
		return Action::None;
	}

	int open()
	{
		if (!channel) return EINVAL;
		return channel->open(open_params);
	}
};

/// Channel mixin that handles lifecycle of child channel
template <typename T, typename S = Base<T>>
class Reopen : public S
{
protected:
	ReopenData _reopen_data = {};
	std::unique_ptr<Channel> _reopen_timer;

public:
	using Base = S;

	/// Read parameters from url or not
	static constexpr bool reopen_init_policy() { return true; }
	/// Open channel in channel open
	static constexpr bool reopen_open_policy() { return true; }

	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{
		if constexpr (T::reopen_init_policy()) {
			auto reader = this->channelT()->channel_props_reader(url);
			if (_reopen_data.init(reader))
				return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());
		}

		auto curl = this->child_url_parse("timer://;clock=realtime", "reopen-timer");
		if (!curl)
			return this->_log.fail(EINVAL, "Failed to parse timer url: {}", curl.error());
		_reopen_timer = this->context().channel(*curl);
		if (!_reopen_timer)
			return this->_log.fail(EINVAL, "Failed to create timer channel");
		_reopen_timer->callback_add([](auto * c, auto * m, void * user) { return static_cast<Reopen *>(user)->_reopen_timer_cb(m); }, this, TLL_MESSAGE_MASK_DATA);
		this->_child_add(_reopen_timer.get(), "reopen-timer");

		return Base::_init(url, master);
	}

	void _reopen_reset(tll::Channel * channel)
	{
		if (_reopen_data.channel)
			_reopen_data.channel->callback_del<Reopen, &Reopen::_reopen_cb>(this, TLL_MESSAGE_MASK_STATE);
		_reopen_data.channel = channel;
		if (_reopen_data.channel)
			_reopen_data.channel->callback_add<Reopen, &Reopen::_reopen_cb>(this, TLL_MESSAGE_MASK_STATE);
		_reopen_data.reset();
		return;
	}

	void _free()
	{
		_reopen_reset(nullptr);
		_reopen_data = {};
		_reopen_timer.reset();
		return Base::_free();
	}

	int _open(const tll::ConstConfig &params)
	{
		_reopen_timer->open();
		if (T::reopen_open_policy() && _reopen_data.channel) {
			_reopen_data.open_params = params.copy();
			if (_reopen_data.open())
				return this->_log.fail(EINVAL, "Failed to open channel");
		}
		return Base::_open(params);
	}

	int _close(bool force = false)
	{
		_reopen_timer->close();
		if (_reopen_data.channel)
			_reopen_data.channel->close(force);
		return Base::_close(force);
	}

 private:
	int _reopen_cb(const tll::Channel *, const tll_msg_t *msg)
	{
		using namespace std::chrono_literals;
		auto state = (tll_state_t) msg->msgid;
		_reopen_data.on_state(state);
		if (this->state() != state::Active && this->state() != state::Opening)
			return 0;
		if (_reopen_data.pending()) {
			_reopen_rearm(_reopen_data.next);
		}
		return 0;
	}

	int _reopen_timer_cb(const tll_msg_t *msg)
	{
		if (!_reopen_data.channel)
			return 0;
		switch (_reopen_data.on_timer(this->_log, tll::time::now())) {
		case ReopenData::Action::Open:
			_reopen_data.open();
			break;
		case ReopenData::Action::Close:
			_reopen_data.channel->close();
			break;
		case ReopenData::Action::None:
			break;
		}
		return 0;
	}

	int _reopen_rearm(const tll::time_point &ts)
	{
		timer_scheme::absolute m = { ts };
		tll_msg_t msg = {};
		msg.type = TLL_MESSAGE_DATA;
		msg.msgid = m.id;
		msg.data = &m;
		msg.size = sizeof(m);
		if (_reopen_timer->post(&msg))
			return this->_log.fail(EINVAL, "Failed to rearm timer");
		return 0;
	}
};

} // namespace tll::channel

#endif//__cplusplus

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

typedef enum tll_channel_reopen_action_t {
	TLL_CHANNEL_REOPEN_NONE,
	TLL_CHANNEL_REOPEN_OPEN,
	TLL_CHANNEL_REOPEN_CLOSE,
} tll_channel_reopen_action_t;

typedef struct tll_channel_reopen_t tll_channel_reopen_t;
tll_channel_reopen_t * tll_channel_reopen_new(const tll_config_t *);
void tll_channel_reopen_free(tll_channel_reopen_t *);
long long tll_channel_reopen_next(tll_channel_reopen_t *);
tll_channel_reopen_action_t tll_channel_reopen_on_timer(tll_channel_reopen_t *, tll_logger_t *, long long now);
void tll_channel_reopen_on_state(tll_channel_reopen_t *, tll_state_t);
tll_channel_t * tll_channel_reopen_set_channel(tll_channel_reopen_t *, tll_channel_t *);
void tll_channel_reopen_set_open_config(tll_channel_reopen_t *, const tll_config_t *);
int tll_channel_reopen_open(tll_channel_reopen_t *);
void tll_channel_reopen_close(tll_channel_reopen_t *);

#ifdef __cplusplus
} // extern "C"
#endif //__cplusplus

#endif//_TLL_CHANNEL_REOPEN_H
