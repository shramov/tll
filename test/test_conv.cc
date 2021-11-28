/*
 * Copyright (c) 2019-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/util/size.h"
#include "tll/util/time.h"
#include "tll/conv/fixed_point.h"

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

	EXPECT_FALSE(to_any<signed char>("0x80"));
	EXPECT_FALSE(to_any<signed char>("-0x81"));
	EXPECT_FALSE(to_any<short>("0x8000"));
	EXPECT_FALSE(to_any<short>("-0x8001"));
	EXPECT_FALSE(to_any<int>("0x80000000"));
	EXPECT_FALSE(to_any<int>("-0x80000001"));
	EXPECT_FALSE(to_any<int>("100000000000"));
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

	EXPECT_EQ(to_string<signed char>(-128), "-128");
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

TEST(Conv, Duration)
{
	using tll::conv::to_any;
	using namespace std::chrono_literals;

	EXPECT_FALSE(to_any<tll::duration>(""));
	EXPECT_FALSE(to_any<tll::duration>("10"));
	EXPECT_FALSE(to_any<tll::duration>("10x"));
	EXPECT_FALSE(to_any<tll::duration>("10MB"));
	EXPECT_FALSE(to_any<tll::duration>("1.5ns"));

	EXPECT_EQ_ANY(to_any<tll::duration>("10ns"), 10ns);
	EXPECT_EQ_ANY(to_any<tll::duration>("10us"), 10us);
	EXPECT_EQ_ANY(to_any<tll::duration>("10ms"), 10ms);
	EXPECT_EQ_ANY(to_any<tll::duration>("10s"), 10s);
	EXPECT_EQ_ANY(to_any<tll::duration>("10m"), 10min);
	EXPECT_EQ_ANY(to_any<tll::duration>("10h"), 10h);
	EXPECT_EQ_ANY(to_any<tll::duration>("10d"), 10 * 24h);

	using fms = std::chrono::duration<double, std::milli>;
	EXPECT_EQ_ANY(to_any<fms>("1.5ms"), 1.5ms);
	EXPECT_EQ_ANY(to_any<fms>("15e-1ms"), 1.5ms);
	EXPECT_EQ_ANY(to_any<fms>("1us"), 1us);

	using ms = std::chrono::milliseconds;
	EXPECT_EQ_ANY(to_any<ms>("1000000ns"), 1ms);
	EXPECT_EQ_ANY(to_any<ms>("2000us"), 2ms);
	EXPECT_EQ_ANY(to_any<ms>("1s"), 1s);

	EXPECT_FALSE(to_any<ms>("100ns"));
	EXPECT_FALSE(to_any<ms>("1500us"));

	using us = std::chrono::microseconds;
	EXPECT_EQ_ANY(tll::duration_cast_exact<fms>(ms(10)), fms(10));
	EXPECT_EQ_ANY(tll::duration_cast_exact<fms>(us(10)), fms(0.01));

	EXPECT_EQ_ANY(tll::duration_cast_exact<us>(fms(10)), 10ms);
	EXPECT_EQ_ANY(tll::duration_cast_exact<us>(fms(0.01)), 10us);

	EXPECT_FALSE(tll::duration_cast_exact<us>(fms(0.0001)));
}

TEST(Conv, TimePoint)
{
	using tll::conv::to_any;
	using namespace std::chrono;

	tll::time_point tp(1609556645s);
	using days = duration<int, std::ratio<86400>>;

	EXPECT_EQ(to_string(tp), "2021-01-02T03:04:05");
	EXPECT_EQ(to_string(tp + 123ms), "2021-01-02T03:04:05.123");
	EXPECT_EQ(to_string(tp + 123us), "2021-01-02T03:04:05.000123");
	EXPECT_EQ(to_string(tp + 123ns), "2021-01-02T03:04:05.000000123");

	EXPECT_EQ(to_string(time_point_cast<days>(tp)), "2021-01-02");

	EXPECT_FALSE(to_any<tll::time_point>("2021"));
	EXPECT_FALSE(to_any<tll::time_point>("2021-01-02X"));
	EXPECT_FALSE(to_any<tll::time_point>("2021-01-02X03:04:05"));
	EXPECT_FALSE(to_any<tll::time_point>("2021-01-02 03:04:05X"));
	EXPECT_FALSE(to_any<tll::time_point>("2021-01-02T03"));
	EXPECT_FALSE(to_any<tll::time_point>("2021-01-02T03:04:05a"));
	EXPECT_FALSE(to_any<tll::time_point>("2021-01-02T03:04:05.a"));
	EXPECT_FALSE(to_any<tll::time_point>("2021-01-02T03:04:05.1234567891"));

	using hour_point = time_point<system_clock, duration<int, std::ratio<3600>>>;
	using minute_point = time_point<system_clock, duration<int, std::ratio<60>>>;
	using seconds_point = time_point<system_clock, seconds>;
	using ms_point = time_point<system_clock, milliseconds>;
	EXPECT_FALSE(to_any<hour_point>("2021-01-02T03:04:00"));
	EXPECT_FALSE(to_any<minute_point>("2021-01-02T03:04:05"));
	EXPECT_FALSE(to_any<seconds_point>("2021-01-02T03:04:05.123"));
	EXPECT_FALSE(to_any<ms_point>("2021-01-02T03:04:05.123123"));

	EXPECT_EQ_ANY(to_any<tll::time_point>("2021-01-02"), time_point_cast<days>(tp));
	EXPECT_EQ_ANY(to_any<tll::time_point>("2021-01-02T03:04:05"), tp);
	EXPECT_EQ_ANY(to_any<tll::time_point>("2021-01-02 03:04:05"), tp);
	EXPECT_EQ_ANY(to_any<tll::time_point>("2021-01-02T03:04:05Z"), tp);
	EXPECT_EQ_ANY(to_any<tll::time_point>("2021-01-02T03:04:05.123"), tp + 123ms);
	EXPECT_EQ_ANY(to_any<tll::time_point>("2021-01-02T03:04:05.123Z"), tp + 123ms);
	EXPECT_EQ_ANY(to_any<tll::time_point>("2021-01-02T03:04:05.000123"), tp + 123us);
	EXPECT_EQ_ANY(to_any<tll::time_point>("2021-01-02T03:04:05.000000123"), tp + 123ns);
	EXPECT_EQ(to_string(*to_any<tll::time_point>("2021-01-02T03:04:05.123456789")), "2021-01-02T03:04:05.123456789");

	EXPECT_EQ_ANY(to_any<hour_point>("2021-01-02T03:00:00"), tp - 240s - 5s);
	EXPECT_EQ_ANY(to_any<minute_point>("2021-01-02T03:04:00"), tp - 5s);
	EXPECT_EQ_ANY(to_any<seconds_point>("2021-01-02T03:04:05"), tp);
	EXPECT_EQ_ANY(to_any<ms_point>("2021-01-02T03:04:05.123"), tp + 123ms);
}

TEST(Conv, FixedPoint)
{
	using tll::util::FixedPoint;
	using S3 = FixedPoint<short, 3>;
	using I3 = FixedPoint<int, 3>;
	using U3 = FixedPoint<unsigned, 3>;

	EXPECT_FALSE(to_any<I3>("x"));
	EXPECT_FALSE(to_any<I3>("10x"));
	EXPECT_FALSE(to_any<I3>("10.x"));
	EXPECT_FALSE(to_any<I3>("10.1x"));
	EXPECT_FALSE(to_any<I3>("10.1ex"));
	EXPECT_FALSE(to_any<I3>("10.1e1x"));
	EXPECT_FALSE(to_any<I3>("."));
	EXPECT_FALSE(to_any<I3>("10.1."));

	EXPECT_EQ_ANY(to_any<I3>("10"), I3(10000));
	EXPECT_EQ_ANY(to_any<I3>("10."), I3(10000));
	EXPECT_EQ_ANY(to_any<I3>("10.0"), I3(10000));
	EXPECT_EQ_ANY(to_any<I3>("10.123"), I3(10123));
	EXPECT_EQ_ANY(to_any<I3>("1.0123E1"), I3(10123));
	EXPECT_EQ_ANY(to_any<I3>("1.0123E+1"), I3(10123));
	EXPECT_EQ_ANY(to_any<I3>("101.23E-1"), I3(10123));
	EXPECT_EQ_ANY(to_any<I3>("10123E-3"), I3(10123));
	EXPECT_EQ_ANY(to_any<I3>("10123.E-3"), I3(10123));
	EXPECT_EQ_ANY(to_any<I3>("+10"), I3(10000));
	EXPECT_EQ_ANY(to_any<I3>("-10"), I3(-10000));

	EXPECT_EQ_ANY(to_any<S3>("1e1"), S3((short) 10000));
	EXPECT_EQ_ANY(to_any<S3>("1000e-6"), S3((short) 1));

	EXPECT_FALSE(to_any<S3>("100000")); // Large mantissa
	EXPECT_FALSE(to_any<S3>("1e3")); // Large divisor
	EXPECT_FALSE(to_any<S3>("1000e-9")); // Large multiplicator

	EXPECT_FALSE(to_any<I3>("10.1234")); // Rounding

	EXPECT_FALSE(to_any<U3>("-10")); // Negative
}
