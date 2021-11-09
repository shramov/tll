#include <math.h>

#include "gtest/gtest.h"

#include "tll/conv/decimal128.h"
#include "tll/util/decimal128.h"
#include "tll/util/fixed_point.h"

using tll::util::Decimal128;

#if defined(__GLIBCXX__) && !defined(__clang__)
#include <decimal/decimal>

#define CHECK_STD128(str, dec, bin) do { \
		std::decimal::decimal128 std(dec); \
		ASSERT_EQ(std, static_cast<std::decimal::decimal128>(bin)) << "Invalid decimal value for " << str; \
	} while (0)
#else
#define CHECK_STD128(str, dec, bin)
#endif

#define CHECK_D128(str, binary, stddecimal, sig, m, e) do { \
		Decimal128::Unpacked u = {}; \
		u.sign = sig; \
		u.mantissa.value = (m); \
		u.exponent = e; \
		Decimal128 dec = {}; \
		ASSERT_EQ(dec.pack(u), 0); \
		Decimal128 bindec = {}; \
		memcpy(&bindec, binary, sizeof(bindec)); \
		ASSERT_EQ(dec.lo, bindec.lo); \
		ASSERT_EQ(dec.hi, bindec.hi); \
		if (e != u.exp_nan && e != u.exp_snan) \
			CHECK_STD128(str, stddecimal, dec); \
		u = {}; \
		dec.unpack(u); \
		ASSERT_EQ(u.sign, sig); \
		ASSERT_EQ(u.mantissa.value, (m)); \
		ASSERT_EQ(u.exponent, e); \
		ASSERT_EQ(tll::conv::to_string(dec), str); \
		auto r = tll::conv::to_any<Decimal128>(str); \
		ASSERT_TRUE(r) << "Failed to convert " << str << ": " << r.error(); \
		ASSERT_EQ(tll::conv::to_string(*r), str); \
	} while (0)

constexpr __uint128_t u128_build_18(uint64_t large, uint64_t small)
{
	constexpr uint64_t exp18 = 1000ull * 1000 * 1000 * 1000 * 1000 * 1000; // 10 ^ 18
	__uint128_t r = large;
	return r * exp18 + small;
}

TEST(Util, Decimal128)
{
#include "test_generated128.h"

	using tll::util::FixedPoint;
	ASSERT_EQ(tll::conv::to_string(Decimal128(FixedPoint<unsigned, 3>(1234u))), "1234.E-3");
	ASSERT_EQ(tll::conv::to_string(Decimal128(FixedPoint<long long, 3>(-1234ll))), "-1234.E-3");
}
