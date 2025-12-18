/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CONV_BITS_H
#define _TLL_CONV_BITS_H

#include <type_traits>

#include <tll/conv/base.h>
#include <tll/util/bits.h>
#include <tll/util/string.h>

namespace tll::conv {

template <typename T>
struct dump<T, typename std::enable_if<std::is_base_of_v<tll::util::Bits<typename T::value_type>, T>>::type> : public to_string_from_string_buf<T>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(T v, Buf &buf)
	{
		const auto desc = T::bits_descriptor();
		T left = v;
		std::string_view r;
		for (auto &[name, bit]: desc) {
			if ((left & bit) != 0) {
				if (r.size())
					r = tll::conv::append(buf, r, " | ");
				r = tll::conv::append(buf, r, name);
				left &= ~bit;
			}
		}
		if (left == T {})
			return r;
		if (r.size())
			r = tll::conv::append(buf, r, " | ");
		auto digits = 0u;
		for (; digits < sizeof(T) * 2; digits++) {
			if (left >> 4 * digits == 0)
				break;
		}
		auto off = r.size() ? r.data() - (char *) buf.data() : 0;
		buf.resize(off + r.size() + digits + 2);
		r = { (char *) buf.data(), r.size() + 2 + digits };
		auto ptr = (char *) r.end() - 2 - digits;
		*ptr++ = '0';
		*ptr++ = 'x';
		static const char lookup[] = "0123456789abcdef";
		for (; digits; digits--)
			*ptr++ = lookup[0xf & (left >> (digits - 1) * 4)];
		return r;
	}
};

template <typename T>
struct parse<T, typename std::enable_if<std::is_base_of_v<tll::util::Bits<typename T::value_type>, T>>::type>
{
	static result_t<T> to_any(std::string_view s)
	{
		T t;
		const auto desc = T::bits_descriptor();
		for (auto v : tll::split<'|', ','>(s)) {
			v = tll::util::strip(v);
			auto r = tll::conv::select(v, desc);
			if (r) {
				t |= *r;
				continue;
			}

			auto ri = tll::conv::to_any<typename T::value_type>(v);
			if (!ri)
				return error("Invalid component value: " + std::string(v));
			t |= T(*ri);
		}
		return t;
	}
};

} // namespace tll::conv

#endif//_TLL_CONV_BITS_H
