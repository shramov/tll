/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_RESULT_H
#define _TLL_UTIL_RESULT_H

#include <tll/compat/expected.h>

#include <string>

namespace tll {

using namespace tll::compat::expected_ns;

using error_t = unexpected<std::string>;
template <typename T, typename E = std::string>
using result_t = expected<T, E>;

[[nodiscard]]
inline error_t error(std::string_view e) { return { std::string(e) }; }

} // namespace tll

#endif//_TLL_UTIL_RESULT_H
