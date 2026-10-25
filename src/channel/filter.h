// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _CHANNEL_FILTER_H
#define _CHANNEL_FILTER_H

#include "tll/channel/message-filter.h"
#include "tll/channel/prefix.h"

namespace tll::channel {
class Filter : public tll::channel::Prefix<Filter>
{
	using Base = tll::channel::Prefix<Filter>;

	MessageFilter _filter;
 public:
	static constexpr std::string_view channel_protocol() { return "filter+"; }

	int _init(const tll::Channel::Url &cfg, tll::Channel *master)
	{
		if (auto r = Base::_init(cfg, master); r)
			return r;

		auto reader = channel_props_reader(cfg);
		auto filter = reader.getT("messages", std::vector<std::string> {});
		if (!reader)
			return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());
		_filter.init(filter);

		return 0;
	}

	int _on_active()
	{
		auto s = _child->scheme();
		if (!s)
			return _log.fail(EINVAL, "Child without scheme, can not filter");
		_filter.fill(s);
		return Base::_on_active();
	}

	int _on_data(const tll_msg_t *msg)
	{
		if (_filter.skip(msg->msgid))
			return 0;
		return _callback_data(msg);
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		if (msg->type != TLL_MESSAGE_DATA)
			return _child->post(msg, flags);
		if (_filter.skip(msg->msgid))
			return 0;
		return _child->post(msg, flags);
	}
};

} // namespace tll::channel

#endif//_CHANNEL_FILTER_H
