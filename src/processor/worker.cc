/*
 * Copyright (c) 2019-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "processor/processor.h"
#include "processor/scheme.h"
#include "processor/worker.h"

using namespace tll::processor::_;

TLL_DECLARE_IMPL(Processor);

int Worker::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto wname = url.getT<std::string>("worker-name", name);
	if (!wname)
		return _log.fail(EINVAL, "Invalid worker-name parameter", wname.error());

	_log = { fmt::format("tll.processor.worker.{}", *wname) };

	auto lcfg = url.copy();
	lcfg.set("name", fmt::format("tll.processor.worker.{}.loop", *wname));
	if (!lcfg.has("time-cache"))
		lcfg.set("time-cache", "yes");
	if (loop.init(lcfg))
		return _log.fail(EINVAL, "Failed to init processor loop");

	_ctx = tll::channel_cast<Processor>(master);
	if (!_ctx)
		return _log.fail(EINVAL, "Invalid master channel, expected processor");
	_ctx = tll::channel_cast<Processor>(master);
	if (!_ctx)
		return _log.fail(EINVAL, "Invalid master channel, expected processor");
	_ipc = context().channel(fmt::format("ipc://;mode=client;dump=no;tll.internal=yes;name={}/ipc", name), master);
	if (!_ipc)
		return _log.fail(EINVAL, "Failed to create IPC client channel");
	_ipc->callback_add(this, TLL_MESSAGE_MASK_DATA);
	_child_add(_ipc.get(), "ipc");
	loop.add(self());
	return 0;
}

int Worker::_open(const ConstConfig &)
{
	loop.stop = false;
	if (loop.time_cache_enable)
		tll::time::cache_enable(true);

	if (_ipc->open())
		return _log.fail(EINVAL, "Failed to open IPC client channel");
	for (auto & o : objects)
		_child_add(o->channel.get());
	state(tll::state::Active);
	return post(scheme::WorkerState { tll::state::Active, this });
}

int Worker::_close()
{
	_log.debug("Post worker state Closed");
	post<scheme::WorkerState>({tll::state::Closed, this});
	_ipc->close();
	_log.debug("Stop loop");
	loop.stop = true;
	if (loop.time_cache_enable)
		tll::time::cache_enable(false);

	loop.del(self());
	return 0;
}

int Worker::callback(const Channel * c, const tll_msg_t * msg)
{
	if (msg->type != TLL_MESSAGE_DATA) return 0;
	switch (msg->msgid) {
	case scheme::Activate::id: {
		auto data = (const scheme::Activate *) msg->data;
		if (data->obj->worker != this) return 0;
		_log.info("Activate object {}", data->obj->name());
		data->obj->open();
		break;
	}
	case scheme::Deactivate::id: {
		auto data = (const scheme::Deactivate *) msg->data;
		if (data->obj->worker != this) return 0;
		auto & channel = (*data->obj);
		auto force = channel->state() == tll::state::Error;
		_log.info("Deactivate object {}", channel->name());
		channel->close(force);
		break;
	}
	case scheme::Exit::id: {
		close();
		break;
	}
	default:
		_log.debug("Unknown message {}", msg->msgid);
	}
	return 0;
}
