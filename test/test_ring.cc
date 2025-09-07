/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/ring.h"
#include "tll/cppring.h"

#include <errno.h>
#include <string.h>

#include <memory>
#include <thread>

#include <fmt/format.h>

using ring_guard = std::unique_ptr<ringbuffer_t, decltype(&ring_free)>;

TEST(Ring, Base)
{
	ringbuffer_t ring = {};
	ring_guard guard(&ring, &ring_free);
	ASSERT_EQ(ring_init(&ring, 128, nullptr), 0);

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

TEST(Ring, CppBase)
{
	auto ring = tll::Ring::allocate(128);

	void * wptr = nullptr;
	const void * rptr = nullptr;
	size_t rsize;

	ASSERT_EQ(ring->read(&rptr, &rsize), EAGAIN);
	ASSERT_EQ(ring->shift(), EAGAIN);

	ASSERT_EQ(ring->write_begin(&wptr, 128), ERANGE);
	ASSERT_EQ(ring->write_begin(&wptr, 16), 0);
	memset(wptr, 'a', 16);

	ASSERT_EQ(ring->read(&rptr, &rsize), EAGAIN);
	ASSERT_EQ(ring->shift(), EAGAIN);

	ASSERT_EQ(ring->write_end(wptr, 8), 0);

	ASSERT_EQ(ring->read(&rptr, &rsize), 0);
	ASSERT_EQ(rsize, 8u);
	ASSERT_EQ(memcmp(rptr, "aaaaaaaa", 8), 0);

	ASSERT_EQ(ring->shift(), 0);

	ASSERT_EQ(ring->read(&rptr, &rsize), EAGAIN);
	ASSERT_EQ(ring->shift(), EAGAIN);
}

TEST(Ring, Iter)
{
	ringbuffer_t ring = {};
	ring_guard guard(&ring, &ring_free);
	ASSERT_EQ(ring_init(&ring, 128, nullptr), 0);
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
	ring_guard guard(&ring, &ring_free);
	ASSERT_EQ(ring_init(&ring, 1024, nullptr), 0);

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

void ringpub(ringbuffer_t * ring, size_t count, bool * stop)
{
	void * ptr;
	for (auto i = 0u; i < count; i++) {
		const auto c = 'A' + i % MDATA;
		const auto size = i % MSIZE;

		if (*stop) return;
		while (ring_write_begin(ring, &ptr, sizeof(size_t) + size))
			ring_shift(ring);
		auto data = (size_t *) ptr;
		*data = i;
		memset(data + 1, c, size);
		ring_write_end(ring, ptr, sizeof(size_t) + size);
	}
	*stop = true;
}

TEST(Ring, IterRead)
{
	ringbuffer_t ring = {};
	ring_guard guard(&ring, &ring_free);
	ASSERT_EQ(ring_init(&ring, 1024 * 1024, nullptr), 0);

	const size_t count = 1000000;
	bool stop = false;
	std::thread t(ringpub, &ring, count, &stop);

	const void * ptr;
	size_t size;
	size_t idx = 0;
	size_t checked = 0;

	char buf[sizeof(size_t) + MSIZE];

	ringiter_t iter;

	EXPECT_EQ(ring_iter_init(&ring, &iter), 0);
	EXPECT_FALSE(ring_iter_invalid(&iter));

	while (!HasFailure() && idx != count && !stop) {
		if (ring_iter_invalid(&iter)) {
			if (ring_iter_init(&ring, &iter))
				continue;
		}

		if (ring_iter_read(&iter, &ptr, &size))
			continue;

		memcpy(buf, ptr, size);

		if (ring_iter_shift(&iter))
			continue;

		auto data = (const size_t *) buf;
		idx = *data;
		EXPECT_EQ(size, sizeof(size_t) + idx % MSIZE);
		const char c = 'A' + idx % MDATA;
		auto str = (const char *) (data + 1);
		for (auto s = str; s < str + size - sizeof(size_t) && !HasFailure(); s++)
			EXPECT_EQ(*s, c);
		checked++;
	}

	fmt::print("Checked {} of {} messages ({:.2f}%)\n", checked, count, 100. * checked / count);

	stop = true;
	t.join();
}

void cppringpub(tll::PubRing * ring, size_t count, bool * stop)
{
	void * ptr;
	for (auto i = 0u; i < count; i++) {
		const auto c = 'A' + i % MDATA;
		const auto size = i % MSIZE;

		if (*stop) return;
		while (ring->write_begin(&ptr, sizeof(size_t) + size))
			ring->shift();
		auto data = (size_t *) ptr;
		*data = i;
		memset(data + 1, c, size);
		ring->write_end(ptr, sizeof(size_t) + size);
	}
	*stop = true;
}

TEST(Ring, CppIterRead)
{
	auto ring = tll::PubRing::allocate(1024 * 1024);

	const size_t count = 1000000;
	bool stop = false;
	std::thread t(cppringpub, ring.get(), count, &stop);

	const void * ptr;
	size_t size;
	size_t idx = 0;
	size_t checked = 0;

	char buf[sizeof(size_t) + MSIZE];

	auto iter = ring->end();

	EXPECT_TRUE(iter.valid());

	while (!HasFailure() && idx != count && !stop) {
		if (!iter.valid()) {
			iter = ring->begin();
			if (!iter.valid())
				continue;
		}

		if (iter.read(&ptr, &size))
			continue;

		memcpy(buf, ptr, size);

		if (iter.shift())
			continue;

		auto data = (const size_t *) buf;
		idx = *data;
		EXPECT_EQ(size, sizeof(size_t) + idx % MSIZE);
		const char c = 'A' + idx % MDATA;
		auto str = (const char *) (data + 1);
		for (auto s = str; s < str + size - sizeof(size_t) && !HasFailure(); s++)
			EXPECT_EQ(*s, c);
		checked++;
	}

	fmt::print("Checked {} of {} messages ({:.2f}%)\n", checked, count, 100. * checked / count);

	stop = true;
	t.join();
}
