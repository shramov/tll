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
};

} // namespace http_scheme
#pragma pack(pop)

namespace http_binder {

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

template <typename Buf>
struct header : public tll::scheme::Binder<Buf>
{
	using tll::scheme::Binder<Buf>::Binder;

	static constexpr size_t meta_size() { return 16; }

	std::string_view get_header() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
	void set_header(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

	std::string_view get_value() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
	void set_value(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }
};

template <typename Buf>
struct connect : public tll::scheme::Binder<Buf>
{
	using tll::scheme::Binder<Buf>::Binder;

	static constexpr size_t meta_size() { return 43; }

	method_t get_method() const { return this->template _get_scalar<method_t>(0); };
	void set_method(method_t v) { return this->template _set_scalar<method_t>(0, v); };

	int16_t get_code() const { return this->template _get_scalar<int16_t>(1); };
	void set_code(int16_t v) { return this->template _set_scalar<int16_t>(1, v); };

	int64_t get_size() const { return this->template _get_scalar<int64_t>(3); };
	void set_size(int64_t v) { return this->template _set_scalar<int64_t>(3, v); };

	std::string_view get_path() const { return this->template _get_string<tll_scheme_offset_ptr_t>(11); }
	void set_path(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(11, v); }
	auto get_path_binder() { return this->template _get_binder<tll::scheme::binder::String<Buf, tll_scheme_offset_ptr_t>>(11); }
	auto get_path_binder() const { return this->template _get_binder<tll::scheme::binder::String<const Buf, tll_scheme_offset_ptr_t>>(11); }

	using type_headers = tll::scheme::binder::List<Buf, header<Buf>, tll_scheme_offset_ptr_t>;
	const type_headers get_headers() const { return this->template _get_binder<type_headers>(19); }
	type_headers get_headers() { return this->template _get_binder<type_headers>(19); }

	const std::array<unsigned char, 8> & get_bytes() const { return this->template _get_bytes<8>(27); }
	void set_bytes(const std::array<unsigned char, 8> &v) { return this->template _set_bytes<8>(27, v); }

	std::string_view get_bytestring() const { return this->template _get_bytestring<8>(35); }
	void set_bytestring(std::string_view v) { return this->template _set_bytestring<8>(35, v); }
};

template <typename Buf>
struct disconnect : public tll::scheme::Binder<Buf>
{
	using tll::scheme::Binder<Buf>::Binder;

	static constexpr size_t meta_size() { return 10; }

	int16_t get_code() const { return this->template _get_scalar<int16_t>(0); };
	void set_code(int16_t v) { return this->template _set_scalar<int16_t>(0, v); };

	std::string_view get_error() const { return this->template _get_string<tll_scheme_offset_ptr_t>(2); }
	void set_error(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(2, v); }
};

template <typename Buf>
struct List : public tll::scheme::Binder<Buf>
{
	using tll::scheme::Binder<Buf>::Binder;

	static constexpr size_t meta_size() { return 20; }

	using type_std = tll::scheme::binder::List<Buf, disconnect<Buf>, tll_scheme_offset_ptr_t>;
	const type_std get_std() const { return this->template _get_binder<type_std>(0); }
	type_std get_std() { return this->template _get_binder<type_std>(0); }

	using type_llong = tll::scheme::binder::List<Buf, disconnect<Buf>, tll_scheme_offset_ptr_legacy_long_t>;
	const type_llong get_llong() const { return this->template _get_binder<type_llong>(8); }
	type_llong get_llong() { return this->template _get_binder<type_llong>(8); }

	using type_lshort = tll::scheme::binder::List<Buf, disconnect<Buf>, tll_scheme_offset_ptr_legacy_short_t>;
	const type_lshort get_lshort() const { return this->template _get_binder<type_lshort>(16); }
	type_lshort get_lshort() { return this->template _get_binder<type_lshort>(16); }
};

} // namespace http_binder

TEST(Scheme, Binder)
{
	std::vector<char> buf;
	http_binder::connect<std::vector<char>> binder(buf);
	buf.resize(binder.meta_size());

	binder.set_code(200);
	binder.set_method(http_binder::method_t::GET);

	binder.set_path("/a");
	ASSERT_EQ(binder.get_path(), "/a");
	binder.get_path_binder() = "/a/b";
	ASSERT_EQ(binder.get_path(), "/a/b");
	binder.set_path("/a/b/c");

	auto headers = binder.get_headers();
	headers.resize(2);
	headers[0].set_header("key-0");
	headers[0].set_value("value-0");
	headers[1].set_header("key-1");
	headers[1].set_value("value-1");

	std::array<unsigned char, 8> bytes = {0, 1, 2, 3, 0, 0, 0, 0};
	binder.set_bytes({0, 1, 2, 3});
	binder.set_bytestring("abc");

	ASSERT_EQ(binder.get_code(), 200);
	ASSERT_EQ(binder.get_method(), http_binder::method_t::GET);
	ASSERT_EQ(binder.get_path(), "/a/b/c");
	ASSERT_EQ(static_cast<std::string_view>(binder.get_path_binder()), "/a/b/c");
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

	//http_binder::connect<const std::vector<char>> cbinder = tll::scheme::make_binder<http_binder::connect>(buf); //cbinder(buf);
	http_binder::connect<const std::vector<char>> cbinder = tll::scheme::make_binder<http_binder::connect>((const std::vector<char> &) buf); //cbinder(buf);

	ASSERT_EQ(binder.get_code(), cbinder.get_code());
	ASSERT_EQ(binder.get_method(), cbinder.get_method());
	ASSERT_EQ(binder.get_path(), cbinder.get_path());
	ASSERT_EQ(binder.get_path(), cbinder.get_path_binder());
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

TEST(Scheme, BinderList)
{
	std::vector<char> buf;
	http_binder::List<std::vector<char>> binder(buf);
	buf.resize(binder.meta_size());

	binder.get_std().resize(2);
	binder.get_llong().resize(2);
	binder.get_lshort().resize(2);

	binder.get_std()[0].set_code(1);
	binder.get_std()[1].set_code(2);
	binder.get_llong()[0].set_code(3);
	binder.get_llong()[1].set_code(4);
	binder.get_lshort()[0].set_code(5);
	binder.get_lshort()[1].set_code(6);

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
}
