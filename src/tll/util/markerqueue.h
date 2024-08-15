#ifndef _MARKER_QUEUE_H
#define _MARKER_QUEUE_H

/*
 * Copyright (c) 2016-2019 Pavel Shramov <shramov@mexmat.net>
 *
 * lqueue is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <vector>
#include <atomic>
#include <cstddef>
#include <cerrno>

#include <tll/compat/align.h>

/**
 * Multiple Input - Single Output queue for simple types.
 * Type must have one designated Zero value (by default 0)
 * that can not be stored and is used as empty cell marker.
 *
 * Useless if std::is_atomic_type<std::atomic<T>> == false
 */
template <typename T = intptr_t, T Zero = 0>
class MarkerQueue {
	typedef std::vector<std::atomic<T> > vector_t;
	vector_t _ring;

	typedef std::atomic<T> * pointer_t;

	pointer_t TLL_ALIGN(64) _head;
	std::atomic<pointer_t> TLL_ALIGN(64) _tail;

	pointer_t _next(pointer_t i)
	{
		if (i == &_ring.back())
			return &_ring.front();
		return i + 1;
	}
public:
	//static const T zero = Zero;

	MarkerQueue(size_t size = 1)
		: _ring(size)
		, _head(&_ring.front())
		, _tail(&_ring.front())
	{}

	/** Store new value
	 * Value must not be Zero (0 by default)
	 *
	 * @return 0 on success, EAGAIN when there is no space available
	 */
	int push(const T& data)
	{
		do {
			auto t = _tail.load(std::memory_order_consume);
			auto next = _next(t);
			if (next == _head) {
				/*
				 * Possible race:
				 *
				 * w0: load tail
				 * w1: load tail, swap value, shift tail
				 * r0: read value, shift head
				 * w0: check tail_w0 + 1 == head
				 *
				 */
				if (_tail.load(std::memory_order_consume) != t)
					continue;
				return EAGAIN;
			}
			T v = Zero;
			if (t->compare_exchange_weak(v, data, std::memory_order_release)) {
				if (_tail.load(std::memory_order_consume) != t) {
					/*
					 * Possible race:
					 *
					 * w0: load tail
					 * w1: load tail, swap value, shift tail
					 * r0: read value, store 0
					 * w0: swap value
					 *
					 */
					t->store(Zero, std::memory_order_release);
					continue;
				}
				_tail.store(next, std::memory_order_release);
				return 0;
			}
		} while (true);
	}

	bool empty() const { return _tail == _head; }

	T pop()
	{
		if (_tail == _head) return 0;
		T r = _head->exchange(Zero, std::memory_order_acq_rel);
		_head = _next(_head);
		return r;
	}

	void clear()
	{
		_head = &_ring.front();
		_tail = &_ring.front();
		for (auto & i : _ring)
			i = Zero;
	}

	void resize(size_t size)
	{
		vector_t tmp(size);
		_ring.swap(tmp);
		clear();
	}
};

#endif//_MARKER_QUEUE_H
