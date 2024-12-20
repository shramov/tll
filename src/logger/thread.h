#ifndef _LOGGER_THREAD_H
#define _LOGGER_THREAD_H

#include "tll/cppring.h"
#include "tll/logger.h"
#include "tll/util/time.h"

#include <poll.h>
#include <sys/eventfd.h>
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

		if (_fd != -1)
			close(_fd);
		_fd = -1;
	}

	int init(size_t size)
	{
		_fd = eventfd(0, EFD_SEMAPHORE | EFD_NONBLOCK | EFD_CLOEXEC);
		if (_fd == -1)
			return EINVAL;
		_ring = tll::Ring::allocate(size);
		if (!_ring)
			return EINVAL;

		_thread = std::thread(&Thread::run, this);
		return 0;
	}

	void stop()
	{
		_stop = true;
		eventfd_write(_fd, 1);
	}

	void run()
	{
		pollfd  pfd = { .fd = _fd, .events = POLLIN };
		_log.debug("Logger thread started");
		while (!_stop || !_ring->empty()) {
			if (_ring->empty())
				poll(&pfd, 1, 1000); // Return code is not important here, used as wakeable sleep
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

		eventfd_t v;
		if (eventfd_read(_fd, &v) != 0 && errno == EAGAIN)
			return; // Try again, so ring and efd count are in sync

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
		_ring->write_end(data, sizeof(Header) + body.size());
		eventfd_write(_fd, 1);
		return 0;
	}
};

} // namespace tll::logger

#endif//_LOGGER_THREAD_H
