/*
 * Copyright (c) 2019-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "processor/processor.h"
#include "processor/scheme.h"
#include "processor/worker.h"

#include "tll/processor/scheme.h"

#ifdef __linux__
#include <sched.h>
#endif

using namespace tll::processor::_;

TLL_DECLARE_IMPL(Processor);

int Worker::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	auto cpuset = reader.getT<std::list<unsigned>>("cpu", {});
	auto wname = reader.getT<std::string>("worker-name", name);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	config_info().set("worker-name", wname);
	_log = { fmt::format("tll.processor.worker.{}", wname) };

	for (auto i : cpuset) {
		if (i >= sizeof(_cpuset) * 8)
			return _log.fail(EINVAL, "CPU number too large: {}, max {}", i, sizeof(_cpuset) * 8 - 1);
		_cpuset |= (1ull << i);
	}

	auto lcfg = url.copy();
	lcfg.set("name", fmt::format("tll.processor.worker.{}.loop", wname));
	if (!lcfg.has("time-cache"))
		lcfg.set("time-cache", "yes");
	if (loop.init(lcfg))
		return _log.fail(EINVAL, "Failed to init processor loop");

	_with_fd = loop.poll_enable();
	_log.info("Worker in {} mode", _with_fd ? "polling" : "spinwait");

	auto curl = child_url_parse("ipc://;mode=client", "ipc");
	if (!curl)
		return _log.fail(EINVAL, "Failed to parse ipc url");
	_ipc = context().channel(*curl, master);
	if (!_ipc)
		return _log.fail(EINVAL, "Failed to create IPC client channel");
	_ipc->callback_add(this, TLL_MESSAGE_MASK_DATA);
	_child_add(_ipc.get(), "ipc");
	loop.add(self());

	if (_stat_enable) {
		_stat.emplace(name);
		internal.stat = &*_stat;
		if (loop.stat(stat()))
			return _log.fail(EINVAL, "Failed to enable stat on the loop");
	}
	return 0;
}

void Worker::_free()
{
	_ipc.reset();
	internal.stat = nullptr;
}

int Worker::_open(const ConstConfig &)
{
	if (_setaffinity())
		return EINVAL;

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

int Worker::_setaffinity()
{
	if (_cpuset == 0)
		return 0;
#ifdef __linux__
	cpu_set_t set;
	CPU_ZERO(&set);
	for (auto i = 0u; i < sizeof(_cpuset) * 8; i++) {
		if (_cpuset & (1ull << i))
			CPU_SET(i, &set);

	}
	if (sched_setaffinity(0, sizeof(set), &set))
		return _log.fail(EINVAL, "Failed to set CPU affinity: {}", strerror(errno));
#else
	_log.warning("CPU affinity not supported on this platform");
#endif
	return 0;
}

int Worker::callback(const Channel * c, const tll_msg_t * msg)
{
	if (msg->type != TLL_MESSAGE_DATA) return 0;
	if (state() != tll::state::Active)
		return 0;
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
		auto force = channel->state() == tll::state::Error || channel->state() == tll::state::Closing;
		_log.info("Deactivate object {}", channel->name());
		channel->close(force);
		break;
	}
	case scheme::Exit::id: {
		close();
		break;
	}
	case processor_scheme::StateUpdate::meta_id():
		break;
	case processor_scheme::MessageForward::meta_id(): {
		auto data = processor_scheme::MessageForward::bind(*msg);
		if (msg->size < data.meta_size())
			return _log.fail(EMSGSIZE, "Invalid message size: {} < min {}", msg->size, data.meta_size());
		auto name = data.get_dest();
		auto message = data.get_data();
		tll_msg_t m = {};
		m.type = message.get_type();
		m.msgid = message.get_msgid();
		m.seq = message.get_seq();
		m.addr.u64 = message.get_addr();
		m.data = message.get_data().data();
		m.size = message.get_data().size();
		for (auto & o : objects) {
			if (o->name() == name) {
				(*o)->post(&m);
				return 0;
			}
		}
		break;
	}
	default:
		_log.debug("Unknown message {}", msg->msgid);
	}
	return 0;
}
