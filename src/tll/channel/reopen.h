/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_REOPEN_H
#define _TLL_CHANNEL_REOPEN_H

#include "tll/channel/base.h"

#include "tll/scheme/channel/timer.h"

#include "tll/util/time.h"

namespace tll::channel {

struct ReopenData
{
	Channel * channel = nullptr;
	tll_state_t state = state::Closed;
	time_point next = {};
	time_point active_ts = {};

	duration timeout_min = std::chrono::seconds(1);
	duration timeout_max = std::chrono::seconds(30);
	duration timeout_tremble = std::chrono::milliseconds(1);

	bool reopen_wait = false;
	unsigned count = 0;
	ConstConfig open_params;

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
		switch (s) {
		case state::Opening:
			next = {};
			active_ts = {};
			if (timeout() < timeout_max)
				count++;
			break;
		case state::Active:
			active_ts = tll::time::now();
			break;
		case state::Error:
			if (state == state::Opening) {
				reopen_wait = true;
			} else if (state == state::Active) {
				if (tll::time::now() - active_ts < timeout_tremble)
					reopen_wait = true;
			}
			break;
		case state::Closing:
			if (state == state::Active) {
				if (tll::time::now() - active_ts < timeout_tremble)
					reopen_wait = true;
			}
			break;
		case state::Closed:
			if (reopen_wait) {
				next = tll::time::now() + timeout();
				reopen_wait = false;
			} else {
				next = tll::time::now();
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

	int open()
	{
		if (!channel) return EINVAL;
		tll::Props props;
		auto v = open_params.get();
		if (v) {
			auto r = Props::parse(*v);
			if (!r)
				return EINVAL;
			props = *r;
		}
		for (auto &[k, c] : open_params.browse("**")) {
			auto v = c.get();
			if (v)
				props[k] = *v;
		}
		return channel->open(conv::to_string(props));
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
			_reopen_data.timeout_min = reader.getT("reopen-timeout-min", _reopen_data.timeout_min);
			_reopen_data.timeout_max = reader.getT("reopen-timeout-max", _reopen_data.timeout_max);
			_reopen_data.timeout_tremble = reader.getT("reopen-active-min", _reopen_data.timeout_tremble);
			if (!reader)
				return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());
		}

		_reopen_timer = this->context().channel(fmt::format("timer://;clock=realtime;name={}/reopen-timer;tll.internal=yes", this->name));
		if (!_reopen_timer)
			return this->_log.fail(EINVAL, "Failed to create timer channel");
		_reopen_timer->callback_add([](auto * c, auto * m, void * user) { return static_cast<Reopen *>(user)->_reopen_timer_cb(m); }, this, TLL_MESSAGE_MASK_DATA);
		this->_child_add(_reopen_timer.get(), "reopen-timer");

		return Base::_init(url, master);
	}

	void _reopen_reset(tll::Channel * channel)
	{
		if (_reopen_data.channel)
			_reopen_data.channel->callback_del(_reopen_cb_static, this, TLL_MESSAGE_MASK_STATE);
		_reopen_data.channel = channel;
		if (_reopen_data.channel)
			_reopen_data.channel->callback_add(_reopen_cb_static, this, TLL_MESSAGE_MASK_STATE);
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

	int _open(const tll::PropsView &params)
	{
		_reopen_timer->open();
		if (T::reopen_open_policy() && _reopen_data.channel) {
			Config cfg;
			for (auto & [k, v]: params)
				cfg.set(k, v);
			_reopen_data.open_params = cfg;
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
	static int _reopen_cb_static(const tll_channel_t *, const tll_msg_t *msg, void * user)
	{
		if (msg->type != TLL_MESSAGE_STATE)
			return 0;
		return static_cast<Reopen *>(user)->_reopen_cb(msg);
	}

	int _reopen_cb(const tll_msg_t *msg)
	{
		using namespace std::chrono_literals;
		auto state = (tll_state_t) msg->msgid;
		_reopen_data.on_state(state);
		if (state == state::Error) { // Async close
			this->_log.debug("Channel failed, schedule close");
			_reopen_rearm(tll::time::now());
		} else if (_reopen_data.next != tll::time::epoch) {
			_reopen_rearm(_reopen_data.next);
		}
		return 0;
	}

	int _reopen_timer_cb(const tll_msg_t *msg)
	{
		if (!_reopen_data.channel)
			return 0;
		if (_reopen_data.state == state::Error)
			_reopen_data.channel->close();
		else if (_reopen_data.state == state::Closed && tll::time::now() >= _reopen_data.next)
			_reopen_data.open();
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

#endif//_TLL_CHANNEL_REOPEN_H
