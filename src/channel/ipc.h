/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
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
#include <mutex>

class ChIpcServer;

template <typename T>
struct lqueue_ref : public lqueue<T>, public tll::util::refbase_t<lqueue_ref<T>, 0>
{
	tll::channel::EventNotify event;
	~lqueue_ref() { event.close(); }
};

class ChIpc : public tll::channel::Event<ChIpc>
{
 public:
	template <typename E> using queue_t = lqueue_ref<tll::util::OwnedMessage>;
	using squeue_t = queue_t<ChIpcServer>;
	using cqueue_t = queue_t<ChIpc>;
	template <typename T> using refptr_t = tll::util::refptr_t<T>;

	struct marker_queue_t : public MarkerQueue<squeue_t *, nullptr>
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

	refptr_t<cqueue_t> _qin;
	refptr_t<squeue_t> _qout;
	std::shared_ptr<marker_queue_t> _markers;
	ChIpcServer * master = nullptr;

 public:
	static constexpr std::string_view param_prefix() { return "ipc"; }

	tll_channel_impl_t * _init_replace(const tll::Channel::Url &url);
	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags);
};

class ChIpcServer : public tll::channel::Event<ChIpcServer>
{
	friend class ChIpc;
	size_t _size = 1024;
	tll_addr_t _addr = {};

	template <typename T> using refptr_t = tll::util::refptr_t<T>;
	std::shared_ptr<ChIpc::marker_queue_t> _markers;
	std::mutex _lock;
	std::map<long long, refptr_t<ChIpc::cqueue_t>> _clients;
 public:
	static constexpr std::string_view param_prefix() { return "ipc"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);

	int _open(const tll::PropsView &);
	int _close();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags);
};

//extern template class tll::channel::Base<ChIpcServer>;

#endif//_TLL_CHANNEL_IPC_H
