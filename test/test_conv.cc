/*
 * Copyright (c) 2019-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/util/size.h"

#define EXPECT_EQ_ANY(l, r) do { \
	EXPECT_TRUE(l) << "Failed to convert: " << l.error(); \
	if (l) { EXPECT_EQ(*l, r); } \
	} while (0);

using tll::conv::to_any;
using tll::conv::to_string;

TEST(Conv, Util)
{
	using tll::conv::append;
	using sv = std::string_view;
	std::vector<unsigned char> v;
	auto vbase = v.data();
	EXPECT_EQ(append(v, "x", ""), "x"); EXPECT_EQ(v.data(), vbase);
	EXPECT_EQ(append(v, "", "x"), "x"); EXPECT_EQ(v.data(), vbase);
	EXPECT_EQ(append(v, "x", "y"), "xy"); EXPECT_NE(v.data(), vbase);
	std::string s = "abcdef";
	auto sbase = s.data();
	EXPECT_EQ(append(s, sv(s).substr(2,2), "z"), "cdz"); EXPECT_EQ(s.data(), sbase);
	EXPECT_EQ(s, "abcdzf");
	EXPECT_EQ(append(s, sv(s).substr(2,2), "zzzzzzzz"), "cdzzzzzzzz");
	EXPECT_EQ(s, "abcdzzzzzzzz");
}

TEST(Conv, Int)
{
	EXPECT_FALSE(to_any<int>(""));
	EXPECT_FALSE(to_any<int>("+"));
	EXPECT_FALSE(to_any<int>("-"));
	EXPECT_FALSE(to_any<int>("0x"));
	EXPECT_FALSE(to_any<int>("+0x"));
	EXPECT_FALSE(to_any<int>("-0x"));
	EXPECT_FALSE(to_any<int>("x"));
	EXPECT_FALSE(to_any<int>("10x"));
	EXPECT_FALSE(to_any<int>("0xz"));
	EXPECT_FALSE(to_any<int>("10.1"));
	EXPECT_FALSE(to_any<unsigned>("-10"));

	EXPECT_FALSE(to_any<char>("0x80"));
	EXPECT_FALSE(to_any<char>("-0x81"));
	EXPECT_FALSE(to_any<short>("0x8000"));
	EXPECT_FALSE(to_any<short>("-0x8001"));
	EXPECT_FALSE(to_any<int>("0x80000000"));
	EXPECT_FALSE(to_any<int>("-0x80000001"));
	EXPECT_FALSE(to_any<int64_t>("0x8000000000000000"));
	EXPECT_FALSE(to_any<int64_t>("-0x8000000000000001"));

	EXPECT_EQ_ANY(to_any<unsigned>("10"), 10u);
	EXPECT_EQ_ANY(to_any<int>("10"), 10);
	EXPECT_EQ_ANY(to_any<int>("-10"), -10);

	EXPECT_EQ_ANY(to_any<unsigned long long>("0x0123456789abcdef"), 0x0123456789abcdefull);
	EXPECT_EQ_ANY(to_any<unsigned long long>("0x0123456789ABCDEF"), 0x0123456789abcdefull);
	EXPECT_EQ_ANY(to_any<int>("0x12345678"), 0x12345678);
	EXPECT_EQ_ANY(to_any<int>("-0x123"), -0x123);

	EXPECT_EQ(to_string<int>(0), "0");
	EXPECT_EQ(to_string<char>(123), "123");
	EXPECT_EQ(to_string<unsigned char>(200), "200");
	EXPECT_EQ(to_string<short>(12345), "12345");
	EXPECT_EQ(to_string<unsigned short>(54321), "54321");
	EXPECT_EQ(to_string<int>(1234567890), "1234567890");
	EXPECT_EQ(to_string<unsigned int>(1234567890), "1234567890");
	EXPECT_EQ(to_string<int64_t>(1234567890), "1234567890");
	EXPECT_EQ(to_string<uint64_t>(1234567890), "1234567890");

	EXPECT_EQ(to_string<char>(-128), "-128");
	EXPECT_EQ(to_string<short>(-0x8000), "-32768");
	EXPECT_EQ(to_string<int>(-1234567890), "-1234567890");
}

TEST(Conv, Float)
{
	EXPECT_FALSE(to_any<double>(""));
	EXPECT_FALSE(to_any<double>("x"));
	EXPECT_FALSE(to_any<double>("10x"));

	EXPECT_EQ_ANY(to_any<double>("10"), 10.);
	EXPECT_EQ_ANY(to_any<double>("10.1"), 10.1);

	//printf("Digits %s: %d/%d\n", "float", std::numeric_limits<float>::max_digits10, std::numeric_limits<float>::digits10);
	//printf("Digits %s: %d/%d\n", "double", std::numeric_limits<double>::max_digits10, std::numeric_limits<double>::digits10);
	//printf("Digits %s: %d/%d\n", "long double", std::numeric_limits<long double>::max_digits10, std::numeric_limits<long double>::digits10);

	EXPECT_EQ(to_string(std::numeric_limits<float>::infinity()), "inf");
	EXPECT_EQ(to_string(-std::numeric_limits<float>::infinity()), "-inf");
	EXPECT_EQ(to_string(std::numeric_limits<float>::quiet_NaN()), "nan");
	EXPECT_EQ(to_string(std::numeric_limits<float>::signaling_NaN()), "nan");

	EXPECT_EQ(to_string(std::numeric_limits<double>::infinity()), "inf");
	EXPECT_EQ(to_string(-std::numeric_limits<double>::infinity()), "-inf");
	EXPECT_EQ(to_string(std::numeric_limits<double>::quiet_NaN()), "nan");
	EXPECT_EQ(to_string(std::numeric_limits<double>::signaling_NaN()), "nan");

	EXPECT_EQ(to_string(std::numeric_limits<long double>::infinity()), "inf");
	EXPECT_EQ(to_string(-std::numeric_limits<long double>::infinity()), "-inf");
	EXPECT_EQ(to_string(std::numeric_limits<long double>::quiet_NaN()), "nan");
	EXPECT_EQ(to_string(std::numeric_limits<long double>::signaling_NaN()), "nan");

	EXPECT_EQ(to_string<float>(10), "10");
	EXPECT_EQ(to_string<float>(10.1f), "10.1");
	EXPECT_EQ(to_string<double>(10.1), "10.1");
	EXPECT_EQ(to_string<long double>(10.1l), "10.1");

	//EXPECT_EQ(to_string<double>(1234567890.1234567890), "1234567890.1234567"); // XXX: Failing
}

TEST(Conv, Size)
{
	using tll::conv::to_any;
	using namespace tll::util;
	EXPECT_FALSE(to_any<Size>(""));
	EXPECT_FALSE(to_any<Size>("10"));
	EXPECT_FALSE(to_any<Size>("10x"));
	EXPECT_FALSE(to_any<Size>("10MB"));
	EXPECT_FALSE(to_any<Size>("10.1b"));
	EXPECT_FALSE(to_any<Size>("-10b"));

	EXPECT_EQ_ANY(to_any<Size>("10b"), 10u);
	EXPECT_EQ_ANY(to_any<Size>("10kb"), 10u * 1024);
	EXPECT_EQ_ANY(to_any<Size>("10mb"), 10u * 1024 * 1024);
	EXPECT_EQ_ANY(to_any<Size>("1gb"), 1ull * 1024 * 1024 * 1024);
	EXPECT_EQ_ANY(to_any<Size>("10kbit"), 10u * 1024 / 8);
	EXPECT_EQ_ANY(to_any<Size>("10mbit"), 10u * 1024 * 1024 / 8);
	EXPECT_EQ_ANY(to_any<Size>("10gbit"), 10ull * 1024 * 1024 * 1024 / 8);

	EXPECT_EQ_ANY(to_any<SizeT<int>>("-1kb"), -1024);
	EXPECT_EQ_ANY(to_any<SizeT<double>>("0.001kb"), 1.024);
}
