#ifndef _TLL_COMPAT_FALLOCATE_H
#define _TLL_COMPAT_FALLOCATE_H

#ifdef __APPLE__
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/** Implement posix_fallocate using fcntl(F_PREALLOCATE, ...) on macos
 * Warning: this implementation does not check that offset + len is greater then
 * file size and can truncate it
 */
inline int posix_fallocate(int fd, off_t offset, off_t len)
{
	fstore_t param = {
		.fst_flags = F_ALLOCATEALL,
		.fst_posmode = F_PEOFPOSMODE,
		.fst_offset = 0,
		.fst_length = offset + len,

	};
	if (fcntl(fd, F_PREALLOCATE, &param) == -1)
		return errno;
	if (ftruncate(fd, offset + len) == -1)
		return errno;
	return 0;
}

#endif

#endif//_TLL_COMPAT_FALLOCATE_H
