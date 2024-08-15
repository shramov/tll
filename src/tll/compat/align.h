// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_COMPAT_ALIGN_H
#define _TLL_COMPAT_ALIGN_H

#ifdef _MSC_VER
#define TLL_ALIGN(SIZE) __declspec(align(SIZE))
#else
#define TLL_ALIGN(SIZE) __attribute__((aligned(SIZE)))
#endif

#endif//_TLL_COMPAT_ALIGN_H
