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

#include <tll/compat/expected.h>

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

namespace fixed_point {

/**
 * Convert mantissa from one exponent to another.
 */
template <typename T>
static constexpr tll::compat::expected<T, std::string_view> convert_mantissa(T m, int expfrom, int expto)
{
	using namespace std::literals;
	using namespace tll::compat;

	if (m == 0)
		return m;
	if (expfrom == expto)
		return m;

	long expdiff = expfrom - expto;
	constexpr auto digits = std::numeric_limits<T>::digits10 + 1;
	if (expdiff < 0) {
		if (-expdiff > digits)
			return unexpected("Exponent difference too large"sv);
		T div = 1;
		for (int i = 0; i != expdiff; i--)
			div *= 10;
		auto r = m % div;
		m = m / div;
		if (r != 0)
			return unexpected("Inexact rounding"sv);
	} else {
		if (expdiff > digits)
			return unexpected("Exponent difference too large"sv);
		T mul = 1;
		for (int i = 0; i != expdiff; i++)
			mul *= 10;
		if (std::numeric_limits<T>::max() / mul < m)
			return unexpected("Value too large"sv);
		m *= mul;
	}
	return m;
}
} // namespace fixed_point

template <typename T, unsigned Prec>
class FixedPoint
{
	T _value = {};

 public:
	using value_type = T;
	static constexpr unsigned precision = Prec;
	static constexpr int exponent = -Prec;
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
	friend FixedPoint operator - (FixedPoint l, const FixedPoint &r) { return l -= r; }
	friend FixedPoint operator * (FixedPoint l, long long r) { return l *= r; }

	[[deprecated("Use tll::util::fixed_point::convert_mantissa")]]
	static constexpr std::variant<T, std::string_view> normalize_mantissa(T m, int expfrom, int expto)
	{
		return fixed_point::convert_mantissa(m, expfrom, expto);
	}

	/// Convert from FixedPoint with different precision
	template <unsigned FPrec>
	[[nodiscard]]
	tll::compat::expected<FixedPoint, std::string_view> from(const FixedPoint<T, FPrec>& rhs)
	{
		auto m = rhs.template into<Prec>();
		if (!m)
			return unexpected(m.error());
		_value = m->_value;
		return *this;
	}

	/// Convert into FixedPoint with different precision
	template <unsigned IPrec>
	constexpr tll::compat::expected<FixedPoint<T, IPrec>, std::string_view> into() const
	{
		auto m = fixed_point::convert_mantissa(_value, exponent, -IPrec);
		if (m)
			return FixedPoint<T, IPrec>{*m};
		return unexpected(m.error());
	}
};

} // namespace tll::util

#endif //_TLL_UTIL_FIXED_POINT_H
