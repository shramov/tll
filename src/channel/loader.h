/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _CHANNEL_LOADER_H
#define _CHANNEL_LOADER_H

#include "tll/channel/base.h"

namespace tll {

class ChLoader : public tll::channel::Base<ChLoader>
{
public:
	static constexpr auto process_policy() { return ProcessPolicy::Never; }
	static constexpr std::string_view channel_protocol() { return "loader"; }

	int _init(const tll::Channel::Url &url, tll::Channel *)
	{
		for (auto &[_, mcfg] : url.browse("module.*", true)) {
			auto m = mcfg.get("module");
			if (!m) continue;
			auto enable = mcfg.getT("enable", true);
			if (!enable)
				return _log.fail(EINVAL, "Invalid 'enable' parameter: {}", enable.error());
			if (!*enable)
				continue;
			auto extra = mcfg.sub("config");
			if (context().load(std::string(*m), extra.value_or(tll::ConstConfig())))
				return _log.fail(EINVAL, "Failed to load module {}", *m);
		}
		auto aliases = url.sub("alias");
		if (!aliases) return 0;
		for (auto & [k, c] : aliases->browse("*", true)) {
			auto v = c.get();
			if (v) {
				if (context().alias_reg(k, *v))
					return _log.fail(EINVAL, "Failed to register alias {} -> {}", k, *v);
			} else if (context().alias_reg(k, c))
				return _log.fail(EINVAL, "Failed to register alias {}", k);
		}
		return 0;
	}
};

} // namespace tll

#endif//_CHANNEL_LOADER_H
