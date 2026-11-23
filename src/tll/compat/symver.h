// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_COMPAT_SYMVER_H
#define _TLL_COMPAT_SYMVER_H

/**
 * Define symver macro that can be used to add new version of some function:
 *
 * // Old versions left for compatibility
 * TLL_SYMVER(func_v0, "func@TLL_0.0.0)
 * int func_v0() { return 0; }
 *
 * // Default variant that is marked with '@@' separator
 * TLL_SYMVER(func_v1, "func@@TLL_0.1.0)
 * int func_v1() { return 1; }
 *
 * // Platforms that do not support symver have only one function
 * #ifndef WITH_TLL_SYMVER
 * int func() { return func_v1(); }
 * #endif
 */

#if defined(__linux__) || defined(__FreeBSD__)
# define WITH_TLL_SYMVER
# if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 10
#  define TLL_SYMVER(func, alias) __attribute__((symver(alias)))
# else
#  define TLL_SYMVER(func, alias) __asm__(".symver " #func "," alias);
# endif
#else
# undef WITH_TLL_SYMVER
# define TLL_SYMVER(func, alias)
#endif

#endif//_TLL_COMPAT_SYMVER_H
