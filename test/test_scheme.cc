/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/scheme.h"
#include "tll/scheme/format.h"
#include "tll/scheme/types.h"
#include "tll/util/memoryview.h"

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

namespace generated {
struct __attribute__((__packed__)) sub
{
    int32_t s0;
    int8_t s1_size;
    double s1[4];
};

struct __attribute__((__packed__)) test
{
    int8_t f0;
    int64_t f1;
    double f2;
    int8_t f3[16];
    char f4[16];
    tll_scheme_offset_ptr_t f5;
    int16_t f6_size;
    sub f6[4];
    tll_scheme_offset_ptr_t f7;
};

} // namespace generated

TEST(Scheme, Format)
{
	SchemePtr s(Scheme::load(R"(yamls://
- name: sub
  fields:
    - {name: s0, type: int32}
    - {name: s1, type: 'double[4]'}
- name: test
  id: 1
  fields:
    - {name: f0, type: int8, options: {a: 10, b: 20}}
    - {name: f1, type: int64, options.type: enum, enum: {A: 123, B: 456}}
    - {name: f2, type: double}
    - {name: f3, type: byte16}
    - {name: f4, type: byte16, options.type: string}
    - {name: f5, type: '*int16'}
    - {name: f6, type: 'sub[4]', list-options.count-type: int16}
    - {name: f7, type: '*string'}
)"), tll_scheme_unref);
	ASSERT_NE(s.get(), nullptr);

	generated::sub sub = {};
	sub.s0 = 123456;
	sub.s1_size = 2;
	sub.s1[0] = 123.456;
	sub.s1[1] = 1.5;

	const tll::scheme::Message * message = nullptr;
	for (message = s->messages; message; message = message->next) {
		if (std::string_view("sub") == message->name)
			break;
	}

	ASSERT_NE(message, nullptr);

	tll::memory mem = { &sub, sizeof(sub) };

	auto r = tll::scheme::to_string(message, tll::make_view(mem));
	ASSERT_TRUE(r);
	fmt::print("sub:\n{}\n", *r);
	ASSERT_EQ(*r, R"(s0: 123456
s1: [123.456, 1.5])");

	struct msgspace : public generated::test
	{
		int16_t f5_ptr[16];
		tll_scheme_offset_ptr_t f7_ptr[8];
		char f7_ptr_ptr[8][32];
	} msg = {};
	msg.f0 = 123;
	msg.f1 = 1234567890123ll;
	msg.f2 = 123.456;
	memcpy(msg.f3, "bytes\x01\x02\x03\x04\x05", 10);
	memcpy(msg.f4, "bytestring", 10);

	msg.f5.size = 3;
	msg.f5.entity = sizeof(int16_t);
	msg.f5.offset = (ptrdiff_t) &msg.f5_ptr - (ptrdiff_t) &msg.f5;
	msg.f5_ptr[0] = 101;
	msg.f5_ptr[1] = 111;
	msg.f5_ptr[2] = 121;
	msg.f5_ptr[3] = 131;

	msg.f6_size = 2;
	msg.f6[0].s0 = 120;
	msg.f6[0].s1_size = 2;
	msg.f6[0].s1[0] = 120.1;
	msg.f6[0].s1[1] = 120.2;
	msg.f6[1].s0 = 220;

	msg.f7.size = 1;
	msg.f7.entity = sizeof(tll_scheme_offset_ptr_t);
	msg.f7.offset = (ptrdiff_t) &msg.f7_ptr - (ptrdiff_t) &msg.f7;
	msg.f7_ptr[0].size = strlen("offset string") + 1;
	msg.f7_ptr[0].entity = 1;
	msg.f7_ptr[0].offset = (ptrdiff_t) &msg.f7_ptr_ptr - (ptrdiff_t) &msg.f7_ptr[0];
	memcpy(msg.f7_ptr_ptr[0], "offset string\0", msg.f7_ptr[0].size);

	for (message = s->messages; message; message = message->next) {
		if (std::string_view("test") == message->name)
			break;
	}

	ASSERT_NE(message, nullptr);

	mem = { &msg, sizeof(msg) };

	r = tll::scheme::to_string(message, tll::make_view(mem));
	ASSERT_TRUE(r);
	fmt::print("test:\n{}\n", *r);
	ASSERT_EQ(*r, R"(f0: 123
f1: 1234567890123
f2: 123.456
f3: "bytes\x01\x02\x03\x04\x05\x00\x00\x00\x00\x00\x00"
f4: "bytestring"
f5: [101, 111, 121]
f6:
  - s0: 120
    s1: [120.1, 120.2]
  - s0: 220
    s1: []
f7: ["offset string"])");

	mem.size = 10;
	ASSERT_FALSE(tll::scheme::to_string(message, tll::make_view(mem)));
	mem.size = message->size + 5;
	r = tll::scheme::to_string(message, tll::make_view(mem));
	ASSERT_FALSE(r);
	ASSERT_EQ(r.error(), "Failed to format field f5: Offset data out of bounds: offset 167 + data 3 * entity 2 > data size 171");

	mem.size = sizeof(msg);
	msg.f7_ptr[0].offset = 500;

	r = tll::scheme::to_string(message, tll::make_view(mem));
	ASSERT_FALSE(r);

	ASSERT_EQ(r.error(), "Failed to format field f7[0]: Offset out of bounds: offset 500 > data size 320");
}

TEST(Scheme, Import)
{
	SchemePtr s(Scheme::load("yaml://import.yaml"), tll_scheme_unref);

	ASSERT_NE(s.get(), nullptr);

	auto m = s->messages; ASSERT_NE(m, nullptr); EXPECT_STREQ(m->name, "bsub");
	m = m->next; ASSERT_NE(m, nullptr); EXPECT_STREQ(m->name, "b");
	m = m->next; ASSERT_NE(m, nullptr); EXPECT_STREQ(m->name, "c");
	m = m->next; ASSERT_NE(m, nullptr); EXPECT_STREQ(m->name, "a");
	m = m->next; ASSERT_NE(m, nullptr); EXPECT_STREQ(m->name, "top");
	m = m->next; ASSERT_EQ(m, nullptr);
}
