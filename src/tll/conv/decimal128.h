/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CONV_DECIMAL128_H
#define _TLL_CONV_DECIMAL128_H

#include <type_traits>

#include "tll/util/decimal128.h"
#include "tll/util/conv.h"

namespace tll::conv {
template <>
struct dump<tll::util::Decimal128> : public to_string_from_string_buf<tll::util::Decimal128>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const tll::util::Decimal128 &v, Buf &buf)
	{
		tll::util::Decimal128::Unpacked u;
		v.unpack(u);

		if (u.exponent == TLL_DECIMAL128_INF) {
			if (u.sign)
				return "-Inf";
			else
				return "Inf";
		} else if (u.exponent == TLL_DECIMAL128_NAN) {
			return "NaN";
		} else if (u.exponent == TLL_DECIMAL128_SNAN) {
			return "sNaN";
		}

		buf.resize(1 + 34 + 3 + 4); //[-]digits.E[-]exp
		constexpr unsigned long long div = 1000ull * 1000 * 1000 * 1000 * 1000 * 1000; //10 ^ 18
		unsigned long long hi = u.mantissa.value / div;
		unsigned long long lo = u.mantissa.value % div;

		auto end = ((char *) buf.data()) + 1 + 34 + 3 + 4;
		auto ptr = end;

		auto exp = u.exponent;
		bool expsign = exp < 0;
		if (expsign)
			exp = -exp;
		do {
			unsigned digit = exp % 10;
			exp = exp / 10;
			*--ptr = '0' + digit;
		} while (exp);
		if (expsign)
			*--ptr = '-';
		*--ptr = 'E';
		*--ptr = '.';

		const auto dot = ptr;

		do {
			unsigned digit = lo % 10;
			lo = lo / 10;
			*--ptr = '0' + digit;
		} while (lo);
		if (hi) {
			while (ptr != dot - 18)
				*--ptr = '0';
			while (hi) {
				unsigned digit = hi % 10;
				hi = hi / 10;
				*--ptr = '0' + digit;
			}
		}
		if (u.sign)
			*--ptr = '-';

		return std::string_view(ptr, end - ptr);
	}
};

template <>
struct dump<tll_decimal128_t> : public to_string_from_string_buf<tll_decimal128_t>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const tll_decimal128_t &v, Buf &buf)
	{
		return tll::conv::to_string_buf<tll_decimal128_t, Buf>(v, buf);
	}
};

} // namespace tll::conv

#endif//_TLL_CONV_DECIMAL128_H
