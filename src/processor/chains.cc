/*
 * Copyright (c) 2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/logger/prefix.h"

#include "processor/chains.h"

using namespace tll::processor::_;

int Chains::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	tll::Channel::Url curl;
	curl.proto(url.proto());
	curl.host(url.host());

	for (auto & p : url.browse("*", true)) {
		if (p.first != "processor")
			curl.set(p.first, p.second.copy());
	}

	for (auto & p : url.browse("processor.*", true)) {
		if (p.first != "processor.chain")
			curl.set(p.first, p.second.copy());
	}

	std::map<std::string, Chain> chains;

	for (auto & p : url.browse("processor.chain.*", true)) {
		auto name = p.first.substr(strlen("processor.chain."));
		auto c = parse_chain(name, p.second);
		if (!c)
			return _log.fail(EINVAL, "Failed to parse chain {}", name);
		chains[name] = *c;
	}

	for (auto &[_, c] : chains) {
		for (auto & l : c.levels) {
			for (auto & s : l.spawn) {
				auto lname = fmt::format("{}/{}", c.name, l.name);
				auto it = chains.find(s);
				if (it == chains.end())
					return _log.fail(EINVAL, "Level '{}' spawns undefined chain '{}'", lname, s);
				if (it->second.spawned)
					return _log.fail(EINVAL, "Chain '{}' is spawned in several places: '{}' and '{}'", it->second.name, *it->second.spawned, lname);
				it->second.spawned = lname;
			}

			for (auto & s : l.join) {
				auto it = chains.find(s);
				if (it == chains.end())
					return _log.fail(EINVAL, "Level '{}/{}' joins undefined chain '{}'", c.name, l.name, s);
			}
		}
	}

	auto objects = *curl.sub("processor.objects", true);

	unsigned index = 0;
	for (auto & [_, c] : chains) {
		_log.debug("Dump chain {}", c.name);
		std::list<std::string> depends;
		if (c.spawned)
			depends = { "chains/" + *c.spawned };
		for (auto & l : c.levels) {
			_log.debug("Dump level {} (depends on {})", c.name, depends);
			auto lname = fmt::format("chains/{}/{}", c.name, l.name);
			tll::Config cfg;
			cfg.set("url", "null://");
			cfg.set("name", lname);

			for (auto & j : l.join)
				depends.push_back(fmt::format("chains/{}/{}", j, "_end"));

			if (depends.size())
				cfg.set("depends", tll::conv::to_string(depends));
			objects.set(lname, cfg);

			depends.clear();
			for (auto &[_, o] : l.objects) {
				o.depends.insert(lname);
				o.config.set("depends", tll::conv::to_string<std::list<std::string>>({o.depends.begin(), o.depends.end()}));

				o.config.set("name", o.name);
				objects.set(fmt::format("chains/{:04d}/{}", index++, o.name), o.config);
				depends.push_back(o.name);
			}

			if (!l.objects.size())
				depends.push_back(lname);
		}

		auto lname = fmt::format("chains/{}/{}", c.name, "_end");
		tll::Config cfg;
		cfg.set("url", "null://");
		cfg.set("name", lname);
		if (depends.size())
			cfg.set("depends", tll::conv::to_string(depends));
		objects.set(lname, cfg);
	}

	return tll::channel::Prefix<Chains>::_init(curl, master);
}

template <typename Log>
std::optional<Chains::Level> Chains::parse_level(Log &l, tll::ConstConfig &cfg)
{
	Level r;
	auto name = cfg.get("name");
	if (!name)
		return l.fail(std::nullopt, "No level name");
	if (*name == "_end")
		return l.fail(std::nullopt, "'_end' is reserved level name");
	r.name = *name;

	auto log = l.prefix("level {}", std::string_view(r.name));

	auto spawn = cfg.getT<std::list<std::string>>("spawn", {});
	if (!spawn)
		return _log.fail(std::nullopt, "Invalid spawn parameter: {}", spawn.error());
	//r.spawn = {spawn->begin(), spawn->end()};
	for (auto & i : *spawn) r.spawn.insert(std::string(tll::util::strip(i)));

	auto join = cfg.getT<std::list<std::string>>("join", {});
	if (!join)
		return _log.fail(std::nullopt, "Invalid join parameter: {}", join.error());
	//r.join = {join->begin(), join->end()};
	for (auto & i : *join) r.join.insert(std::string(tll::util::strip(i)));

	auto ocfg = cfg.sub("objects");
	if (!ocfg)
		return r;
	for (auto & p : ocfg->browse("*", true)) {
		Object o;
		o.name = p.first;
		o.config = p.second.copy();

		auto depends = cfg.getT<std::list<std::string>>("depends", {});
		if (!depends)
			return _log.fail(std::nullopt, "object {}: Invalid depends parameter: {}", o.name, depends.error());

		o.depends = {depends->begin(), depends->end()};

		r.objects[o.name] = o;
	}
	return r;
}

std::optional<Chains::Chain> Chains::parse_chain(std::string_view name, tll::ConstConfig &cfg)
{
	Chain r;
	r.name = name;
	auto log = _log.prefix("chain {}", name);

	for (auto & p : cfg.browse("*", true)) {
		auto l = parse_level(log, p.second);
		if (!l)
			return log.fail(std::nullopt, "Failed to parse level {}", p.first);
		r.levels.push_back(*l);
	}
	return r;
}
