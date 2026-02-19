/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CONV_FLOAT_H
#define _TLL_CONV_FLOAT_H

#include <tll/conv/base.h>

#include <cstdio>
#include <cmath>
#include <limits>

namespace tll::conv {

template <typename T>
inline char * rwrite_uint(char * end, T v, int pad = 0)
{
	auto ptr = end;
	while (v) {
		unsigned digit = v % 10;
		v = v / 10;
		*--ptr = '0' + digit;
	}
	while (end - ptr < pad)
		*--ptr = '0';
	return ptr;
}

#ifndef _WIN32
template <>
inline char * rwrite_uint(char * end, unsigned __int128 v, int pad)
{
	constexpr unsigned long long div = 1000ull * 1000 * 1000 * 1000; //10 ^ 12

	unsigned long long lo = v % div;
	v = v / div;
	unsigned long long mid = v % div;
	unsigned long long hi = v / div;
	if (hi) {
		auto ptr = rwrite_uint(end, lo, 12);
		ptr = rwrite_uint(ptr, mid, 12);
		return rwrite_uint(ptr, hi, pad - 24);
	} else if (mid) {
		auto ptr = rwrite_uint(end, lo, 12);
		return rwrite_uint(ptr, mid, pad - 12);
	} else
		return rwrite_uint(end, lo, pad);
}
#endif

template <typename T>
struct unpacked_float
{
	bool sign = false;
	int exponent = 0;
	T mantissa = {};

	unpacked_float(bool s, T v, int exp) : sign(s), exponent(exp), mantissa(v) {}
	unpacked_float(T v, int exp) : exponent(exp)
	{
		if (v < 0) {
			sign = true;
			mantissa = -v;
		} else
			mantissa = v;
	}

	unpacked_float() = default;
	unpacked_float(const unpacked_float &) = default;
	unpacked_float(unpacked_float &&) = default;

	enum Flags {
		ZeroAfterDot = 0x01,
		ZeroBeforeDot = 0x02,
		LowerCaseE = 0x04,
	};

	template <typename Buf>
	inline std::string_view to_string_buf(Buf &buf, unsigned flags = 0) const
	{
		constexpr size_t size = 1 + (3 * 8 * sizeof(mantissa) / 10 + 1) + 4 + (3 * 8 * sizeof(exponent) / 10 + 1);
		buf.resize(size); //[-]digits.[0]E[-]exp

		const auto end = ((char *) buf.data()) + size;
		auto ptr = end;
		auto exp = exponent;

		if (exp > 0 || exp < -9 || sizeof(mantissa) > 8) { // Scientific notation
			bool expsign = exp < 0;
			if (expsign)
				exp = -exp;
			ptr = rwrite_uint(ptr, exp, 1);
			if (expsign)
				*--ptr = '-';
			if (flags & LowerCaseE)
				*--ptr = 'e';
			else
				*--ptr = 'E';
			if (flags & ZeroAfterDot)
				*--ptr = '0';
			*--ptr = '.';

			ptr = rwrite_uint(ptr, mantissa, (flags & ZeroBeforeDot) ? 1 : 0);
		} else if (exp == 0) {
			if (flags & ZeroAfterDot)
				*--ptr = '0';
			*--ptr = '.';
			ptr = rwrite_uint(ptr, mantissa, (flags & ZeroBeforeDot) ? 1 : 0);
		} else { // Exp in [-9, 0) range
			unsigned div = 1;
			for (auto i = exp; i; i++)
				div *= 10;
			auto lo = mantissa % div;
			auto hi = mantissa / div;
			ptr = rwrite_uint(ptr, lo, -exp);
			*--ptr = '.';
			ptr = rwrite_uint(ptr, hi, 1);
		}

		if (sign)
			*--ptr = '-';

		return std::string_view(ptr, end - ptr);
	}
};

template <typename T>
struct dump<unpacked_float<T>> : public to_string_from_string_buf<unpacked_float<T>>
{
	using value_type = unpacked_float<T>;

	template <typename Buf>
	static inline std::string_view to_string_buf(const value_type &v, Buf &buf)
	{
		return v.to_string_buf(buf);
	}
};

template <typename T>
struct parse<unpacked_float<T>>
{
	using value_type = unpacked_float<T>;
	static result_t<value_type> to_any(std::string_view s)
	{
		if (s.size() == 0)
			return error("Empty string");
		unpacked_float<T> r = {};
		if (s[0] == '-') {
			r.sign = true;
			s = s.substr(1);
		} else if (s[0] == '+') {
			s = s.substr(1);
		}
		if (s.size() == 0)
			return error("Empty number");

		bool dot = false;
		bool empty = true;
		for (; s.size(); s = s.substr(1)) {
			if (s[0] == '.') {
				if (dot)
					return error("Duplicate '.'");
				dot = true;
				continue;
			}
			unsigned char c = s[0] - '0';
			if (c > 10)
				break;
			empty = false;
			if (r.mantissa > std::numeric_limits<T>::max() / 10 - 10)
				return error("Significand too large");
			r.mantissa = r.mantissa * 10 + c;
			if (dot)
				r.exponent--;
		}

		if (s.size() == 0) {
			if (empty)
				return error("No digits");
			return r;
		}

		if (s[0] != 'e' && s[0] != 'E')
			return error("Invalid exponent suffix: " + std::string(s));
		if (empty)
			return error("No digits");
		s = s.substr(1);

		if (s.size() == 0)
			return error("Empty exponent");
		auto exp = conv::to_any<int>(s);
		if (!exp)
			return error("Invalid exponent string: " + std::string(s));
		r.exponent += *exp;
		return r;
	}
};

template <typename T>
struct dump<T, typename std::enable_if<std::is_floating_point_v<T>>::type> : public to_string_from_string_buf<T>
{
	static constexpr std::string_view _snprintf_format()
	{
		if constexpr (std::is_same_v<T, long double>)
			return "%.*Lg";
		return "%.*g";
	}

	static constexpr int _snprintf_precision()
	{
		if constexpr (std::is_same_v<T, double>)
			return std::numeric_limits<T>::max_digits10 - 1;
		return std::numeric_limits<T>::max_digits10 - 1;
	}

	template <typename Buf>
	static std::string_view to_string_buf(const T &value, Buf &buf)
	{
		constexpr auto prec = _snprintf_precision();
		buf.resize(prec + 16);
		auto l = snprintf(buf.data(), buf.size(), _snprintf_format().data(), prec, value);
		auto r = std::string_view(buf.data(), std::min<int>(l, buf.size()));
		return r;
		//if (r.find('.') != r.npos) return r;
		//return append(buf, r, ".0");
	}
};

template <typename T>
struct parse<T, typename std::enable_if<std::is_floating_point_v<T>>::type>
{
	static result_t<T> to_any(std::string_view s)
	{
		auto u = tll::conv::to_any<unpacked_float<unsigned long long>>(s);
		if (!u)
			return error(u.error());
		T r = powl(10, u->exponent) * u->mantissa;
		if (u->sign)
			return -r;
		return r;
	}
};

} // namespace tll::conv

#endif//_TLL_CONV_FLOAT_H
