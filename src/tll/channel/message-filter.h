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
	bool wildcard = false;
	std::variant<bool, std::set<int>> messages;

	void init(const std::vector<std::string> &filter)
	{
		for (auto &s: filter) {
			if (s == "*")
				wildcard = true;
			else if (s.size() && s[0] == '!')
				exclude.emplace(s.substr(1));
			else
				include.emplace(s);
		}
		if (include.empty())
			wildcard = true;
	}

	bool skip(int msgid) const
	{
		auto visit = [msgid](const auto & f) {
			using T = std::decay_t<decltype(f)>;
			if constexpr (std::is_same_v<T, bool>)
				return false;
			else
				return f.find(msgid) == f.end();
		};
		return std::visit(visit, messages);
	}
	bool match(int msgid) const { return !skip(msgid); }

	tll::result_t<void, std::string> numeric()
	{
		if (exclude.size())
			return error("Excludes not supported without scheme");
		if (wildcard) {
			messages = true;
			return {};
		}
		std::set<int> f;
		for (auto &i: include) {
			if (auto r = tll::conv::to_any<int>(i); !r)
				return error(std::string("Invalid message id '") + i + "': " + r.error());
			else
				f.emplace(*r);
		}
		messages = f;
		return {};
	}

	void fill(const tll::Scheme * s)
	{
		if (wildcard && exclude.empty()) {
			messages = true;
			return;
		}

		std::set<int> f;
		for (auto m = s->messages; m; m = m->next) {
			if (!m->msgid)
				continue;
			if (exclude.find(m->name) != exclude.end())
				continue;
			if (!wildcard && include.find(m->name) == include.end())
				continue;
			f.emplace(m->msgid);
		}
		messages = f;
	}
};

struct MessageFilters
{
	MessageFilters() = default;
	MessageFilters(MessageFilters &&) = default;
	MessageFilters(const MessageFilters &) = delete;

	MessageFilters & operator = (MessageFilters &&) = default;
	MessageFilters & operator = (const MessageFilters &) = delete;

	std::map<std::string, MessageFilter, std::less<>> filters;
	const MessageFilter * default_filter = nullptr;

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
		if (auto r = cfg.get("filter-default"); r && r->size()) {
			if (auto f = mf.find(*r); f)
				mf.default_filter = f;
			else
				return error(fmt::format("Default filter '{}' not found", *r));
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
