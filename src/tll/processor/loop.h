/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_PROCESSOR_LOOP_H
#define _TLL_PROCESSOR_LOOP_H

#include "tll/channel.h"
#include "tll/logger.h"
#include "tll/util/time.h"

#include <errno.h>

#ifdef __linux__
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#elif defined(__FreeBSD__)
#define WITH_KQUEUE
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef __cplusplus

#include "tll/util/time.h"

#include <chrono>
#include <list>
#include <vector>

namespace tll::processor {

template <typename T>
struct List
{
	std::vector<T *> list;
	unsigned size = 0;

	using iterator = typename std::vector<T *>::iterator;
	using reference = typename std::vector<T *>::reference;

	iterator begin() { return list.begin(); }
	iterator end() { return list.begin() + size; }

	reference operator [] (size_t i) { return list[i]; }

	void rebuild()
	{
		auto to = begin();
		for (auto & i : *this) {
			if (!i) continue;
			std::swap(i, *to++);
		}
		size = to - begin();
	}

	void add(T *v)
	{
		for (auto & i : *this) {
			if (i == v)
				return;
			if (!i) {
				i = v;
				return;
			}
		}
		if (size < list.size()) {
			list[size++] = v;
			return;
		}
		list.push_back(v);
		size++;
	}

	void del(const T *v)
	{
		for (unsigned i = 0; i < size; i++) {
			if (list[i] == v) {
				list[i] = nullptr;
				break;
			}
		}

		for (; size > 0; size--) {
			if (list[size - 1] != nullptr)
				break;
		}
	}
};
} // namespace tll::processor

struct tll_processor_loop_t
{
	tll::Logger _log;
#ifdef __linux__
	int fd = { epoll_create1(0) };
	int fd_pending = { eventfd(1, EFD_NONBLOCK) };
#elif defined(WITH_KQUEUE)
	int kq = { kqueue() };
	struct kevent kev_pending = {};
	static constexpr int pending_ident = 0x746c6c; // 'tll'
#endif
	std::list<tll::Channel *> list; // All registered channels
	tll::processor::List<tll::Channel> list_process; // List of channels to process
	tll::processor::List<tll::Channel> list_pending; // List of channels with pending data
	tll::processor::List<tll::Channel> list_nofd; // List of channels to process that don't have file descriptors

	int stop = 0; ///< Stop flag that can be toggled to stop loop iteration
	bool time_cache_enable = false; ///< Enable feeding time cache

	tll_processor_loop_t(std::string_view name = "")
		: _log(name.size() ? name : "tll.processor.loop")
	{
#ifdef __linux__
		epoll_event ev = {};
		ev.data.ptr = (void *) this;
		epoll_ctl(fd, EPOLL_CTL_ADD, fd_pending, &ev);
#elif defined(WITH_KQUEUE)
		EV_SET(&kev_pending, pending_ident, EVFILT_USER, EV_ADD, NOTE_FFNOP, 0, this);
		kevent(kq, &kev_pending, 1, nullptr, 0, nullptr);
#endif
	}

	~tll_processor_loop_t()
	{
#ifdef __linux__
		if (fd_pending != -1) ::close(fd_pending);
		if (fd != -1) ::close(fd);
#elif defined(WITH_KQUEUE)
		if (kq != -1) ::close(kq);
#endif
		for (auto c : list) {
			if (c) c->callback_del(this, TLL_MESSAGE_MASK_CHANNEL | TLL_MESSAGE_MASK_STATE);
		}
	}

	bool pending() { _log.debug("Pending check: {}, {}", list_nofd.size, list_pending.size); return list_nofd.size + list_pending.size; }
	//bool pending() const { return list_nofd.size + list_pending.size; }

	int step(tll::duration timeout)
	{
		auto c = poll(timeout);
		if (c != nullptr)
			return c->process();
		else
			process();
		return 0;
	}

	int run(tll::duration timeout = std::chrono::milliseconds(1000))
	{
		while (!stop)
			step(timeout);
		return 0;
	}

	tll::Channel * poll(tll::duration timeout)
	{
#ifdef __linux__
		epoll_event ev = {};
		int r = epoll_wait(fd, &ev, 1, std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
		if (!r)
			return 0;
		if (r < 0) {
			if (errno == EINTR)
				return 0;
			return _log.fail(nullptr, "epoll failed: {}", strerror(errno));
		}

		if (ev.data.ptr == this) {
			_log.debug("Poll on pending list");
			if (time_cache_enable)
				tll::time::now();
			for (auto & c: list_pending)
				c->process();
			return nullptr;
		}

		auto c = (tll::Channel *) ev.data.ptr;
		_log.debug("Poll on {}", c->name());
		if (time_cache_enable)
			tll::time::now();
		return c;
#elif defined(WITH_KQUEUE)
		struct timespec ts;
		{
			using namespace std::chrono;
			auto s = duration_cast<seconds>(timeout);
			ts = { (time_t) s.count(), (long) nanoseconds(timeout - s).count() };
		}
		struct kevent kev = {};
		int r = kevent(kq, nullptr, 0, &kev, 1, &ts);
		if (!r)
			return 0;
		if (r < 0) {
			if (errno == EINTR)
				return 0;
			return _log.fail(nullptr, "kevent failed: {}", strerror(errno));
		}

		if (kev.filter == EVFILT_USER && kev.udata == this) {
			_log.debug("Poll on pending list");
			if (time_cache_enable)
				tll::time::now();
			for (auto & c: list_pending)
				c->process();
			return nullptr;
		}

		auto c = (tll::Channel *) kev.udata;
		_log.debug("Poll on {}", c->name());
		if (time_cache_enable)
			tll::time::now();
		return c;
#endif
		return nullptr;
	}

	int process()
	{
		int r = 0;
		for (unsigned i = 0; i < list_pending.size; i++) {
			if (list_pending[i] == nullptr) continue;
			r |= list_pending[i]->process() ^ EAGAIN;
		}
		for (unsigned i = 0; i < list_nofd.size; i++) {
			if (list_nofd[i] == nullptr) continue;
			r |= list_nofd[i]->process() ^ EAGAIN;
		}
		return r == 0?EAGAIN:0;
	}

	int add(tll::Channel *c)
	{
		_log.info("Add channel {} with fd {}", c->name(), c->fd());
		c->callback_add(this, TLL_MESSAGE_MASK_CHANNEL | TLL_MESSAGE_MASK_STATE);
		list.push_back(c);
		if (c->dcaps() & tll::dcaps::Process)
			list_process.add(c);
		if (c->dcaps() & tll::dcaps::Pending)
			pending_add(c);
		if (poll_add(c))
			return _log.fail(EINVAL, "Failed to enable poll on channel {}", c->name());
		for (auto i = c->children(); i; i = i->next) {
			if (add(static_cast<tll::Channel *>(i->channel)))
				return _log.fail(EINVAL, "Failed to add child {} of channel {}", tll_channel_name(i->channel), c->name());
		}
		return 0;
	}

	void pending_add(tll::Channel *c)
	{
		bool empty = list_pending.size == 0;
		list_pending.add(c);
		if (!empty) return;
#ifdef __linux__
		epoll_event ev = {};
		ev.events = EPOLLIN;
		ev.data.ptr = this;
		epoll_ctl(fd, EPOLL_CTL_MOD, fd_pending, &ev);
#elif defined(WITH_KQUEUE)
		EV_SET(&kev_pending, pending_ident, EVFILT_USER, EV_ENABLE, NOTE_FFNOP | NOTE_TRIGGER, 0, this);
		kevent(kq, &kev_pending, 1, nullptr, 0, nullptr);
#endif
	}

	void pending_del(const tll::Channel *c)
	{
		list_pending.del(c);
		if (list_pending.size) return;
#ifdef __linux__
		epoll_event ev = {};
		ev.data.ptr = this;
		epoll_ctl(fd, EPOLL_CTL_MOD, fd_pending, &ev);
#elif defined(WITH_KQUEUE)
		EV_SET(&kev_pending, pending_ident, EVFILT_USER, EV_DISABLE, NOTE_FFNOP, 0, this);
		kevent(kq, &kev_pending, 1, nullptr, 0, nullptr);
#endif
	}

	int poll_add(tll::Channel *c)
	{
		if (c->fd() == -1) {
			if (c->state() == tll::state::Opening || c->state() == tll::state::Active) {
				_log.info("Add channel {} to nofd list", c->name(), c->fd());
				list_nofd.add(c);
			}
			return 0;
		}

		_log.info("Add channel {} to poll with fd {}", c->name(), c->fd());
		update_poll(c, c->dcaps(), true);
		return 0;
	}

	int poll_del(const tll::Channel *c, int fd)
	{
		if (fd == -1) {
			_log.info("Drop channel {} from nofd list", c->name(), c->fd());
			list_nofd.del(c);
			return 0;
		}

#ifdef __linux__
		epoll_event ev = {};
		epoll_ctl(this->fd, EPOLL_CTL_DEL, fd, &ev);
#elif defined(WITH_KQUEUE)
		struct kevent kev[2];
		EV_SET(kev + 0, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
		EV_SET(kev + 1, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
		kevent(kq, kev, 2, nullptr, 0, nullptr);
#endif
		return 0;
	}

	int del(const tll::Channel *c)
	{
		_log.info("Delete channel {}", c->name());
		for (auto i = c->children(); i; i = i->next)
			del(static_cast<const tll::Channel *>(i->channel));

		for (auto it = list.begin(); it != list.end(); it++) {
			if (*it != c)
				continue;
			list.erase(it);
			break;
		}

		list_process.del(c);
		pending_del(c);
		const_cast<tll::Channel *>(c)->callback_del(this, TLL_MESSAGE_MASK_CHANNEL | TLL_MESSAGE_MASK_STATE);
		return 0;
	}

	int update_nofd(tll::Channel *c, unsigned caps, unsigned old)
	{
		auto delta = caps ^ old;

		if (delta & tll::dcaps::Suspend) {
			if (caps & tll::dcaps::Suspend)
				list_nofd.add(c);
			else
				list_nofd.del(c);
		}

		if (delta & tll::dcaps::Process) {
			if (caps & tll::dcaps::Process) {
				_log.debug("Add channel {} to nofd list", c->name(), c->fd());
				list_process.add(c);
				list_nofd.add(c);
			} else {
				_log.debug("Drop channel {} to nofd list", c->name(), c->fd());
				list_process.del(c);
				list_nofd.del(c);
			}
		}

		if (delta & tll::dcaps::Pending) {
			if (caps & tll::dcaps::Pending)
				pending_add((tll::Channel *) c);
			else
				pending_del(c);
		}

		return 0;
	}

	int update(const tll::Channel *c, unsigned caps, unsigned old)
	{
		auto delta = caps ^ old;
		_log.debug("Update caps {}: {:b} -> {:b} (delta {:b})", c->name(), old, caps, delta);

		if (c->fd() == -1)
			return update_nofd(const_cast<tll::Channel *>(c), caps, old);

		if (delta & (tll::dcaps::CPOLLMASK | tll::dcaps::Suspend)) {
			update_poll(c, caps);
		}

		if (delta & tll::dcaps::Process) {
			if (caps & tll::dcaps::Process)
				list_process.add((tll::Channel *) c);
			else
				list_process.del(c);
		}

		if (delta & tll::dcaps::Pending) {
			if (caps & tll::dcaps::Pending)
				pending_add((tll::Channel *) c);
			else
				pending_del(c);
		}

		return 0;
	}

	int update_poll(const tll::Channel *c, unsigned caps, bool add = false)
	{
#ifdef __linux__
		epoll_event ev = {};
		if (!(caps & tll::dcaps::Suspend)) {
			if (caps & tll::dcaps::CPOLLIN) ev.events |= EPOLLIN;
			if (caps & tll::dcaps::CPOLLOUT) ev.events |= EPOLLOUT;
		}
		ev.data.ptr = (void *) c;
		epoll_ctl(fd, add?EPOLL_CTL_ADD:EPOLL_CTL_MOD, c->fd(), &ev);
#elif defined(WITH_KQUEUE)
		auto fd = c->fd();
		struct kevent kev[2];
		EV_SET(kev + 0, fd, EVFILT_READ, EV_ADD | EV_DISABLE, 0, 0, (void *) c);
		EV_SET(kev + 1, fd, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, (void *) c);
		if (!(caps & tll::dcaps::Suspend)) {
			if (caps & tll::dcaps::CPOLLIN) kev[0].flags = EV_ADD | EV_ENABLE;
			if (caps & tll::dcaps::CPOLLOUT) kev[1].flags = EV_ADD | EV_ENABLE;
		}
		kevent(kq, kev, 2, nullptr, 0, nullptr);
#endif
		return 0;
	}

	int update_fd(tll::Channel *c, int old)
	{
		_log.info("Update channel {} fd: {} -> {}", c->name(), old, c->fd());
		poll_del(c, old);
		poll_add(c);
		return 0;
	}

	int callback(const tll::Channel *c, const tll_msg_t *msg)
	{
		if (msg->type == TLL_MESSAGE_STATE) {
			if (msg->msgid == tll::state::Opening)
				return poll_add(const_cast<tll::Channel *>(c));
			else if (msg->msgid == tll::state::Closing)
				return poll_del(c, c->fd());
			else if (msg->msgid == tll::state::Destroy)
				return del(c);
			return 0;
		} else if (msg->type != TLL_MESSAGE_CHANNEL) return 0;

		if (msg->msgid == TLL_MESSAGE_CHANNEL_ADD)
			return add(*((tll::Channel **) msg->data));
		else if (msg->msgid == TLL_MESSAGE_CHANNEL_DELETE)
			return del(*((tll::Channel **) msg->data));
		else if (msg->msgid == TLL_MESSAGE_CHANNEL_UPDATE)
			return update(c, c->dcaps(), *(unsigned *) msg->data);
		else if (msg->msgid == TLL_MESSAGE_CHANNEL_UPDATE_FD)
			return update_fd(const_cast<tll::Channel *>(c), *(int *) msg->data);
		return 0;
	}
};

namespace tll::processor { using Loop = tll_processor_loop_t; }

#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tll_processor_loop_t tll_processor_loop_t;

tll_processor_loop_t * tll_processor_loop_new(const char * name, int len);
void tll_processor_loop_free(tll_processor_loop_t *);

int tll_processor_loop_add(tll_processor_loop_t *, tll_channel_t *);
int tll_processor_loop_del(tll_processor_loop_t *, const tll_channel_t *);
tll_channel_t * tll_processor_loop_poll(tll_processor_loop_t *, long timeout);
int tll_processor_loop_process(tll_processor_loop_t *);
int tll_processor_loop_pending(tll_processor_loop_t *);

int tll_processor_loop_stop_get(const tll_processor_loop_t *);
int tll_processor_loop_stop_set(tll_processor_loop_t *, int flag);
int tll_processor_loop_run(tll_processor_loop_t *, long timeout);
int tll_processor_loop_step(tll_processor_loop_t *, long timeout);

#ifdef __cplusplus
} // extern "C"
#endif

#endif//_TLL_PROCESSOR_LOOP_H
