/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_BASE_H
#define _TLL_CHANNEL_BASE_H

#include <errno.h>

#include <vector>

#include "tll/util/url.h"
#include "tll/logger.h"
#include "tll/stat.h"

#include "tll/channel.h"
#include "tll/channel/impl.h"

inline std::string_view tll_state_str(tll_state_t s)
{
	using namespace tll::state;
	switch (s) {
	case Closed: return "Closed";
	case Opening: return "Opening";
	case Active: return "Active";
	case Error: return "Error";
	case Closing: return "Closing";
	case Destroy: return "Destroy";
	}
	return "Unknown";
}

namespace tll {

template <typename T>
T * channel_cast(tll_channel_t * c)
{
	if (!c) return nullptr;
	if (c->impl == &T::impl) return static_cast<T *>(c->data);
	if ((tll_channel_caps(c) & caps::Proxy) == 0)
		return nullptr;
	auto kids = tll_channel_children(c);
	if (!kids) return nullptr;
	return channel_cast<T>(kids->channel);
}

#define TLL_DEFINE_IMPL(type, ...) template <> tll::channel_impl<type> tll::channel::Base<type>::impl = {__VA_ARGS__};
#define TLL_DECLARE_IMPL(type, ...) extern template class tll::channel::Base<type>;

namespace channel {

using Stat = tll_channel_stat_t;

template <typename T>
struct StatType { using type = Stat; };

template <typename T>
class Base
{
 protected:
	tll::Logger _log = { "tll.channel" }; //fmt::format("tll.channel.{}", T::param_prefix()) };

 public:
	typedef T ChannelT;
	ChannelT * channelT() { return static_cast<ChannelT *>(this); }
	const ChannelT * channelT() const { return static_cast<ChannelT *>(this); }

	static tll::channel_impl<ChannelT> impl;

	// Static properties
	/// Parameter prefix used for parsing init/open property strings
	static constexpr std::string_view param_prefix() { return "base"; }
	/// Protocol name
	static constexpr std::string_view impl_protocol() { return T::param_prefix(); }
	/// If channel is prefix or not
	static constexpr bool impl_prefix_channel() { return false; }

	enum class ProcessPolicy { Normal, Never, Always, Custom };
	static constexpr auto process_policy() { return ProcessPolicy::Normal; }

	enum class OpenPolicy { Auto, Manual };
	static constexpr auto open_policy() { return OpenPolicy::Auto; }

	enum class ClosePolicy { Normal, Long };
	static constexpr auto close_policy() { return ClosePolicy::Normal; }

	enum class ChildPolicy { Never, Single, Many };
	static constexpr auto child_policy() { return ChildPolicy::Never; }

	tll_channel_internal_t internal = {};

	stat::Block<typename StatType<T>::type> _stat_block = { "" };
	bool _stat_enable = false;

	scheme::ConstSchemePtr _scheme = {nullptr, &tll_scheme_unref};
	scheme::ConstSchemePtr _scheme_control = {nullptr, &tll_scheme_unref};
	std::string name;
	tll::Config _config;
	tll::Config _config_defaults;
	tll_channel_log_msg_format_t _dump = log_msg_format::Disable;

	bool _scheme_cache = true;
	std::optional<std::string> _scheme_url;

	Base()
	{
		tll_channel_internal_init(&internal);
		internal.config = _config;
	}

	//virtual ~Base() {}

	void _dump_msg(const tll_msg_t *msg, std::string_view text) const
	{
		if (_dump == log_msg_format::Disable)
			return;
		tll_channel_log_msg(self(), _log.name(), logger::Info, _dump, msg, text.data(), text.size());
	}

	inline int _callback(tll_msg_t msg) const { return _callback(&msg); }

	inline int _callback_data(const tll_msg_t * msg)
	{
		_dump_msg(msg, "Recv");
		//return tll_channel_callback_data(_this, msg);
		return tll_channel_callback_data(&internal, msg);
	}

	inline int _callback(const tll_msg_t * msg) const
	{
		_dump_msg(msg, "Recv");
		return tll_channel_callback(&internal, msg);
	}

	template <typename Props>
	PropsReaderT<PropsChainT<Props, std::string_view, Props, Config, std::string_view>> channel_props_reader(const Props &props)
	{
		auto chain = make_props_chain(props, T::param_prefix(), props, _config_defaults, T::param_prefix());
		return PropsReaderT<decltype(chain)>(chain); //, T::param_prefix());
	}

	int init(const Channel::Url &url, tll::Channel *master, tll_channel_context_t *ctx)
	{
		_log.info("Init channel {}", tll::conv::to_string(url));
		_config_defaults = context().config_defaults();
		internal.state = state::Closed;
		_config.set("state", "Closed");
		_config.set("url", url.copy());

		auto replace = channelT()->_init_replace(url);
		if (replace) {
			self()->impl = replace;
			return EAGAIN;
		}

		auto reader = channel_props_reader(url); //, T::param_prefix());
		name = reader.template getT<std::string>("name", "noname");
		_log = { fmt::format("tll.channel.{}", name) };
		_scheme_url = reader.get("scheme");
		_scheme_cache = reader.getT("scheme-cache", true);
		_stat_enable = reader.getT("stat", false);

		enum rw_t { None = 0, R = 1, W = 2, RW = R | W };
		auto dir = reader.getT("dir", None, {{"r", R}, {"w", W}, {"rw", RW}, {"in", R}, {"out", W}, {"inout", RW}});
		{
			using namespace tll::channel::log_msg_format;
			_dump = reader.getT("dump", Disable, {{"no", Disable}, {"yes", Text}, {"frame", Frame}, {"text", Text}, {"text+hex", TextHex}, {"scheme", Scheme}});
		}
		//_log = { fmt::format("tll.channel.{}.{}", T::param_prefix(), name) };
		if (!reader)
			return _log.fail(EINVAL, "Invalid url: {}", reader.error());

		if (dir & R) internal.caps |= caps::Input;
		if (dir & W) internal.caps |= caps::Output;

		internal.name = name.c_str();

		switch (channelT()->child_policy()) {
		case ChildPolicy::Never:
			break;
		case ChildPolicy::Single:
			internal.caps |= caps::Parent | caps::Proxy;
			break;
		case ChildPolicy::Many:
			internal.caps |= caps::Parent;
			break;
		}

		if (ChannelT::close_policy() == ClosePolicy::Long)
			internal.caps |= caps::LongClose;

		auto r = channelT()->_init(url, master);

		if (r) return r;

		if (_stat_enable) {
			_stat_block.name = name.c_str();
			internal.stat = &_stat_block;
		}

		return 0;
	}

	const tll_channel_impl_t * _init_replace(const Channel::Url &url) { return nullptr; }
	int _init(const tll::Channel::Url &url, tll::Channel *master) { return 0; }

	void free()
	{
		_log.info("Destroy channel");
		if (state() != state::Closed)
			close(true);
		state(state::Destroy);
		static_cast<ChannelT *>(this)->_free();
		tll_channel_internal_clear(&internal);
	}

	void _free() {}

	int open(std::string_view params)
	{
		if (state() != state::Closed)
			return _log.fail(EINVAL, "Open failed: invalid state {}", tll_state_str(state()));
		_log.info("Open channel: {}", params);
		state(state::Opening);
		switch (T::process_policy()) {
		case ProcessPolicy::Normal:
		case ProcessPolicy::Always:
			_update_dcaps(dcaps::Process);
			break;
		case ProcessPolicy::Custom:
		case ProcessPolicy::Never:
			break;
		}

		if (_scheme_url) {
			_log.debug("Loading scheme from {}...", _scheme_url->substr(0, 64));
			_scheme.reset(context().scheme_load(*_scheme_url, _scheme_cache));
			if (!_scheme) {
				state(state::Error);
				return _log.fail(EINVAL, "Failed to load scheme from {}...", _scheme_url->substr(0, 64));
			}
		}
		auto props = tll::PropsView::parse(params);
		if (!props)
			return _log.fail(EINVAL, "Invalid props: {}", props.error());
		auto r = static_cast<ChannelT *>(this)->_open(*props);
		if (r)
			state(state::Error);
		else if (T::open_policy() == OpenPolicy::Auto)
			state(state::Active);
		return r;
	}

	int _open(const tll::PropsView &params)
	{
		return 0;
	}

	int close(bool force = false)
	{
		if (state() == state::Closed)
			return 0;
		if (state() == state::Closing && !force) {
			return 0;
		}
		state(state::Closing);
		int r = 0;
		if constexpr (ChannelT::close_policy() == ClosePolicy::Long)
			r = channelT()->_close(force);
		else
			r = channelT()->_close();

		if (ChannelT::close_policy() == ClosePolicy::Long && !force) {
			// Errors are only allowed in long closes
			if (state() != state::Closed) {
				if (r)
					state(state::Error);
				return r;
			}
		}

		_close();
		return 0;
	}

	/// Common cleanup code that can be called from finalizing part of long close
	int _close(bool force = false)
	{
		_scheme.reset();
		_update_dcaps(0, dcaps::Process | dcaps::CPOLLMASK);
		state(state::Closed);
		return 0;
	}

	int process(long timeout, int flags)
	{
		//return _process(timeout, flags);
		if (state() == state::Error || state() == state::Closed)
			return 0;
		int r = static_cast<ChannelT *>(this)->_process(timeout, flags);
		if (r && r != EAGAIN) {
			_log.error("Process failed");
			state(tll::state::Error);
		}
		return r;
	}

	int _process(long timeout, int flags)
	{
		return 0;
	}

	int post(const tll_msg_t *msg, int flags)
	{
		if (state() != state::Active)
			return _log.fail(EINVAL, "Post in invalid state {}", tll_state_str(state()));
		_dump_msg(msg, "Post");
		auto r = static_cast<ChannelT *>(this)->_post(msg, flags);
		if (r)
			_log.error("Post failed: {}", strerror(r));
		return r;
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		return ENOSYS;
	}

	const Scheme * scheme(int type) const
	{
		this->_log.trace("Request scheme {} ({}/{})", type, (void *) _scheme.get(), (void *) _scheme_control.get());
		switch (type) {
		case TLL_MESSAGE_DATA: return _scheme.get();
		case TLL_MESSAGE_CONTROL: return _scheme_control.get();
		default: return nullptr;
		}
		//return nullptr;
	}

	template <tll_msg_type_t Type = TLL_MESSAGE_DATA, typename Id>
	const scheme::Message * scheme_lookup(Id id)
	{
		if constexpr (Type == TLL_MESSAGE_DATA)
			return scheme_lookup(_scheme.get(), id);
		if constexpr (Type == TLL_MESSAGE_CONTROL)
			return scheme_lookup(_scheme_control.get(), id);
		return nullptr;
	}

	const scheme::Message * scheme_lookup(const Scheme * scheme, int msgid)
	{
		if (!scheme) return nullptr;
		for (auto m = scheme->messages; m; m = m->next) {
			if (m->msgid != 0 && m->msgid == msgid)
				return m;
		}
		return nullptr;
	}

	const scheme::Message * scheme_lookup(const Scheme * scheme, std::string_view name)
	{
		if (!scheme) return nullptr;
		for (auto m = scheme->messages; m; m = m->next) {
			if (m->name && m->name == name)
				return m;
		}
		return nullptr;
	}

	channel::Context context() { return Context(internal.self->context); }
	const channel::Context context() const { return Context(internal.self->context); }

	Channel * self() { return static_cast<Channel *>(internal.self); }
	const Channel * self() const { return static_cast<Channel *>(internal.self); }
	operator Channel * () { return self(); }
	operator const Channel * () const { return self(); }

	int fd() const { return internal.fd; }

	tll_state_t state() const { return internal.state; }
	tll_state_t state(tll_state_t s)
	{
		auto old = state();
		if (s == old)
			return old;
		_log.info("State change: {} -> {}", tll_state_str(old), tll_state_str(s));
		internal.state = s;
		_config.set("state", tll_state_str(s));
		_callback({TLL_MESSAGE_STATE, s});
		return old;
	}

	int _child_add(tll::Channel *c, std::string_view tag = "")
	{
		_log.info("Add custom channel {}", tll_channel_name(c));
		if (auto r = tll_channel_internal_child_add(&internal, c, tag.data(), tag.size()))
			return _log.fail(r, "Failed to add child channel {}: {}", tll_channel_name(c), strerror(r));
		return 0;
	}

	int _child_del(tll::Channel *c, std::string_view tag = "")
	{
		_log.info("Delete custom channel {}", tll_channel_name(c));
		if (auto r = tll_channel_internal_child_del(&internal, c, tag.data(), tag.size()))
			return _log.fail(r, "Failed to del child channel {}: {}", tll_channel_name(c), strerror(r));
		return 0;
	}

	void _dcaps_poll(unsigned caps)
	{
		return _update_dcaps(caps, dcaps::CPOLLMASK);
	}

	void _dcaps_pending(bool pending)
	{
		return _update_dcaps(pending?dcaps::Pending:0, dcaps::Pending);
	}

	void _update_dcaps(unsigned caps) { return _update_dcaps(caps, caps); }

	void _update_dcaps(unsigned caps, unsigned mask)
	{
		caps &= mask;
		auto old = internal.dcaps;
		if ((old & mask) == caps)
			return;
		internal.dcaps ^= (old & mask) ^ caps;
		_log.debug("Update caps: {:02b} + {:02b} -> {:02b}", old, caps, internal.dcaps);
		tll_msg_t msg = {TLL_MESSAGE_CHANNEL, TLL_MESSAGE_CHANNEL_UPDATE};
		msg.data = &old;
		msg.size = sizeof(old);
		_callback(msg);
	}

	int _update_fd(int fd)
	{
		if (fd == internal.fd)
			return fd;
		std::swap(fd, internal.fd);
		_log.debug("Update fd: {} -> {}", fd, this->fd());
		tll_msg_t msg = {TLL_MESSAGE_CHANNEL, TLL_MESSAGE_CHANNEL_UPDATE_FD};
		msg.data = &fd;
		msg.size = sizeof(fd);
		_callback(msg);
		return fd;
	}
};

} // namespace channel

} // namespace tll

#endif//_TLL_CHANNEL_BASE_H
