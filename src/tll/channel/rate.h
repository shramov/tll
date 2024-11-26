/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_RATE_H
#define _TLL_CHANNEL_RATE_H

#include "tll/util/time.h"

namespace tll::channel::rate {

struct Settings
{
	using fseconds = std::chrono::duration<double>;

	enum class Unit { Byte, Message } unit = Unit::Byte; ///< What to count - bytes or messages
	double speed = 0; ///< Tickets per second
	long long limit = 1000; ///< Maximum number of tickets
	long long initial = 500; ///< Initial number of tickets
	long long watermark = 1; ///< High watermark, when to report availability of send

	tll::duration size2time(size_t size) const
	{
		return std::chrono::duration_cast<tll::duration>(fseconds(size) / speed);
	}

	long long time2size(tll::duration dt) const
	{
		return (fseconds(dt) * speed).count();
	}
};

struct Bucket
{
	long long tickets = 0; ///< Current number of tickets
	tll::time_point last = {}; ///< Last update timestamp

	void reset()
	{
		last = {};
		tickets = 1;
	}

	/// Update number of available tickets
	void update(const Settings &conf, const tll::time_point &now)
	{
		if (last.time_since_epoch().count() == 0) {
			last = now;
			tickets = conf.initial;
			return;
		}
		auto ds = conf.time2size(now - last);
		if (ds == 0)
			return;
		last = now;
		tickets = std::min(conf.limit, tickets + ds);
	}

	/// Get time to next available send
	tll::duration next(const Settings &conf, const tll::time_point &now) const
	{
		if (tickets > 0)
			return {};
		return conf.size2time(conf.watermark - tickets);
	}

	/// Check if send is not available
	bool empty() const { return tickets <= 0; }

	/// Consume tickets
	void consume(size_t size) { tickets -= size; }
};

} // namespace tll::channel::rate

#endif//_TLL_CHANNEL_RATE_H
