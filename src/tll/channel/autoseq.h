/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_AUTOSEQ_H
#define _TLL_CHANNEL_AUTOSEQ_H

#include "tll/channel/base.h"

namespace tll::channel {

namespace autoseq {
struct AutoSeq
{
	tll_msg_t msg = {};
	long long seq = -1;
	bool enable = true;

	long long reset(long long s)
	{
		std::swap(seq, s);
		return s;
	}

	const tll_msg_t * update(const tll_msg_t * m)
	{
		if (!enable)
			return m;
		msg = *m;
		msg.seq = ++seq;
		return &msg;
	}
};

};

/// Channel mixin that tracks incoming seq numbers and increments them
template <typename T, typename S = Base<T>>
class AutoSeq : public S
{
 protected:
	autoseq::AutoSeq _autoseq = {};

 public:
	using Base = S;

	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{
		auto reader = this->channel_props_reader(url);
		_autoseq.enable = reader.getT("autoseq", false);
		if (!reader)
			return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

		return Base::_init(url, master);
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		if (msg->type != TLL_MESSAGE_DATA)
			return Base::_post(msg, flags);
		return Base::_post(_autoseq.update(msg), flags);
	}
};

} // namespace tll::channel

#endif//_TLL_CHANNEL_AUTOSEQ_H
