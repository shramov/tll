#ifndef _LOGGER_THREAD_H
#define _LOGGER_THREAD_H

#include "tll/cppring.h"
#include "tll/logger.h"
#include "tll/util/time.h"

#include <poll.h>
#ifdef __linux__
#include <sys/eventfd.h>
#elif defined(__FreeBSD__) || defined(__APPLE__)
#define WITH_KQUEUE 1
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif
#include <unistd.h>

#include <cstdlib>
#include <mutex>
#include <thread>

#include "logger/common.h"

namespace tll::logger {

struct Header
{
	uint16_t level = TLL_LOGGER_DEBUG;
	tll::logger::Logger * logger = nullptr;
	tll::time_point timestamp;
};

struct Thread
{
	std::mutex _lock;
	std::unique_ptr<tll::Ring> _ring;
	int _fd = -1;
	bool _stop = false;
	std::thread _thread;

	tll::Logger _log { "tll.logger.thread" };

	~Thread()
	{
		stop();

		if (_thread.joinable())
			_thread.join();
		_thread = {};

#if defined(__linux__) || defined(WITH_KQUEUE)
		if (_fd != -1)
			close(_fd);
#endif
		_fd = -1;
	}

	int init(size_t size)
	{
#ifdef __linux__
		_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
		if (_fd == -1)
			return EINVAL;
#elif defined(WITH_KQUEUE)
		_fd = kqueue();
		if (_fd == -1)
			return EINVAL;
		struct kevent kev = {};
		EV_SET(&kev, 0, EVFILT_USER, EV_ADD, NOTE_FFNOP, 0, nullptr);
		kevent(_fd, &kev, 1, nullptr, 0, nullptr);
#endif
		_ring = tll::Ring::allocate(size);
		if (!_ring)
			return EINVAL;

		_thread = std::thread(&Thread::run, this);
		return 0;
	}

	void stop()
	{
		_stop = true;
		wake();
	}

	void wake()
	{
#ifdef __linux__
		eventfd_write(_fd, 1);
#elif defined(WITH_KQUEUE)
		struct kevent kev = {};
		EV_SET(&kev, 0, EVFILT_USER, EV_ENABLE, NOTE_FFNOP | NOTE_TRIGGER, 0, nullptr);
		kevent(_fd, &kev, 1, nullptr, 0, nullptr);
#endif
	}

	void run()
	{
#ifdef __linux__
		pollfd  pfd = { .fd = _fd, .events = POLLIN };
#endif
		_log.debug("Logger thread started");
		while (!_stop || !_ring->empty()) {
			if (_ring->empty()) {
#ifdef __linux__
				if (poll(&pfd, 1, 1000) == 0)
					continue;
#elif defined(WITH_KQUEUE)
				struct kevent kev = {};
				struct timespec ts = { .tv_sec = 1 };
				if (kevent(_fd, nullptr, 0, &kev, 1, &ts) == 0)
					continue;
#endif
			}
			step();
		}
		_log.debug("Logger thread finished");
	}

	void step()
	{
		const void * data;
		size_t size;
		if (_ring->read(&data, &size))
			return;

		if (size < sizeof(Header))
			return _log.error("Invalid data header, too small: {} < minimal {}", size, sizeof(Header));

		auto header = (const Header *) data;

		std::string_view body((const char *) (header + 1), size - sizeof(Header));

		{
			std::unique_lock<std::mutex> lck(header->logger->lock);
			header->logger->impl->log(header->timestamp, (tll_logger_level_t) header->level, body);
		}
		header->logger->unref();
		_ring->shift();
		if (_ring->empty()) {
#ifdef __linux__
			eventfd_t v;
			eventfd_read(_fd, &v);
#elif defined(WITH_KQUEUE)
			struct kevent kev = {};
			EV_SET(&kev, 0, EVFILT_USER, EV_DISABLE, NOTE_FFNOP | NOTE_TRIGGER, 0, nullptr);
			kevent(_fd, &kev, 1, nullptr, 0, nullptr);
#endif
			if (!_ring->empty()) // Check for race
				wake();
		}
	}

	int push(Logger * log, tll::time_point ts, tll_logger_level_t level, std::string_view body)
	{
		std::unique_lock<std::mutex> lock(_lock);
		void * data;
		if (_ring->write_begin(&data, sizeof(Header) + body.size())) {
			_lock.unlock();
			return log->impl->log(ts, level, body);
		}
		auto header = (Header *) data;
		header->logger = log->ref();
		header->timestamp = ts;
		header->level = level;
		memcpy(header + 1, body.data(), body.size());
		wake();
		_ring->write_end(data, sizeof(Header) + body.size());
		return 0;
	}
};

} // namespace tll::logger

#endif//_LOGGER_THREAD_H
