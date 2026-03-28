/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "processor/deps.h"
#include "processor/worker.h"
#include "processor/scheme.h"

using namespace tll;
using namespace tll::processor::_;

int Object::init(const tll::Channel::Url &url)
{
	auto chain = tll::make_props_chain(url.sub("processor"), url, channel->context().config_defaults().sub("processor"));
	auto reader = tll::make_props_reader(chain);

	shutdown = reader.getT("shutdown-on", Shutdown::None, {{"none", Shutdown::None}, {"close", Shutdown::Close}, {"error", Shutdown::Error}});
	reopen.timeout_open = reader.getT("open-timeout", reopen.timeout_open);
	reopen.timeout_min = reader.getT("reopen-timeout", reopen.timeout_min);
	reopen.timeout_max = reader.getT("reopen-timeout-max", reopen.timeout_max);
	reopen.timeout_tremble = reader.getT("reopen-active-min", reopen.timeout_tremble);
	reopen.timeout_close = reader.getT("close-timeout", reopen.timeout_close);
	verbose = reader.getT("tll.processor-verbose", false);
	control_active = reader.getT("tll.processor.active-on-control", std::string {});
	if (!reader) {
		tll::Logger _log("tll.processor");
		return _log.fail(EINVAL, "Object '{}': Invalid parameters: {}", channel->name(), reader.error());
	}
	if (control_active.size()) {
		channel->callback_add<Object, &Object::callback_control>(this, TLL_MESSAGE_MASK_CONTROL);
		update_control_active();
	}
	return 0;
}

int Object::callback(const Channel *c, const tll_msg_t *msg)
{
	if (msg->type != TLL_MESSAGE_STATE) return 0;
	auto s = (tll_state_t) msg->msgid;

	if (s == tll::state::Active && !control_active.empty()) {
		update_control_active();
		if (control_active_msgid != 0) {
			worker->logger().info("Channel '{}' has active-on-control message '{}', ignore Active", name(), control_active);
			return 0;
		}
	} else if (s == tll::state::Opening)
		control_active_sent = false;

	if (s == tll::state::Closing) {
		if (shutdown <= Shutdown::Close)
			worker->post(scheme::Exit { 0, get() });
	} else if (s == tll::state::Error) {
		if (shutdown <= Shutdown::Error)
			worker->post(scheme::Exit { 1, get() });
	}

	return callback_common(c, s);
}

int Object::callback_common(const tll::Channel *c, tll_state_t s)
{
	scheme::State data;
	data.channel = c;
	data.worker = worker;
	data.state = s;
	worker->post(data);

	if (auto block = worker->stat(); block) {
		if (auto page = block->acquire(); page) {
			page->state.update(1);
			if (s == tll::state::Error)
				page->error.update(1);
			block->release(page);
		}
	}

	return 0;
}

int Object::callback_control(const Channel *c, const tll_msg_t *msg)
{
	if (msg->type != TLL_MESSAGE_CONTROL) return 0;
	if (control_active_msgid && msg->msgid == control_active_msgid && !control_active_sent) {
		worker->logger().info("Channel '{}' got active control message '{}'", name(), control_active);
		control_active_sent = true;
		return callback_common(c, tll::state::Active);
	}
	return 0;
}

void Object::update_control_active()
{
	if (control_active.empty())
		return;
	auto s = channel->scheme(TLL_MESSAGE_CONTROL);
	if (!s)
		return;
	auto r = 0;
	if (auto m = s->lookup(control_active); m)
		r = m->msgid;
	control_active_msgid = r;
}
