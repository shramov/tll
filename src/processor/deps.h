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

	tll::channel::ReopenData reopen;

	Shutdown shutdown = Shutdown::None;
	Worker * worker = nullptr;

	std::vector<Object *> depends;
	std::vector<Object *> rdepends;

	std::list<std::string> depends_names; //< Temporary storage used during initialization

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

	bool ready_open()
	{
		if (std::any_of(depends.begin(), depends.end(), [](auto & o) { return o->decay; }))
			return false;
		if (std::any_of(rdepends.begin(), rdepends.end(), [](auto & o) { return o->decay; }))
			return false;
		return std::all_of(depends.begin(), depends.end(), [](auto & o) { return o->state == state::Active; });
	}

	bool ready_close()
	{
		if (std::any_of(rdepends.begin(), rdepends.end(), [](auto & o) { return o->opening; }))
			return false;
		return std::all_of(rdepends.begin(), rdepends.end(), [](auto & o) { return o->state == state::Closed; });
	}

	void on_state(tll_state_t s)
	{
		reopen.on_state(s);
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
