// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _CHANNEL_CONVERT_H
#define _CHANNEL_CONVERT_H

#include "tll/channel/convert-buf.h"
#include "tll/channel/prefix.h"

namespace tll::channel {
class Convert : public tll::channel::Prefix<Convert>
{
	using Base = tll::channel::Prefix<Convert>;

	ConvertBuf _convert_into;
	ConvertBuf _convert_from;

	bool _derive_caps = false;

 public:
	static constexpr auto prefix_scheme_policy() { return PrefixSchemePolicy::Override; }
	static constexpr std::string_view channel_protocol() { return "convert+"; }

	int _init(const tll::Channel::Url &cfg, tll::Channel *master)
	{
		if (auto r = Base::_init(cfg, master); r)
			return r;
		if (!_scheme_url)
			return _log.fail(EINVAL, "Convert prefix needs scheme");

		auto reader = tll::make_props_reader(cfg);
		_convert_into.settings.init(reader);
		_convert_from.settings = _convert_into.settings;
		if (!reader)
			return _log.fail(EINVAL, "Invalid params: {}", reader.error());

		_derive_caps = (internal.caps & tll::caps::InOut) == 0;
		return 0;
	}

	int _on_active()
	{
		auto s = _child->scheme();
		if (!s)
			return _log.fail(EINVAL, "Child without scheme, can not convert");
		if (_derive_caps) {
			auto caps = _child->caps() & tll::caps::InOut;
			if (caps == 0)
				caps = tll::caps::InOut;
			internal.caps ^= (internal.caps & tll::caps::InOut) | caps;
		}
		if (auto r = _scheme_load(*this->_scheme_url); r)
			return r;
		if (internal.caps & tll::caps::Input) {
			if (auto r = _convert_from.init(_log, s, _scheme.get()); r)
				return _log.fail(r, "Can not initialize converter from the child");
		} else
			_log.debug("Do not initialize converter from child, no Input cap");
		if (internal.caps & tll::caps::Output) {
			if (auto r = _convert_into.init(_log, _scheme.get(), s); r)
				return _log.fail(r, "Can not initialize converter into the child");
		} else
			_log.debug("Do not initialize converter into child, no Output cap");
		return Base::_on_active();
	}

	int _on_closed()
	{
		_convert_from.reset();
		_convert_into.reset();
		return Base::_on_closed();
	}

	int _on_data(const tll_msg_t *msg)
	{
		if (auto m = _convert_from.convert(msg); m) {
			if (*m)
				return _callback_data(*m);
			return 0;
		}
		return _log.fail(EINVAL, "Failed to convert message {} at {}: {}", msg->msgid, _convert_from.format_stack(), _convert_from.error);
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		if (msg->type != TLL_MESSAGE_DATA)
			return Base::_post(msg, flags);

		if (auto m = _convert_into.convert(msg); m) {
			if (*m)
				return _child->post(*m, flags);
			return 0;
		}
		return _log.fail(EINVAL, "Failed to convert message {} at {}: {}", msg->msgid, _convert_into.format_stack(), _convert_into.error);
	}
};

} // namespace tll::channel

#endif//_CHANNEL_CONVERT_H
