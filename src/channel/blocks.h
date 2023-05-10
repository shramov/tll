/*
 * Copyright (c) 2023 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_BLOCKS_H
#define _TLL_CHANNEL_BLOCKS_H

#include "tll/channel/base.h"

namespace tll::channel {

class Blocks : public tll::channel::Base<Blocks>
{
	using Base = tll::channel::Base<Blocks>;

	Blocks * _master = nullptr;

	long long _seq = -1;
	std::map<std::string, std::list<long long>, std::less<>> _blocks;
	std::string _filename;
	std::string _default_type;

 public:
	static constexpr std::string_view channel_protocol() { return "blocks"; }
	static constexpr auto open_policy() { return OpenPolicy::Manual; }
	static constexpr auto process_policy() { return ProcessPolicy::Never; }

	int _init(const tll::Channel::Url &url, tll::Channel * master);
	int _open(const tll::ConstConfig &);
	int _open_input(const tll::ConstConfig &);
	int _close();

	int _post(const tll_msg_t *msg, int flags);

	int _create_block(std::string_view block, long long seq, bool store = true);
};

} // namespace tll::channel

#endif//_TLL_CHANNEL_BLOCKS_H
