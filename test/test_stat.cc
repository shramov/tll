/*
 * Copyright (c) 2019 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"
#include "fmt/format.h"

#include "tll/stat.h"

#include "test_compat.h"

template <typename T>
class StatT : public ::testing::Test {};

using StatTypes = ::testing::Types<tll_stat_int_t, tll_stat_float_t>;
TYPED_TEST_SUITE(StatT, StatTypes);

TYPED_TEST(StatT, Field)
{
	using T = TypeParam;
	using namespace tll::stat;
	FieldT<T, Sum, Bytes, 'r', 'x'> rsum;
	FieldT<T, Min, Bytes, 'r', 'x'> rmin;
	FieldT<T, Max, Bytes, 'r', 'x'> rmax;
	FieldT<T, Last, Bytes, 'r', 'x'> rlst;

	tll_stat_type_t type = std::is_same_v<T, tll_stat_int_t> ? TLL_STAT_INT : TLL_STAT_FLOAT;
	ASSERT_EQ((tll_stat_type_t) rsum.type, type);
	ASSERT_STREQ((const char *) rsum.name, "rx");
	ASSERT_EQ(rsum.value(), 0);
	ASSERT_EQ(rmin.value(), std::numeric_limits<T>::max());
	ASSERT_EQ(rmax.value(), std::numeric_limits<T>::min());
	ASSERT_EQ(rlst.value(), std::numeric_limits<T>::min());

	rsum = 10;
	rmin = 10;
	rmax = 10;
	rlst = 10;

	ASSERT_EQ(rsum.value(), 10);
	ASSERT_EQ(rmin.value(), 10);
	ASSERT_EQ(rmax.value(), 10);
	ASSERT_EQ(rlst.value(), 10);

	rsum.update(20);
	rmin.update(20);
	rmax.update(20);
	rlst.update(20);

	ASSERT_EQ(rsum.value(), 10 + 20);
	ASSERT_EQ(rmin.value(), 10);
	ASSERT_EQ(rmax.value(), 20);
	ASSERT_EQ(rlst.value(), 20);

	if (std::is_same_v<T, tll_stat_int_t>) {
		tll_stat_field_update_int(&rsum, 5);
		tll_stat_field_update_int(&rmin, 5);
		tll_stat_field_update_int(&rlst, 5);
	} else {
		tll_stat_field_update_float(&rsum, 5);
		tll_stat_field_update_float(&rmin, 5);
		tll_stat_field_update_float(&rmax, 5);
		tll_stat_field_update_float(&rlst, 5);
	}

	ASSERT_EQ(rsum.value(), 10 + 20 + 5);
	ASSERT_EQ(rmin.value(), 5);
	ASSERT_EQ(rmax.value(), 20);
	ASSERT_EQ(rlst.value(), 5);

	rsum.reset(); rmin.reset(); rmax.reset(); rlst.reset();

	ASSERT_EQ(rsum.value(), 0);
	ASSERT_EQ(rmin.value(), std::numeric_limits<T>::max());
	ASSERT_EQ(rmax.value(), std::numeric_limits<T>::min());
	ASSERT_EQ(rlst.value(), std::numeric_limits<T>::min());
}

struct Data
{
	tll::stat::Integer<tll::stat::Sum, tll::stat::Bytes, 'r', 'x'> rsum;
	tll::stat::Integer<tll::stat::Min, tll::stat::Bytes, 'r', 'x'> rmin;
	tll::stat::Integer<tll::stat::Max, tll::stat::Bytes, 'r', 'x'> rmax;
};

TEST(Stat, Page)
{
	tll::stat::PageT<Data> p;
	ASSERT_EQ(p.fields, &p.data.rsum);
	ASSERT_EQ(p.size, 3u);
}

TEST(Stat, Block)
{
	tll::stat::Block<Data> b("test");
	ASSERT_STREQ(b.name, "test");

	ASSERT_FALSE(b.lock == nullptr);
	auto active = b.active;
	auto inactive = b.inactive;

	auto p = b.acquire();
	ASSERT_EQ(tll::stat::PageT<Data>().page_cast(p), active);
	ASSERT_EQ(b.lock, nullptr);
	ASSERT_EQ(tll::stat::swap(&b), nullptr);

	b.release(p);
	ASSERT_EQ(b.lock, tll::stat::PageT<Data>().page_cast(p));

	auto p1 = tll::stat::swap(&b);
	ASSERT_EQ(p1, active);
	ASSERT_EQ(b.lock, inactive);
	ASSERT_EQ(b.active, inactive);
	ASSERT_EQ(b.inactive, active);
}

TEST(Stat, List)
{
	tll::stat::OwnedList list;
	tll::stat::List l1 = list;
	tll::stat::Block<Data> b("test");
	ASSERT_EQ(*list.begin(), nullptr);
	list.add(b);
	auto it = list.begin();
	ASSERT_NE(*it, nullptr);
	ASSERT_EQ(tll_stat_iter_block(it), (tll_stat_block_t *) &b);

	auto page = tll_stat_iter_swap(it);
	ASSERT_NE(page, nullptr);
	ASSERT_EQ(page->size, b.inactive->size);
	ASSERT_STREQ(tll_stat_iter_name(it), "test");

	ASSERT_EQ(*++it, nullptr);
}
