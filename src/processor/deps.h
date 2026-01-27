/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _PROCESSOR_LEVEL_H
#define _PROCESSOR_LEVEL_H

#include "tll/logger.h"
#include "tll/channel.h"
#include "tll/channel/reopen.h"
#include "tll/util/time.h"

#include <algorithm>
#include <list>
#include <string>

namespace tll::processor::_ {

struct Worker;

enum class Shutdown { Close, Error, None };

struct Object
{
	std::unique_ptr<Channel> channel;

	tll_state_t state = tll::state::Closed;
	tll_state_t state_prev = tll::state::Closed;
	bool decay = false;
	bool opening = false;
	bool verbose = false;
	bool stage = false;
	bool subtree_closed = true;

	tll::channel::ReopenData reopen;

	Shutdown shutdown = Shutdown::None;
	Worker * worker = nullptr;

	std::vector<Object *> depends;
	std::vector<Object *> rdepends;

	std::list<std::string> depends_names; //< Temporary storage used during initialization
	std::string stage_name; //< Short name without processor/stage/ prefix for stage object

	Object(std::unique_ptr<Channel> && ptr)
		: channel(std::move(ptr))
	{
		reopen.channel = channel.get();
		channel->callback_add(this, TLL_MESSAGE_MASK_STATE);
	}

	int init(const tll::Channel::Url &url);

	int open() { return reopen.open(); }

	std::string_view name() const { return channel->name(); }

	Channel * get() { return channel.get(); }
	const Channel * get() const { return channel.get(); }

	Channel * operator -> () { return get(); }
	const Channel * operator -> () const { return get(); }

	Channel & operator * () { return *get(); }
	const Channel & operator * () const { return *get(); }

	int callback(const Channel *, const tll_msg_t *);

	template <typename F>
	bool mark_subtree_closed(F func)
	{
		if (subtree_closed) {
			decay = false;
			if (ready_open()) {
				func(this);
				return true;
			}
			return false;
		}
		if (!std::all_of(rdepends.begin(), rdepends.end(), [](auto & o) { return o->subtree_closed; }))
			return false;
		if (state != tll::state::Closed || opening)
			return false;
		subtree_closed = true;
		decay = false;
		bool r = false;
		for (auto & o : depends)
			r = r || o->mark_subtree_closed(func);
		if (ready_open()) {
			func(this);
			r = true;
		}
		return r;
	}

	void mark_subtree_open()
	{
		if (!subtree_closed)
			return;
		subtree_closed = false;
		for (auto & o : depends)
			o->mark_subtree_open();
	}

	bool ready_restore() const
	{
		return std::all_of(rdepends.begin(), rdepends.end(), [](auto & o) { return o->subtree_closed && !o->decay; });
	}

	bool ready_open() const
	{
		if (decay)
			return false;
		if (std::any_of(depends.begin(), depends.end(), [](auto & o) { return o->decay; }))
			return false;
		return std::all_of(depends.begin(), depends.end(), [](auto & o) { return o->state == state::Active; });
	}

	bool ready_close() const
	{
		return std::all_of(rdepends.begin(), rdepends.end(), [](auto & o) { return o->subtree_closed; });
	}

	void on_state(tll_state_t s)
	{
		reopen.on_state(s);
		if (s != state::Closed)
			mark_subtree_open();
		switch (s) {
		case state::Opening:
			opening = false;
			break;
		default:
			break;
		}
	}
};

} // namespace tll::processor

#endif//_PROCESSOR_LEVEL_H
