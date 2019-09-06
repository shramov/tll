#include "tll/stat.h"

#include <cerrno>
#include <list>
#include <mutex>
#include <shared_mutex>
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

struct tll_stat_iter_t
{
	tll_stat_block_t * block = nullptr;
	tll_stat_block_t * cached = nullptr;
	struct tll_stat_iter_t * next = nullptr;

	std::mutex lock;
	std::vector<tll_stat_field_t> buf;
	tll_stat_page_t page;
	std::string name;

	tll_stat_page_t * swap()
	{
		std::lock_guard<std::mutex> l(lock);
		if (!block) return nullptr;

		auto p = tll::stat::swap(block);
		if (!p) return nullptr;

		if (cached != block)
			update();

		for (auto i = 0u; i < p->size; i++) {
			page.fields[i].value = p->fields[i].value;
			tll_stat_field_reset(&p->fields[i]);
		}
		return &page;
	}

	/**
	 * Update local copy of page and name.
	 *
	 * It has to be done in swap() call so add/remove of block can not invalidate
	 * name or page buffer.
	 */
	void update()
	{
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
	tll_stat_iter_t * head = nullptr;
	std::mutex lock;

	~tll_stat_list_t()
	{
		auto i = head;
		while (i) {
			auto tmp = i->next;
			delete i;
			i = tmp;
		}
	}
};

tll_stat_iter_t * tll_stat_list_begin(tll_stat_list_t *l)
{
	if (!l) return nullptr;
	return l->head;
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

const char * tll_stat_iter_name(const tll_stat_iter_t *i)
{
	if (!i || !i->block) return nullptr;
	return i->name.c_str();
}

tll_stat_iter_t * tll_stat_iter_next(tll_stat_iter_t *i)
{
	if (!i) return nullptr;
	return i->next;
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
	std::lock_guard<std::mutex> l(list->lock);

	for (auto i = list->head; i; i = i->next) {
		if (i->block == b) return EEXIST;
	}

	auto i = &list->head;
	for (; *i; i = &(*i)->next) {
		if ((*i)->block) continue;

		std::lock_guard<std::mutex> li((*i)->lock);
		if ((*i)->block) continue;
		(*i)->block = b;
		(*i)->cached = nullptr;
		return 0;
	}
	*i = new tll_stat_iter_t { b };
	return 0;
}

int tll_stat_list_remove(tll_stat_list_t * list, tll_stat_block_t * b)
{
	if (!list) return EINVAL;

	for (auto i = list->head; i; i = i->next) {
		if (i->block != b) continue;

		std::lock_guard<std::mutex> li(i->lock);
		if (i->block != b) continue;
		i->block = nullptr;
		return 0;
	}

	return ENOENT;
}
