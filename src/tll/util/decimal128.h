/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_DECIMAL128_H
#define _TLL_UTIL_DECIMAL128_H

#include <errno.h>
#include <stdint.h>

#ifdef __cplusplus

#include "tll/util/fixed_point.h"

#if defined(__GLIBCXX__) && !defined(__clang__) && defined(__x86_64__)
#define TLL_DECIMAL128_GLIBCXX 1
#include <decimal/decimal>
#endif

extern "C" {
#endif

#pragma pack(push, 1)

typedef struct
{
	union {
		struct {
			uint64_t lo;
			union {
				uint64_t hi;
				struct {
					uint64_t hisig:49;
					uint64_t combination:14;
					uint64_t sign:1;
				};
			};
		};
#if defined(__cplusplus) && defined(TLL_DECIMAL128_GLIBCXX)
		std::decimal::decimal128 stddecimal = {};
#endif
	};
} tll_decimal128_t;

#pragma pack(pop)

typedef union
{
	struct {
		uint64_t lo;
		uint64_t hi;
	};
	__uint128_t value;
} tll_uint128_t;

static const short TLL_DECIMAL128_INF = 10000;
static const short TLL_DECIMAL128_NAN = 10001;
static const short TLL_DECIMAL128_SNAN = 10002;

typedef struct tll_decimal128_unpacked_t
{
	short sign;
	short exponent;
	tll_uint128_t mantissa;

#ifdef __cplusplus
	static constexpr short exp_inf = TLL_DECIMAL128_INF;
	static constexpr short exp_nan = TLL_DECIMAL128_NAN;
	static constexpr short exp_snan = TLL_DECIMAL128_SNAN;

	static constexpr tll_decimal128_unpacked_t inf() { return { 0, exp_inf, {} }; }
	static constexpr tll_decimal128_unpacked_t nan() { return { 0, exp_nan, {} }; }
	static constexpr tll_decimal128_unpacked_t snan() { return { 0, exp_snan, {} }; }

	bool isnan() const { return exponent == exp_nan || exponent == exp_snan; }
	bool isinf() const { return exponent == exp_inf; }
#endif
} tll_decimal128_unpacked_t;

int tll_decimal128_pack(tll_decimal128_t * d, const tll_decimal128_unpacked_t * u);
int tll_decimal128_unpack(tll_decimal128_unpacked_t * u, const tll_decimal128_t * d);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

namespace tll::util {

struct Decimal128 : public tll_decimal128_t
{
	using Unpacked = tll_decimal128_unpacked_t;

	static constexpr short exp_min = -6176;
	static constexpr short exp_max = 6111;

	static constexpr uint64_t hisig_max = 0x1ed09bead87c0ull; // (10 ** 34) >> 64

	static constexpr uint32_t high_mask = 0xc0u << 6;

	static constexpr uint32_t inf_value = 0xf0u << 6;
	static constexpr uint32_t inf_mask = 0xf8u << 6;

	static constexpr uint32_t nan_value = 0xf8u << 6;
	static constexpr uint32_t nan_mask = 0xfcu << 6;

	static constexpr uint32_t snan_value = 0xfcu << 6;
	static constexpr uint32_t snan_mask = 0xfcu << 6;

	Decimal128() {};

	explicit Decimal128(const Unpacked &u)
	{
		if (pack(u))
			pack(Unpacked::nan());
	}

	Decimal128(bool s, __uint128_t m, short exponent)
	{
		if (pack(s, m, exponent))
			pack(Unpacked::nan());
	}

	template <typename T, unsigned Prec>
	Decimal128(const FixedPoint<T, Prec> &f)
	{
		if constexpr (std::is_unsigned_v<T>) {
			pack(false, f.value(), -(short) f.precision);
		} else {
			if (f.value() < 0) {
				pack(true, -f.value(), -(short) f.precision);
			} else
				pack(false, f.value(), -(short) f.precision);
		}
	}

#ifdef TLL_DECIMAL128_GLIBCXX
	Decimal128(const std::decimal::decimal128 &v) { stddecimal = v; }
#endif

	Decimal128(const Decimal128 &) = default;
	Decimal128(Decimal128 &&) = default;

	Decimal128 & operator = (const Decimal128 &) = default;
	Decimal128 & operator = (Decimal128 &&) = default;

#ifdef TLL_DECIMAL128_GLIBCXX
	operator std::decimal::decimal128 () const { return stddecimal; }
#endif

	bool isnan() const { Unpacked u; unpack(u); return u.isnan(); }
	bool isinf() const { Unpacked u; unpack(u); return u.isinf(); }

	void unpack(Unpacked &u) const
	{
		u.sign = sign;
		u.exponent = 0;
		u.mantissa.value = 0;
		if ((combination & high_mask) == high_mask) {
			if ((combination & inf_mask) == inf_value) {
				u.exponent = u.exp_inf;
			} else if ((combination & nan_mask) == nan_value) {
				u.exponent = u.exp_nan;
			} else if ((combination & snan_mask) == snan_value) {
				u.exponent = u.exp_snan;
			} else {
				/*
				 * The "11" 2-bit sequence after the sign bit indicates that there is an implicit "100" 3-bit prefix to the significand.
				 * Treat as overflow
				 */
			}
			return;
		}
		u.exponent = combination + exp_min;
		u.mantissa.hi = hisig;
		u.mantissa.lo = lo;
	}

	int pack(const tll_decimal128_unpacked_t &u) { return pack(u.sign, u.mantissa, u.exponent); }
	int pack(int sign, const __uint128_t &mantissa, short exponent)
	{
		tll_uint128_t m;
		m.value = mantissa;
		return pack(sign, m, exponent);
	}
	int pack(int sign, const tll_uint128_t &mantissa, short exponent)
	{
		if (exponent > exp_max) {
			hi = lo = 0;
			switch (exponent) {
			case Unpacked::exp_inf:
				combination = inf_value;
				this->sign = sign;
				return 0;
			case Unpacked::exp_nan:
				combination = nan_value;
				return 0;
			case Unpacked::exp_snan:
				combination = snan_value;
				return 0;
			default:
				return ERANGE;
			}
		} else if (exponent < exp_min)
			return ERANGE;
		if (mantissa.hi > hisig_max)
			return ERANGE;
		this->sign = sign;
		combination = exponent - exp_min;
		hisig = mantissa.hi;
		lo = mantissa.lo;
		return 0;
	}
};

} // namespace tll::util

#endif

#endif//_TLL_UTIL_DECIMAL128_H
