/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_MEM_H
#define _TLL_CHANNEL_MEM_H

#include "tll/channel/base.h"

#include "tll/ring.h"

class ChMem : public tll::channel::Base<ChMem>
{
	int &_fd_in = internal.fd;
	int _fd_out = -1;
	bool _with_fd = true;
	size_t _size = 1024;
	std::shared_ptr<ringbuffer_t> _rin;
	std::shared_ptr<ringbuffer_t> _rout;
	ChMem * master = nullptr;

 public:
	static constexpr std::string_view param_prefix() { return "mem"; }

	int _init(const tll::UrlView &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags);
};

//extern template class tll::channel::Base<ChMem>;

#endif//_TLL_CHANNEL_MEM_H
