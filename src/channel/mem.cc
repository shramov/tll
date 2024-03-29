/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/mem.h"

#include "tll/channel/event.hpp"
#include "tll/ring.h"
#include "tll/util/size.h"

#include <mutex>

using namespace tll;

namespace frame {
struct Short
{
	int64_t seq;
	int32_t msgid;
	int32_t unused;

	Short(const tll_msg_t &msg) : seq(msg.seq), msgid(msg.msgid), unused(0) {}

	void fill(tll_msg_t &msg)
	{
		msg.seq = seq;
		msg.msgid = msgid;
	}
};

struct Full
{
	int64_t seq;
	int32_t msgid;
	int16_t type;
	uint16_t flags;
	uint64_t addr;
	int64_t time;

	Full(const tll_msg_t &msg) : seq(msg.seq), msgid(msg.msgid), type(msg.type), flags(msg.flags), addr(msg.addr.u64), time(msg.time) {}

	void fill(tll_msg_t &msg)
	{
		msg.seq = seq;
		msg.msgid = msgid;
		msg.type = type;
		msg.flags = flags;
		msg.addr.u64 = addr;
		msg.time = time;
	}
};
}

template <typename F>
class Mem : public tll::channel::Event<Mem<F>>
{
	using Frame = F;
	using Base = tll::channel::Event<Mem<F>>;

	struct Ring {
		Ring(size_t size) { ring_init(&ring, size, 0); }
		~Ring()
		{
			ring_free(&ring);
			notify.close();
		}

		ringbuffer_t ring = {};
		tll::channel::EventNotify notify;
	};

	size_t _size = 1024;
	std::mutex _mutex; // shared_ptr initialization lock
	auto _lock() { return std::unique_lock<std::mutex>(_mutex); }
	std::shared_ptr<Ring> _rin;
	std::shared_ptr<Ring> _rout;
	bool _child = false;
	Mem<Frame> * _sibling = nullptr;
	bool _empty() const;

 public:
	static constexpr std::string_view channel_protocol() { return "mem"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();
	void _free();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags);
};

TLL_DEFINE_IMPL(ChMem);
TLL_DEFINE_IMPL(Mem<frame::Short>);
TLL_DEFINE_IMPL(Mem<frame::Full>);

std::optional<const tll_channel_impl_t *> ChMem::_init_replace(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	auto normal = reader.getT("frame", true, {{"normal", true}, {"full", false}});
	if (!reader)
		return _log.fail(std::nullopt, "Invalid url: {}", reader.error());

	if (normal)
		return &Mem<frame::Short>::impl;
	return &Mem<frame::Full>::impl;
}

template <typename F>
int Mem<F>::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	if (master) {
		_sibling = channel_cast<Mem<F>>(master);
		if (!_sibling)
			return this->_log.fail(EINVAL, "Parent {} must be mem:// channel", master->name());
		this->_log.debug("Init child of master {}", master->name());
		_child = true;
		_sibling->_sibling = this;
		this->_with_fd = _sibling->_with_fd;
		if (!this->_with_fd)
			this->_log.debug("Event notification disabled by master {}", master->name());
		return 0;
	}
	auto reader = this->channel_props_reader(url);
	_size = reader.getT("size", util::Size {64 * 1024});
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	return Base::_init(url, master);
}

template <typename F>
void Mem<F>::_free()
{
	if (_sibling) {
		this->_log.info("Remove sibling reference from {}", _sibling->name);
		_sibling->_sibling = nullptr;
	}
}

template <typename F>
bool Mem<F>::_empty() const
{
	const void * data;
	size_t size;
	return ring_read(&_rin->ring, &data, &size) == EAGAIN;
}

template <typename F>
int Mem<F>::_open(const tll::ConstConfig &url)
{
	if (Base::_open(url))
		return this->_log.fail(EINVAL, "Failed to open event");

	if (_child) {
		if (!_sibling)
			return this->_log.fail(EINVAL, "Master channel already destroyed");
		auto lock = _sibling->_lock();
		_rin = _sibling->_rout;
		_rout = _sibling->_rin;
		lock.unlock();
		_rin->notify = this->event_detached();
	} else {
		auto rin = std::make_shared<Ring>(_size);
		auto rout = std::make_shared<Ring>(_size);
		if (!rin->ring.header || !rout->ring.header)
			return this->_log.fail(EINVAL, "Failed to create buffers");
		rin->notify = this->event_detached();
		auto lock = _lock();
		std::swap(_rin, rin);
		std::swap(_rout, rout);
	}

	if (!_empty()) {
		this->_log.debug("Pending data, arm notification");
		this->event_notify();
	}

	this->state(state::Active);
	return 0;
}

template <typename F>
int Mem<F>::_close()
{
	Base::_close();
	auto lock = _lock();
	_rin.reset();
	_rout.reset();
	return 0;
}

template <typename F>
int Mem<F>::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA) {
		if constexpr (!std::is_same_v<F, frame::Full>)
			return 0;
		if (msg->type == TLL_MESSAGE_STATE || msg->type == TLL_MESSAGE_CHANNEL)
			return 0;
	}
	Frame * data;
	const size_t size = sizeof(Frame) + msg->size;
	if (int r = ring_write_begin(&_rout->ring, (void **) &data, size)) {
		if (r == EAGAIN)
			return r;
		return this->_log.fail(r, "Failed to allocate message: {}", strerror(r));
	}
	*data = *msg;
	memcpy(data + 1, msg->data, msg->size);
	ring_write_end(&_rout->ring, data, size);
	if (_rout->notify.notify())
		return this->_log.fail(EINVAL, "Failed to arm event");
	return 0;
}

template <typename F>
int Mem<F>::_process(long timeout, int flags)
{
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	Frame * frame;
	size_t size;
	if (ring_read(&_rin->ring, (const void **) &frame, &size))
		return EAGAIN;
	if (size < sizeof(Frame))
		return this->_log.fail(EMSGSIZE, "Got invalid payload size {} < {}", size, sizeof(Frame));
	frame->fill(msg);
	msg.size = size - sizeof(Frame);
	msg.data = frame + 1;
	if constexpr (std::is_same_v<frame::Short, F>)
		this->_callback_data(&msg);
	else
		this->_callback(&msg);
	ring_shift(&_rin->ring);

	auto empty = _empty();
	this->_dcaps_pending(!empty);
	if (empty)
		return this->event_clear_race([this]() -> bool { return !this->_empty(); });
	return 0;
}
