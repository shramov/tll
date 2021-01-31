/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/ring.h"

#include <errno.h>
#include <string.h>

#include <memory>
#include <thread>

using ring_guard = std::unique_ptr<ringbuffer_t, decltype(&ring_free)>;

TEST(Ring, Base)
{
	ringbuffer_t ring = {};
	ASSERT_EQ(ring_init(&ring, 128, nullptr), 0);
	ring_guard guard(&ring, &ring_free);

	void * wptr;
	const void * rptr;
	size_t rsize;

	ASSERT_EQ(ring_read(&ring, &rptr, &rsize), EAGAIN);
	ASSERT_EQ(ring_shift(&ring), EAGAIN);

	ASSERT_EQ(ring_write_begin(&ring, &wptr, 128), ERANGE);
	ASSERT_EQ(ring_write_begin(&ring, &wptr, 16), 0);
	memset(wptr, 'a', 16);

	ASSERT_EQ(ring_read(&ring, &rptr, &rsize), EAGAIN);
	ASSERT_EQ(ring_shift(&ring), EAGAIN);

	ASSERT_EQ(ring_write_end(&ring, wptr, 8), 0);

	ASSERT_EQ(ring_read(&ring, &rptr, &rsize), 0);
	ASSERT_EQ(rsize, 8u);
	ASSERT_EQ(memcmp(rptr, "aaaaaaaa", 8), 0);

	ASSERT_EQ(ring_shift(&ring), 0);

	ASSERT_EQ(ring_read(&ring, &rptr, &rsize), EAGAIN);
	ASSERT_EQ(ring_shift(&ring), EAGAIN);
}

TEST(Ring, Iter)
{
	ringbuffer_t ring = {};
	ASSERT_EQ(ring_init(&ring, 128, nullptr), 0);
	ring_guard guard(&ring, &ring_free);
	ringiter_t iter = {};

	ASSERT_EQ(ring_iter_init(&ring, &iter), 0);

	void * wptr;
	const void * rptr;
	size_t rsize;

	ASSERT_EQ(ring_iter_read(&iter, &rptr, &rsize), EAGAIN);
	ASSERT_EQ(ring_iter_shift(&iter), EAGAIN);

	ASSERT_EQ(ring_write_begin(&ring, &wptr, 16), 0);
	memset(wptr, 'a', 16);

	ASSERT_EQ(ring_iter_read(&iter, &rptr, &rsize), EAGAIN);
	ASSERT_EQ(ring_iter_shift(&iter), EAGAIN);

	ASSERT_EQ(ring_write_end(&ring, wptr, 8), 0);

	ASSERT_EQ(ring_iter_read(&iter, &rptr, &rsize), 0);
	ASSERT_EQ(rsize, 8u);
	ASSERT_EQ(memcmp(rptr, "aaaaaaaa", 8), 0);

	ASSERT_EQ(ring_iter_shift(&iter), 0);

	ASSERT_EQ(ring_iter_read(&iter, &rptr, &rsize), EAGAIN);
	ASSERT_EQ(ring_iter_shift(&iter), EAGAIN);

	ASSERT_EQ(ring_iter_init(&ring, &iter), 0);
	ASSERT_EQ(ring_shift(&ring), 0);

	ASSERT_EQ(ring_iter_invalid(&iter), EINVAL);
	ASSERT_EQ(ring_iter_read(&iter, &rptr, &rsize), EINVAL);
	ASSERT_EQ(ring_iter_shift(&iter), EINVAL);

}

static constexpr unsigned MSIZE = 37;
static constexpr unsigned MDATA = 57;

void writer(ringbuffer_t * ring, size_t count, bool * stop)
{
	void * ptr;
	for (auto i = 0u; i < count; i++) {
		const auto c = 'A' + i % MDATA;
		const auto s = i % MSIZE;
		int r = EAGAIN;
		do {
			if (*stop) return;
			r = ring_write_begin(ring, &ptr, sizeof(size_t) + s);
			if (r) {
				std::this_thread::yield();
				continue;
			}
			auto data = (size_t *) ptr;
			*data = i;
			memset(data + 1, c, s);
			ring_write_end(ring, ptr, sizeof(size_t) + s);
		} while (r);
	}
}

TEST(Ring, Thread)
{
	ringbuffer_t ring = {};
	ASSERT_EQ(ring_init(&ring, 1024, nullptr), 0);
	ring_guard guard(&ring, &ring_free);

	const size_t count = 1000;
	bool stop = false;
	std::thread t(writer, &ring, count, &stop);

	const void * ptr;
	size_t size;
	size_t idx = 0;
	while (!HasFailure()) {
		if (idx == count) break;
		auto r = ring_read(&ring, &ptr, &size);
		if (r) {
			EXPECT_EQ(r, EAGAIN);
			std::this_thread::yield();
			continue;
		}

		const char c = 'A' + idx % MDATA;
		auto data = (const size_t *) ptr;
		EXPECT_EQ(size, sizeof(size_t) + idx % MSIZE);
		EXPECT_EQ(*data, idx);
		auto str = (const char *) (data + 1);
		for (auto s = str; s < str + size - sizeof(size_t) && !HasFailure(); s++)
			EXPECT_EQ(*s, c);
		idx++;
		EXPECT_EQ(ring_shift(&ring), 0);
	}

	stop = true;
	t.join();
}
