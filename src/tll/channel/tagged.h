/*
 * Copyright (c) 2021-2022 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_TAGGED_H
#define _TLL_CHANNEL_TAGGED_H

#include "tll/channel/base.h"
#include "tll/util/time.h"

#include <set>

namespace tll::channel {

template <unsigned Mask = TLL_MESSAGE_MASK_ALL>
struct Tag {
	static constexpr unsigned mask = Mask;
};

struct Input : public Tag<TLL_MESSAGE_MASK_ALL> { static constexpr std::string_view name() { return "input"; } };
struct Output : public Tag<TLL_MESSAGE_MASK_ALL ^ TLL_MESSAGE_MASK_DATA> { static constexpr std::string_view name() { return "output"; } };

template <typename Tag>
struct TaggedChannel : public tll::Channel {};

namespace _ {

template <typename Tag>
struct TaggedHelper
{
	static constexpr bool derived = std::is_base_of_v<tll::Channel, TaggedChannel<Tag>>;

	using channel_type = std::conditional_t<derived, TaggedChannel<Tag> *, TaggedChannel<Tag>>;

	static bool eq(const channel_type &l, const tll::Channel *r)
	{
		if constexpr (derived) {
			return l == r;
		} else {
			return l.channel == r;
		}
	}

	static void set(channel_type &v, tll::Channel *c)
	{
		if constexpr (derived) {
			v = static_cast<channel_type>(c);
		} else {
			v.channel = c;
		}
	}

	static tll::Channel * get(const channel_type *v) { return get(*v); }
	static tll::Channel * get(const channel_type &v) {
		if constexpr (derived) {
			return v;
		} else {
			return v.channel;
		}
	}
};

template <typename Self, typename... Tags>
struct TaggedStorage {};

template <typename Self>
struct TaggedStorage<Self> {
	void push(Self * self, std::string_view name, tll::Channel * c) {}
	void remove(const tll::Channel * c) {}
	void callback_add(const tll::Channel * c) {}
	void callback_del(const tll::Channel * c) {}
	void clear() {}
};

template <typename Self, typename Tag, typename... Tags>
struct TaggedStorage<Self, Tag, Tags...>
{
	template <typename T>
	using channel_type = typename TaggedHelper<T>::channel_type;

	template <typename T>
	using pair_type = std::pair<channel_type<T>, Self *>;


	template <typename T>
	using storage_type = std::list<pair_type<T>>;

	storage_type<Tag> channels;
	TaggedStorage<Self, Tags...> rest;

	void push(Self * self, std::string_view tag, tll::Channel * c)
	{
		if (Tag::name() == tag) {
			channel_type<Tag> v = {};
			TaggedHelper<Tag>::set(v, c);
			channels.emplace_back(pair_type<Tag>(v, self));
		}
		rest.push(self, tag, c);
	}

	void remove(const tll::Channel * c)
	{
		channels.remove_if([&c](auto v) { return TaggedHelper<Tag>::eq(v.first, c); });
		rest.remove(c);
	}

	void callback_add(const tll::Channel * c)
	{
		for (auto & p : channels) {
			if (TaggedHelper<Tag>::eq(p.first, c)) {
				TaggedHelper<Tag>::get(p.first)->callback_add(callback, &p, Tag::mask);
			}
		}
		rest.callback_add(c);
	}

	void callback_del(const tll::Channel * c)
	{
		for (auto & p : channels) {
			if (TaggedHelper<Tag>::eq(p.first, c)) {
				TaggedHelper<Tag>::get(p.first)->callback_del(callback, &p, Tag::mask);
			}
		}
		rest.callback_del(c);
	}

	void clear()
	{
		for (auto & p : channels) {
			TaggedHelper<Tag>::get(p.first)->callback_del(callback, &p, Tag::mask);
		}
		rest.clear();
	}

	static int callback(const tll_channel_t *, const tll_msg_t *m, void *user)
	{
		auto p = static_cast<pair_type<Tag> *>(user);
		if constexpr (TaggedHelper<Tag>::derived) {
			return p->second->callback_tag_wrapper(p->first, m);
		} else {
			return p->second->callback_tag_wrapper(&p->first, m);
		}
	}

	template <typename T>
	storage_type<T> & get()
	{
		if constexpr (std::is_same_v<Tag, T>) {
			return channels;
		} else {
			return rest.template get<T>();
		}
	}
};

}

template <typename T, typename... Tags>
class Tagged : public tll::channel::Base<T>
{
 protected:
	using Base = tll::channel::Base<T>;

	_::TaggedStorage<T, Tags...> _channels;

	size_t _skipped = 0;

 public:
	struct StatType : public Base::StatType
	{
		tll::stat::IntegerGroup<tll::stat::Ns, 't', 'i', 'm', 'e'> time;
	};

	stat::BlockT<StatType> * stat() { return static_cast<stat::BlockT<StatType> *>(this->internal.stat); }

	static constexpr auto process_policy() { return Base::ProcessPolicy::Never; }

	template <typename Tag>
	int check_channels_size(ssize_t min, ssize_t max)
	{
		ssize_t size = _channels.template get<Tag>().size();
		if (min >= 0 && min == max) {
			if (size != min)
				return this->_log.fail(ERANGE, "Need exactly {} '{}' channels, got {}", min, Tag::name(), size);
			return 0;
		}
		if (min >= 0 && size < min)
			return this->_log.fail(ERANGE, "Need more then {} '{}' channels, got {}", min, Tag::name(), size);
		if (max >= 0 && size > max)
			return this->_log.fail(ERANGE, "Need less then {} '{}' channels, got {}", max, Tag::name(), size);
		return 0;
	}

	int init(const tll::Channel::Url &url, tll::Channel *master, tll_channel_context_t *ctx)
	{
		std::vector<std::string_view> tags = {Tags::name()...};
		std::set<std::string_view> tagset(tags.begin(), tags.end());

		std::set<tll::Channel *> cset;
		for (auto & p : url.browse("tll.channel.**")) {
			auto tag = p.first.substr(strlen("tll.channel."));
			if (tagset.find(tag) == tagset.end())
				return this->_log.fail(EINVAL, "Invalid tag: {}, known tags are {}", tag, tags);
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
				_channels.push(this->channelT(), tag, c);
				cset.insert(c);
			}

		}
		int r = this->Base::init(url, master, ctx);
		if (r) return r;
		this->_log.debug("Add callbacks");
		for (auto & c : cset) {
			_channels.callback_add(c);
		}
		return 0;
	}

	void _free()
	{
		_channels.clear();
		Base::_free();
	}

	int _open(const tll::ConstConfig &cfg)
	{
		if (_skipped) {
			this->_log.warning("Skipped {} messages in inactive state", _skipped);
			_skipped = 0;
		}
		return Base::_open(cfg);
	}

	template <typename Tag>
	int callback_tag_wrapper(TaggedChannel<Tag> * c, const tll_msg_t *msg)
	{
		if (msg->type == TLL_MESSAGE_STATE && msg->msgid == tll::state::Destroy) {
			_channels.remove(_::TaggedHelper<Tag>::get(c));
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
			return this->channelT()->callback_tag(c, msg);

		auto start = tll::time::now();
		auto r = this->channelT()->callback_tag(c, msg);
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

	///< Override function
	template <typename Tag>
	int callback_tag(TaggedChannel<Tag> * c, const tll_msg_t *msg) { return 0; }
};

} // namespace tll::logic

#endif//_TLL_CHANNEL_TAGGED_H
