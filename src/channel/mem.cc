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

namespace {
struct frame_t
{
	int64_t seq;
	int32_t msgid;
	int32_t unused;
};

constexpr frame_t msg2frame(const tll_msg_t *msg)
{
	return frame_t { msg->seq, msg->msgid, 0 };
}
}

TLL_DEFINE_IMPL(ChMem);

int ChMem::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	if (master) {
		_sibling = channel_cast<ChMem>(master);
		if (!_sibling)
			return _log.fail(EINVAL, "Parent {} must be mem:// channel", master->name());
		_log.debug("Init child of master {}", master->name());
		_child = true;
		_sibling->_sibling = this;
		_with_fd = _sibling->_with_fd;
		if (!_with_fd)
			_log.debug("Event notification disabled by master {}", master->name());
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
	if (_sibling) {
		_log.info("Remove sibling reference from {}", _sibling->name);
		_sibling->_sibling = nullptr;
	}
}

bool ChMem::_empty() const
{
	const void * data;
	size_t size;
	return ring_read(&_rin->ring, &data, &size) == EAGAIN;
}

int ChMem::_open(const PropsView &url)
{
	if (Event<ChMem>::_open(url))
		return _log.fail(EINVAL, "Failed to open event");

	if (_child) {
		if (!_sibling)
			return _log.fail(EINVAL, "Master channel already destroyed");
		auto lock = _sibling->_lock();
		_rin = _sibling->_rout;
		_rout = _sibling->_rin;
		lock.unlock();
		_rin->notify = event_detached();
	} else {
		auto rin = std::make_shared<Ring>(_size);
		auto rout = std::make_shared<Ring>(_size);
		if (!rin->ring.header || !rout->ring.header)
			return _log.fail(EINVAL, "Failed to create buffers");
		rin->notify = event_detached();
		auto lock = _lock();
		std::swap(_rin, rin);
		std::swap(_rout, rout);
	}

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
	auto lock = _lock();
	_rin.reset();
	_rout.reset();
	return 0;
}

int ChMem::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	frame_t * data;
	const size_t size = sizeof(frame_t) + msg->size;
	if (int r = ring_write_begin(&_rout->ring, (void **) &data, size)) {
		if (r == EAGAIN)
			return r;
		return _log.fail(r, "Failed to allocate message: {}", strerror(r));
	}
	*data = msg2frame(msg);
	memcpy(data + 1, msg->data, msg->size);
	ring_write_end(&_rout->ring, data, size);
	if (_rout->notify.notify())
		return _log.fail(EINVAL, "Failed to arm event");
	return 0;
}

int ChMem::_process(long timeout, int flags)
{
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	frame_t * frame;
	size_t size;
	if (ring_read(&_rin->ring, (const void **) &frame, &size))
		return EAGAIN;
	if (size < sizeof(frame_t))
		return _log.fail(EMSGSIZE, "Got invalid payload size {} < {}", size, sizeof(frame_t));
	msg.seq = frame->seq;
	msg.msgid = frame->msgid;
	msg.size = size - sizeof(frame_t);
	msg.data = frame + 1;
	_callback_data(&msg);
	ring_shift(&_rin->ring);

	auto empty = _empty();
	_dcaps_pending(!empty);
	if (empty)
		return event_clear_race([this]() -> bool { return !this->_empty(); });
	return 0;
}
