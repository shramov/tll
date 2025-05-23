/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_IPC_H
#define _TLL_CHANNEL_IPC_H

#include "tll/channel/base.h"
#include "tll/channel/event.h"

#include "tll/util/lqueue.h"
#include "tll/util/markerqueue.h"
#include "tll/util/ownedmsg.h"
#include "tll/util/refptr.h"

#include <memory>
#include <atomic>

class ChIpcServer;

struct EventQueue : public lqueue<tll::util::OwnedMessage>
{
	tll::channel::EventNotify event;
	~EventQueue() { event.close(); }
};

struct QueuePair : public tll::util::refbase_t<QueuePair, 0>
{
	EventQueue server;
	EventQueue client;
};

class ChIpc : public tll::channel::Event<ChIpc>
{
 public:
	template <typename T> using refptr_t = tll::util::refptr_t<T>;

	struct marker_queue_t : public MarkerQueue<QueuePair *, nullptr>
	{
		using MarkerQueue::MarkerQueue;
		~marker_queue_t()
		{
			while (auto q = pop())
				q->unref();
		}
	};

 private:
	tll_addr_t _addr = {};

	refptr_t<QueuePair> _queue;
	std::shared_ptr<marker_queue_t> _markers;
	ChIpcServer * master = nullptr;

 public:
	static constexpr std::string_view channel_protocol() { return "ipc"; }

	std::optional<const tll_channel_impl_t *> _init_replace(const tll::Channel::Url &url, tll::Channel *master);
	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags)
	{
		if (msg->type != TLL_MESSAGE_DATA)
			return _log.fail(EINVAL, "Non-data messages are not supported");
		return _post_nocheck(msg, flags);
	}

	int _post_nocheck(const tll_msg_t *msg, int flags);
	int _post_control(int msgid)
	{
		tll_msg_t msg = { TLL_MESSAGE_CONTROL };
		msg.msgid = msgid;
		return _post_nocheck(&msg, 0);
	}
};

class ChIpcServer : public tll::channel::Event<ChIpcServer>
{
	friend class ChIpc;
	size_t _size = 1024;
	std::atomic<uint64_t> _addr = {};

	bool _broadcast = false;

	template <typename T> using refptr_t = tll::util::refptr_t<T>;
	std::shared_ptr<ChIpc::marker_queue_t> _markers;
	std::map<long long, refptr_t<QueuePair>> _clients;
 public:
	static constexpr std::string_view channel_protocol() { return "ipc"; }
	static constexpr std::string_view scheme_control_string();

	int _init(const tll::Channel::Url &, tll::Channel *master);

	int _open(const tll::ConstConfig &);
	int _close();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags);

	tll_addr_t addr() { return tll_addr_t { ++_addr }; }
};

//extern template class tll::channel::Base<ChIpcServer>;

#endif//_TLL_CHANNEL_IPC_H
