// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_COMPAT_PWRITE_H
#define _TLL_COMPAT_PWRITE_H

#ifndef _WIN32
#include <sys/uio.h>
#include <unistd.h>
#else
#include <io.h>
#endif

#ifdef __APPLE__
#if MAC_OS_X_VERSION_MIN_REQUIRED <= 1010

static inline ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
	if (lseek(fd, offset, SEEK_SET) < 0)
		return -1;
	return writev(fd, iov, iovcnt);
}

#endif
#elif defined(_WIN32)

static inline ssize_t _pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
	if (lseek(fd, offset, SEEK_SET) < 0)
		return -1;
	ssize_t full = 0;
	for (auto i = iov; i != iov + iovcnt; i++) {
		auto r = write(fd, iov->iov_base, iov->iov_len);
		if (r < 0)
			return r;
		full += r;
		if (r != iov->iov_len)
			break;
	}
	return full;
}

static inline ssize_t _pwrite(int fd, const void * buf, size_t count, off_t offset)
{
	if (lseek(fd, offset, SEEK_SET) < 0)
		return -1;
	return write(fd, buf, count);
}

static inline ssize_t pread(int fd, void * buf, size_t count, off_t offset)
{
	if (lseek(fd, offset, SEEK_SET) < 0)
		return -1;
	return read(fd, buf, count);
}

#endif

#endif//_TLL_COMPAT_PWRITE_H
