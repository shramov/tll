/*
 * Copyright (c) 2013 Pavel Shramov <shramov@mexmat.net>
 *
 * ring is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _LL_RING_H
#define _LL_RING_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {

#if __cplusplus > 202002L
#  include <stdatomic.h>
#else
#  define _Atomic(x) x
#endif

#else // __cplusplus
#include <stdatomic.h>
#endif

#ifdef _MSC_VER
#define ALIGN(SIZE) __declspec(align(SIZE))
#else
#define ALIGN(SIZE) __attribute__((aligned(SIZE)))
#endif

typedef struct
{
    int32_t magic;
    int32_t version;
    size_t size;

    _Atomic(size_t) ALIGN(64) head;
    _Atomic(uint64_t) generation_pre;
    _Atomic(uint64_t) generation_post;

    _Atomic(size_t) ALIGN(64) tail;

    char ALIGN(64) data[];
} ring_header_t;

#undef ALIGN

#define _S(x, s) (((int32_t) (x)) << 8 * (s))
static const int32_t ring_magic = _S('r', 3) | _S('i', 2) | _S('n', 1) | 'g';
#undef _S

typedef int32_t ring_size_t; // Negative numbers are needed for skips

typedef struct ringbuffer_t
{
    ring_header_t * header;
} ringbuffer_t;

typedef struct ringiter_t
{
    const ring_header_t *header;
    uint64_t generation;
    size_t offset;
} ringiter_t;


int ring_init(ringbuffer_t *ring, size_t size, void * memory);
int ring_init_file(ringbuffer_t *ring, size_t size, int fd);
void ring_free(ringbuffer_t *ring);
void ring_clear(ringbuffer_t *ring);

int ring_write_begin(ringbuffer_t *ring, void ** data, size_t size);
int ring_write_end(ringbuffer_t *ring, void * data, size_t size);
int ring_write(ringbuffer_t *ring, const void * data, size_t size);

int ring_read(const ringbuffer_t *ring, const void **data, size_t *size);
int ring_shift(ringbuffer_t *ring);

size_t ring_available(const ringbuffer_t *ring);


// Wrappers, not needed
const void * ring_next(ringbuffer_t *ring);
ring_size_t ring_next_size(ringbuffer_t *ring);

int ring_iter_init(const ringbuffer_t *ring, ringiter_t *iter);
int ring_iter_invalid(const ringiter_t *iter);
int ring_iter_shift(ringiter_t *iter);
int ring_iter_read(const ringiter_t *iter, const void **data, size_t *size);

void ring_dump(ringbuffer_t *ring, const char *name);

#ifdef __cplusplus
}; //extern "C"
#endif

#endif//_LL_RING_H
