/*
 * Copyright (c) 2023 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_IMPL_CHANNEL_RESOLVE_H
#define _TLL_IMPL_CHANNEL_RESOLVE_H

#include "tll/channel/convert-buf.h"
#include "tll/channel/prefix.h"

namespace tll::channel {

class Resolve : public tll::channel::Prefix<Resolve>
{
	using Base = Prefix<Resolve>;

	std::unique_ptr<Channel> _request;

	std::vector<char> _request_buf;
	tll::ConstConfig _open_cfg;
	tll::ConstConfig _resolve_init_cfg;

	ConvertBuf _convert_into;
	ConvertBuf _convert_from;

	enum RequestMode { Once, Always } _request_mode = Once;
	enum class State { Closed, Opening, Active, Closing } _state = State::Closed;
 public:
	static constexpr std::string_view channel_protocol() { return "resolve"; }
	static constexpr auto child_policy() { return ChildPolicy::Proxy; }

	static constexpr auto prefix_scheme_policy() { return Base::PrefixSchemePolicy::Override; }
	static constexpr auto prefix_active_policy() { return Base::PrefixActivePolicy::Manual; }

	int _init(const tll::Channel::Url &url, tll::Channel *master);
	void _free()
	{
		_request.reset();
		return Base::_free();
	}

	int _open(const tll::ConstConfig &cfg)
	{
		if (_child && _request_mode == Once)
			return Base::_open(cfg);
		_state = State::Opening;
		_open_cfg = cfg;
		_request->open();
		return 0;
	}

	int _close(bool force)
	{
		_state = State::Closing;
		if (_request->state() != tll::state::Closed)
			_request->close(true);
		if (_child && _child->state() != tll::state::Closed)
			return Base::_close(force);
		state(tll::state::Closed);
		return 0;
	}

	int _on_active();
	int _on_closed()
	{
		_convert_from.reset();
		_convert_into.reset();
		_state = State::Closed;
		return Base::_on_closed();
	}

	int _on_data(const tll_msg_t *msg)
	{
		if (_convert_from.scheme_from) {
			if (auto m = _convert_from.convert(msg); m) {
				if (*m)
					return _callback_data(*m);
				return 0;
			}
			return _log.fail(EINVAL, "Failed to convert message {} at {}: {}", msg->msgid, _convert_from.format_stack(), _convert_from.error);
		}
		return _callback_data(msg);
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		if (msg->type != TLL_MESSAGE_DATA)
			return _child->post(msg, flags);

		if (_convert_into.scheme_from) {
			if (auto m = _convert_into.convert(msg); m) {
				if (*m)
					return _child->post(*m, flags);
				return 0;
			}
			return _log.fail(EINVAL, "Failed to convert message {} at {}: {}", msg->msgid, _convert_into.format_stack(), _convert_into.error);
		}
		return _child->post(msg, flags);
	}
 private:
	static int _on_request(const tll_channel_t *c, const tll_msg_t *msg, void * user)
	{
		switch (msg->type) {
		case TLL_MESSAGE_STATE:
			return static_cast<Resolve *>(user)->_on_request_state(msg);
		case TLL_MESSAGE_DATA:
			return static_cast<Resolve *>(user)->_on_request_data(msg);
		default:
			break;
		}
		return 0;
	}

	int _on_request_state(const tll_msg_t *msg)
	{
		if (_state != State::Opening)
			return 0;
		switch ((tll_state_t) msg->msgid) {
		case tll::state::Active:
			if (_on_request_active())
				return state_fail(0, "Failed to request channel parameters");
			return 0;
		case tll::state::Error:
			return state_fail(0, "Request channel failed");
		case tll::state::Closed:
			switch (state()) {
			case tll::state::Opening:
			case tll::state::Active:
				return state_fail(0, "Request channel closed before resolve finished");
			default:
				return 0;
			}
		default:
			return 0;
		}
	}

	int _on_request_data(const tll_msg_t *msg);
	int _on_request_active();
};

} // namespace tll::channel

#endif//_TLL_IMPL_CHANNEL_RESOLVE_H
