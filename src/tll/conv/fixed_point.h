/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CONV_DECIMAL128_H
#define _TLL_CONV_DECIMAL128_H

#include <limits>
#include <type_traits>

#include "tll/conv/float.h"
#include "tll/util/fixed_point.h"

namespace tll::conv {
template <typename T, unsigned P>
struct dump<tll::util::FixedPoint<T, P>> : public to_string_from_string_buf<tll::util::FixedPoint<T, P>>
{
	using value_type = tll::util::FixedPoint<T, P>;

	template <typename Buf>
	static inline std::string_view to_string_buf(const value_type &v, Buf &buf)
	{
		unpacked_float<std::make_unsigned_t<T>> u;
		if constexpr (!std::is_unsigned_v<T>) {
			u.sign = v.value() < 0;
			if (u.sign)
				u.mantissa = -v.value();
			else
				u.mantissa = v.value();
		} else {
			u.sign = 0;
			u.mantissa = v.value();
		}

		u.exponent = -P;

		return tll::conv::to_string_buf(u, buf);
	}
};

template <typename T, unsigned P>
struct parse<tll::util::FixedPoint<T, P>>
{
	using value_type = tll::util::FixedPoint<T, P>;
	static result_t<value_type> to_any(std::string_view s)
	{
		constexpr int prec = -(int) P;

		auto u = conv::to_any<unpacked_float<T>>(s);
		if (!u)
			return error(u.error());
		auto m = u->mantissa;
		if (u->sign) {
			if constexpr (std::is_unsigned_v<T>)
				return error("Negative value");
			m = -m;
		}
		auto r = tll::util::FixedPoint<T, 0>::normalize_mantissa(m, u->exponent, prec);
		if (!std::holds_alternative<T>(r))
			return error(std::get<1>(r));
		return value_type(std::get<0>(r));
	}
};

} // namespace tll::conv

#endif//_TLL_CONV_DECIMAL128_H
