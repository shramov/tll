/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_MEM_H
#define _TLL_CHANNEL_MEM_H

#include "tll/channel/event.h"

#include "tll/ring.h"

#include <mutex>

class ChMem : public tll::channel::Event<ChMem>
{
	struct Ring {
		Ring(size_t size) { ring_init(&ring, size, 0); }
		~Ring()
		{
			ring_free(&ring);
			notify.close();
		}

		ringbuffer_t ring = {};
		tll::channel::EventNotify notify;
	};

	size_t _size = 1024;
	std::mutex _mutex; // shared_ptr initialization lock
	auto _lock() { return std::unique_lock<std::mutex>(_mutex); }
	std::shared_ptr<Ring> _rin;
	std::shared_ptr<Ring> _rout;
	bool _child = false;
	ChMem * _sibling = nullptr;
	bool _empty() const;

 public:
	static constexpr std::string_view channel_protocol() { return "mem"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();
	void _free();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags);
};

//extern template class tll::channel::Base<ChMem>;

#endif//_TLL_CHANNEL_MEM_H
