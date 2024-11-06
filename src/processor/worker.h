/*
 * Copyright (c) 2019-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _PROCESSOR_WORKER_H
#define _PROCESSOR_WORKER_H

#include <string>

#include "processor/deps.h"
#include "tll/channel/base.h"
#include "tll/processor/loop.h"

namespace tll::processor::_ {

struct Processor;

struct Worker : public tll::channel::Base<Worker>
{
	static constexpr auto open_policy() { return OpenPolicy::Manual; }
	static constexpr auto process_policy() { return ProcessPolicy::Never; }
	static constexpr auto child_policy() { return ChildPolicy::Many; }
	static constexpr auto stat_policy() { return StatPolicy::Manual; }
	static constexpr std::string_view channel_protocol() { return "tll.worker"; }

	tll::processor::Loop loop;

	struct StatType : public Base::StatType
	{
		tll::stat::Integer<tll::stat::Sum, tll::stat::Unknown, 's', 't', 'e', 'p'> step;
		tll::stat::Integer<tll::stat::Sum, tll::stat::Ns, 'p', 'o', 'l', 'l'> poll;
		tll_stat_field_t padding[2] = {};
	};

	std::optional<tll::stat::Block<StatType>> _stat;

	std::list<Object *> objects;
	struct {
		tll_state_t state = TLL_STATE_CLOSED;
		tll_addr_t addr = {};
	} proc;

	std::unique_ptr<tll::Channel> _ipc;

	unsigned long long _cpuset;

	int _init(const tll::Channel::Url &url, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();
	void _free();

	using tll::channel::Base<Worker>::post;

	template <typename T> int post(const T& body)
	{
		if (state() == tll::state::Closed)
			return 0;
		tll_msg_t msg = { TLL_MESSAGE_DATA };
		msg.msgid = T::id;
		msg.data = (void *) &body;
		msg.size = sizeof(body);
		return _ipc->post(&msg);
	}

	friend struct tll::CallbackT<Worker>;
	int callback(const Channel * c, const tll_msg_t * msg);

	int _setaffinity();
};

} // namespace tll

#endif//_PROCESSOR_WORKER_H
