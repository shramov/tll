/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/util/bin2ascii.h"
#include "tll/util/browse.h"
#include "tll/util/string.h"
#include "tll/util/url.h"
#include "tll/util/varint.h"
#include "tll/util/zlib.h"

#include <list>
#include <stdio.h>

#include "test_compat.h"

template <bool Skip>
std::list<std::string> splitL(const std::string_view &s)
{
	std::list<std::string> r;
	return tll::splitl<',', Skip>(r, s);
}

TEST(Util, SplitT)
{
	typedef std::list<std::string> list;
	EXPECT_EQ(splitL<false>(""), list{""});
	EXPECT_EQ(splitL<true>(""), list{});
	EXPECT_EQ(splitL<false>(","), (list{"", ""}));
	EXPECT_EQ(splitL<true>(","), list{});
	EXPECT_EQ(splitL<false>("a"), (list{"a"}));
	EXPECT_EQ(splitL<true>("a"), (list{"a"}));
	EXPECT_EQ(splitL<false>("a,"), (list{"a", ""}));
	EXPECT_EQ(splitL<true>("a,"), (list{"a"}));
	EXPECT_EQ(splitL<false>(",b"), (list{"", "b"}));
	EXPECT_EQ(splitL<true>(",b"), (list{"b"}));
	EXPECT_EQ(splitL<false>("a,b"), (list{"a", "b"}));
	EXPECT_EQ(splitL<true>("a,b"), (list{"a", "b"}));
	EXPECT_EQ(splitL<false>("a,,b"), (list{"a", "", "b"}));
	EXPECT_EQ(splitL<true>("a,,b"), (list{"a", "b"}));
}

TEST(Util, SplitIter)
{
	auto s = tll::split<','>("aa,bbb,cccc,");
	auto i0 = s.begin();
	auto i1 = --s.end();
	EXPECT_EQ(*i0, "aa");
	EXPECT_EQ(*i1, std::string_view(""));
	i1--;
	EXPECT_EQ(*i1, std::string_view("cccc"));
	EXPECT_EQ(*++i0, *--i1);
	EXPECT_EQ(*i0, std::string_view("bbb"));
	EXPECT_EQ(*--i0, *s.begin());
}

template <typename T>
class UtilProps : public ::testing::Test {};

using PropsTypes = ::testing::Types<tll::PropsView, tll::Props>;
TYPED_TEST_SUITE(UtilProps, PropsTypes);

TYPED_TEST(UtilProps, Props)
{
	typedef TypeParam P;
	auto p = P::parse("a=1;b=2;c=zzz");

	ASSERT_TRUE(p);

	EXPECT_FALSE(p->template getT<int>("c"));
	EXPECT_FALSE(p->template getT<int>("z"));
	EXPECT_EQ(*p->template getT<int>("a"), 1);
	EXPECT_EQ(*p->template getT<int>("z", 1), 1);

	EXPECT_FALSE(P::parse("a;b=2;c=3"));
	EXPECT_FALSE(P::parse("a=1;b=2;a=3"));
}

template <typename T>
class UtilUrl : public ::testing::Test {};

using UrlTypes = ::testing::Types<tll::UrlView, tll::Url>;
TYPED_TEST_SUITE(UtilUrl, UrlTypes);

TYPED_TEST(UtilUrl, Url)
{
	typedef TypeParam P;
	auto p = P::parse("proto://host;a=1;b=2;c=zzz");

	ASSERT_TRUE(p);

	EXPECT_EQ(p->proto, "proto");
	EXPECT_EQ(p->host, "host");
	EXPECT_FALSE(p->template getT<int>("c"));
	EXPECT_FALSE(p->template getT<int>("z"));
	EXPECT_EQ(*p->template getT<int>("a"), 1);
	EXPECT_EQ(*p->template getT<int>("z", 1), 1);

	EXPECT_FALSE(P::parse("proto://host;a;b=2;c=3"));
	EXPECT_FALSE(P::parse("proto://host;a=1;b=2;a=3"));
	EXPECT_FALSE(P::parse("proto:host;a=1;b=2;a=3"));
	EXPECT_FALSE(P::parse("proto://;a=1;b=2;a=3"));
	EXPECT_FALSE(P::parse("://host;a=1;b=2;c=3"));
}

TEST(Util, PropsReader)
{
	auto p = tll::PropsView::parse("a=1;b=yes;c=zzz");

	ASSERT_TRUE(p);

	auto reader = tll::make_props_reader(*p);

	EXPECT_EQ(reader.getT("a", 0), 1);
	EXPECT_TRUE(reader);
	EXPECT_EQ(reader.getT("b", false), true);
	EXPECT_TRUE(reader);
	EXPECT_EQ(reader.getT("z", 20.), 20.);
	EXPECT_TRUE(reader);
	EXPECT_EQ(reader.getT("c", 10), 10.);
	EXPECT_FALSE(reader);
}

TEST(Util, PropsChain)
{
	auto p0 = tll::PropsView::parse("a=1;b=zzz;p.b=20.;p.c=yes");
	auto p1 = tll::Props::parse("a=100;b=101.;d=zzz");
	ASSERT_TRUE(p0);
	ASSERT_TRUE(p1);

	auto chain = tll::make_props_chain(*p0, std::string_view("p"), *p0, *p1);
	auto reader = tll::make_props_reader(chain);

	EXPECT_TRUE(chain.has("a"));
	EXPECT_TRUE(chain.has("b"));
	EXPECT_TRUE(chain.has("c"));
	EXPECT_TRUE(chain.has("d"));

	EXPECT_EQ(reader.getT("a", 0), 1);
	EXPECT_TRUE(reader);

	EXPECT_EQ(reader.getT("b", 0.), 20.);
	EXPECT_TRUE(reader);

	EXPECT_EQ(reader.getT("c", false), true);
	EXPECT_TRUE(reader);

	EXPECT_EQ(reader.getT("d", 1), 1);
	EXPECT_FALSE(reader);

	ASSERT_TRUE(chain.getT("a", 0));
	EXPECT_EQ(*chain.getT("a", 0), 1);

	ASSERT_TRUE(chain.getT("b", 0.));
	EXPECT_EQ(*chain.getT("b", 0.), 20.);

	ASSERT_TRUE(chain.getT("c", false));
	EXPECT_EQ(*chain.getT("c", false), true);

	EXPECT_FALSE(chain.getT("d", false));
}

TEST(Util, Match)
{
	using tll::match;

	EXPECT_EQ(true, match("", ""));
	EXPECT_EQ(false, match("", "a"));
	EXPECT_EQ(false, match("*", ""));
	EXPECT_EQ(true, match("*", "a"));
	EXPECT_EQ(true, match("*", "abc"));
	EXPECT_EQ(false, match("*", "a.b"));
	EXPECT_EQ(true, match("*.b", "a.b"));
	EXPECT_EQ(true, match("a.*", "a.b"));
	EXPECT_EQ(true, match("*.*", "a.b"));

	EXPECT_EQ(true, match("**", "a"));
	EXPECT_EQ(true, match("**", "a.b"));
	EXPECT_EQ(false, match("**.**", "a.b"));
}

TEST(Util, Hex)
{
	using namespace std::literals;

	std::string_view bin = "\x00\x01\x02\x03\x04\x05\x06\x07"sv;
	std::string_view hex = "0001020304050607";
	auto r = tll::util::bin2hex(bin);
	ASSERT_EQ(r, hex);

	auto h = tll::util::hex2bin(hex);
	ASSERT_TRUE(h);
	ASSERT_EQ(std::string_view(h->data(), h->size()), bin);

	h = tll::util::hex2bin(hex.substr(0, 3));
	ASSERT_FALSE(h);
}

std::string b64d(const std::string_view &s)
{
	auto r = tll::util::b64_decode(s);
	if (r) return std::string(r->data(), r->size());
	return "Invalid base64";
}

TEST(Util, Base64)
{
	using namespace std::literals;
	using namespace tll::util;

	ASSERT_EQ("", b64_encode(""sv));
	ASSERT_EQ("AA==", b64_encode("\0"sv));
	ASSERT_EQ("AAA=", b64_encode("\0\0"sv));
	ASSERT_EQ("AAAA", b64_encode("\0\0\0"sv));
	ASSERT_EQ("/w==", b64_encode("\377"sv));
	ASSERT_EQ("//8=", b64_encode("\377\377"sv));
	ASSERT_EQ("////", b64_encode("\377\377\377"sv));
	ASSERT_EQ("/+8=", b64_encode("\xff\xef"sv));

	ASSERT_EQ(b64d(""), std::string(""sv));
	ASSERT_EQ(b64d("AA=="), std::string("\0"sv));
	ASSERT_EQ(b64d("AAA="), std::string("\0\0"sv));
	ASSERT_EQ(b64d("AAAA"), std::string("\0\0\0"sv));
	ASSERT_EQ(b64d("/w=="), std::string("\377"sv));
	ASSERT_EQ(b64d("//8="), std::string("\377\377"sv));
	ASSERT_EQ(b64d("////"), std::string("\377\377\377"sv));
	ASSERT_EQ(b64d("/+8="), std::string("\xff\xef"sv));

	ASSERT_FALSE(b64_decode("^"));
	ASSERT_FALSE(b64_decode("A"));
	ASSERT_FALSE(b64_decode("A^"));
	ASSERT_FALSE(b64_decode("AA"));
	ASSERT_FALSE(b64_decode("AA="));
	ASSERT_FALSE(b64_decode("AA==="));
	ASSERT_FALSE(b64_decode("AA=x"));
	ASSERT_FALSE(b64_decode("AAA"));
	ASSERT_FALSE(b64_decode("AAA^"));
	//ASSERT_FALSE(b64_decode("AB=="));
	//ASSERT_FALSE(b64_decode("AAB="));
}

TEST(Util, Zlib)
{
	using namespace std::literals;

	std::string_view zdata = "x\x9cKLJNI$\x02\x03\x00;\x87\x0f\x65"sv;
	std::string_view data = "abcdabcdabcdabcdabcdabcdabcdabcdabcdabcd";
	auto r = tll::zlib::decompress(zdata);
	ASSERT_TRUE(r);
	ASSERT_EQ(std::string_view(r->data(), r->size()), data);

	r = tll::zlib::decompress(zdata.substr(0, 10));
	ASSERT_FALSE(r);
	ASSERT_EQ(r.error(), "Truncated compressed data");

	auto z = tll::zlib::compress(data);
	ASSERT_TRUE(z);
	ASSERT_EQ(std::string_view(z->data(), z->size()), zdata);
}

TEST(Util, Varint)
{
	using namespace std::literals;

#define CHECK_VARINT(ev, el, s) do { \
		size_t v; auto l = tll::varint::decode_uint(v, s); \
		EXPECT_EQ(l, el); \
		EXPECT_EQ(v, ev); \
		std::string buf;  \
		l = tll::varint::encode_uint(ev, buf); \
		EXPECT_EQ(l, el); \
		EXPECT_EQ(buf, s); \
	} while (0);

	CHECK_VARINT(0x5u, 1, "\x05"sv);
	CHECK_VARINT(0x285u, 2, "\x85\x05"sv);

	CHECK_VARINT(0x3fffu, 2, "\xff\x7f"sv);
	CHECK_VARINT(0x1fffffu, 3, "\xff\xff\x7f"sv);
	CHECK_VARINT(0xfffffffu, 4, "\xff\xff\xff\x7f"sv);
}
