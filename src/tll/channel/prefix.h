/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_PREFIX_H
#define _TLL_CHANNEL_PREFIX_H

#include "tll/channel/base.h"

namespace tll::channel {

template <typename T>
class Prefix : public Base<T>
{
protected:
	std::unique_ptr<Channel> _child;
public:

	static constexpr bool impl_prefix_channel() { return true; }
	static constexpr auto open_policy() { return Base<T>::OpenPolicy::Manual; }
	static constexpr auto child_policy() { return Base<T>::ChildPolicy::Single; }
	static constexpr auto close_policy() { return Base<T>::ClosePolicy::Long; }
	static constexpr auto process_policy() { return Base<T>::ProcessPolicy::Never; }

	const Scheme * scheme(int type) const
	{
		this->_log.debug("Request scheme {}", type);
		return _child->scheme(type);
	}

	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{

		auto proto = url.proto();
		auto sep = proto.find("+");
		if (sep == proto.npos)
			return this->_log.fail(EINVAL, "Invalid url proto '{}': no + found", proto);
		auto pproto = proto.substr(0, sep);

		tll::Channel::Url curl = url.copy();
		curl.proto(proto.substr(sep + 1));
		curl.host(url.host());
		curl.set("name", fmt::format("{}/{}", this->name, pproto));
		curl.set("tll.internal", "yes");

		/*
		auto sub = curl.sub("sub");
		if (sub) {
			curl.detach(sub);
			curl.merge(sub, true);
		}
		*/

		_child = this->context().channel(curl, master);
		if (!_child)
			return this->_log.fail(EINVAL, "Failed to create child channel");
		_child->callback_add(this);
		this->_child_add(_child.get(), proto);

		return Base<T>::_init(url, master);
	}

	void _free()
	{
		_child.reset();
		return Base<T>::_free();
	}

	int _open(const tll::PropsView &params)
	{
		return _child->open(conv::to_string(params));
	}

	int _close(bool force)
	{
		return _child->close(force);
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		return _child->post(msg, flags);
	}

	int callback(const Channel * c, const tll_msg_t *msg)
	{
		if (msg->type == TLL_MESSAGE_DATA)
			return this->channelT()->prefix_data(msg);
		else if (msg->type == TLL_MESSAGE_STATE)
			return this->channelT()->prefix_state(msg);
		return this->channelT()->prefix_other(msg);
	}

	int prefix_data(const tll_msg_t *msg)
	{
		return this->_callback_data(msg);
	}

	int prefix_state(const tll_msg_t *msg)
	{
		auto s = (tll_state_t) msg->msgid;
		switch (s) {
		case tll::state::Active:
			if (this->channelT()->_on_active()) {
				this->state(tll::state::Error);
				return 0;
			}
			break;
		case tll::state::Error:
			return this->channelT()->_on_error();
		case tll::state::Closing:
			return this->channelT()->_on_closing();
		case tll::state::Closed:
			return this->channelT()->_on_closed();
		default:
			break;
		}
		this->state(s);
		return 0;
	}

	int prefix_other(const tll_msg_t *msg)
	{
		return this->_callback(msg);
	}

	int _on_active() { this->state(tll::state::Active); return 0; }
	int _on_error() { this->state(tll::state::Error); return 0; }
	int _on_closing()
	{
		auto s = this->state();
		if (s == tll::state::Opening || s == tll::state::Active)
			this->state(tll::state::Closing);
		return 0;
	}

	int _on_closed()
	{
		if (this->state() == tll::state::Closing)
			Base<T>::_close();
		return 0;
	}
};

} // namespace tll::channel

#endif//_TLL_CHANNEL_PREFIX_H
