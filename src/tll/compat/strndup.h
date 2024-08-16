// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_COMPAT_STRNDUP_H
#define _TLL_COMPAT_STRNDUP_H

#ifdef WIN32
#include <stddef.h>
#include <stdlib.h>
#include <memory.h>

static inline char *strndup(const char * s, size_t n)
{
	const char * ptr = (const char *) memchr(s, 0, n);
	if (ptr != 0)
		n = ptr - s;
	char * r = (char *) malloc(n + 1);
	memcpy(r, s, n);
	r[n] = 0;
	return r;
}
#endif

#endif//_TLL_COMPAT_STRNDUP_H
