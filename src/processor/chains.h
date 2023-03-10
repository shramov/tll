/*
 * Copyright (c)2020-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _PROCESSOR_CHAINS_H
#define _PROCESSOR_CHAINS_H

#include <set>

#include "tll/channel/prefix.h"

namespace tll::processor::_ {

class Chains : public tll::channel::Prefix<Chains>
{
public:
	static constexpr std::string_view channel_protocol() { return "ppp-chains+"; }

	struct Object
	{
		std::string name;
		tll::Config config;
		std::set<std::string> depends;
	};

	struct Level
	{
		std::string name;
		std::set<std::string> join;
		std::set<std::string> spawn;
		std::map<std::string, Object> objects;
	};

	struct Chain
	{
		std::string name;
		std::list<Level> levels;
		std::optional<std::string> spawned;
	};

	int _init(const tll::Channel::Url &, tll::Channel *);

	template <typename Log>
	std::optional<Level> parse_level(Log &, tll::Config &cfg);
	std::optional<Chain> parse_chain(std::string_view name, tll::Config &cfg);
};

} // namespace tll::processor::_

#endif//_PROCESSOR_CHAINS_H
