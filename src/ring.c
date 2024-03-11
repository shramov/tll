/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

/*
 * Copyright (c) 2013 Pavel Shramov <shramov@mexmat.net>
 *
 * ring is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

// mah: requirements:
// this _must_ run on i386, x86_64, arm UP and SMP
// it seems [3] does all we need

// comparison references:
// [1] https://subversion.assembla.com/svn/portaudio/portaudio/trunk/src/common/pa_ringbuffer.h
// [2] https://subversion.assembla.com/svn/portaudio/portaudio/trunk/src/common/pa_ringbuffer.c
// [3] https://subversion.assembla.com/svn/portaudio/portaudio/trunk/src/common/pa_memorybarrier.h
// [4]: https://github.com/jackaudio/jack2/blob/master/common/ringbuffer.c
// [5]: http://julien.benoist.name/lockfree.html
// [6]: http://julien.benoist.name/lockfree/lockfree.tar.bz2/atomic-queue/lfq.c

// general comment: I'm very wary how the atomic ops in [6] relate to this - see
// the CAS,DWCAS ops there?


#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "tll/ring.h"

#define MAX(x, y) (((x) > (y))?(x):(y))

/// Round up X to closest upper alignment boundary
static const int align = 8;
static ring_size_t size_aligned(ring_size_t x)
{
    return x + (-x & (align - 1));
}

int ring_init(ringbuffer_t *ring, size_t size, void * memory)
{
    if (!memory) {
	ring->header = (ring_header_t *)malloc(sizeof(ring_header_t) + size);
	if (ring->header == NULL)
	    return ENOMEM;
	ring->header->magic = ring_magic;
	ring->header->size = size;
	ring->header->head = ring->header->tail = 0;
	ring->header->generation_pre = ring->header->generation_post = 0;
    } else
	ring->header = (ring_header_t *)memory;
    return 0;
}

void ring_free(ringbuffer_t *ring)
{
	if (!ring) return;
	free(ring->header);
}

void ring_clear(ringbuffer_t *ring)
{
	if (!ring) return;
	ring_header_t *h = ring->header;

	h->head = h->tail = 0;
}

static inline ring_size_t * _size_at(const ring_header_t *header, size_t off)
{
    return (ring_size_t *) &header->data[off];
}

int ring_write_begin(ringbuffer_t *ring, void ** data, size_t sz)
{
    ring_header_t *h = ring->header;
    size_t a = size_aligned(sz + sizeof(ring_size_t));
    if (a > h->size)
	return ERANGE;

    size_t free = (h->size + h->head - h->tail - 1) % h->size + 1; // -1 + 1 is needed for head==tail

    //printf("Free space: %d; Need %zd\n", free, a);
    if (free <= a) return EAGAIN;
    if (h->tail + a > h->size) {
	if (h->head <= a)
	    return EAGAIN;
	*data = _size_at(h, 0) + 1;
	return 0;
    }
    *data = _size_at(h, h->tail) + 1;
    return 0;
}

int ring_write_end(ringbuffer_t *ring, void * data, size_t sz)
{
    ring_header_t *h = ring->header;
    size_t a = size_aligned(sz + sizeof(ring_size_t));
    if (data == _size_at(h, 0) + 1) {
	// Wrap
	*_size_at(h, h->tail) = -1;
	h->tail = 0;
    }
    *_size_at(h, h->tail) = sz;
    // mah: see [2]:144
    // PaUtil_WriteMemoryBarrier(); ???

    // mah: see [6]:69
    // should this be CAS(&(h->tail, h->tail, h->tail+a) ?
    h->tail = (h->tail + a) % h->size;
    //printf("New head/tail: %zd/%zd\n", h->head, h->tail);
    return 0;
}

int ring_write(ringbuffer_t *ring, const void * data, size_t sz)
{
    void * ptr;
    int r = ring_write_begin(ring, &ptr, sz);
    if (r) return r;
    memmove(ptr, data, sz);
    return ring_write_end(ring, ptr, sz);
}
static inline int _ring_read_at(const ring_header_t *h, size_t offset, const void **data, size_t *size)
{
    if (offset == h->tail)
	return EAGAIN;

    // mah: see [2]:181
    // PaUtil_ReadMemoryBarrier(); ???

    //printf("Head/tail: %zd/%zd\n", h->head, h->tail);
    const ring_size_t *sz = _size_at(h, offset);
    if (*sz < 0)
        return _ring_read_at(h, 0, data, size);

    *size = *sz;
    *data = sz + 1;
    return 0;
}

const void * ring_next(ringbuffer_t *ring)
{
    const void *data;
    size_t size;
    if (ring_read(ring, &data, &size)) return 0;
    return data;
}

ring_size_t ring_next_size(ringbuffer_t *ring)
{
    const void *data;
    size_t size;
    if (ring_read(ring, &data, &size)) return -1;
    return size;
}

int ring_read(const ringbuffer_t *ring, const void **data, size_t *size)
{
    return _ring_read_at(ring->header, ring->header->head, data, size);
}

static ssize_t _ring_shift_offset(const ring_header_t *h, size_t offset)
{
    if (h->head == h->tail)
	return -1;
    // mah: [2]:192 
    // PaUtil_FullMemoryBarrier(); ???
    ring_size_t size = *_size_at((ring_header_t *) h, offset);
    if (size < 0)
	return _ring_shift_offset(h, 0);
    size = size_aligned(size + sizeof(ring_size_t));
    return (offset + size) % h->size;
}

int ring_shift(ringbuffer_t *ring)
{
    ssize_t off = _ring_shift_offset(ring->header, ring->header->head);
    if (off < 0) return EAGAIN;
    uint64_t gen = ring->header->generation_pre + 1;
    ring->header->generation_pre = gen;
    ring->header->head = off;
    ring->header->generation_post = gen;
    return 0;
}

size_t ring_available(const ringbuffer_t *ring)
{
    const ring_header_t *h = ring->header;
    int avail = 0;
//    printf("Head/tail: %zd/%zd\n", h->head, h->tail);
    if (h->tail < h->head)
        avail = h->head - h->tail;
    else
        avail = MAX(h->head, h->size - h->tail);
    return MAX(0, avail - 2 * align);
}

void ring_dump(ringbuffer_t *ring, const char *name)
{
    if (ring_next_size(ring) < 0) {
	printf("Ring %s is empty\n", name);
	return;
    }
    printf("Data in %s: %d %.*s\n", name,
	   ring_next_size(ring), ring_next_size(ring), (char *) ring_next(ring));
}

int ring_iter_init(const ringbuffer_t *ring, ringiter_t *iter)
{
    iter->header = ring->header;
    iter->generation = ring->header->generation_post;
    iter->offset = ring->header->head;
    if (ring->header->generation_pre != iter->generation)
        return EAGAIN;
    return 0;
}

int ring_iter_invalid(const ringiter_t *iter)
{
    asm volatile("": : :"memory"); // Do not reorder or optimize this check
    if (iter->header->generation_pre > iter->generation)
        return EINVAL;
    return 0;
}

int ring_iter_shift(ringiter_t *iter)
{
    if (ring_iter_invalid(iter)) return EINVAL;
    if (iter->offset == iter->header->tail) return EAGAIN;
    ssize_t off = _ring_shift_offset(iter->header, iter->offset);
    if (ring_iter_invalid(iter)) return EINVAL;
    if (off < 0) return EAGAIN;
//    printf("Offset to %zd\n", off);
    iter->generation++;
    iter->offset = off;
    return 0;
}

int ring_iter_read(const ringiter_t *iter, const void **data, size_t *size)
{
    if (ring_iter_invalid(iter)) return EINVAL;

//    printf("Read at %zd\n", iter->offset);
    int r = _ring_read_at(iter->header, iter->offset, data, size);
    if (r) return r;
    if (ring_iter_invalid(iter)) return EINVAL; // Check, that data and size are valid
    return 0;
}

#if 0
int main()
{
    ringbuffer_t ring1, ring2;
    ringbuffer_t *ro = &ring1, *rw = &ring2;
    ring_init(ro, 1024, 0);
    ring_init(rw, 0, ro->header);

    ring_dump(ro, "ro"); ring_dump(rw, "rw");

    ring_write(rw, "test", 4);
    ring_dump(ro, "ro"); ring_dump(rw, "rw");

    ring_write(rw, "test", 0);
    ring_dump(ro, "ro"); ring_dump(rw, "rw");

    ring_shift(ro);
    ring_dump(ro, "ro"); ring_dump(rw, "rw");

    ring_shift(ro);
    ring_dump(ro, "ro"); ring_dump(rw, "rw");
}



Ring ro is empty
Ring rw is empty

Data in ro: 4 test
Data in rw: 4 test

Data in ro: 4 test
Data in rw: 4 test

Data in ro: 0 
Data in rw: 0 

Ring ro is empty
Ring rw is empty

#endif

// vim: sts=4 sw=4 noet
