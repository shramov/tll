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

/*
template <typename T>
struct dump<T, typename std::enable_if<!std::is_same_v<std::result_of_t<typename tll::util::BitsDescriptor<T>::descriptor>, void>>::type> : public to_string_from_string_buf<T>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(T v, Buf &buf) { return to_string_buf_int<T, Buf, 16>((typename T::value_type) v, buf); }
};
*/

template <typename T>
struct parse<T, typename std::enable_if<std::is_base_of_v<tll::util::Bits<typename T::value_type>, T>>::type>
{
	static result_t<T> to_any(std::string_view s)
	{
		T t;
		const auto desc = T::bits_descriptor();
		for (auto v : tll::split<'|'>(s)) {
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
