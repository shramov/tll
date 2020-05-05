/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/mem.h"

#include "tll/channel/event.hpp"
#include "tll/util/size.h"

using namespace tll;

TLL_DEFINE_IMPL(ChMem);

int ChMem::_init(const UrlView &url, tll::Channel *master)
{
	if (master) {
		_sibling = channel_cast<ChMem>(master);
		if (!_sibling)
			return _log.fail(EINVAL, "Parent {} must be mem:// channel", master->name());
		_log.debug("Init child of master {}", this->_sibling->name);
		_child = true;
		_sibling->_sibling = this;
		return 0;
	}
	auto reader = channel_props_reader(url);
	_size = reader.getT("size", util::Size {64 * 1024});
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	return Event<ChMem>::_init(url, master);
}

void ChMem::_free()
{
	if (_child) {
		_log.info("Remove sibling reference from master {}", _sibling->name);
		_sibling->_sibling = nullptr;
	}
}

namespace {
ringbuffer_t * ring_new(size_t size)
{
	auto ptr = std::make_unique<ringbuffer_t>();
	if (ring_init(ptr.get(), size, 0))
		return 0;
	return ptr.release();
}

void ring_delete(ringbuffer_t *ring)
{
	if (!ring) return;
	ring_free(ring);
	delete ring;
}

}

bool ChMem::_empty() const
{
	const void * data;
	size_t size;
	return ring_read(_rin.get(), &data, &size) == EAGAIN;
}

int ChMem::_open(const PropsView &url)
{
	if (_child) {
		_rin = _sibling->_rout;
		_rout = _sibling->_rin;
	} else {
		_rin.reset(ring_new(_size), ring_delete);
		_rout.reset(ring_new(_size), ring_delete);
		if (!_rin.get() || !_rout.get())
			return _log.fail(EINVAL, "Failed to create buffers");
	}

	if (Event<ChMem>::_open(url))
		return _log.fail(EINVAL, "Failed to open event");

	if (!_empty()) {
		_log.debug("Pending data, arm notification");
		event_notify();
	}

	state(TLL_STATE_ACTIVE);
	return 0;
}

int ChMem::_close()
{
	Event<ChMem>::_close();
	_rin.reset();
	_rout.reset();
	return 0;
}

int ChMem::_post(const tll_msg_t *msg, int flags)
{
	void * data;
	if (int r = ring_write_begin(_rout.get(), &data, msg->size)) {
		if (r == EAGAIN)
			return r;
		return _log.fail(r, "Failed to allocate message: {}", strerror(r));
	}
	memcpy(data, msg->data, msg->size);
	ring_write_end(_rout.get(), data, msg->size);
	if (_sibling->event_notify())
		return _log.fail(EINVAL, "Failed to arm event");
	return 0;
}

int ChMem::_process(long timeout, int flags)
{
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	if (ring_read(_rin.get(), (const void **) &msg.data, &msg.size))
		return EAGAIN;
	_callback_data(&msg);
	ring_shift(_rin.get());
	return event_clear_race([this]() -> bool { return !this->_empty(); });
}
