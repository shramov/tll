/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/scheme/binder.h"

#include <memory>

#include "test_compat.h"
#include "scheme-large-item.h"
#include "scheme-http.h"

#pragma pack(push, 1)
namespace http_scheme {

enum class method_t : int8_t
{
	UNDEFINED = -1,
	GET = 0,
	HEAD = 1,
	POST = 2,
	PUT = 3,
	DELETE = 4,
	CONNECT = 5,
	OPTIONS = 6,
	TRACE = 7,
	PATCH = 8,
};

struct header
{
	tll::scheme::String<tll_scheme_offset_ptr_t> header;
	tll::scheme::String<tll_scheme_offset_ptr_t> value;
};

struct connect
{
	method_t method;
	int16_t code;
	int64_t size;
	tll::scheme::String<tll_scheme_offset_ptr_t> path;
	tll::scheme::offset_ptr_t<header, tll_scheme_offset_ptr_t> headers;
	tll::scheme::Bytes<8> bytes;
	tll::scheme::ByteString<8> bytestring;
};

struct disconnect
{
	int16_t code;
	tll::scheme::String<tll_scheme_offset_ptr_t> error;
};

struct List
{
	tll::scheme::offset_ptr_t<disconnect, tll_scheme_offset_ptr_t> std;
	tll::scheme::offset_ptr_t<disconnect, tll_scheme_offset_ptr_legacy_long_t> llong;
	tll::scheme::offset_ptr_t<disconnect, tll_scheme_offset_ptr_legacy_short_t> lshort;
	tll::scheme::offset_ptr_t<int16_t, tll_scheme_offset_ptr_t> scalar;
};

} // namespace http_scheme
#pragma pack(pop)

TEST(Scheme, Binder)
{
	std::vector<char> buf;
	auto binder = http_binder::connect::bind(buf);
	buf.resize(binder.meta_size());

	binder.set_code(200);
	binder.set_method(http_binder::method_t::GET);

	binder.set_path("/a");
	ASSERT_EQ(binder.get_path(), "/a");
	binder.set_path("/a/b");
	ASSERT_EQ(binder.get_path(), "/a/b");
	binder.set_path("/a/b/c");

	auto headers = binder.get_headers();
	headers.resize(2);
	headers[0].set_header("key-0");
	headers[0].set_value("value-0");
	headers[1].set_header("key-1");
	headers[1].set_value("value-1");

	std::array<unsigned char, 8> bytes = {0, 1, 2, 3, 0, 0, 0, 0};
	binder.set_bytes(std::string_view("\x00\x01\x02\x03", 4));
	binder.set_bytestring("abc");

	ASSERT_EQ(binder.get_code(), 200);
	ASSERT_EQ(binder.get_method(), http_binder::method_t::GET);
	ASSERT_EQ(binder.get_path(), "/a/b/c");
	//ASSERT_EQ(static_cast<std::string_view>(binder.get_path_binder()), "/a/b/c");
	ASSERT_EQ(binder.get_bytes(), bytes);
	ASSERT_EQ(binder.get_bytestring(), "abc");

	ASSERT_EQ(headers.size(), 2u);
	ASSERT_EQ(headers[0].get_header(), "key-0");
	ASSERT_EQ(headers[0].get_value(), "value-0");
	ASSERT_EQ(headers[1].get_header(), "key-1");
	ASSERT_EQ(headers[1].get_value(), "value-1");

	auto connect = (const http_scheme::connect *) buf.data();

	ASSERT_EQ(binder.get_code(), connect->code);
	ASSERT_EQ((int) binder.get_method(), (int) connect->method);
	EXPECT_EQ(connect->path.size, 7u);
	EXPECT_EQ(connect->path.entity, 1u);
	EXPECT_EQ(connect->path.offset, 32u);
	ASSERT_EQ(binder.get_path(), connect->path);

	ASSERT_EQ(binder.get_bytes(), connect->bytes);
	ASSERT_EQ(binder.get_bytestring(), connect->bytestring);

	ASSERT_EQ(headers.size(), connect->headers.size);

	auto mhi = connect->headers.begin();
	ASSERT_EQ(headers[0].get_header(), mhi->header);
	ASSERT_EQ(headers[0].get_value(), mhi->value);
	mhi++;
	ASSERT_EQ(headers[1].get_header(), mhi->header);
	ASSERT_EQ(headers[1].get_value(), mhi->value);

	ASSERT_EQ(mhi - 1, connect->headers.begin());
	ASSERT_EQ(mhi + 1, connect->headers.end());

	ASSERT_EQ(headers[0].get_header(), *connect->headers[0].header);
	ASSERT_EQ(headers[0].get_value(), connect->headers[0].value);
	ASSERT_EQ(headers[1].get_header(), connect->headers[1].header);
	ASSERT_EQ(headers[1].get_value(), connect->headers[1].value);

	auto cbinder = http_binder::connect::bind((const std::vector<char> &) buf);

	ASSERT_EQ(binder.get_code(), cbinder.get_code());
	ASSERT_EQ(binder.get_method(), cbinder.get_method());
	ASSERT_EQ(binder.get_path(), cbinder.get_path());
	ASSERT_EQ(binder.get_bytes(), cbinder.get_bytes());
	ASSERT_EQ(binder.get_bytestring(), cbinder.get_bytestring());

	auto cheaders = cbinder.get_headers();
	ASSERT_EQ(headers.size(), cheaders.size());

	auto hi = headers.begin();
	auto chi = cheaders.begin();
	ASSERT_EQ(hi, headers.begin());
	ASSERT_EQ(chi, cheaders.begin());
	ASSERT_NE(hi, headers.end());
	ASSERT_NE(chi, cheaders.end());

	ASSERT_EQ(hi->get_header(), chi->get_header());
	ASSERT_EQ(hi->get_header(), chi->get_header());
	hi++; ++chi;

	ASSERT_NE(hi, headers.begin());
	ASSERT_NE(chi, cheaders.begin());
	ASSERT_NE(hi, headers.end());
	ASSERT_NE(chi, cheaders.end());

	ASSERT_EQ(hi->get_header(), chi->get_header());
	ASSERT_EQ(hi->get_header(), chi->get_header());

	hi += 1;
	chi = chi + 1;

	ASSERT_EQ(hi, headers.end());
	ASSERT_EQ(chi, cheaders.end());
}

TEST(Scheme, BinderLargeItem)
{
	std::vector<char> buf;
	using namespace std;
	auto binder = large_item_scheme::Data::bind_reset(buf);

	auto list = binder.get_list();
	list.resize(2);
	ASSERT_EQ(buf.size(), binder.meta_size() + 4 + 266 * 2);
	ASSERT_EQ(std::string_view(buf.data(), 12), "\x08\x00\x00\x00\x02\x00\x00\xff\x0a\x01\x00\x00"sv);

	list[0].set_body("0000");
	ASSERT_EQ(std::string_view(buf.data(), 12), "\x08\x00\x00\x00\x02\x00\x00\xff\x0a\x01\x00\x00"sv);
	ASSERT_EQ(std::string_view(buf.data() + 12, 5), "0000\0"sv);
	ASSERT_EQ(list[0].get_body(), "0000"sv);
	ASSERT_EQ(list[1].get_body(), ""sv);

	list[1].set_body("1111");
	ASSERT_EQ(std::string_view(buf.data(), 12), "\x08\x00\x00\x00\x02\x00\x00\xff\x0a\x01\x00\x00"sv);
	ASSERT_EQ(std::string_view(buf.data() + 12, 5), "0000\0"sv);
	ASSERT_EQ(std::string_view(buf.data() + 12 + 266, 5), "1111\0"sv);
	ASSERT_EQ(list[0].get_body(), "0000"sv);
	ASSERT_EQ(list[1].get_body(), "1111"sv);
}

template <typename View>
void verify_list(View buf)
{
	auto binder = http_binder::List::bind(buf);

	ASSERT_EQ(binder.get_scalar().size(), 2);

	int16_t v = 1;

	for (auto i : binder.get_std()) {
		ASSERT_EQ(i.get_code(), v++);
	}

	ASSERT_EQ(binder.get_std()[0].get_code(), 1);
	ASSERT_EQ(binder.get_std()[1].get_code(), 2);

	v = 100;

	for (auto i : binder.get_scalar()) {
		ASSERT_EQ(i, v++);
	}

	ASSERT_EQ(binder.get_scalar()[0], 100);
	ASSERT_EQ(binder.get_scalar()[1], 101);
}

TEST(Scheme, BinderList)
{
	std::vector<char> buf;
	auto binder = http_binder::List::bind_reset(buf);

	binder.get_std().resize(2);
	binder.get_llong().resize(2);
	binder.get_lshort().resize(2);
	binder.get_scalar().resize(2);

	binder.get_std()[0].set_code(1);
	binder.get_std()[1].set_code(2);
	binder.get_llong()[0].set_code(3);
	binder.get_llong()[1].set_code(4);
	binder.get_lshort()[0].set_code(5);
	binder.get_lshort()[1].set_code(6);
	binder.get_scalar()[0] = 100;
	binder.get_scalar()[1] = 101;

	auto ptr = (const http_scheme::List *) buf.data();
	ASSERT_EQ(ptr->std.size, 2);
	ASSERT_EQ(ptr->llong.size, 2);
	ASSERT_EQ(ptr->lshort.size, 2);
	ASSERT_EQ(ptr->std.entity, sizeof(http_scheme::disconnect));
	ASSERT_EQ(ptr->llong.entity, sizeof(http_scheme::disconnect));

	ASSERT_EQ(ptr->std[0].code, 1);
	ASSERT_EQ(ptr->std[1].code, 2);
	ASSERT_EQ(ptr->llong[0].code, 3);
	ASSERT_EQ(ptr->llong[1].code, 4);
	ASSERT_EQ(ptr->lshort[0].code, 5);
	ASSERT_EQ(ptr->lshort[1].code, 6);

	verify_list<std::vector<char> &>(buf);
	verify_list<const std::vector<char> &>(buf);
	verify_list<tll::memory>({buf.data(), buf.size()});
	verify_list<tll::const_memory>({buf.data(), buf.size()});
}

TEST(Scheme, BinderCopy)
{
	std::vector<char> buf;
	auto rhs = http_binder::Copy::bind_reset(buf);
	rhs.get_header().set_header("header");
	rhs.get_header().set_value("value");
	rhs.set_i64(0x01020304050607);
	rhs.set_f64(123.456);
	rhs.set_s64("s64");
	rhs.set_str("string");

	std::vector<char> bcopy;
	auto copy = http_binder::Copy::bind_reset(bcopy);
	copy.copy(rhs);

	buf.assign(buf.size(), 0);

	ASSERT_EQ(copy.get_header().get_header(), "header");
	ASSERT_EQ(copy.get_header().get_value(), "value");
	ASSERT_EQ(copy.get_i64(), 0x01020304050607);
	ASSERT_EQ(copy.get_f64(), 123.456);
	ASSERT_EQ(copy.get_s64(), "s64");
	ASSERT_EQ(copy.get_str(), "string");
}
