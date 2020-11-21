/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/scheme.h"
#include "tll/scheme/types.h"

#include <memory>

static const char scheme[] = "yaml://" SCHEME_PATH;

using namespace tll::scheme;

#define CHECK_FIELD(f, n, t, s, o) do { \
		ASSERT_NE(f, nullptr); \
		EXPECT_STREQ(f->name, n); \
		EXPECT_EQ(f->type, t); \
		EXPECT_EQ(f->size, s); \
		EXPECT_EQ(f->offset, o); \
	} while (0)

TEST(Scheme, Size)
{
	SchemePtr s(Scheme::load(scheme), tll_scheme_unref);
	ASSERT_NE(s.get(), nullptr);
	auto m = s->messages;
	ASSERT_NE(m, nullptr);
	EXPECT_STREQ(m->name, "sub");
	EXPECT_EQ(m->msgid, 0);
	EXPECT_EQ(m->enums, nullptr);
	auto f = m->fields;
	CHECK_FIELD(f, "s0", Field::Int32, 4u, 0u); f = f->next;
	CHECK_FIELD(f, "s1", Field::Array, 1u + 8u * 4u, 4u);
	CHECK_FIELD(f->count_ptr, "s1_count", Field::Int8, 1u, 0u);
	CHECK_FIELD(f->type_array, "s1", Field::Double, 8u, 1u);
	f = f->next;
	ASSERT_EQ(f, nullptr);
	const size_t sub_size = 4u + 1u + 4 * 8u;
	EXPECT_EQ(m->size, sub_size);

	m = m->next;
	ASSERT_NE(m, nullptr);
	EXPECT_STREQ(m->name, "test");
	EXPECT_EQ(m->msgid, 1);
	EXPECT_EQ(m->enums, nullptr);
	f = m->fields;
	CHECK_FIELD(f, "f0", Field::Int8, 1u, 0u); f = f->next;
	CHECK_FIELD(f, "f1", Field::Int64, 8u, 1u); f = f->next;
	CHECK_FIELD(f, "f2", Field::Double, 8u, 9u); f = f->next;
	CHECK_FIELD(f, "f3", Field::Decimal128, 16u, 17u); f = f->next;
	CHECK_FIELD(f, "f4", Field::Bytes, 32u, 33u); f = f->next;
	CHECK_FIELD(f, "f5", Field::Pointer, 8u, 65u);
	CHECK_FIELD(f->type_ptr, "f5", Field::Int16, 2u, 0u); f = f->next;
	CHECK_FIELD(f, "f6", Field::Array, 2u + 4 * sub_size, 73u);
	CHECK_FIELD(f->count_ptr, "f6_count", Field::Int16, 2u, 0u);
	CHECK_FIELD(f->type_array, "f6", Field::Message, sub_size, 2u); ASSERT_EQ(f->type_array->type_msg, s->messages); f = f->next;
	CHECK_FIELD(f, "f7", Field::Pointer, 8u, 73u + 2u +  4 * sub_size); ASSERT_EQ(f->sub_type, Field::ByteString);
	CHECK_FIELD(f->type_ptr, "f7", Field::Int8, 1u, 0u); f = f->next;
	CHECK_FIELD(f, "f8", Field::Pointer, 8u, 73u + 2u +  4 * sub_size + 8); ASSERT_EQ(f->sub_type, Field::SubNone);
	CHECK_FIELD(f->type_ptr, "f8", Field::Pointer, 8u, 0u);  ASSERT_EQ(f->type_ptr->sub_type, Field::ByteString);
	CHECK_FIELD(f->type_ptr->type_ptr, "f8", Field::Int8, 1u, 0u); f = f->next;
	ASSERT_EQ(f, nullptr);

	EXPECT_EQ(m->size, 1u + 8u + 8u + 16u + 32u + 8u + 2u + 4 * sub_size + 8 + 8);
	m = m->next;

	ASSERT_NE(m, nullptr);
	EXPECT_STREQ(m->name, "enums");
	EXPECT_EQ(m->msgid, 10);
	//EXPECT_EQ(m->enums, nullptr);
	f = m->fields;
	CHECK_FIELD(f, "f0", Field::Int8, 1u, 0u); EXPECT_EQ(f->sub_type, Field::Enum); EXPECT_NE(f->type_enum, nullptr); EXPECT_STREQ(f->type_enum->name, "e1"); f = f->next;
	CHECK_FIELD(f, "f1", Field::Int16, 2u, 1u); EXPECT_EQ(f->sub_type, Field::Enum); EXPECT_NE(f->type_enum, nullptr); EXPECT_STREQ(f->type_enum->name, "f1"); f = f->next;
	CHECK_FIELD(f, "f2", Field::Int32, 4u, 3u); EXPECT_EQ(f->sub_type, Field::Enum); EXPECT_NE(f->type_enum, nullptr); EXPECT_STREQ(f->type_enum->name, "e4"); f = f->next;
	CHECK_FIELD(f, "f3", Field::Int64, 8u, 7u); EXPECT_EQ(f->sub_type, Field::Enum); EXPECT_NE(f->type_enum, nullptr); EXPECT_STREQ(f->type_enum->name, "e8"); f = f->next;
	ASSERT_EQ(f, nullptr);

	EXPECT_EQ(m->size, 1u + 2u + 4u + 8u);
	m = m->next;

	ASSERT_NE(m, nullptr);
	EXPECT_STREQ(m->name, "time");
	EXPECT_EQ(m->msgid, 20);
	EXPECT_EQ(m->enums, nullptr);
	f = m->fields;
	CHECK_FIELD(f, "f0", Field::Double, 8u, 0u); EXPECT_EQ(f->sub_type, Field::Duration); EXPECT_EQ(f->time_resolution, TLL_SCHEME_TIME_DAY); f = f->next;
	CHECK_FIELD(f, "f1", Field::Int16, 2u, 8u); EXPECT_EQ(f->sub_type, Field::Duration); EXPECT_EQ(f->time_resolution, TLL_SCHEME_TIME_SECOND); f = f->next;
	CHECK_FIELD(f, "f2", Field::Int64, 8u, 10u); EXPECT_EQ(f->sub_type, Field::TimePoint); EXPECT_EQ(f->time_resolution, TLL_SCHEME_TIME_NS); f = f->next;
	ASSERT_EQ(f, nullptr);

	m = m->next;

	ASSERT_NE(m, nullptr);
	EXPECT_STREQ(m->name, "aliases");
	EXPECT_EQ(m->enums, nullptr);
	f = m->fields;
	CHECK_FIELD(f, "f0", Field::Bytes, 32u, 0u); EXPECT_EQ(f->sub_type, Field::ByteString); f = f->next;
	CHECK_FIELD(f, "f1", Field::Pointer, 8u, 32u); EXPECT_EQ(f->sub_type, Field::SubNone);
	CHECK_FIELD(f->type_ptr, "f1", Field::Bytes, 32u, 0u);  ASSERT_EQ(f->type_ptr->sub_type, Field::ByteString);  f = f->next;
	CHECK_FIELD(f, "f2", Field::Pointer, 8u, 40u); EXPECT_EQ(f->sub_type, Field::SubNone);
	CHECK_FIELD(f->type_ptr, "f2", Field::Bytes, 32u, 0u);  ASSERT_EQ(f->type_ptr->sub_type, Field::ByteString); f = f->next;
	ASSERT_EQ(f, nullptr);

	m = m->next;
	EXPECT_EQ(m, nullptr);
}

TEST(Scheme, Copy)
{
	SchemePtr ptr(Scheme::load(scheme), tll_scheme_unref);
	ASSERT_NE(ptr.get(), nullptr);
	if (!ptr) return;
	SchemePtr copy(ptr->copy(), tll_scheme_unref);
	auto m = copy->messages;
	ASSERT_NE(m, nullptr);
	ASSERT_STREQ(m->name, "sub");
	/*
	ASSERT_NE(m->fields, nullptr); ASSERT_NE(m->fields->next, nullptr); ASSERT_NE(m->fields->next->next, nullptr);

	auto f = m->fields->next;
	ASSERT_STREQ(f->name, "s1_count");
	ASSERT_STREQ(f->next->name, "s1");
	ASSERT_NE(f->next->count_ptr, nullptr);
	ASSERT_STREQ(f->next->count_ptr->name, "s1_count");
	ASSERT_EQ(f, f->next->count_ptr);
	*/
}

TEST(Scheme, OptionGetT)
{
	static const char options_scheme[] = "yamls://[{name: '', options: {a: 2, b: yes}}]";
	SchemePtr ptr(Scheme::load(options_scheme), tll_scheme_unref);
	ASSERT_NE(ptr.get(), nullptr);

	auto reader = tll::make_props_reader(ptr->options);
	ASSERT_EQ(reader.has("a"), true);
	ASSERT_EQ(reader.has("c"), false);

	ASSERT_EQ(2, reader.getT("a", 0));
	ASSERT_EQ(true, reader.getT("b", false));
	ASSERT_EQ(true, !!reader);
}
