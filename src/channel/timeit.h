/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_TIMEIT_H
#define _TLL_CHANNEL_TIMEIT_H

#include "tll/channel/prefix.h"

class ChTimeIt : public tll::channel::Prefix<ChTimeIt>
{
 public:
	using Base = tll::channel::Prefix<ChTimeIt>;
	static constexpr std::string_view channel_protocol() { return "timeit+"; }
	// Use timeit+ as test for Extend policy until normal user appears
	static constexpr auto prefix_config_policy() { return PrefixConfigPolicy::Extend; }
	static constexpr auto prefix_export_policy() { return PrefixExportPolicy::Strip; }

	struct StatType : public Base::StatType
	{
		tll::stat::IntegerGroup<tll::stat::Ns, 'r', 'x', 't'> rx;
		tll::stat::IntegerGroup<tll::stat::Ns, 't', 'x', 't'> tx;
	};
	tll::stat::BlockT<StatType> * stat() { return static_cast<tll::stat::BlockT<StatType> *>(this->internal.stat); }

	int _post(const tll_msg_t *msg, int flags);
	int _on_data(const tll_msg_t *msg);
};

#endif//_TLL_CHANNEL_TIMEIT_H
