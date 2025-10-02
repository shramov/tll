/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_NULL_H
#define _TLL_CHANNEL_NULL_H

#include "tll/channel/base.h"

class ChNull : public tll::channel::Base<ChNull>
{
 public:
	static constexpr std::string_view channel_protocol() { return "null"; }
	static constexpr auto process_policy() { return ProcessPolicy::Never; }

	int _init(const tll::Channel::Url &, tll::Channel *master) { return 0; }

	int _open(const tll::ConstConfig &)
	{
		tll::Config cfg;
		cfg.set("init.tll.proto", "null");
		if (_scheme) {
			auto full = _scheme->dump("yamls+gz");
			if (auto s = _scheme->dump("sha256"); s) {
				cfg.set("init.scheme", *s);
				cfg.sub("scheme", true)->set(*s, *full);
			} else if (full)
				cfg.set("init.scheme", *full);
		}
		_config.set("client", cfg);
		return 0;
	}

	int _close()
	{
		_config.unlink("client");
		return 0;
	}

	int _process(long timeout, int flags) { return 0; }
	int _post(const tll_msg_t *msg, int flags) { return 0; }
};

#endif//_TLL_CHANNEL_NULL_H
