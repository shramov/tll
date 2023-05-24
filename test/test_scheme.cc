/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/scheme.h"
#include "tll/scheme/conv.h"
#include "tll/scheme/format.h"
#include "tll/scheme/types.h"
#include "tll/scheme/merge.h"
#include "tll/util/memoryview.h"
#include "tll/util/listiter.h"

#include <memory>

#include "test_compat.h"

static const char scheme[] = "yaml://" SCHEME_PATH;

using namespace tll::scheme;

#define CHECK_FIELD(f, n, t, s, o) do { \
		ASSERT_NE((f), nullptr); \
		EXPECT_STREQ((f)->name, n); \
		EXPECT_EQ((f)->type, t); \
		EXPECT_EQ((f)->size, s); \
		EXPECT_EQ((f)->offset, o); \
	} while (0)

#define CHECK_BIT_FIELD(f, n, s, o) do { \
		ASSERT_NE(f, nullptr); \
		EXPECT_STREQ(f->name, n); \
		EXPECT_EQ(f->size, s); \
		EXPECT_EQ(f->offset, o); \
	} while (0)

void verify_scheme(tll::scheme::Scheme * s)
{
	ASSERT_NE(s, nullptr);

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

	ASSERT_NE(m, nullptr);
	EXPECT_STREQ(m->name, "bits");
	EXPECT_EQ(m->enums, nullptr);
	f = m->fields;
	CHECK_FIELD(f, "f0", Field::Int8, 1u, 0u); EXPECT_EQ(f->sub_type, Field::Bits);
	CHECK_BIT_FIELD(f->bitfields, "a", 1u, 0u); CHECK_BIT_FIELD(f->bitfields->next, "b", 1u, 1u);
	EXPECT_EQ(f->bitfields->next->next, nullptr); f = f->next;

	CHECK_FIELD(f, "f1", Field::UInt32, 4u, 1u); EXPECT_EQ(f->sub_type, Field::Bits);
	CHECK_BIT_FIELD(f->bitfields, "c", 1u, 0u); CHECK_BIT_FIELD(f->bitfields->next, "d", 1u, 1u);
	EXPECT_EQ(f->bitfields->next->next, nullptr); f = f->next;
	ASSERT_EQ(f, nullptr);

	EXPECT_EQ(m->size, 1u + 4u);

	m = m->next;

	ASSERT_NE(m, nullptr);
	EXPECT_STREQ(m->name, "unions");
	EXPECT_EQ(m->enums, nullptr);
	f = m->fields;
	CHECK_FIELD(f, "u0", Field::Union, 1u + 8u, 0u); EXPECT_EQ(f->sub_type, Field::SubNone);
	ASSERT_NE(f->type_union, nullptr); ASSERT_STREQ(f->type_union->name, "u0"); ASSERT_EQ(f->type_union->fields_size, 3u);
	ASSERT_EQ(f->type_union->union_size, 8u);
	CHECK_FIELD(f->type_union->fields + 0, "i8", Field::Int8, 1u, 1u);
	CHECK_FIELD(f->type_union->fields + 1, "d", Field::Double, 8u, 1u);
	CHECK_FIELD(f->type_union->fields + 2, "s", Field::Pointer, 8u, 1u);
	f = f->next;
	CHECK_FIELD(f, "u1", Field::Union, 1u + 37u, 9u); EXPECT_EQ(f->sub_type, Field::SubNone);
	ASSERT_NE(f->type_union, nullptr); ASSERT_STREQ(f->type_union->name, "u1"); ASSERT_EQ(f->type_union->fields_size, 2u);
	ASSERT_EQ(f->type_union->union_size, 37u);
	CHECK_FIELD(f->type_union->fields + 0, "b32", Field::Bytes, 32u, 1u);
	CHECK_FIELD(f->type_union->fields + 1, "m", Field::Message, 37u, 1u);
	f = f->next;
	ASSERT_EQ(f, nullptr);

	ASSERT_EQ(m->size, 9u + 38u);

	m = m->next;
	EXPECT_EQ(m, nullptr);
}

TEST(Scheme, Size)
{
	SchemePtr s(Scheme::load(scheme));
	verify_scheme(s.get());
}

TEST(Scheme, Copy)
{
	SchemePtr ptr(Scheme::load(scheme));
	ASSERT_NE(ptr.get(), nullptr);

	SchemePtr copy(ptr->copy());
	verify_scheme(copy.get());
}

TEST(Scheme, Dump)
{
	SchemePtr ptr(Scheme::load(scheme));
	ASSERT_NE(ptr.get(), nullptr);

	auto dump = tll_scheme_dump(ptr.get(), "yamls");
	ASSERT_NE(dump, nullptr);
	fmt::print("Scheme:\n{}\n", dump);
	SchemePtr copy(Scheme::load(dump));
	free(dump);
	verify_scheme(copy.get());
}

TEST(Scheme, OptionGetT)
{
	static const char options_scheme[] = "yamls://[{name: '', options: {a: 2, b: yes}}]";
	SchemePtr ptr(Scheme::load(options_scheme));
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
    int64_t f8;
};

struct __attribute__((__packed__)) unions
{
    int8_t u0_type;
    union {
	    int8_t f0;
	    sub f1;
    } u0;
};

struct __attribute__((__packed__)) unions_cpp
{
    struct __attribute__((__packed__)) u0 : public tll::scheme::UnionBase<uint8_t, sizeof(sub)> {
	    int8_t unchecked_f0() const { return uncheckedT<int8_t>(); }
	    int8_t * get_f0() { return getT<int8_t>(0); }
	    const int8_t * get_f0() const { return getT<int8_t>(0); }
	    void set_f0(int8_t v) { setT(0, v); }

	    sub & unchecked_f1() { return uncheckedT<sub>(); }
	    const sub & unchecked_f1() const { return uncheckedT<sub>(); }
	    sub * get_f1() { return getT<sub>(1); }
	    const sub * get_f1() const { return getT<sub>(1); }
	    sub & set_f1() { return setT<sub>(1); }
    } u0;
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
    - {name: f8, type: int64, options.type: fixed3}
- name: unions
  fields:
    - {name: u0, type: union, union: [{name: f0, type: int8}, {name: f1, type: sub}]}
)"));
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
	msg.f8 = 12345;

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
f7: ["offset string"]
f8: 12.345)");

	mem.size = 10;
	ASSERT_FALSE(tll::scheme::to_string(message, tll::make_view(mem)));
	mem.size = message->size + 5;
	r = tll::scheme::to_string(message, tll::make_view(mem));
	ASSERT_FALSE(r);
	ASSERT_EQ(r.error(), "Failed to format field f5: Offset data out of bounds: offset 175 + data 3 * entity 2 > data size 179");

	mem.size = sizeof(msg);
	msg.f7_ptr[0].offset = 500;

	r = tll::scheme::to_string(message, tll::make_view(mem));
	ASSERT_FALSE(r);

	ASSERT_EQ(r.error(), "Failed to format field f7[0]: Offset out of bounds: offset 500 > data size 320");

	for (message = s->messages; message; message = message->next) {
		if (std::string_view("unions") == message->name)
			break;
	}

	ASSERT_NE(message, nullptr);

	generated::unions unions = {};
	auto unions_cpp = (generated::unions_cpp *) &unions;

	unions_cpp->u0.set_f0(123);
	ASSERT_EQ(unions.u0_type, 0);
	ASSERT_EQ(unions.u0.f0, 123);

	ASSERT_EQ(unions_cpp->u0.type(), 0);
	ASSERT_EQ(unions_cpp->u0.unchecked_f0(), 123);
	ASSERT_NE(unions_cpp->u0.get_f0(), nullptr);
	ASSERT_EQ(*unions_cpp->u0.get_f0(), 123);
	ASSERT_EQ(unions_cpp->u0.get_f1(), nullptr);

	mem = { &unions, sizeof(unions) };

	r = tll::scheme::to_string(message, tll::make_view(mem));
	ASSERT_TRUE(r);
	ASSERT_EQ(*r, R"({u0: {f0: 123}})");

	auto & usub = unions_cpp->u0.set_f1();
	usub.s0 = 123456;

	ASSERT_EQ(unions_cpp->u0.get_f0(), nullptr);
	ASSERT_NE(unions_cpp->u0.get_f1(), nullptr);
	ASSERT_EQ(unions_cpp->u0.get_f1()->s0, 123456);
	ASSERT_EQ(unions_cpp->u0.unchecked_f1().s0, 123456);

	ASSERT_EQ(unions.u0_type, 1);
	ASSERT_EQ(unions.u0.f1.s0, 123456);

	//unions.u0_type = 1;
	unions.u0.f1.s0 = 123456;
	unions.u0.f1.s1_size = 2;
	unions.u0.f1.s1[0] = 123.456;
	unions.u0.f1.s1[1] = 1.5;

	r = tll::scheme::to_string(message, tll::make_view(mem));
	ASSERT_TRUE(r);
	ASSERT_EQ(*r, R"(u0:
  f1:
    s0: 123456
    s1: [123.456, 1.5])");

}

TEST(Scheme, Import)
{
	SchemePtr s(Scheme::load("yaml://import.yaml"));

	ASSERT_NE(s.get(), nullptr);

	auto m = s->messages; ASSERT_NE(m, nullptr); EXPECT_STREQ(m->name, "bsub");
	m = m->next; ASSERT_NE(m, nullptr); EXPECT_STREQ(m->name, "c");
	m = m->next; ASSERT_NE(m, nullptr); EXPECT_STREQ(m->name, "b");
	m = m->next; ASSERT_NE(m, nullptr); EXPECT_STREQ(m->name, "a");
	m = m->next; ASSERT_NE(m, nullptr); EXPECT_STREQ(m->name, "top");
	m = m->next; ASSERT_EQ(m, nullptr);
}

void check_load_fail(std::string_view scheme)
{
	SchemePtr s(Scheme::load(scheme));
	ASSERT_EQ(s.get(), nullptr);
}

TEST(Scheme, Duplicates)
{
	check_load_fail(R"(yamls://
- name: msg
  fields:
    - {name: s0, type: int32}
- name: other
  fields:
    - {name: s0, type: int32}
- name: msg
  fields:
    - {name: s0, type: int32}
)");
	check_load_fail(R"(yamls://
- name: msg
  fields:
    - {name: s0, type: int32}
    - {name: s1, type: 'double[4]'}
    - {name: s0, type: int64}
)");
	check_load_fail(R"(yamls://
- name: ''
  aliases:
    - {name: a0, type: int32}
    - {name: a1, type: int32}
    - {name: a0, type: int32}
)");
}

template <typename T>
class SchemeOffsetPtr : public ::testing::Test {};

using PtrTypes = ::testing::Types<tll_scheme_offset_ptr_t, tll_scheme_offset_ptr_legacy_long_t, tll_scheme_offset_ptr_legacy_short_t>;
TYPED_TEST_SUITE(SchemeOffsetPtr, PtrTypes);

template <typename T>
void _test_offset_ptr(T * ptr, size_t entity_size)
{
	ASSERT_EQ(ptr->entity_size(), entity_size);

	auto it = ptr->begin();
	ASSERT_EQ(it, ptr->begin());
	ASSERT_EQ(it + 2, ptr->end());
	ASSERT_EQ(&*ptr->begin(), ptr->data());
	ASSERT_EQ(&*ptr->end(), ptr->data() + 2 * entity_size / 4);
}

TYPED_TEST(SchemeOffsetPtr, OffsetPtr)
{
	using Ptr = TypeParam;
	using namespace tll::scheme;
	using ptr_t = offset_ptr_t<int32_t, Ptr>;
	ptr_t ptr = {};
	ptr.size = 2;
	size_t entity_size = 8;
	if constexpr (!std::is_same_v<Ptr, tll_scheme_offset_ptr_legacy_short_t>)
		ptr.entity = 8;
	else
		entity_size = 4;

	_test_offset_ptr(&ptr, entity_size);
	_test_offset_ptr((const ptr_t *) &ptr, entity_size); 
}

TEST(Scheme, Conv)
{
	ASSERT_EQ(tll::conv::to_string(tll::scheme::Field::Int8), "int8");
	ASSERT_EQ(tll::conv::to_string(tll::scheme::Field::Duration), "duration");
}

static constexpr std::string_view pmap_scheme_string = R"(yamls://
- name: msg
  fields:
    - {name: f0, type: int32}
    - {name: pmap, type: uint32, options.pmap: yes}
    - {name: f1, type: int32}
)";

void verify_pmap(tll::scheme::Scheme * s)
{
	ASSERT_NE(s, nullptr);

	auto m = s->messages;
	ASSERT_NE(m, nullptr);
	EXPECT_STREQ(m->name, "msg");
	ASSERT_NE(m->pmap, nullptr);
	ASSERT_STREQ(m->pmap->name, "pmap");
	auto f = m->fields;
	CHECK_FIELD(f, "f0", Field::Int32, 4u, 0u); f = f->next;
	ASSERT_EQ(m->pmap, f);
	CHECK_FIELD(f, "pmap", Field::UInt32, 4u, 4u); f = f->next;
	CHECK_FIELD(f, "f1", Field::Int32, 4u, 8u); f = f->next;
	ASSERT_EQ(f, nullptr);
}

TEST(Scheme, PMap)
{
	SchemePtr s(Scheme::load(pmap_scheme_string));
	verify_pmap(s.get());

	check_load_fail(R"(yamls://
- name: msg
  fields:
    - {name: f0, type: int32, options.pmap: yes}
    - {name: f1, type: int32, options.pmap: yes}
)");

	check_load_fail(R"(yamls://
- name: msg
  fields:
    - {name: f0, type: int32, options.pmap: xxx}
)");
	check_load_fail(R"(yamls://
- name: msg
  fields:
    - {name: f0, type: double, options.pmap: yes}
)");
}

TEST(Scheme, PMapCopy)
{
	SchemePtr ptr(Scheme::load(pmap_scheme_string));
	ASSERT_NE(ptr.get(), nullptr);

	SchemePtr copy(ptr->copy());
	verify_pmap(copy.get());
}

TEST(Scheme, Merge)
{
	std::unique_ptr<Scheme> s0 { Scheme::load(R"(yamls://
- name: M0
  id: 10
  fields:
    - {name: f0, type: int32}
)") };
	std::unique_ptr<Scheme> s1 { Scheme::load(R"(yamls://
- name: Sub
  fields:
    - {name: f0, type: int32}
- name: M1
  id: 11
  fields:
    - {name: f0, type: Sub}
)") };
	std::unique_ptr<Scheme> s2 { Scheme::load(R"(yamls://
- name: M0
  id: 10
  fields:
    - {name: f0, type: int32}
- name: M2
  id: 12
  fields:
    - {name: f0, type: int32}
)") };

	auto r = tll::scheme::merge({nullptr, s0.get(), nullptr, s1.get(), nullptr, s2.get()});
	ASSERT_TRUE(r);

	std::unique_ptr<Scheme> result { *r };

	auto m = result->messages;
	ASSERT_NE(m, nullptr);
	ASSERT_STREQ(m->name, "M0"); ASSERT_EQ(m->msgid, 10);

	m = m->next;
	ASSERT_STREQ(m->name, "Sub"); ASSERT_EQ(m->msgid, 0);

	m = m->next;
	ASSERT_STREQ(m->name, "M1"); ASSERT_EQ(m->msgid, 11);

	m = m->next;
	ASSERT_STREQ(m->name, "M2"); ASSERT_EQ(m->msgid, 12);

	m = m->next;
	ASSERT_EQ(m, nullptr);

	std::unique_ptr<Scheme> serr { Scheme::load(R"(yamls://
- name: M0
  id: 11
  fields:
    - {name: f0, type: int32}
)") };

	ASSERT_NE(serr.get(), nullptr);
	r = tll::scheme::merge({s0.get(), serr.get()});
	ASSERT_FALSE(r);

	serr.reset(Scheme::load(R"(yamls://
- name: Sub
  fields:
    - {name: f0, type: byte4}
- name: M1
  id: 11
  fields:
    - {name: f0, type: Sub}
)"));

	ASSERT_NE(serr.get(), nullptr);
	r = tll::scheme::merge({s1.get(), serr.get()});
	ASSERT_FALSE(r);
}
