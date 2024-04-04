/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_BASE_H
#define _TLL_CHANNEL_BASE_H

#include <errno.h>

#include <vector>

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
	/** Protocol name
	 *
	 * Define channel protocol name, for prefix channels add '+' in the end: "proto+".
	 * This function is required since protocol name is used not only to instantiate channel but
	 * also for debugging purposes.
	 */
	static constexpr std::string_view channel_protocol(); // { return "base"; }

	/** Parameter prefix used for parsing init/open property strings, optional
	 *
	 * By default it's derived from channel_protocol() by removing trailing '+'
	 */
	static constexpr std::string_view param_prefix()
	{
		constexpr auto s = T::channel_protocol();
		if constexpr (s.size() == 0) {
			return s;
		} else if constexpr (s[s.size() - 1] == '+') {
			return std::string_view(s.data(), s.size() - 1);
		} else
			return s;
	}

	enum class ProcessPolicy { Normal, Never, Always, Custom };
	static constexpr auto process_policy() { return ProcessPolicy::Normal; }

	enum class OpenPolicy { Auto, Manual };
	static constexpr auto open_policy() { return OpenPolicy::Auto; }

	enum class ClosePolicy { Normal, Long };
	static constexpr auto close_policy() { return ClosePolicy::Normal; }

	enum class ChildPolicy
	{
		Never, ///< Channel has no child objects
		Proxy, ///< Channel has some child objects, first one can be casted with tll::channel_cast<SubType>
		Many, ///< Channel has some child objects, tll::channel_cast does not check children
		Single = Proxy, ///< Old name, alias for Proxy
	};
	static constexpr auto child_policy() { return ChildPolicy::Never; }

	enum class SchemePolicy { Normal, Manual };
	static constexpr auto scheme_policy() { return SchemePolicy::Normal; }

	/// Post policy, enable or disable posting in non-active states
	enum class PostPolicy { Disable, Enable };
	/// Post in Opening state policy
	static constexpr auto post_opening_policy() { return PostPolicy::Disable; }
	/// Post in Closing state policy
	static constexpr auto post_closing_policy() { return PostPolicy::Disable; }

	tll_channel_internal_t internal = {};

	using StatType = tll_channel_stat_t;

	stat::BlockT<StatType> * stat() { return static_cast<stat::BlockT<StatType> *>(internal.stat); }
	bool _stat_enable = false;
	bool _with_fd = true;

	scheme::ConstSchemePtr _scheme;
	scheme::ConstSchemePtr _scheme_control;
	std::string name;
	tll::Config _config;
	tll::Config _config_defaults;

	bool _scheme_cache = true;
	std::optional<std::string> _scheme_url;

	Base()
	{
		tll_channel_internal_init(&internal);
		internal.config = _config;
	}

	//virtual ~Base() {}

	inline int _callback(tll_msg_t msg) const { return _callback(&msg); }

	inline int _callback_data(const tll_msg_t * msg)
	{
		return tll_channel_callback_data(&internal, msg);
	}

	inline int _callback(const tll_msg_t * msg) const
	{
		return tll_channel_callback(&internal, msg);
	}

	template <typename Props>
	decltype(auto) channel_props_reader(const Props &props)
	{
		if constexpr (std::is_base_of_v<tll::Config, Props> || std::is_base_of_v<tll::ConstConfig, Props>) {
			auto chain = make_props_chain(ConstConfig(props).sub(T::param_prefix()), props, _config_defaults.sub(T::param_prefix()));
			return PropsReaderT<decltype(chain)>(chain);
		} else {
			auto chain = make_props_chain(make_props_prefix(props, T::param_prefix()), props, _config_defaults.sub(T::param_prefix()));
			return PropsReaderT<decltype(chain)>(chain);
		}
	}

	tll::result_t<Channel::Url> child_url_parse(std::string_view url, std::string_view suffix) const
	{
		auto cfg = Channel::Url::parse(url);
		if (cfg)
			child_url_fill(*cfg, suffix);
		return cfg;
	}

	void child_url_fill(Channel::Url &url, std::string_view suffix) const
	{
		url.set("name", fmt::format("{}/{}", name, suffix));
		url.set("tll.internal", "yes");
		if (!_with_fd && !url.has("fd"))
			url.set("fd", "no");
	}

	int init(const Channel::Url &url, tll::Channel *master, tll_channel_context_t *ctx)
	{
		_log.info("Init channel {}", tll::conv::to_string(url));
		_config_defaults = context().config_defaults();
		internal.state = state::Closed;
		_config.set("state", "Closed");
		_config.set("url", url.copy());

		auto replace = channelT()->_init_replace(url, master);
		if (!replace)
			return _log.fail(EINVAL, "Failed to find impl replacement");
		if (*replace) {
			self()->impl = *replace;
			return EAGAIN;
		}

		auto reader = channel_props_reader(url); //, T::param_prefix());
		name = reader.template getT<std::string>("name", "noname");
		_log = { fmt::format("tll.channel.{}", name) };
		_scheme_url = reader.get("scheme");
		_scheme_cache = reader.getT("scheme-cache", true);
		_stat_enable = reader.getT("stat", false);
		_with_fd = reader.getT("fd", true);

		enum rw_t { None = 0, R = 1, W = 2, RW = R | W };
		auto dir = reader.getT("dir", None, {{"r", R}, {"w", W}, {"rw", RW}, {"in", R}, {"out", W}, {"inout", RW}});
		{
			using namespace tll::channel::log_msg_format;
			internal.dump = reader.getT("dump", Disable, {{"no", Disable}, {"yes", Auto}, {"frame", Frame}, {"text", Text}, {"text+hex", TextHex}, {"scheme", Scheme}, {"auto", Auto}});
		}
		//_log = { fmt::format("tll.channel.{}.{}", T::param_prefix(), name) };
		if (!reader)
			return _log.fail(EINVAL, "Invalid url: {}", reader.error());

		if (dir & R) internal.caps |= caps::Input;
		if (dir & W) internal.caps |= caps::Output;

		internal.name = name.c_str();
		internal.logger = _log.ptr();

		switch (channelT()->child_policy()) {
		case ChildPolicy::Never:
			break;
		case ChildPolicy::Proxy:
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

		internal.logger = tll_logger_copy(_log.ptr());

		if (_stat_enable)
			internal.stat = new stat::Block<typename T::StatType>(name);

		return 0;
	}

	std::optional<const tll_channel_impl_t *> _init_replace(const Channel::Url &url, tll::Channel *master) { return nullptr; }
	int _init(const tll::Channel::Url &url, tll::Channel *master) { return 0; }

	void free()
	{
		_log.info("Destroy channel");
		if (state() != state::Closed)
			close(true);
		state(state::Destroy);
		static_cast<ChannelT *>(this)->_free();
		if (internal.stat)
			delete static_cast<stat::Block<typename T::StatType> *>(internal.stat);
		_scheme.reset();
		_scheme_control.reset();
		tll_channel_internal_clear(&internal);
	}

	void _free() {}

	int open(const tll::ConstConfig &cfg)
	{
		if (state() != state::Closed)
			return _log.fail(EINVAL, "Open failed: invalid state {}", tll_state_str(state()));
		{
			std::string r;
			for (auto &[k, c] : cfg.browse("**")) {
				auto v = c.get();
				if (r.size())
					r += ";";
				r += fmt::format("{}={}", k, v.value_or(""));
			}
			_log.info("Open channel: {}", r);
		}
		_config.unlink("open");
		_config.set("open", cfg.copy());

		state(state::Opening);
		switch (ChannelT::process_policy()) {
		case ProcessPolicy::Normal:
		case ProcessPolicy::Always:
			_update_dcaps(dcaps::Process);
			break;
		case ProcessPolicy::Custom:
		case ProcessPolicy::Never:
			break;
		}

		if (ChannelT::scheme_policy() == SchemePolicy::Normal && _scheme_url) {
			_log.debug("Loading scheme from {}...", _scheme_url->substr(0, 64));
			_scheme.reset(context().scheme_load(*_scheme_url, _scheme_cache));
			if (!_scheme)
				return state_fail(EINVAL, "Failed to load scheme from {}...", _scheme_url->substr(0, 64));
		}
		auto r = static_cast<ChannelT *>(this)->_open(cfg);
		if (r)
			state(state::Error);
		else if (ChannelT::open_policy() == OpenPolicy::Auto)
			state(state::Active);
		return r;
	}

	int _open(const tll::ConstConfig &cfg)
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
		if (ChannelT::scheme_policy() == SchemePolicy::Normal)
			_scheme.reset();
		_update_dcaps(0, dcaps::Process | dcaps::Pending | dcaps::CPOLLMASK);

		for (auto i = self()->children(); i; ) {
			auto c = static_cast<tll::Channel *>(i->channel);
			i = i->next;
			if (c && c->state() != state::Closed)
				c->close(true);
		}

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
		auto s = state();
		if (s != state::Active) {
			if (s == state::Opening && channelT()->post_opening_policy() == PostPolicy::Enable) {
				// Post in Opening is enabled
			} else if (s == state::Closing && channelT()->post_closing_policy() == PostPolicy::Enable) {
				// Post in Closing is enabled
			} else
				return _log.fail(EINVAL, "Post in invalid state {}", tll_state_str(s));
		}
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

	channel::Context context() { return Context(internal.self->context); }
	const channel::Context context() const { return Context(internal.self->context); }

	Channel * self() { return static_cast<Channel *>(internal.self); }
	const Channel * self() const { return static_cast<Channel *>(internal.self); }
	operator Channel * () { return self(); }
	operator const Channel * () const { return self(); }

	int fd() const { return internal.fd; }

	/// Subtree for channel custom info, like last seq
	Config config_info() { return *_config.sub("info", true); } // Sub can not fail in this case

	tll_state_t state() const { return internal.state; }
	tll_state_t state(tll_state_t s)
	{
		auto old = state();
		if (s == old)
			return old;
		_log.info("State change: {} -> {}", tll_state_str(old), tll_state_str(s));
		internal.state = s;
		_config.set("state", tll_state_str(s));
		_callback({.type = TLL_MESSAGE_STATE, .msgid = s});
		return old;
	}

	template <typename R, typename... Args>
	[[nodiscard]]
	R state_fail(R err, tll::logger::format_string<Args...> format, Args && ... args)
	{
		_log.error(format, std::forward<Args>(args)...);
		state(tll::state::Error);
		return err;
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
		_log.trace("Update caps: {:02b} + {:02b} -> {:02b}", old, caps, internal.dcaps);
		tll_msg_t msg = {.type = TLL_MESSAGE_CHANNEL, .msgid = TLL_MESSAGE_CHANNEL_UPDATE};
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
		tll_msg_t msg = {.type = TLL_MESSAGE_CHANNEL, .msgid = TLL_MESSAGE_CHANNEL_UPDATE_FD};
		msg.data = &fd;
		msg.size = sizeof(fd);
		_callback(msg);
		return fd;
	}
};

} // namespace channel

} // namespace tll

#endif//_TLL_CHANNEL_BASE_H
