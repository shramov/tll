/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/mem.h"

#include "tll/util/size.h"

#include <sys/eventfd.h>
#include <unistd.h>

using namespace tll;

TLL_DEFINE_IMPL(ChMem);

int ChMem::_init(const UrlView &url, tll::Channel *master)
{
	if (master) {
		this->master = channel_cast<ChMem>(master);
		if (!this->master)
			return _log.fail(EINVAL, "Parent {} must be mem:// channel", master->name());
		_log.debug("Init child of master {}", this->master->name);
		return 0;
	}
	auto reader = channel_props_reader(url);
	_size = reader.getT("size", util::Size {64 * 1024});
	_with_fd = reader.getT("fd", true);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	return 0;
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

int ChMem::_open(const PropsView &url)
{
	if (master) {
		_log.info("Bind to master {}", master->name);
		_rin = master->_rout;
		_rout = master->_rin;
		_fd_out = master->_fd_in;
		_fd_in = master->_fd_out;
	} else {
		if (_with_fd) {
			_fd_in = eventfd(0, EFD_NONBLOCK);
			if (_fd_in == -1)
				return _log.fail(EINVAL, "Failed to create eventfd: {}", strerror(errno));

			_fd_out = eventfd(0, EFD_NONBLOCK);
			if (_fd_out == -1)
				return _log.fail(EINVAL, "Failed to create eventfd: {}", strerror(errno));
		}

		_rin.reset(ring_new(_size), ring_delete);
		_rout.reset(ring_new(_size), ring_delete);
		if (!_rin.get() || !_rout.get())
			return _log.fail(EINVAL, "Failed to create buffers");
	}

	_dcaps_poll(dcaps::CPOLLIN);
	state(TLL_STATE_ACTIVE);
	return 0;
}

int ChMem::_close()
{
	_rin.reset();
	_rout.reset();
	if (!master) {
		if (_fd_in != -1) ::close(_fd_in);
		if (_fd_out != -1) ::close(_fd_out);
	}
	_fd_in = -1;
	_fd_out = -1;
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
	if (_fd_out != -1) {
		int64_t w = 1;
		if (write(_fd_out, &w, sizeof(w)) != sizeof(w))
			return _log.fail(EINVAL, "Failed to write to eventfd: {}", strerror(errno));
	}
	return 0;
}

int ChMem::_process(long timeout, int flags)
{
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	if (ring_read(_rin.get(), (const void **) &msg.data, &msg.size))
		return EAGAIN;
	_callback_data(&msg);
	ring_shift(_rin.get());
	if (_fd_in != -1) {
		if (!ring_read(_rin.get(), (const void **) &msg.data, &msg.size))
			return 0;
		int64_t w;
		if (read(_fd_in, &w, sizeof(w)) < 0 && errno != EAGAIN)
			return _log.fail(EINVAL, "Failed to read from eventfd: {}", strerror(errno));
		if (ring_read(_rin.get(), (const void **) &msg.data, &msg.size))
			return 0;
		_log.debug("Rearm fd");
		// Rearm fd, race
		w = 1;
		if (write(_fd_in, &w, sizeof(w)) != sizeof(w))
			return _log.fail(EINVAL, "Failed to write to eventfd: {}", strerror(errno));
	}
	return 0;
}
