/*
 * Copyright (c) 2023 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef __LOGIC_QUANTILE_H
#define __LOGIC_QUANTILE_H

#include <tll/channel/tagged.h>

using tll::channel::TaggedChannel;

using tll::channel::Input;
struct Timer : public tll::channel::Tag<TLL_MESSAGE_MASK_DATA> { static constexpr std::string_view name() { return "timer"; } };

class Quantile : public tll::channel::Tagged<Quantile, Input, Timer>
{
	using Base = tll::channel::Tagged<Quantile, Input, Timer>;

	struct Bucket
	{
		long count = 0;
		std::vector<unsigned> data;

		void reset()
		{
			count = 0;
			data.resize(0);
		}

		void push(size_t idx)
		{
			count++;
			if (data.size() < idx + 1)
				data.resize(idx + 1);
			data[idx]++;
		}
	};

	struct Buckets
	{
		Bucket local;
		Bucket global;

		Buckets(size_t skip) { global.count = -skip; }
	};

	std::map<std::string, Buckets, std::less<>> _data;

	size_t _skip = 1000;
	std::vector<unsigned> _quantiles;

 public:
	static constexpr std::string_view channel_protocol() { return "quantile"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &props);
	int _close();

	int callback_tag(TaggedChannel<Input> * c, const tll_msg_t *msg);
	int callback_tag(TaggedChannel<Timer> * c, const tll_msg_t *msg);

	int _report(std::string_view name, Bucket &bucket, bool global = false);
};

#endif//__LOGIC_QUANTILE_H
