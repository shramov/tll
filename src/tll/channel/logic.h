/*
 * Copyright (c) 2018-2019 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_LOGIC_H
#define _TLL_CHANNEL_LOGIC_H

#include "tll/channel/base.h"
#include "tll/util/time.h"

#include <algorithm>

namespace tll {

namespace channel {

template <typename T>
class Logic : public Base<T>
{
 protected:
	std::map<std::string, std::vector<tll::Channel *>, std::less<>> _channels;
	size_t _skipped = 0;

 public:

	struct StatType : public Base<T>::StatType
	{
		tll::stat::IntegerGroup<tll::stat::Ns, 't', 'i', 'm', 'e'> time;
	};

	stat::BlockT<StatType> * stat() { return static_cast<stat::BlockT<StatType> *>(this->internal.stat); }

	static constexpr auto process_policy() { return Base<T>::ProcessPolicy::Never; }

	int init(const tll::Channel::Url &url, tll::Channel *master, tll_channel_context_t *ctx)
	{
		for (auto & p : url.browse("tll.channel.**")) {
			auto tag = p.first.substr(strlen("tll.channel."));
			auto v = p.second.get();
			if (!v || !v->size()) continue;
			auto l = conv::to_any<std::list<std::string_view>>(*v);
			if (!l)
				return this->_log.fail(EINVAL, "Invalid channel list '{}': {}", v, l.error());
			std::vector<tll::Channel *> r;
			r.reserve(l->size());
			for (auto & i : *l) {
				auto n = tll::util::strip(i);
				auto c = this->context().get(n);
				if (!c)
					return this->_log.fail(ENOENT, "Channel '{}' not found (tag '{}')", n, tag);
				r.push_back(c);
			}
			_channels.emplace(std::string(tag), std::move(r));
		}
		this->_log.debug("Add callbacks");
		int r = this->Base<T>::init(url, master, ctx);
		if (r) return r;
		for (auto & p : _channels) {
			for (auto & c : p.second)
				c->callback_add(logic_callback, this, TLL_MESSAGE_MASK_ALL);
		}
		return 0;
	}

	void _free()
	{
		for (auto & p : _channels) {
			for (auto & c : p.second)
				c->callback_del(logic_callback, this, TLL_MESSAGE_MASK_ALL);
		}
		Base<T>::_free();
	}

	int _open(const tll::ConstConfig &cfg)
	{
		if (_skipped) {
			this->_log.warning("Skipped {} messages in inactive state", _skipped);
			_skipped = 0;
		}
		return Base<T>::_open(cfg);
	}

	int logic(const Channel * c, const tll_msg_t *msg) { return 0; }

 private:
	int _logic(const Channel * c, const tll_msg_t *msg)
	{
		if (msg->type == TLL_MESSAGE_STATE && msg->msgid == tll::state::Destroy) {
			for (auto & p : _channels) {
				auto & v = p.second;
				v.erase(std::remove(v.begin(), v.end(), c), v.end());
			}
		}

		switch (this->state()) {
		case tll::state::Opening:
		case tll::state::Active:
		case tll::state::Closing:
			break;
		default:
			if (msg->type == TLL_MESSAGE_DATA)
				_skipped++;
			return 0;
		}

		if (!this->_stat_enable)
			return this->channelT()->logic(c, msg);

		auto start = tll::time::now();
		auto r = this->channelT()->logic(c, msg);
		auto dt = tll::time::now() - start;
		auto page = this->channelT()->stat()->acquire();
		if (page) {
			page->time = dt.count();
			if (msg->type == TLL_MESSAGE_DATA) {
				page->rx = 1;
				page->rxb = msg->size;
			}
			this->channelT()->stat()->release(page);
		}
		return r;
	}

	static int logic_callback(const tll_channel_t * c, const tll_msg_t *msg, void * user)
	{
		return static_cast<Logic<T> *>(user)->_logic(static_cast<const Channel *>(c), msg);
	}
};

} // namespace channel

template <typename T>
using LogicBase = channel::Logic<T>;

} // namespace tll

#endif//_TLL_CHANNEL_LOGIC_H
