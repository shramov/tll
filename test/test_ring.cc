/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/ring.h"
#include "tll/cppring.h"

#include "tll/compat/jthread.h"

#include <errno.h>
#include <string.h>

#include <memory>
#include <thread>

#include <fmt/format.h>

struct ring_guard : public ringbuffer_t { ~ring_guard() { ring_free(this); } };

TEST(Ring, Base)
{
	ring_guard ring = {};
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
	ring_guard ring = {};
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

void writer(std::stop_token stop, ringbuffer_t * ring, size_t count)
{
	void * ptr;
	for (auto i = 0u; i < count; i++) {
		const auto c = 'A' + i % MDATA;
		const auto s = i % MSIZE;
		int r = EAGAIN;
		do {
			if (stop.stop_requested()) return;
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
	ring_guard ring = {};
	ASSERT_EQ(ring_init(&ring, 1024, nullptr), 0);

	const size_t count = 10000;
	std::jthread t(writer, &ring, count);

	const void * ptr;
	size_t size;
	size_t idx = 0;
	while (idx != count - 1) {
		auto r = ring_read(&ring, &ptr, &size);
		if (r) {
			ASSERT_EQ(r, EAGAIN);
			std::this_thread::yield();
			continue;
		}

		const char c = 'A' + idx % MDATA;
		auto data = (const size_t *) ptr;
		ASSERT_EQ(size, sizeof(size_t) + idx % MSIZE);
		ASSERT_EQ(*data, idx);
		auto str = (const char *) (data + 1);
		for (auto s = str; s < str + size - sizeof(size_t); s++)
			ASSERT_EQ(*s, c);
		idx++;
		ASSERT_EQ(ring_shift(&ring), 0);
	}
}

template <bool Gen>
void cppwriter(std::stop_token stop, tll::RingT<Gen, Gen> * ring, size_t count)
{
	void * ptr;
	for (auto i = 0u; i < count && !stop.stop_requested();) {
		const auto c = 'A' + i % MDATA;
		const auto s = i % MSIZE;

		if (ring->write_begin(&ptr, sizeof(size_t) + s)) {
			std::this_thread::yield();
			continue;
		}

		auto data = (size_t *) ptr;
		*data = i;
		memset(data + 1, c, s);
		ring->write_end(ptr, sizeof(size_t) + s);
		i++;
	}
}

template <typename Gen>
class RingT : public ::testing::Test {};

using GenTypes = ::testing::Types<std::true_type, std::false_type>;
TYPED_TEST_SUITE(RingT, GenTypes);

TYPED_TEST(RingT, Thread)
{
	auto ring = tll::RingT<TypeParam::value, TypeParam::value>::allocate(1024);

	const size_t count = 100000;
	std::jthread t(cppwriter<TypeParam::value>, ring.get(), count);

	const void * ptr;
	size_t size;
	size_t idx = 0;
	while (idx != count - 1) {
		if (auto r = ring->read(&ptr, &size); r) {
			ASSERT_EQ(r, EAGAIN);
			std::this_thread::yield();
			continue;
		}

		const char c = 'A' + idx % MDATA;
		auto data = (const size_t *) ptr;
		ASSERT_EQ(size, sizeof(size_t) + idx % MSIZE);
		ASSERT_EQ(*data, idx);
		auto str = (const char *) (data + 1);
		for (auto s = str; s < str + size - sizeof(size_t); s++)
			ASSERT_EQ(*s, c);
		idx++;
		ASSERT_EQ(ring->shift(), 0);
	}
}

void ringpub(std::stop_token stop, ringbuffer_t * ring, size_t count)
{
	void * ptr;
	for (auto i = 0u; i < count && !stop.stop_requested(); i++) {
		const auto c = 'A' + i % MDATA;
		const auto size = i % MSIZE;

		while (ring_write_begin(ring, &ptr, sizeof(size_t) + size))
			ring_shift(ring);
		auto data = (size_t *) ptr;
		*data = i;
		memset(data + 1, c, size);
		ring_write_end(ring, ptr, sizeof(size_t) + size);
	}
}

TEST(Ring, IterRead)
{
	ring_guard ring = {};
	ASSERT_EQ(ring_init(&ring, 1024 * 1024, nullptr), 0);

	const size_t count = 1000000;
	std::jthread t(ringpub, &ring, count);

	const void * ptr;
	size_t size;
	size_t idx = 0;
	size_t checked = 0;

	char buf[sizeof(size_t) + MSIZE];

	ringiter_t iter;

	ASSERT_EQ(ring_iter_init(&ring, &iter), 0);
	ASSERT_FALSE(ring_iter_invalid(&iter));

	while (idx != count - 1) {
		if (ring_iter_invalid(&iter)) {
			if (ring_iter_init(&ring, &iter))
				continue;
		}

		if (ring_iter_read(&iter, &ptr, &size))
			continue;

		ASSERT_LE(size, sizeof(size_t) + MSIZE);
		memcpy(buf, ptr, size);

		if (ring_iter_shift(&iter))
			continue;

		auto data = (const size_t *) buf;
		idx = *data;
		ASSERT_EQ(size, sizeof(size_t) + idx % MSIZE);
		const char c = 'A' + idx % MDATA;
		auto str = (const char *) (data + 1);
		for (auto s = str; s < str + size - sizeof(size_t); s++)
			ASSERT_EQ(*s, c);
		checked++;
	}

	fmt::print("Checked {} of {} messages ({:.2f}%)\n", checked, count, 100. * checked / count);
}

void cppringpub(std::stop_token stop, tll::PubRing * ring, size_t count)
{
	void * ptr;
	for (auto i = 0u; i < count && !stop.stop_requested(); i++) {
		const auto c = 'A' + i % MDATA;
		const auto size = i % MSIZE;

		while (ring->write_begin(&ptr, sizeof(size_t) + size))
			ring->shift();
		auto data = (size_t *) ptr;
		*data = i;
		memset(data + 1, c, size);
		ring->write_end(ptr, sizeof(size_t) + size);
	}
}

TEST(Ring, CppIterRead)
{
	auto ring = tll::PubRing::allocate(1024 * 1024);

	const size_t count = 1000000;
	std::jthread t(cppringpub, ring.get(), count);

	const void * ptr;
	size_t size;
	size_t idx = 0;
	size_t checked = 0;

	char buf[sizeof(size_t) + MSIZE];

	auto iter = ring->end();

	ASSERT_TRUE(iter.valid());

	while (idx != count - 1) {
		if (!iter.valid()) {
			iter = ring->begin();
			if (!iter.valid())
				continue;
		}

		if (iter.read(&ptr, &size))
			continue;

		ASSERT_LE(size, sizeof(size_t) + MSIZE);
		memcpy(buf, ptr, size);

		if (iter.shift())
			continue;

		auto data = (const size_t *) buf;
		idx = *data;
		ASSERT_EQ(size, sizeof(size_t) + idx % MSIZE);
		const char c = 'A' + idx % MDATA;
		auto str = (const char *) (data + 1);
		for (auto s = str; s < str + size - sizeof(size_t); s++)
			ASSERT_EQ(*s, c);
		checked++;
	}

	fmt::print("Checked {} of {} messages ({:.2f}%)\n", checked, count, 100. * checked / count);
}
