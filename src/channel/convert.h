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

 public:
	static constexpr auto prefix_scheme_policy() { return PrefixSchemePolicy::Override; }
	static constexpr std::string_view channel_protocol() { return "convert+"; }

	int _init(const tll::Channel::Url &cfg, tll::Channel *master)
	{
		if (auto r = Base::_init(cfg, master); r)
			return r;
		if (!_scheme_url)
			return _log.fail(EINVAL, "Convert prefix needs scheme");
		return 0;
	}

	int _on_active()
	{
		auto s = _child->scheme();
		if (!s)
			return _log.fail(EINVAL, "Child without scheme, can not convert");
		if (auto r = _scheme_load(*this->_scheme_url); r)
			return r;
		if (auto r = _convert_from.init(_log, s, _scheme.get()); r)
			return _log.fail(r, "Can not initialize converter from the child");
		if (auto r = _convert_into.init(_log, _scheme.get(), s); r)
			return _log.fail(r, "Can not initialize converter into the child");
		return Base::_on_active();
	}

	int _on_data(const tll_msg_t *msg)
	{
		if (auto m = _convert_from.convert(msg); m)
			return _callback_data(m);
		return _log.fail(EINVAL, "Failed to convert message {} at {}: {}", msg->msgid, _convert_from.format_stack(), _convert_from.error);
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		if (auto m = _convert_into.convert(msg); m)
			return _child->post(m, flags);
		return _log.fail(EINVAL, "Failed to convert message {} at {}: {}", msg->msgid, _convert_into.format_stack(), _convert_into.error);
	}
};

} // namespace tll::channel

#endif//_CHANNEL_CONVERT_H
