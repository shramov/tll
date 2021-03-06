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
		for (auto & p : url.browse("module.*", true)) {
			auto m = p.second.get("module");
			if (!m) continue;
			if (context().load(std::string(*m), "channel_module"))
				return _log.fail(EINVAL, "Failed to load module {}", *m);
		}
		return 0;
	}
};

} // namespace tll

#endif//_CHANNEL_LOADER_H
