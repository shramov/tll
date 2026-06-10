/*
 * Copyright (c) 2019-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/stat.h"
#include "tll/util/refptr.h"

#include <cerrno>
#include <mutex>
#include <vector>

tll_stat_int_t tll_stat_default_int(tll_stat_method_t t)
{
	return tll::stat::default_value<tll_stat_int_t>(t);
}

tll_stat_float_t tll_stat_default_float(tll_stat_method_t t)
{
	return tll::stat::default_value<tll_stat_float_t>(t);
}

void tll_stat_field_reset(tll_stat_field_t * f)
{
	if (f->type == TLL_STAT_INT)
		f->value = tll::stat::default_value<tll_stat_int_t>((tll_stat_method_t) f->method);
	else
		f->fvalue = tll::stat::default_value<double>((tll_stat_method_t) f->method);
}

void tll_stat_field_update_int(tll_stat_field_t * f, tll_stat_int_t v)
{
	tll::stat::update((tll_stat_method_t) f->method, f->value, v);
}

void tll_stat_field_update_float(tll_stat_field_t * f, tll_stat_float_t v)
{
	tll::stat::update((tll_stat_method_t) f->method, f->fvalue, v);
}

tll_stat_page_t * tll_stat_page_acquire(tll_stat_block_t *b)
{
	return tll::stat::acquire(b);
}

void tll_stat_page_release(tll_stat_block_t *b, tll_stat_page_t *p)
{
	tll::stat::release(b, p);
}

tll_stat_page_t * tll_stat_page_swap(tll_stat_block_t *b)
{
	return tll::stat::swap(b);
}

struct tll_stat_iter_t : public tll::util::refbase_t<tll_stat_iter_t, 0>
{
	tll_stat_block_t * block = nullptr;
	tll::util::refptr_t<struct tll_stat_iter_t> next = nullptr;

	std::mutex lock;
	std::vector<tll_stat_field_t> buf;
	tll_stat_page_t page;
	std::string name;

	tll_stat_iter_t(tll_stat_block_t * b) { reset(b); }

	tll_stat_page_t * swap()
	{
		std::lock_guard<std::mutex> l(lock);
		if (!block) return nullptr;

		auto p = tll::stat::swap(block);
		if (!p) return nullptr;

		for (auto i = 0u; i < p->size; i++) {
			page.fields[i].value = p->fields[i].value;
			tll_stat_field_reset(&p->fields[i]);
		}
		return &page;
	}

	/**
	 * Update local copy of page and name, called with lock
	 */
	void reset(tll_stat_block_t * b)
	{
		block = b;
		name = block->name;
		buf.resize(block->inactive->size);
		page.fields = &buf.front();
		page.size = buf.size();
		for (auto i = 0u; i < block->inactive->size; i++)
			page.fields[i] = block->inactive->fields[i];
	}
};

struct tll_stat_list_t
{
	tll::util::refptr_t<tll_stat_iter_t> head = nullptr;
	std::mutex lock;

	~tll_stat_list_t()
	{
		head.reset();
	}
};

tll_stat_iter_t * tll_stat_list_begin(tll_stat_list_t *l)
{
	if (!l) return nullptr;
	std::lock_guard<std::mutex> lock(l->lock);
	if (!l->head)
		return nullptr;
	return l->head->ref();
}

tll_stat_iter_t * tll_stat_iter_ref(tll_stat_iter_t *i)
{
	if (i)
		i->ref();
	return i;
}

void tll_stat_iter_free(tll_stat_iter_t *i)
{
	if (i)
		i->unref();
}

int tll_stat_iter_empty(const tll_stat_iter_t *i)
{
	return (!i || !i->block);
}

tll_stat_block_t * tll_stat_iter_block(tll_stat_iter_t *i)
{
	if (!i) return nullptr;
	return i->block;
}

const char * tll_stat_iter_name(tll_stat_iter_t *i)
{
	if (!i) return nullptr;
	return i->name.c_str();
}

tll_stat_iter_t * tll_stat_iter_next(tll_stat_iter_t *i)
{
	if (!i) return nullptr;

	tll::util::refptr_t<tll_stat_iter_t> ptr;
	{
		std::lock_guard<std::mutex> l(i->lock);
		ptr = i->next;
	}
	i->unref();

	return ptr.release();
}

tll_stat_page_t * tll_stat_iter_swap(tll_stat_iter_t *i)
{
	if (!i) return nullptr;
	return i->swap();
}

tll_stat_list_t * tll_stat_list_new()
{
	return new tll_stat_list_t();
}

void tll_stat_list_free(tll_stat_list_t *l)
{
	if (!l) return;
	delete l;
}

int tll_stat_list_add(tll_stat_list_t * list, tll_stat_block_t * b)
{
	if (!list) return EINVAL;

	std::lock_guard<std::mutex> lock(list->lock);
	for (auto i = list->head; i; i = i->next) {
		if (i->block == b) return EEXIST;
	}

	tll::util::refptr_t<tll_stat_iter_t> ptr = new tll_stat_iter_t { b };
	ptr->next = list->head;
	list->head = std::move(ptr);

	return 0;
}

int tll_stat_list_remove(tll_stat_list_t * list, tll_stat_block_t * b)
{
	if (!list) return EINVAL;

	tll::util::refptr_t<tll_stat_iter_t> prev;
	std::lock_guard<std::mutex> lock(list->lock);
	for (auto i = list->head; i; prev = i, i = i->next) {
		if (i->block != b) continue;

		if (prev) {
			std::lock_guard<std::mutex> li(prev->lock);
			prev->next = i->next;
		} else
			list->head = i->next;
		std::lock_guard<std::mutex> li(i->lock);
		i->block = nullptr;
		return 0;
	}

	return ENOENT;
}
