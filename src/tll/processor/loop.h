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
#include "tll/stat.h"
#include "tll/util/time.h"

#include <errno.h>

#ifdef __linux__
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>
#elif defined(__FreeBSD__) || defined(__APPLE__)
#define WITH_KQUEUE 1
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

namespace loop {
static constexpr timespec tll2ts(tll::duration ts)
{
	using namespace std::chrono;
	auto s = duration_cast<seconds>(ts);
	return { (time_t) s.count(), (long) nanoseconds(ts - s).count() };
}

#ifdef __linux__
struct EPoll
{
	int fd = -1;
	int fd_pending = -1;
	int fd_nofd = -1;

	EPoll() = default;
	EPoll(EPoll &&rhs)
	{
		std::swap(fd, rhs.fd);
		std::swap(fd_pending, rhs.fd_pending);
		std::swap(fd_nofd, rhs.fd_nofd);
	}

	~EPoll()
	{
		if (fd != -1)
			::close(fd);
		fd = -1;
		if (fd_pending != -1)
			::close(fd_pending);
		fd_pending = -1;
		if (fd_nofd != -1)
			::close(fd_nofd);
		fd_nofd = -1;
	}

	int init(tll::Logger &log, const tll::duration &nofd_interval)
	{
		fd = epoll_create1(EPOLL_CLOEXEC);
		if (fd == -1) return log.fail(EINVAL, "Failed to create epoll: {}", strerror(errno));
		fd_pending = eventfd(1, EFD_NONBLOCK | EFD_CLOEXEC);
		if (fd_pending == -1) return log.fail(EINVAL, "Failed to create eventfd: {}", strerror(errno));
		fd_nofd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
		if (fd_nofd == -1) return log.fail(EINVAL, "Failed to create timerfd: {}", strerror(errno));

		epoll_event ev = {};

		ev.data.ptr = &fd_pending;
		if (epoll_ctl(fd, EPOLL_CTL_ADD, fd_pending, &ev))
			return log.fail(EINVAL, "Failed to add pending list fd: {}", strerror(errno));

		ev.data.ptr = &fd_nofd;
		if (epoll_ctl(fd, EPOLL_CTL_ADD, fd_nofd, &ev))
			return log.fail(EINVAL, "Failed to add nofd list fd: {}", strerror(errno));

		struct itimerspec its = {};
		its.it_interval = tll2ts(nofd_interval);
		its.it_value = tll2ts(nofd_interval);
		if (timerfd_settime(fd_nofd, 0, &its, nullptr))
			return log.fail(EINVAL, "Failed to rearm timerfd: {}", strerror(errno));

		return 0;
	}

	void _update_helper(int * fd, int events)
	{
		epoll_event ev = {};
		ev.events = events;
		ev.data.ptr = fd;
		epoll_ctl(this->fd, EPOLL_CTL_MOD, *fd, &ev);
	}

	void pending_enable() { return _update_helper(&fd_pending, EPOLLIN); }
	void pending_disable() { return _update_helper(&fd_pending, 0); }

	void nofd_enable() { return _update_helper(&fd_nofd, EPOLLIN); }
	void nofd_disable() { return _update_helper(&fd_nofd, 0); }
	void nofd_flush()
	{
		uint64_t v;
		auto r = read(fd_nofd, &v, sizeof(v));
		(void) r;
	}

	static constexpr int caps2events(unsigned caps)
	{
		int r = 0;
		if (!(caps & tll::dcaps::Suspend)) {
			if (caps & tll::dcaps::CPOLLIN) r |= EPOLLIN;
			if (caps & tll::dcaps::CPOLLOUT) r |= EPOLLOUT;
		}
		return r;
	}

	void _poll_helper(int flags, int cfd, const tll::Channel * c, unsigned caps)
	{
		epoll_event ev = {};
		ev.events = caps2events(caps);
		ev.data.ptr = (void *) c;
		epoll_ctl(fd, flags, cfd, &ev);
	}

	void poll_add(int cfd, const tll::Channel * c, unsigned caps) { _poll_helper(EPOLL_CTL_ADD, cfd, c, caps); }
	void poll_update(int cfd, const tll::Channel * c, unsigned caps) { _poll_helper(EPOLL_CTL_MOD, cfd, c, caps); }
	void poll_del(int cfd) { _poll_helper(EPOLL_CTL_DEL, cfd, nullptr, 0); }

	bool is_timeout(void *c) const { return c == nullptr; }
	bool is_pending(void *c) { return c == (void *) &fd_pending; }
	bool is_nofd(void *c) { return c == (void *) &fd_nofd; }
	bool is_error(void *c) { return c == this; }

	void * poll(tll::duration timeout)
	{
		epoll_event ev = {};
		int r = epoll_wait(fd, &ev, 1, std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
		if (!r)
			return nullptr;
		if (r < 0) {
			if (errno == EINTR)
				return nullptr;
			return this; //_log.fail(nullptr, "epoll failed: {}", strerror(errno));
		}

		return ev.data.ptr;
	}
};
#endif//__linux__

#ifdef WITH_KQUEUE
struct KQueue
{
	int kq = -1;
	struct kevent kev_pending = {};
	struct kevent kev_nofd = {};
	static constexpr int pending_ident = 0x746c6c; // 'tll'

	KQueue() = default;
	KQueue(KQueue &&rhs)
	{
		std::swap(kq, rhs.kq);
	}

	~KQueue()
	{
		if (kq != -1)
			::close(kq);
		kq = -1;
	}

	int init(tll::Logger &log, const tll::duration &nofd_interval)
	{
		kq = kqueue();
		if (kq == -1)
			return log.fail(EINVAL, "Failed to create kqueue: {}", strerror(errno));

		if (_pending_helper(EV_ADD, NOTE_FFNOP))
			return log.fail(EINVAL, "Failed to add pending list kevent: {}", strerror(errno));

		EV_SET(&kev_nofd, pending_ident, EVFILT_TIMER, EV_DISABLE, NOTE_NSECONDS, nofd_interval.count(), &kev_nofd);
		if (_nofd_helper(EV_ADD | EV_DISABLE))
			return log.fail(EINVAL, "Failed to add nofd list kevent: {}", strerror(errno));
		return 0;
	}

	int _pending_helper(int flags, int note)
	{
		EV_SET(&kev_pending, pending_ident, EVFILT_USER, flags, note, 0, &kev_pending);
		return kevent(kq, &kev_pending, 1, nullptr, 0, nullptr);
	}

	void pending_enable() { _pending_helper(EV_ENABLE, NOTE_FFNOP | NOTE_TRIGGER); }
	void pending_disable() { _pending_helper(EV_DISABLE, NOTE_FFNOP); }

	int _nofd_helper(int flags)
	{
		kev_nofd.flags = flags;
		return kevent(kq, &kev_nofd, 1, nullptr, 0, nullptr);
	}

	void nofd_enable() { _nofd_helper(EV_ENABLE); }
	void nofd_disable() { _nofd_helper(EV_DISABLE); }
	void nofd_flush() {}

	void _fd_helper(int flags, int fd, const tll::Channel * c, unsigned caps)
	{
		struct kevent kev[2];
		EV_SET(kev + 0, fd, EVFILT_READ, flags, 0, 0, (void *) c);
		EV_SET(kev + 1, fd, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, (void *) c);
		if (!(caps & tll::dcaps::Suspend)) {
			if (caps & tll::dcaps::CPOLLIN) kev[0].flags = EV_ADD | EV_ENABLE;
			if (caps & tll::dcaps::CPOLLOUT) kev[1].flags = EV_ADD | EV_ENABLE;
		}
		kevent(kq, kev, 2, nullptr, 0, nullptr);
	}

	void poll_add(int fd, const tll::Channel * c, unsigned caps) { _fd_helper(EV_ADD | EV_DISABLE, fd, c, caps); }
	void poll_update(int fd, const tll::Channel * c, unsigned caps) { _fd_helper(EV_ADD | EV_DISABLE, fd, c, caps); }
	void poll_del(int fd) { _fd_helper(EV_DISABLE, fd, nullptr, 0); }

	bool is_timeout(void *c) const { return c == nullptr; }
	bool is_pending(void *c) { return c == (void *) &kev_pending; }
	bool is_nofd(void *c) { return c == (void *) &kev_nofd; }
	bool is_error(void *c) { return c == this; }

	void * poll(tll::duration timeout)
	{
		struct timespec ts = tll2ts(timeout);
		struct kevent kev = {};
		int r = kevent(kq, nullptr, 0, &kev, 1, &ts);
		if (!r)
			return nullptr;
		if (r < 0) {
			if (errno == EINTR)
				return nullptr;
			return this; //_log.fail(nullptr, "kevent failed: {}", strerror(errno));
		}

		return kev.udata;
	}
};
#endif//WITH_KQUEUE
} // namespace loop

} // namespace tll::processor

struct tll_processor_loop_t
{
	bool _poll_enable = true;
#ifdef __linux__
	tll::processor::loop::EPoll _poll;
#elif defined(WITH_KQUEUE)
	tll::processor::loop::KQueue _poll;
#endif
	using StatStep = tll::stat::Integer<tll::stat::Sum, tll::stat::Unknown, 's', 't', 'e', 'p'>;
	using StatPoll = tll::stat::Integer<tll::stat::Sum, tll::stat::Ns, 'p', 'o', 'l', 'l'>;

	tll_stat_block_t * _stat = nullptr;
	unsigned _stat_step_index = -1;
	unsigned _stat_poll_index = -1;
	unsigned _pending_count = 0;
	unsigned _pending_steps = 0;

	tll::duration _poll_interval = std::chrono::milliseconds(10);

	tll::Logger _log;

	std::list<tll::Channel *> list; // All registered channels
	tll::processor::List<tll::Channel> list_process; // List of channels to process
	tll::processor::List<tll::Channel> list_pending; // List of channels with pending data
	tll::processor::List<tll::Channel> list_nofd; // List of channels to process that don't have file descriptors

	int stop = 0; ///< Stop flag that can be toggled to stop loop iteration
	bool time_cache_enable = false; ///< Enable feeding time cache

	tll_processor_loop_t(std::string_view name = "")
		: _log(name.size() ? name : "tll.processor.loop")
	{
	}

	int init(const tll::ConstConfig &cfg)
	{
		constexpr auto nofd_interval_default =
#ifdef __linux__
			std::chrono::milliseconds(100);
#else
			std::chrono::milliseconds(10);
#endif
		auto reader = tll::make_props_reader(cfg);
		auto name = reader.getT<std::string>("name", "");
		_poll_enable = reader.getT("poll", true);
		_poll_interval = reader.getT<tll::duration>("poll-interval", std::chrono::milliseconds(100));
		auto nofd_interval = reader.getT<tll::duration>("nofd-interval", nofd_interval_default);
		time_cache_enable = reader.getT("time-cache", false);
		if (_poll_enable)
			_pending_steps = reader.getT("pending-steps", 0u);

		_log = { name.size() ? name : "tll.processor.loop" };
		if (!reader)
			return _log.fail(EINVAL, "Invalid parameters: {}", reader.error());

		if (_poll_enable) {
			if (_poll.init(_log, nofd_interval))
				return _log.fail(EINVAL, "Failed to init poll subsystem");
		}

		return 0;
	}

	int stat(tll_stat_block_t * block)
	{
		if (!block) {
			_stat = nullptr;
			return 0;
		}
		auto page = tll::stat::acquire(block);
		if (!page)
			return _log.fail(EINVAL, "Failed to set stat: unable to acquire page");
		int step = -1, poll = -1;
		for (auto i = 0u; i < page->size; i++) {
			auto f = static_cast<const tll::stat::Field *>(page->fields + i);
			if (f->name() == "step")
				step = i;
			else if (f->name() == "poll")
				poll = i;
		}
		tll::stat::release(block, page);
		if (step == -1 || poll == -1)
			return _log.fail(ENOENT, "Failed to set stat: required fields 'step' and 'poll' not found");
		_log.debug("Stat index: step {}, poll {}", step, poll);
		_stat_step_index = step;
		_stat_poll_index = poll;
		_stat = block;
		if (_poll_enable)
			time_cache_enable = false;
		return 0;
	}

	bool poll_enable() const { return _poll_enable; }
	int poll_fd() const
	{
#ifdef __linux__
		if (_poll_enable)
			return _poll.fd;
#endif
		return -1;
	}

	~tll_processor_loop_t()
	{
		for (auto c : list) {
			if (c) c->callback_del(this, TLL_MESSAGE_MASK_CHANNEL | TLL_MESSAGE_MASK_STATE);
		}
	}

	bool pending() { _log.debug("Pending check: {}, {}", list_nofd.size, list_pending.size); return list_nofd.size + list_pending.size; }
	//bool pending() const { return list_nofd.size + list_pending.size; }

	int step(tll::duration timeout)
	{
		auto c = poll(timeout);
		if (c)
			c->process();
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
		if (_poll_enable) {
			tll::time_point start = {};
			if (_pending_steps && list_pending.size) {
				if (_pending_count++ < _pending_steps) {
					if (_stat) {
						if (auto s = tll::stat::acquire(_stat); s) {
							static_cast<StatStep *>(s->fields + _stat_step_index)->update(1);
							tll::stat::release(_stat, s);
						}
					}
					_log.trace("Process pending: {} channels", list_pending.size);
					process_list(list_pending);
					return nullptr;
				} else
					_pending_count = 0;
			}
			if (_stat)
				start = tll::time::now();
			auto r = _poll.poll(timeout);
			if (_stat) {
				std::chrono::nanoseconds dt = tll::time::now() - start;
				if (auto s = tll::stat::acquire(_stat); s) {
					static_cast<StatStep *>(s->fields + _stat_step_index)->update(1);
					static_cast<StatPoll *>(s->fields + _stat_poll_index)->update(dt.count());
					tll::stat::release(_stat, s);
				}
			} else if (time_cache_enable)
				tll::time::now();
			if (_poll.is_pending(r)) {
				_log.trace("Process pending: {} channels", list_pending.size);
				process_list(list_pending);
				return nullptr;
			} else if (_poll.is_nofd(r)) {
				_log.trace("Process nofd: {} channels", list_nofd.size);
				process_list(list_nofd);
				_poll.nofd_flush();
				return nullptr;
			} else if (_poll.is_timeout(r)) {
				return nullptr;
			} else if (_poll.is_error(r)) {
				return _log.fail(nullptr, "Poll failed: {}", strerror(errno));
			} else {
				auto c = static_cast<tll::Channel *>(r);
				_log.trace("Poll on {}", c->name());
				return c;
			}
			return nullptr;
		}

		if (_stat) {
			if (auto s = tll::stat::acquire(_stat); s) {
				static_cast<StatStep *>(s->fields + _stat_step_index)->update(1);
				tll::stat::release(_stat, s);
			}
		}
		if (time_cache_enable)
			tll::time::now();
		process_list(list_pending);
		process_list(list_process);
		return nullptr;
	}

	int process_list(tll::processor::List<tll::Channel> &l)
	{
		int r = 0;
		for (unsigned i = 0; i < l.size; i++) {
			if (l[i] == nullptr) continue;
			r |= l[i]->process() ^ EAGAIN;
		}
		return r == 0 ? EAGAIN : 0;
	}

	int add(tll::Channel *c)
	{
		_log.debug("Add channel {} with fd {}", c->name(), c->fd());
		c->callback_add(this, TLL_MESSAGE_MASK_CHANNEL | TLL_MESSAGE_MASK_STATE);
		list.push_back(c);

		process_add(c, c->fd(), c->dcaps());

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
		if (_poll_enable)
			_poll.pending_enable();
	}

	void pending_del(const tll::Channel *c)
	{
		list_pending.del(c);
		if (list_pending.size) return;
		if (_poll_enable)
			_poll.pending_disable();
	}

	int process_add(tll::Channel *c, int fd, unsigned caps)
	{
		if (!tll::dcaps::need_process(caps))
			return 0;

		list_process.add(c);

		if (caps & tll::dcaps::Pending)
			pending_add(c);

		if (fd == -1) {
			_log.debug("Add channel {} to nofd list", c->name());
			bool empty = list_nofd.size == 0;
			list_nofd.add(c);
			if (!empty) return 0;
			if (_poll_enable)
				_poll.nofd_enable();
		} else if (_poll_enable) {
			_log.debug("Add channel {} to poll with fd {}", c->name(), fd);
			_poll.poll_add(fd, c, caps);
		}
		return 0;
	}

	int process_del(const tll::Channel *c, int fd)
	{
		list_process.del(c);
		pending_del(c);

		if (fd == -1) {
			_log.debug("Drop channel {} from nofd list", c->name());
			list_nofd.del(c);
			if (list_nofd.size) return 0;
			if (_poll_enable)
				_poll.nofd_disable();
		} else if (_poll_enable) {
			_log.debug("Drop channel {} from poll with fd {}", c->name(), fd);
			_poll.poll_del(fd);
		}
		return 0;
	}

	int del(const tll::Channel *c)
	{
		_log.debug("Delete channel {}", c->name());
		for (auto i = c->children(); i; i = i->next)
			del(static_cast<const tll::Channel *>(i->channel));

		process_del(c, c->fd());

		for (auto it = list.begin(); it != list.end(); it++) {
			if (*it != c)
				continue;
			list.erase(it);
			break;
		}

		const_cast<tll::Channel *>(c)->callback_del(this, TLL_MESSAGE_MASK_CHANNEL | TLL_MESSAGE_MASK_STATE);
		return 0;
	}

	int update(const tll::Channel *c, unsigned caps, unsigned old)
	{
		auto delta = caps ^ old;
		_log.debug("Update caps {}: {:b} -> {:b} (delta {:b})", c->name(), old, caps, delta);

		auto fd = c->fd();
		if (delta & tll::dcaps::ProcessMask) {
			if (tll::dcaps::need_process(caps) != tll::dcaps::need_process(old)) {
				if (tll::dcaps::need_process(caps))
					process_add((tll::Channel *) c, fd, caps);
				else
					process_del(c, fd);
			}
		}

		if (!tll::dcaps::need_process(caps))
			return 0;

		if (delta & (tll::dcaps::CPOLLMASK)) {
			if (_poll_enable && fd != -1)
				_poll.poll_update(c->fd(), c, caps);
		}

		if (delta & tll::dcaps::Pending) {
			if (caps & tll::dcaps::Pending)
				pending_add((tll::Channel *) c);
			else
				pending_del(c);
		}

		return 0;
	}

	int update_fd(tll::Channel *c, int old)
	{
		auto fd = c->fd();
		auto caps = c->dcaps();
		_log.debug("Update channel {} fd: {} -> {}", c->name(), old, fd);
		if (tll::dcaps::need_process(caps)) {
			process_del(c, old);
			process_add(c, fd, caps);
		}
		return 0;
	}

	int callback(const tll::Channel *c, const tll_msg_t *msg)
	{
		if (msg->type == TLL_MESSAGE_STATE) {
			if (msg->msgid == tll::state::Opening) {
				_log.debug("Enable opening channel {}", c->name());
				return process_add(const_cast<tll::Channel *>(c), c->fd(), c->dcaps());
			} else if (msg->msgid == tll::state::Closed || msg->msgid == tll::state::Error) {
				_log.debug("Disable {} channel {}", msg->msgid == tll::state::Closed ? "closed" : "failed", c->name());
				return process_del(c, c->fd());
			} else if (msg->msgid == tll::state::Destroy)
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
tll_processor_loop_t * tll_processor_loop_new_cfg(const tll_config_t *cfg);
void tll_processor_loop_free(tll_processor_loop_t *);

int tll_processor_loop_add(tll_processor_loop_t *, tll_channel_t *);
int tll_processor_loop_del(tll_processor_loop_t *, const tll_channel_t *);
tll_channel_t * tll_processor_loop_poll(tll_processor_loop_t *, long timeout);
int tll_processor_loop_process(tll_processor_loop_t *);
int tll_processor_loop_pending(tll_processor_loop_t *);

int tll_processor_loop_get_fd(const tll_processor_loop_t *);
int tll_processor_loop_stop_get(const tll_processor_loop_t *);
int tll_processor_loop_stop_set(tll_processor_loop_t *, int flag);
int tll_processor_loop_run(tll_processor_loop_t *, long timeout);
/// Run loop but stop on specified signals
int tll_processor_loop_run_signal(tll_processor_loop_t *, long timeout, const int * signals, size_t sigsize);
int tll_processor_loop_step(tll_processor_loop_t *, long timeout);

#ifdef __cplusplus
} // extern "C"
#endif

#endif//_TLL_PROCESSOR_LOOP_H
