/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/util/url.h"

#include "processor/deps.h"
#include "processor/worker.h"
#include "processor/scheme.h"

using namespace tll;
using namespace tll::processor::_;

int Object::init(const tll::Channel::Url &url)
{
	auto reader = tll::make_props_reader(url);
	shutdown = reader.getT("shutdown-on", Shutdown::None, {{"none", Shutdown::None}, {"close", Shutdown::Close}, {"error", Shutdown::Error}});
	reopen.timeout_min = reader.getT("reopen-timeout", reopen.timeout_min);
	reopen.timeout_max = reader.getT("reopen-timeout-max", reopen.timeout_max);
	reopen.timeout_tremble = reader.getT("reopen-active-min", reopen.timeout_tremble);
	verbose = reader.getT("tll.processor-verbose", false);
	if (!reader) {
		tll::Logger _log("tll.processor");
		return _log.fail(EINVAL, "Object '{}': Invalid parameters: {}", channel->name(), reader.error());
	}
	return 0;
}

int Object::callback(const Channel *c, const tll_msg_t *msg)
{
	if (msg->type != TLL_MESSAGE_STATE) return 0;
	auto s = (tll_state_t) msg->msgid;

	if (s == tll::state::Closing) {
		if (shutdown <= Shutdown::Close)
			worker->post(scheme::Exit { 0, get() });
	} else if (s == tll::state::Error) {
		if (shutdown <= Shutdown::Error)
			worker->post(scheme::Exit { 1, get() });
	}

	scheme::State data;
	data.channel = c;
	data.worker = worker;
	data.state = s;
	worker->post(data);

	return 0;
}
