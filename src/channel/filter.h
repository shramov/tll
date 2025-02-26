// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _CHANNEL_FILTER_H
#define _CHANNEL_FILTER_H

#include <set>

#include "tll/channel/prefix.h"

namespace tll::channel {
class Filter : public tll::channel::Prefix<Filter>
{
	using Base = tll::channel::Prefix<Filter>;

	std::set<std::string> _include;
	std::set<std::string> _exclude;
	std::set<int> _messages;
 public:
	static constexpr std::string_view channel_protocol() { return "filter+"; }

	int _init(const tll::Channel::Url &cfg, tll::Channel *master)
	{
		if (auto r = Base::_init(cfg, master); r)
			return r;

		auto reader = channel_props_reader(cfg);
		auto filter = reader.getT("messages", std::list<std::string> {});
		if (!reader)
			return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());
		for (auto m : filter) {
			if (m.size() && m[0] == '!')
				_exclude.insert(m.substr(1));
			else
				_include.insert(m);
		}

		return 0;
	}

	int _on_active()
	{
		_messages.clear();
		auto s = _child->scheme();
		if (!s)
			return _log.fail(EINVAL, "Child without scheme, can not filter");
		for (auto m = s->messages; m; m = m->next) {
			if (!m->msgid)
				continue;
			if (_exclude.find(m->name) != _exclude.end())
				continue;
			if (_include.size() && _include.find(m->name) == _include.end())
				continue;
			_messages.insert(m->msgid);
		}
		return Base::_on_active();
	}

	int _on_data(const tll_msg_t *msg)
	{
		if (_messages.find(msg->msgid) == _messages.end())
			return 0;
		return _callback_data(msg);
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		if (msg->type != TLL_MESSAGE_DATA)
			return _child->post(msg, flags);
		if (_messages.find(msg->msgid) == _messages.end())
			return 0;
		return _child->post(msg, flags);
	}
};

} // namespace tll::channel

#endif//_CHANNEL_FILTER_H
