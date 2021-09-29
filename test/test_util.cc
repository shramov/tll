/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/compat/filesystem.h"

#include "tll/util/bin2ascii.h"
#include "tll/util/browse.h"
#include "tll/util/cppring.h"
#include "tll/util/fixed_point.h"
#include "tll/util/string.h"
#include "tll/util/time.h"
#include "tll/util/url.h"
#include "tll/util/varint.h"
#include "tll/util/zlib.h"

#include <fmt/format.h>
#include <list>
#include <stdio.h>
#include <thread>

#include "test_compat.h"

TEST(Util, Strip)
{
	EXPECT_EQ(tll::util::strip("abc"), "abc");
	EXPECT_EQ(tll::util::strip(" abc"), "abc");
	EXPECT_EQ(tll::util::strip("abc "), "abc");
	EXPECT_EQ(tll::util::strip(" abc "), "abc");
	EXPECT_EQ(tll::util::strip("   a b c  "), "a b c");

	EXPECT_EQ(tll::util::strip(" .abc. ", " ,."), "abc");
	EXPECT_EQ(tll::util::strip(",,abc", " ,."), "abc");
	EXPECT_EQ(tll::util::strip(" abc", ",."), " abc");
}

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
	EXPECT_EQ(p->template getT<int>("z").value_or(1), 1);

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

TEST(Util, Time)
{
	using namespace std::chrono_literals;

	auto tnow = tll::time::now_cached();
	std::this_thread::sleep_for(1us);
	ASSERT_NE(tnow, tll::time::now_cached());
	auto snow = std::chrono::system_clock::now();
	ASSERT_LE(tnow, snow);

	tll::time::cache_enable(true);
	tll::time::cache_enable(true);
	tnow = tll::time::now();
	ASSERT_EQ(tnow, tll::time::now_cached());

	auto tnow1 = tll::time::now();
	ASSERT_LE(tnow, tnow1);
	ASSERT_EQ(tnow1, tll::time::now_cached());

	tll::time::cache_enable(false);
	ASSERT_EQ(tnow1, tll::time::now_cached());

	tll::time::cache_enable(false);
	ASSERT_LT(tnow1, tll::time::now_cached());
}

TEST(Util, Filesystem)
{
	using namespace std::filesystem;
	// Using path in ASSERT_EQ is not working on 18.04
//#define ASSERT_PATH(p, r) ASSERT_EQ(path(p).lexically_normal().string(), path(r).string())

#if defined(__GNUC__) && __GNUC__ < 8 && !defined(__llvm__)
	std::string slash_suffix = "";
#else
	std::string slash_suffix = "/";
#endif
#define ASSERT_PATH(p, r) EXPECT_EQ(tll::filesystem::compat_lexically_normal(p).string(), path(r).string())
	ASSERT_PATH("", "");
	ASSERT_PATH(".", ".");
	ASSERT_PATH("./", ".");
	ASSERT_PATH("./.", ".");
	ASSERT_PATH("./././", ".");
	ASSERT_PATH("./././.", ".");
	ASSERT_PATH(".//.", ".");
	ASSERT_PATH("a/", "a" + slash_suffix);
	ASSERT_PATH("a/.", "a" + slash_suffix);
	ASSERT_PATH("/a/", "/a" + slash_suffix);
	ASSERT_PATH("/a/.", "/a" + slash_suffix);
	ASSERT_PATH("./..", "..");
	ASSERT_PATH("./a/../../b", "../b");
	ASSERT_PATH("..", "..");
	ASSERT_PATH("../", "..");
	ASSERT_PATH("../.", "..");
	ASSERT_PATH("../../", "../..");
	ASSERT_PATH("../a", "../a");
	ASSERT_PATH("../a/../b", "../b");
	ASSERT_PATH("/", "/");
	//ASSERT_PATH("//", "//"); // Compares to "/" as path, but not as string. For clang string() is "/", not "//"
	ASSERT_PATH("/.", "/");
	ASSERT_PATH("/./", "/");
	ASSERT_PATH("/..", "/");
	ASSERT_PATH("/../", "/");
	ASSERT_PATH("/../a", "/a");
#undef ASSERT_PATH

#define ASSERT_PATH(p, b, r) EXPECT_EQ(tll::filesystem::compat_relative_simple(p, b).string(), path(r).string())
//#define ASSERT_PATH(p, b, r) EXPECT_EQ(std::filesystem::relative(p, b).string(), path(r).string())
	ASSERT_PATH("/a", "/", "a");
	ASSERT_PATH("/a/b/c", "/d/e", "../../a/b/c");
	ASSERT_PATH("/a/b/c", "/a/b/d/e", "../../c");
	ASSERT_PATH("/a/b/c", "/a/b/c/d", "..");
	ASSERT_PATH("/a/b/c", "/a/b/c/d/", "..");
	ASSERT_PATH("/a/b/c/", "/a/b/c/d", ".." + slash_suffix);
	ASSERT_PATH("/a/b/c/", "/a/b/c/d/.", ".." + slash_suffix);
#undef ASSERT_PATH
}

TEST(Util, Ring)
{
	tll::util::Ring<unsigned> ring;

	ring.resize(8);

	ASSERT_EQ(ring.begin(), ring.end());

	for(auto i = 0u; i < 7; i++) {
		ASSERT_EQ(ring.size(), i);
		ASSERT_NE(ring.push_back(i), nullptr);
	}

	auto sum = 0;

	for (auto & i : ring) sum += i;
	ASSERT_EQ(sum, 0 + 1 + 2 + 3 + 4 + 5 + 6);

	ASSERT_EQ(ring.push_back(8), nullptr);
	ASSERT_EQ(ring.front(), 0u);
	ring.pop_front();

	ASSERT_EQ(ring.size(), 6u);
	ASSERT_EQ(ring.front(), 1u);

	sum = 0;
	for (auto & i : ring) sum += i;
	ASSERT_EQ(sum, 1 + 2 + 3 + 4 + 5 + 6);

	ASSERT_NE(ring.push_back(7), nullptr);
	ASSERT_EQ(ring.size(), 7u);

	sum = 0;
	for (auto & i : ring) sum += i;
	ASSERT_EQ(sum, 1 + 2 + 3 + 4 + 5 + 6 + 7);
}

TEST(Util, DataRing)
{
	tll::util::DataRing<unsigned> ring(8, 64);

	std::string data(64 - 4, 'a');
	ASSERT_NE(ring.push_back(1, data.data(), 28), nullptr);
	data = std::string(64 - 4, 'b');
	ASSERT_NE(ring.push_back(2, data.data(), 28), nullptr);

	ASSERT_EQ(*ring.front().frame, 1u);
	ASSERT_EQ(ring.front().size, 28u);

	ASSERT_EQ(*ring.back().frame, 2u);
	ASSERT_EQ(ring.back().size, 28u);

	ASSERT_EQ(ring.push_back(3, "", 0), nullptr);

	ring.pop_front();

	for (auto i = 0u; i < 4; i++) {
		std::string data(4, 'a' + 2 + i);
		fmt::print("Push {}: {}\n", i, data);
		ASSERT_NE(ring.push_back(3 + i, data.data(), data.size()), nullptr);
	}

	auto sum = 0;
	for (auto & i : ring) sum += *i.frame;
	ASSERT_EQ(sum, 2 + 3 + 4 + 5 + 6);

	ring.pop_front();

	for (auto i = 0u; i < 2; i++) {
		std::string data(12, 'a' + 6 + i);
		fmt::print("Push {}: {}\n", i, data);
		ASSERT_NE(ring.push_back(7 + i, data.data(), data.size()), nullptr);
	}

	sum = 0;
	for (auto & i : ring) sum += *i.frame;
	ASSERT_EQ(sum, 3 + 4 + 5 + 6 + 7 + 8);

	ring.pop_front();
	ring.pop_front();

	ASSERT_NE(ring.push_back(9, nullptr, 0), nullptr);
	ASSERT_NE(ring.push_back(10, nullptr, 0), nullptr);
	ASSERT_NE(ring.push_back(11, nullptr, 0), nullptr);
	ASSERT_EQ(ring.push_back(12, nullptr, 0), nullptr);
}

TEST(Util, DataRingVoid)
{
	tll::util::DataRing<void> ring(8, 64);
	auto sum = [](auto ring) { auto sum = 0; for (auto &i: ring) sum += i.size; return sum; };

	std::string data(32, 'a');
	ASSERT_NE(ring.push_back(data.data(), data.size()), nullptr);

	auto it = ring.begin();

	ASSERT_EQ(ring.size(), 1u);
	ASSERT_EQ(sum(ring), 32);

	{
		ASSERT_EQ(it->data(), it->frame);
		ASSERT_EQ(it->size, 32u);
		ASSERT_EQ(std::string_view((const char *) it->data(), it->size), data);
	}

	ASSERT_EQ(++it, ring.end());

	data = std::string(32, 'b');
	ASSERT_NE(ring.push_back(data.data(), data.size()), nullptr);

	ASSERT_EQ(ring.size(), 2u);
	ASSERT_EQ(sum(ring), 64);

	ASSERT_NE(it, ring.end());

	{
		ASSERT_EQ(it->data(), it->frame);
		ASSERT_EQ(it->size, 32u);
		ASSERT_EQ(std::string_view((const char *) it->data(), it->size), data);
	}

	ASSERT_NE(ring.push_back("", 0), nullptr);
	ASSERT_NE(ring.push_back("", 0), nullptr);

	ASSERT_EQ(ring.size(), 4u);
	ASSERT_EQ(sum(ring), 64);

	ASSERT_EQ(ring.push_back("z", 1), nullptr);
}

TEST(Util, FixedPoint)
{
	using tll::util::FixedPoint;
	using F3 = FixedPoint<int32_t, 3>;
	{ using F = FixedPoint<int64_t, 3>; ASSERT_EQ(F::divisor, 1000u); }
	{ using F = FixedPoint<int32_t, 3>; ASSERT_EQ(F::divisor, 1000u); }
	{ using F = FixedPoint<int32_t, 1>; ASSERT_EQ(F::divisor, 10u); }
	{ using F = FixedPoint<int32_t, 0>; ASSERT_EQ(F::divisor, 1u); }

	F3 f;
	ASSERT_EQ(f.value(), 0);

	f = F3(1234);

	ASSERT_EQ(f.value(), 1234);
	ASSERT_EQ((double) f, 1.234);

	f = F3(1.234);

	ASSERT_EQ(f.value(), 1234);
	ASSERT_EQ((double) f, 1.234);

	ASSERT_EQ(f, F3(1234));

	f *= 2;

	ASSERT_EQ(f.value(), 2468);

	f -= F3(100);

	ASSERT_EQ(f.value(), 2368);

	f += F3(100);

	ASSERT_EQ(f.value(), 2468);
	ASSERT_TRUE(F3(1234) <= F3(1234)); ASSERT_FALSE(F3(1234) <= F3(1233));
	ASSERT_TRUE(F3(1234) >= F3(1234)); ASSERT_FALSE(F3(1234) >= F3(1235));
	ASSERT_TRUE(F3(1234) < F3(1235)); ASSERT_FALSE(F3(1234) < F3(1233));
	ASSERT_TRUE(F3(1234) > F3(1233)); ASSERT_FALSE(F3(1234) > F3(1235));
}
