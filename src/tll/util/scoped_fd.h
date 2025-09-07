// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_UTIL_SCOPED_FD_H
#define _TLL_UTIL_SCOPED_FD_H

#include <utility>

#include <unistd.h>

namespace tll::util {

class ScopedFd
{
	int _fd = -1;

 public:
	explicit ScopedFd(int fd) noexcept : _fd(fd) {}
	ScopedFd(const ScopedFd &) = delete;
	ScopedFd(ScopedFd && rhs) noexcept : _fd(std::exchange(rhs._fd, -1)) {}
	~ScopedFd() { reset(); }

	ScopedFd & operator = (const ScopedFd &) = delete;
	ScopedFd & operator = (ScopedFd && rhs) noexcept { std::swap(_fd, rhs._fd); return *this; }

	void reset(int fd = -1) noexcept
	{
		if (_fd != -1)
			::close(_fd);
		_fd = fd;
	}

	constexpr operator int () const noexcept { return _fd; }
	constexpr int get() const noexcept { return _fd; }

	int release() noexcept { return std::exchange(_fd, -1); }
};

constexpr auto format_as(const ScopedFd &fd) noexcept { return fd.get(); }

} // namespace tll::util

#endif//_TLL_UTIL_SCOPED_FD_H
