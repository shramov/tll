/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_FIXED_POINT_H
#define _TLL_UTIL_FIXED_POINT_H

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
};

} // namespace tll::util

#endif //_TLL_UTIL_FIXED_POINT_H
