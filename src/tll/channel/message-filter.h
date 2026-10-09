// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_CHANNEL_MESSAGE_FILTER_H
#define _TLL_CHANNEL_MESSAGE_FILTER_H

#include <map>
#include <set>
#include <string>
#include <vector>

#include <tll/config.h>
#include <tll/scheme.h>
#include <tll/util/result.h>
#include <tll/conv/integer.h>

namespace tll::channel {

struct MessageFilter
{
	std::set<std::string> include;
	std::set<std::string> exclude;
	std::set<int> messages;

	void init(const std::vector<std::string> &filter)
	{
		for (auto &s: filter) {
			if (s.size() && s[0] == '!')
				exclude.emplace(s.substr(1));
			else
				include.emplace(s);
		}
	}

	bool skip(int msgid) const { return messages.find(msgid) == messages.end(); }
	bool match(int msgid) const { return !skip(msgid); }

	tll::result_t<void, std::string> numeric()
	{
		if (exclude.size())
			return error("Excludes not supported without scheme");
		for (auto &i: include) {
			if (auto r = tll::conv::to_any<int>(i); !r)
				return error(std::string("Invalid message id '") + i + "': " + r.error());
			else
				messages.emplace(*r);
		}
		return {};
	}

	void fill(const tll::Scheme * s)
	{
		messages.clear();
		for (auto m = s->messages; m; m = m->next) {
			if (!m->msgid)
				continue;
			if (exclude.find(m->name) != exclude.end())
				continue;
			if (include.size() && include.find(m->name) == include.end())
				continue;
			messages.emplace(m->msgid);
		}
	}
};

struct MessageFilters
{
	std::map<std::string, MessageFilter, std::less<>> filters;

	const tll::channel::MessageFilter * find(std::string_view name) const
	{
		if (auto it = filters.find(name); it != filters.end())
			return &it->second;
		return nullptr;
	}

	static tll::result_t<MessageFilters> load(tll::ConstConfig cfg, bool scheme)
	{
		MessageFilters mf;
		for (auto &[path, _]: cfg.browse("filter.*", true)) {
			auto name = path.substr(strlen("filter."));
			auto r = cfg.getT<std::vector<std::string>>(path);
			if (!r)
				return error(fmt::format("Invalid filter list '{}': {}", path, r.error()));
			tll::channel::MessageFilter f;
			f.init(*r);
			if (!scheme) {
				if (auto r = f.numeric(); !r)
					return error(fmt::format("Failed to init filter '{}': {}", name, r.error()));
			}
			mf.filters.emplace(name, std::move(f));
		}
		return mf;
	}

	void open(const tll::Scheme * scheme)
	{
		if (!scheme)
			return;
		for (auto &[_, f]: filters)
			f.fill(scheme);
	}
};

} // namespace tll::channel

#endif//_TLL_CHANNEL_MESSAGE_FILTER_H
