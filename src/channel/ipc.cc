/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/ipc.h"

#include "tll/channel/event.hpp"
#include "tll/scheme/channel/ipc.h"
#include "tll/util/size.h"

#include <unistd.h>

using namespace tll;

TLL_DEFINE_IMPL(ChIpc);
TLL_DEFINE_IMPL(ChIpcServer);

std::optional<const tll_channel_impl_t *> ChIpc::_init_replace(const Channel::Url &url, tll::Channel *master)
{
	auto client = url.getT("mode", true, {{"client", true}, {"server", false}});
	if (!client)
		return _log.fail(std::nullopt, "Invalid mode field: {}", client.error());
	if (!*client)
		return &ChIpcServer::impl;
	return nullptr;
}

int ChIpc::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	this->master = channel_cast<ChIpcServer>(master);
	if (!this->master)
		return _log.fail(EINVAL, "Parent {} must be ipc://;mode=server channel", master?master->name():"NULL");
	_log.debug("Init child of master {}", tll_channel_name(master));

	return Event<ChIpc>::_init(url, master);
}

int ChIpc::_open(const ConstConfig &url)
{
	if (master->state() != TLL_STATE_ACTIVE)
		return _log.fail(EINVAL, "Parent is not active: {}", tll_state_str(master->state()));

	if (Event<ChIpc>::_open(url))
		return _log.fail(EINVAL, "Failed to open event parent");

	_queue.reset(new QueuePair);
	_queue->client.event = master->event_detached();;
	_queue->server.event = this->event_detached();
	_markers = master->_markers;
	_addr = master->addr();

	if (!_scheme && master->_scheme) {
		_log.debug("Inherit scheme from master {}", master->name);
		_scheme.reset(master->_scheme->ref());
	}

	_post_control(ipc_scheme::Connect::meta_id());
	return 0;
}

int ChIpc::_close()
{
	if (_markers)
		_post_control(ipc_scheme::Disconnect::meta_id());
	_queue.reset(nullptr);
	_markers.reset();
	return Event<ChIpc>::_close();
}

int ChIpc::_post_nocheck(const tll_msg_t *msg, int flags)
{
	tll::util::OwnedMessage m(msg);
	m.addr = _addr;
	auto ref = _queue;
	if (_markers->push(ref.get()))
		return EAGAIN;
	ref.release();
	_log.trace("Notify fd {}", _queue->client.event.fd);
	if (_queue->client.event.notify())
		_log.error("Failed to arm event");
	_queue->client.push(std::move(m));
	return 0;
}

int ChIpc::_process(long timeout, int flags)
{
	auto ref = _queue;
	auto msg = _queue->server.pop();
	if (!msg)
		return EAGAIN;

	_callback_data(*msg);

	return event_clear_race([&ref]() -> bool { return !ref->server.empty(); });
}

int ChIpcServer::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	_size = reader.getT<tll::util::Size>("size", 64 * 1024);
	_broadcast = reader.getT("broadcast", false);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	_scheme_control.reset(this->context().scheme_load(ipc_scheme::scheme_string));
	if (!_scheme_control.get())
		return _log.fail(EINVAL, "Failed to load control scheme");

	return Event<ChIpcServer>::_init(url, master);
}

int ChIpcServer::_open(const ConstConfig &url)
{
	_addr = {};
	_clients.clear();
	_markers.reset(new ChIpc::marker_queue_t(_size));
	if (Event<ChIpcServer>::_open(url))
		return _log.fail(EINVAL, "Failed to open event parent");
	return 0;
}

int ChIpcServer::_close()
{
	Event<ChIpcServer>::_close();
	_clients.clear();
	_markers.reset();
	_addr = {};
	return 0;
}

int ChIpcServer::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	if (msg->addr.u64 == 0 && _broadcast) {
		for (auto & [addr, c] : _clients) {
			c->server.push(tll::util::OwnedMessage(msg));
			if (c->server.event.notify())
				_log.warning("Failed to arm event for client {}", addr);
		}
		return 0;
	}

	auto it = _clients.find(msg->addr.u64);
	if (it == _clients.end()) return ENOENT;
	it->second->server.push(tll::util::OwnedMessage(msg));
	if (it->second->server.event.notify())
		return _log.fail(EINVAL, "Failed to arm event");
	return 0;
}

int ChIpcServer::_process(long timeout, int flags)
{
	auto markers = _markers;
	auto q = markers->pop();
	if (!q)
		return EAGAIN;
	auto msg = q->client.pop();
	while (!msg) {
		msg = q->client.pop();
	}

	if (msg->type != TLL_MESSAGE_DATA) {
		if (msg->type == TLL_MESSAGE_CONTROL) {
		switch (msg->msgid) {
		case ipc_scheme::Connect::meta_id():
			_log.info("Connected client {}", msg->addr.u64);
			_clients.emplace(msg->addr.u64, q);
			break;
		case ipc_scheme::Disconnect::meta_id():
			_log.info("Disconnected client {}", msg->addr.u64);
			_clients.erase(msg->addr.u64);
			break;
		}
		}
		_callback(*msg);
	} else
		_callback_data(*msg);
	q->unref();

	return event_clear_race([&markers]() -> bool { return !markers->empty(); });
}
