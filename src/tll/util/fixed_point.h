/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_FIXED_POINT_H
#define _TLL_UTIL_FIXED_POINT_H

#include <limits>
#include <string_view>
#include <variant>

namespace tll::util {

namespace {
template <unsigned exponent>
inline constexpr unsigned long long _pow10()
{
	if constexpr (exponent == 0)
		return 1;
	else
		return 10 * _pow10<exponent - 1>();
}
}

template <typename T, unsigned Prec>
class FixedPoint
{
	T _value = {};

 public:
	using value_type = T;
	static constexpr unsigned precision = Prec;
	static constexpr unsigned long long divisor = _pow10<precision>();

	FixedPoint() = default;

	explicit FixedPoint(value_type v) : _value(v) {}
	explicit FixedPoint(double v) : _value(v * divisor) {}

	T value() const { return _value; }

	explicit operator double () const { return ((double) _value) / divisor; }

	FixedPoint & operator += (const FixedPoint &r) { _value += r._value; return *this; }
	FixedPoint & operator -= (const FixedPoint &r) { _value -= r._value; return *this; }
	FixedPoint & operator *= (long long v) { _value *= v; return *this; }

	bool operator == (const FixedPoint &r) const { return _value == r._value; }
	bool operator != (const FixedPoint &r) const { return _value != r._value; }
	bool operator <= (const FixedPoint &r) const { return _value <= r._value; }
	bool operator >= (const FixedPoint &r) const { return _value >= r._value; }
	bool operator <  (const FixedPoint &r) const { return _value <  r._value; }
	bool operator >  (const FixedPoint &r) const { return _value >  r._value; }

	friend FixedPoint operator + (FixedPoint l, const FixedPoint &r) { return l += r; }
	friend FixedPoint operator - (FixedPoint l, const FixedPoint &r) { return l += r; }
	friend FixedPoint operator * (FixedPoint l, long long r) { return l *= r; }

	static constexpr std::variant<T, std::string_view> normalize_mantissa(T m, int expfrom, int expto)
	{
		using namespace std::literals;

		if (m == 0)
			return m;
		if (expfrom == expto)
			return m;

		long expdiff = expfrom - expto;
		constexpr auto digits = std::numeric_limits<T>::digits10 + 1;
		if (expdiff < 0) {
			if (-expdiff > digits)
				return "Exponent difference too large"sv;
			T div = 1;
			for (int i = 0; i != expdiff; i--)
				div *= 10;
			auto r = m % div;
			m = m / div;
			if (r != 0)
				return "Inexact rounding"sv;
		} else {
			if (expdiff > digits)
				return "Exponent difference too large"sv;
			T mul = 1;
			for (int i = 0; i != expdiff; i++)
				mul *= 10;
			if (std::numeric_limits<T>::max() / mul < m)
				return "Value too large"sv;
			m *= mul;
		}
		return m;
	}
};

} // namespace tll::util

#endif //_TLL_UTIL_FIXED_POINT_H
