/*
 * Copyright (c) 2019-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

/** @file
 * Subsystem for gathering simple runtime statistics inside program with minimal overhead
 * for provider.
 */

#ifndef __TLL_STAT_H
#define __TLL_STAT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Different aggregation methods for field values
typedef enum {
	TLL_STAT_SUM = 0, ///< Sum of all values
	TLL_STAT_MIN = 1, ///< Minimum of values
	TLL_STAT_MAX = 2, ///< Maximum of values
	TLL_STAT_LAST = 3, ///< Value of last update
} tll_stat_method_t;

/// Type of value
typedef enum {
	TLL_STAT_INT = 0, ///< Integer value
	TLL_STAT_FLOAT = 1, ///< Floating point value
} tll_stat_type_t;

typedef enum {
	TLL_STAT_UNIT_UNKNOWN = 0,
	TLL_STAT_UNIT_BYTES = 1,
	TLL_STAT_UNIT_NS = 2,
} tll_stat_unit_t;

typedef int64_t tll_stat_int_t;
typedef double tll_stat_float_t;
/**
 * Single stat data. Divided into two parts: 8 bytes of data (`value`) and 8 bytes of descriptor
 * (`method`, `unit` and `name`). Descriptor is written on initialization and is never changed later.
 * Value is updated with new data according to aggregation rule in `method`.
 */
typedef struct
{
	unsigned char method:4; ///< Aggregation method @ref tll_stat_method_t
	unsigned char type:1; ///< Value type, @ref tll_stat_type_t
	unsigned char unit:3; ///< Field units @ref tll_stat_unit_t
	char name[7];         ///< Field name. Trailing `\0` may be not included for 7 byte names
	union {
		tll_stat_int_t value;    ///< Integer aggregated value
		tll_stat_float_t fvalue; ///< Floating point aggregated value
	};
} tll_stat_field_t;

typedef struct
{
	tll_stat_field_t * fields;
	size_t size;
} tll_stat_page_t;

typedef struct
{
	tll_stat_page_t * lock;
	tll_stat_page_t * active;
	tll_stat_page_t * inactive;
	const char * name;
} tll_stat_block_t;

tll_stat_int_t tll_stat_default_int(tll_stat_method_t);
tll_stat_float_t tll_stat_default_float(tll_stat_method_t);

void tll_stat_field_reset(tll_stat_field_t *);
void tll_stat_field_update_int(tll_stat_field_t *, tll_stat_int_t value);
void tll_stat_field_update_float(tll_stat_field_t *, tll_stat_float_t value);

tll_stat_page_t * tll_stat_page_acquire(tll_stat_block_t *);
void tll_stat_page_release(tll_stat_block_t *, tll_stat_page_t *);
tll_stat_page_t * tll_stat_page_swap(tll_stat_block_t *);

typedef struct tll_stat_list_t tll_stat_list_t;
typedef struct tll_stat_iter_t tll_stat_iter_t;

tll_stat_list_t * tll_stat_list_new();
void tll_stat_list_free(tll_stat_list_t *);

int tll_stat_list_add(tll_stat_list_t *, tll_stat_block_t *);
int tll_stat_list_remove(tll_stat_list_t *, tll_stat_block_t *);

/// Get first element of stat list
tll_stat_iter_t * tll_stat_list_begin(tll_stat_list_t *);

/// Shift stat list iterator
tll_stat_iter_t * tll_stat_iter_next(tll_stat_iter_t *);

/**
 * Get block from iterator
 *
 * @return `NULL` if there is no block
 * @return pointer to underlying block, that also can be `NULL` if iterator is empty
 * @note returned pointer may be invalidated by @ref tll_stat_list_remove call and may be
 * used only when stat blocks are stable and can not be deleted while iterator is processed.
 * When they can be added and removed from other threads use @ref tll_stat_iter_swap and
 * @ref tll_stat_iter_name calls
 */
tll_stat_block_t * tll_stat_iter_block(tll_stat_iter_t *);

/**
 * Get cached name of block in iterator. Calls to @ref tll_stat_list_remove don't affect this pointer
 *
 * @return `NULL` if there is no block
 */
const char * tll_stat_iter_name(tll_stat_iter_t *);

/// Check if there is non-NULL block in this iterator
int tll_stat_iter_empty(const tll_stat_iter_t *);

/**
 * Swap active/inactive pages of underlying block.
 *
 * @return `NULL` if there is no block or if swap failed
 * @return pointer to copy of inactive page
 *
 * Swap is performed under lock, so concurent deletion of block from stat list
 * would not invalidate page or name pointers
 */
tll_stat_page_t * tll_stat_iter_swap(tll_stat_iter_t *);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <atomic>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

namespace tll::stat {

using Method = tll_stat_method_t;
constexpr auto Sum = TLL_STAT_SUM;
constexpr auto Min = TLL_STAT_MIN;
constexpr auto Max = TLL_STAT_MAX;
constexpr auto Last = TLL_STAT_LAST;

using Unit = tll_stat_unit_t;
constexpr auto Unknown = TLL_STAT_UNIT_UNKNOWN;
constexpr auto Bytes = TLL_STAT_UNIT_BYTES;
constexpr auto Ns = TLL_STAT_UNIT_NS;

template <typename T>
constexpr T default_value(Method t)
{
	switch (t) {
	case Sum: return 0;
	case Min: return std::numeric_limits<T>::max();
	case Max: return std::numeric_limits<T>::min();
	case Last: return std::numeric_limits<T>::min();
	}
	return std::numeric_limits<T>::min();;
}

template <typename T>
constexpr void update(Method m, T& v0, T v1)
{
	switch (m) {
	case Sum: v0 += v1; break;
	case Min: v0 = std::min(v0, v1); break;
	case Max: v0 = std::max(v0, v1); break;
	case Last: v0 = v1; break;
	}
}

template <typename T, Method M, Unit U = Unknown, char C0 = 0, char C1 = 0, char C2 = 0, char C3 = 0, char C4 = 0, char C5 = 0, char C6 = 0>
struct FieldT : public tll_stat_field_t
{
	FieldT()
	{
		static_assert(M == Sum || M == Min || M == Max || M == Last, "Unknown stat method");
		static_assert(std::is_same_v<T, tll_stat_int_t> || std::is_same_v<T, tll_stat_float_t>, "Invalid value type");

		type = std::is_same_v<T, tll_stat_int_t> ? TLL_STAT_INT : TLL_STAT_FLOAT;
		method = M;
		unit = U;
		const char tmp[] = {C0, C1, C2, C3, C4, C5, C6};
		memcpy(name, tmp, 7);
		reset();
	}

	T & value()
	{
		if constexpr (std::is_same_v<T, tll_stat_int_t>)
			return tll_stat_field_t::value;
		else
			return tll_stat_field_t::fvalue;
	}

	void reset()
	{
		value() = default_value<T>(M);
	}

	FieldT & operator = (T v) { update(v); return *this; }

	void update(T v) { return tll::stat::update(M, value(), v); }

};

template <Method M, Unit U = Unknown, char C0 = 0, char C1 = 0, char C2 = 0, char C3 = 0, char C4 = 0, char C5 = 0, char C6 = 0>
using Integer = FieldT<tll_stat_int_t, M, U, C0, C1, C2, C3, C4, C5, C6>;

template <Method M, Unit U = Unknown, char C0 = 0, char C1 = 0, char C2 = 0, char C3 = 0, char C4 = 0, char C5 = 0, char C6 = 0>
using Float = FieldT<tll_stat_float_t, M, U, C0, C1, C2, C3, C4, C5, C6>;

struct Page : public tll_stat_page_t
{
	Page(tll_stat_field_t * start, size_t size)
	{
		fields = start;
		this->size = size;
	}

	Page(tll_stat_field_t * start, tll_stat_field_t * last)
	{
		fields = start;
		size = last - start + 1;
	}

	Page(Page &&) = delete;
	Page(const Page &) = delete;
};

template <typename T>
struct PageT : public Page
{
	T data;

	PageT() : Page((tll_stat_field_t *) &data, sizeof(T) / sizeof(tll_stat_field_t)) {}

	// offsetof is not permitted for non standard layout types
	constexpr ptrdiff_t _offset() const { return ((const char *) &data) - ((const char *) this); }
	static constexpr ptrdiff_t offset() { return static_cast<const PageT<T> *>(nullptr)->_offset(); }
	static constexpr PageT * page_cast(T * ptr)
	{
		return (PageT *) (((char *) ptr) - offset());
	}
};

/**
 * Acquire lock of active stat page
 *
 * @return Pointer or `NULL` if active page is locked by another writer
 *
 * For single-writer programs `NULL` check can be omitted.
 */
inline tll_stat_page_t * acquire(tll_stat_block_t * b)
{
	auto a = (std::atomic<tll_stat_page_t *> *) &b->lock;
	return a->exchange(nullptr, std::memory_order_acquire);
}

/**
 * Release lock of active page
 *
 * @param p Pointer to page returned from `acquire` call, not `NULL`
 */
inline void release(tll_stat_block_t * b, tll_stat_page_t * p)
{
	auto a = (std::atomic<tll_stat_page_t *> *) &b->lock;
	a->store(p, std::memory_order_release);
}

/**
 * Swap active and inactive pages if possible
 * @return `NULL` if active page is locked
 * @return pointer to inactive page on success
 */
inline tll_stat_page_t * swap(tll_stat_block_t * b)
{
	auto a = (std::atomic<tll_stat_page_t *> *) &b->lock;
	auto tmp = b->active;
	if (!a->compare_exchange_weak(tmp, b->inactive))
		return nullptr;
	std::swap(b->active, b->inactive);
	return b->inactive;
}

template <typename T>
class BlockT : public tll_stat_block_t
{
 protected:
	static constexpr bool derived = std::is_base_of_v<tll_stat_page_t, T>;
	using page_t = std::conditional_t<derived, T, PageT<T>>;

	std::string _name;
 public:
	BlockT(const BlockT &) = delete;

	BlockT(std::string_view name) : _name(name)
	{
		static_assert(std::atomic<void *>::is_always_lock_free, "Need lock free atomic<void *>");
		static_assert(sizeof(std::atomic<void *>) == sizeof(void *), "Need castable atomic");
		*(tll_stat_block_t *)this = {};
		this->name = _name.c_str();
	}

	T * acquire()
	{
		auto p = (page_t *) tll::stat::acquire(this);
		if constexpr (derived)
			return p;
		else
			return &p->data;
	}

	void release(T * p)
	{
		if constexpr (derived)
			tll::stat::release(this, p);
		else
			tll::stat::release(this, page_t::page_cast(p));
	}
};

template <typename T>
class Block : public BlockT<T>
{
	typename BlockT<T>::page_t _pages[2];
 public:
	Block(std::string_view name) : BlockT<T>(name)
	{
		this->lock = this->active = &_pages[0];
		this->inactive = &_pages[1];
	}
};

template <bool Owned = false>
class ListT
{
	friend class ListT<!Owned>;
	tll_stat_list_t * _ptr = nullptr;
 public:
	class iterator {
		tll_stat_iter_t * _ptr;
	 public:
		iterator(tll_stat_iter_t *ptr) : _ptr(ptr) {}

		bool operator == (const iterator &rhs) const { return _ptr == rhs._ptr; }
		bool operator != (const iterator &rhs) const { return _ptr != rhs._ptr; }

		iterator & operator ++ () { _ptr = tll_stat_iter_next(_ptr); return *this; }
		iterator operator ++ (int) { auto tmp = *this; ++*this; return tmp; }

		operator tll_stat_iter_t * () { return _ptr; }
		tll_stat_iter_t * operator * () { return _ptr; }
	};

	explicit ListT(tll_stat_list_t *ptr) : _ptr(ptr) {}
	ListT() { if (Owned) _ptr = tll_stat_list_new(); }
	~ListT() { if (Owned) tll_stat_list_free(_ptr); }

	ListT(ListT &&l) { std::swap(_ptr, l._ptr); }

	ListT(const ListT &l) : _ptr(l._ptr) { static_assert(!Owned, "Can not copy owned list"); }
	ListT(const ListT<!Owned> &l) : _ptr(l._ptr) { static_assert(!Owned, "Can not copy owned list"); }

	iterator begin() { return iterator(tll_stat_list_begin(_ptr)); }
	iterator end() { return iterator(nullptr); }

	operator tll_stat_list_t * () { return _ptr; }

	int add(tll_stat_block_t * block) { return tll_stat_list_add(_ptr, block); }
	int remove(tll_stat_block_t * block) { return tll_stat_list_remove(_ptr, block); }

	int add(tll_stat_block_t & block) { return add(&block); }
	int remove(tll_stat_block_t & block) { return remove(&block); }
};

using List = ListT<false>;
using OwnedList = ListT<true>;

} // namespace tll::stat

#endif//__cplusplus

#endif//__TLL_STAT_H
