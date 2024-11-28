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

/** Base class for prefix channels
 *
 * Provides common code for creation and lifecycle management of child channel.
 *
 * Derived class in addition to _init/_open/_close and _free functions can override _on_* group of functions:
 *  - @ref _on_init: change Url of child channel.
 *  - @ref _on_active, @ref _on_error, @ref _on_closing, @ref _on_closed: handle state changes.
 *  - @ref _on_data, @ref _on_state, @ref _on_other: handle Data, State or any other messages.
 *    In most cases instead of overriding _on_state it's better to use _on_active/... functions described above.
 */
template <typename T>
class Prefix : public Base<T>
{
protected:
	std::unique_ptr<Channel> _child;
public:

	static constexpr auto open_policy() { return Base<T>::OpenPolicy::Manual; }
	static constexpr auto child_policy() { return Base<T>::ChildPolicy::Proxy; }
	static constexpr auto close_policy() { return Base<T>::ClosePolicy::Long; }
	static constexpr auto process_policy() { return Base<T>::ProcessPolicy::Never; }
	static constexpr auto scheme_policy() { return Base<T>::SchemePolicy::Manual; }
	static constexpr auto post_opening_policy() { return Base<T>::PostPolicy::Enable; }
	static constexpr auto post_closing_policy() { return Base<T>::PostPolicy::Enable; }

	enum class PrefixSchemePolicy
	{
		Derive, ///< Scheme is derived from child channel
		Override, ///< Prefix can hold scheme different from its child
	};
	static constexpr auto prefix_scheme_policy() { return PrefixSchemePolicy::Derive; }

	const Scheme * scheme(int type) const
	{
		this->_log.trace("Request scheme {}", type);
		if (type == TLL_MESSAGE_DATA) {
			switch (this->channelT()->prefix_scheme_policy()) {
			case PrefixSchemePolicy::Derive:
				return _child->scheme(type);
			case PrefixSchemePolicy::Override:
				return this->_scheme.get();
			}
		}
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
		this->child_url_fill(curl, pproto);

		for (auto &k : std::vector<std::string_view> { "dump", "stat" }) {
			if (curl.has(k))
				curl.remove(k);
		}

		if (this->channelT()->_on_init(curl, url, master))
			return this->_log.fail(EINVAL, "Init hook returned error");

		_child = this->context().channel(curl, master);
		if (!_child)
			return this->_log.fail(EINVAL, "Failed to create child channel");
		_child->callback_add(this, TLL_MESSAGE_MASK_ALL);
		this->_child_add(_child.get(), "child");

		return Base<T>::_init(url, master);
	}

	void _free()
	{
		_child.reset();
		return Base<T>::_free();
	}

	int _open(const tll::ConstConfig &params)
	{
		return _child->open(params);
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
		if (msg->type == TLL_MESSAGE_DATA) {
			if (auto r = this->channelT()->_on_data(msg); r)
				return this->state_fail(r, "Data hook failed");
		} else if (msg->type == TLL_MESSAGE_STATE) {
			if (auto r = this->channelT()->_on_state(msg); r)
				return this->state_fail(r, "State hook failed");
		} else {
			if (auto r = this->channelT()->_on_other(msg); r)
				return this->state_fail(r, "Other hook failed");
		}
		return 0;
	}

	/// Modify Url of child channel
	int _on_init(tll::Channel::Url &curl, const tll::Channel::Url &url, const tll::Channel * master)
	{
		return 0;
	}

	/// Handle data messages
	int _on_data(const tll_msg_t *msg)
	{
		return this->_callback_data(msg);
	}

	/** Handle state messages
	 *
	 * In most cases override of this function is not needed. See @ref _on_active, @ref _on_error and @ref _on_closed.
	 */
	int _on_state(const tll_msg_t *msg)
	{
		auto s = (tll_state_t) msg->msgid;
		switch (s) {
		case tll::state::Active:
			return this->channelT()->_on_active();
		case tll::state::Error:
			return this->channelT()->_on_error();
		case tll::state::Closing:
			return this->channelT()->_on_closing();
		case tll::state::Closed:
			return this->channelT()->_on_closed();
		default:
			break;
		}
		return 0;
	}

	/// Handle non-state and non-data messages
	int _on_other(const tll_msg_t *msg)
	{
		return this->_callback(msg);
	}

	/// Channel is ready to enter Active state
	int _on_active()
	{
		if (auto client = _child->config().sub("client"); client) {
			if (this->channelT()->_on_client_export(*client))
				return this->_log.fail(EINVAL, "Failed to export client parameters");
		}


		if (this->channelT()->prefix_scheme_policy() == PrefixSchemePolicy::Override) {
			if (this->_scheme_url)
				this->_scheme_load(*this->_scheme_url);
			else if (auto s = _child->scheme(); s)
				this->_scheme.reset(s->ref());
		}

		this->state(tll::state::Active);
		return 0;
	}
	/// Channel is broken and needs to enter Error state
	int _on_error() { this->state(tll::state::Error); return 0; }
	/// Channel starts closing
	int _on_closing()
	{
		auto s = this->state();
		if (s == tll::state::Opening || s == tll::state::Active)
			this->state(tll::state::Closing);
		return 0;
	}

	/// Channel close is finished
	int _on_closed()
	{
		if (this->state() == tll::state::Closing)
			Base<T>::_close();
		return 0;
	}

	int _on_client_export(const tll::ConstConfig &cfg)
	{
		auto proto = cfg.get("init.tll.proto");
		if (!proto) {
			this->_log.warning("Client parameters without tll.proto");
			return 0;
		}
		this->_config.set("client", cfg.copy());
		this->_config.set("client.init.tll.proto", fmt::format("{}{}", this->channelT()->channel_protocol(), *proto));
		return 0;
	}
};

} // namespace tll::channel

#endif//_TLL_CHANNEL_PREFIX_H
