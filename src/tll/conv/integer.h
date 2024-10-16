// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_CONV_INTEGER_H
#define _TLL_CONV_INTEGER_H

#include <tll/conv/base.h>

#include <limits>

namespace tll::conv {

template <unsigned Base>
struct Digits {};

template <>
struct Digits<10> { static char decode(char c) { if (c < '0' || c > '9') return 10; return c - '0'; } };

template <>
struct Digits<16>
{
	static char decode(char c)
	{
		static const char lookup[] = ""
			"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0x00
			"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0x10
			"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0x20
			"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x10\x10\x10\x10\x10\x10" // 0x30
			"\x10\x0a\x0b\x0c\x0d\x0e\x0f\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0x40
			"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0x50
			"\x10\x0a\x0b\x0c\x0d\x0e\x0f\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0x60
			"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0x70
			"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0x10
			"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0x90
			"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0xa0
			"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0xb0
			"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0xc0
			"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0xd0
			"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0xe0
			"\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10\x10" // 0xf0
			"";
		return lookup[(unsigned char) c];
	}
};

template <typename I, I Limit = std::numeric_limits<I>::max(), int Base = 10>
inline result_t<I> to_any_uint_base(std::string_view s)
{
	constexpr I Thresold = (Limit / Base > Base) ? Limit / Base - Base : 0;

	if (!s.size()) return error("Empty string");
	I r = 0;
	auto ptr = s.begin();
	for (; ptr != s.end(); ptr++) {
		auto x = Digits<Base>::decode(*ptr);
		if (x == Base)
			return error("Invalid digit: " + std::string(ptr, 1));
		if (r > Thresold) {
			auto old = r;
			r = Base * r + x;
			if (old > r || r > Limit)
				return error("Overflow");
		} else
			r = Base * r + x;
	}
	return r;
}

template <typename I, I Limit = std::numeric_limits<I>::max()>
inline result_t<I> to_any_uint(std::string_view s)
{
	if (s.size() > 2) {
		if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
			return to_any_uint_base<I, Limit, 16>(s.substr(2));
	}
	return to_any_uint_base<I, Limit, 10>(s);
}

template <typename I>
inline result_t<I> to_any_sint(std::string_view s)
{
	I mul = 1;
	if (s.size() == 0)
		return error("Empty string");
	auto ptr = s.begin();
	if (*ptr == '-') {
		mul = -1;
		ptr++;
	} else if (*ptr == '+') {
		mul = 1;
		ptr++;
	}

	using uI = typename std::make_unsigned_t<I>;
	if (mul > 0) {
		auto r = to_any_uint<uI, std::numeric_limits<I>::max()>(s.substr(ptr - s.begin()));
		if (!r) return error(r.error());
		return *r;
	} else {
		auto r = to_any_uint<uI, static_cast<uI>(std::numeric_limits<I>::max()) + 1>(s.substr(ptr - s.begin()));
		if (!r) return error(r.error());
		return -*r;
	}
}

template <typename I>
inline result_t<I> to_any_int(std::string_view s)
{
	if constexpr (std::is_unsigned_v<I>)
		return to_any_uint<I>(s);
	else if constexpr (!std::is_unsigned_v<I>) //XXX: clang needs this
		return to_any_sint<I>(s);
	return error("Not integer type");
}

template <int Base>
struct to_string_buf_uint {
	template <typename I, typename Buf>
	static inline std::string_view to_string_buf(const I &v, Buf &buf);
};

template <>
struct to_string_buf_uint<16> {
	template <typename I, typename Buf>
	static inline std::string_view to_string_buf(I v, Buf &buf)
	{
		static const char lookup[] = "0123456789abcdef";
		buf.resize(3 + sizeof(I) * 2); // One for possible sign
		auto end = ((char *) buf.data()) + buf.size();
		auto ptr = end;
		for (unsigned i = 0; i < sizeof(I); i++) { // For unroll
			unsigned char lo = v & 0xfu;
			unsigned char hi = (v & 0xffu) >> 4;
			v >>= 8;
			*--ptr = lookup[lo];
			*--ptr = lookup[hi];
			if (!v) break;
		}
		return std::string_view(ptr, end - ptr);
	}
};

template <>
struct to_string_buf_uint<10> {
	template <typename I, typename Buf>
	static inline std::string_view to_string_buf(I v, Buf &buf)
	{
		buf.resize(1 + sizeof(I) * 3); // One for possible sign
		auto end = ((char *) buf.data()) + buf.size();
		auto ptr = end;
		do {
			I r = v % 10;
			v /= 10;
			*--ptr = '0' + r;
		} while (v);
		return std::string_view(ptr, end - ptr);
	}
};

template <typename I, typename Buf, int Base = 10>
inline std::string_view to_string_buf_int(I v, Buf &buf)
{
	if constexpr (std::is_unsigned_v<I>)
		return to_string_buf_uint<Base>::template to_string_buf<I, Buf>(v, buf);
	if (v >= 0)
		return to_string_buf_uint<Base>::template to_string_buf<std::make_unsigned_t<I>, Buf>(v, buf);
	auto r = to_string_buf_uint<Base>::template to_string_buf<std::make_unsigned_t<I>, Buf>(-v, buf);
	auto ptr = (char *) r.data();
	*--ptr = '-';
	return std::string_view(ptr, r.size() + 1);
}

template <typename T>
struct dump<T, typename std::enable_if<std::is_integral_v<T>>::type> : public to_string_from_string_buf<T>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(T v, Buf &buf) { return to_string_buf_int<T, Buf, 10>(v, buf); }
};

template <typename T>
struct parse<T, typename std::enable_if<std::is_integral_v<T>>::type>
{
	static result_t<T> to_any(std::string_view s) { return to_any_int<T>(s); }
};

} // namespace tll::conv

#endif//_TLL_CONV_INTEGER_H
