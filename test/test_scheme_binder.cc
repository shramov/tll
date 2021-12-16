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
};

struct disconnect
{
	int16_t code;
	tll::scheme::String<tll_scheme_offset_ptr_t> error;
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

	static constexpr size_t meta_size() { return 27; }

	method_t get_method() const { return this->template _get_scalar<method_t>(0); };
	void set_method(method_t v) { return this->template _set_scalar<method_t>(0, v); };

	int16_t get_code() const { return this->template _get_scalar<int16_t>(1); };
	void set_code(int16_t v) { return this->template _set_scalar<int16_t>(1, v); };

	int64_t get_size() const { return this->template _get_scalar<int64_t>(3); };
	void set_size(int64_t v) { return this->template _set_scalar<int64_t>(3, v); };

	std::string_view get_path() const { return this->template _get_string<tll_scheme_offset_ptr_t>(11); }
	void set_path(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(11, v); }

	using type_headers = tll::scheme::binder::List<Buf, header<Buf>, tll_scheme_offset_ptr_t>;
	const type_headers get_headers() const { return this->template _get_binder<type_headers>(19); }
	type_headers get_headers() { return this->template _get_binder<type_headers>(19); }
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

} // namespace http_binder

TEST(Scheme, Binder)
{
	std::vector<char> buf;
	http_binder::connect<std::vector<char>> binder(buf);
	buf.resize(binder.meta_size());

	binder.set_code(200);
	binder.set_method(http_binder::method_t::GET);
	binder.set_path("/a");
	binder.set_path("/a/b/c");
	auto headers = binder.get_headers();
	headers.resize(2);
	headers[0].set_header("key-0");
	headers[0].set_value("value-0");
	headers[1].set_header("key-1");
	headers[1].set_value("value-1");

	ASSERT_EQ(binder.get_code(), 200);
	ASSERT_EQ(binder.get_method(), http_binder::method_t::GET);
	ASSERT_EQ(binder.get_path(), "/a/b/c");

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
	EXPECT_EQ(connect->path.offset, 16u);
	ASSERT_EQ(binder.get_path(), connect->path);

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
