/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _PROCESSOR_CONTEXT_H
#define _PROCESSOR_CONTEXT_H

#include "tll/channel/base.h"
#include "tll/channel/impl.h"
#include "tll/channel/logic.h"

#include "tll/processor.h"
#include "tll/processor/loop.h"
#include "tll/logger.h"

#include "processor/deps.h"
#include "processor/worker.h"

#include <map>
#include <list>

namespace tll::processor::_ {

struct Processor : public tll::channel::Base<Processor>
{
	static constexpr auto open_policy() { return OpenPolicy::Manual; }
	static constexpr auto close_policy() { return ClosePolicy::Long; }
	static constexpr auto process_policy() { return ProcessPolicy::Never; }
	static constexpr auto child_policy() { return ChildPolicy::Single; } // Set Proxy cap to access IPC child channel
	static constexpr std::string_view param_prefix() { return "processor"; }

	tll::processor::Loop loop;
	tll::Config _cfg;
	std::list<Object> _objects;
	std::multimap<tll::time_point, Object *> _pending;

	tll_channel_t context_channel = {};
	tll_channel_internal_t context_internal = { TLL_STATE_CLOSED };

	std::list<std::unique_ptr<tll::Channel>> _workers_ptr;
	std::map<std::string, tll::processor::_::Worker *, std::less<>> _workers;
	std::unique_ptr<tll::Channel> _ipc;
	std::unique_ptr<tll::Channel> _timer;

	~Processor() { _free(); }

	std::optional<tll::Channel::Url> parse_common(std::string_view type, std::string_view path, const Config &cfg);
	int parse_deps(Object &obj, const Config &cfg);

	int init_one(std::string_view name, const Config &cfg, bool logic);
	int init_depends();
	Worker * init_worker(std::string_view name);

	void decay(Object * obj, bool root = false);
	void activate(Object & obj);

	int build_rdepends();

	int _init(const tll::Channel::Url &, tll::Channel *);
	int _open(const tll::PropsView &);
	int _close(bool force);
	void _close_workers();
	void _free();

	void activate();
	void update(const Channel *, tll_state_t state);

	Object * find(const Channel *c)
	{
		for (auto & i : _objects) {
			if (i.get() == c)
				return &i;
		}
		return nullptr;
	}

	Object * find(std::string_view name)
	{
		for (auto & i : _objects) {
			if (i->name() == name)
				return &i;
		}
		return nullptr;
	}

	friend struct tll::CallbackT<Processor>;
	int cb(const Channel * c, const tll_msg_t * msg);

	using tll::channel::Base<Processor>::post;

	template <typename T> int post(tll_addr_t addr, T body);
	template <typename T> int post(const Object *o, T body) { return post<T>(o->worker->proc.addr, body); }

	bool pending_has(const tll::time_point &ts, const Object * o)
	{
		auto it = _pending.find(ts);
		if (it == _pending.end())
			return false;
		for (; it->first == ts; it++) {
			if (it->second == o)
				return true;
		}
		return false;
	}

	void pending_add(tll::time_point ts, Object * o);
	void pending_del(const tll::time_point &ts, const Object * o);
	int pending_rearm(const tll::time_point &ts);

	static int pending_process(const tll_channel_t * c, const tll_msg_t * msg, void * user)
	{
		if (msg->type != TLL_MESSAGE_DATA)
			return 0;
		return static_cast<Processor *>(user)->pending_process(msg);
	}

	int pending_process(const tll_msg_t * msg);
};

} // namespace tll::processor

#endif//_PROCESSOR_CONTEXT_H
