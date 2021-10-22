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
#include "tll/conv/float.h"

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

		unpacked_float<__uint128_t> fu {u.sign != 0, u.mantissa.value, u.exponent};

		return tll::conv::to_string_buf(fu, buf);
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

template <>
struct parse<tll::util::Decimal128>
{
	using value_type = tll::util::Decimal128;
	static result_t<value_type> to_any(std::string_view s)
	{
		if (s == "Inf")
			return value_type(false, 0, value_type::Unpacked::exp_inf);
		else if (s == "-Inf")
			return value_type(true, 0, value_type::Unpacked::exp_inf);
		else if (s == "NaN")
			return value_type(value_type::Unpacked::nan());
		else if (s == "sNaN")
			return value_type(value_type::Unpacked::snan());

		auto u = conv::to_any<unpacked_float<__uint128_t>>(s);
		if (!u)
			return error(u.error());

		value_type r;
		if (r.pack(u->sign, u->mantissa, u->exponent))
			return error("Failed to pack value");
		return r;
	}
};

template <>
struct parse<tll_decimal128_t>
{
	using value_type = tll_decimal128_t;
	static result_t<value_type> to_any(std::string_view s)
	{
		auto r = conv::to_any<tll::util::Decimal128>(s);
		if (r)
			return static_cast<tll_decimal128_t>(*r);
		return error(r.error());
	}
};

} // namespace tll::conv

#endif//_TLL_CONV_DECIMAL128_H
