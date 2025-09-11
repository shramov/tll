#ifndef __LQUEUE_H
#define __LQUEUE_H

/*
 * Copyright (c) 2014-2019 Pavel Shramov <shramov@mexmat.net>
 *
 * lqueue is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <atomic>
#include <memory>
#include <optional>

/*
 * XXX: Probably there are some races...
 */
template <typename T>
class lqueue
{
	struct node {
		node * next = nullptr;
		T value = {};
	};

	std::atomic<node *> _head;
	std::atomic<node *> _tail;

public:
	lqueue() : _head(new node), _tail(_head.load()) {}
	~lqueue()
	{
		for (auto ptr = _head.load(); ptr;)
		{
			std::unique_ptr<node> p(ptr);
			ptr = ptr->next;
		}
	}

	// Passing by value to have fast push(T &&)
	void push(T value)
	{
		auto e = new node;
		do {
			auto p = _tail.load();
			if (!_tail.compare_exchange_weak(p, e))
				continue;
			std::swap(p->value, value);
			//XXX: Write barrier is needed here
			std::atomic_thread_fence(std::memory_order_release);
			p->next = e;
			return;
		} while (1);
	}

	bool empty() const
	{
		return _head.load()->next == nullptr;
	}

	std::optional<T> pop()
	{
		do {
			auto p = _head.load();
			if (!p->next) return {};
			if (!_head.compare_exchange_weak(p, p->next))
				continue;
			std::unique_ptr<node> ptr(p);
			return std::move(p->value);
		} while (1);
	}
};

#endif//__LQUEUE_H
