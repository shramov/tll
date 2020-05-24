/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_RESULT_H
#define _TLL_UTIL_RESULT_H

#include <string>
#include <variant>

/*
 * Naive implementation of std::expected https://wg21.link/P0323
 */

namespace tll {

template <typename E>
class unexpected
{
	E _error;
 public:
	unexpected(const E &error) : _error(error) {}
	unexpected(E &&error) : _error(std::move(error)) {}

	E & error() & { return _error; }
	E && error() && { return _error; }
	const E & error() const & { return _error; }
};

struct error_tag_t {};
constexpr error_tag_t error_tag = {};

template <typename T, typename E>
class expected : public std::variant<T, E>
{
 public:
	typedef std::variant<T, E> parent_variant;
	typedef E expectedype;
	typedef T value_type;

	expected(const T & value) : parent_variant(std::in_place_index<0>, value) {}
	expected(T && value) : parent_variant(std::in_place_index<0>, std::move(value)) {}

	template <typename S>
	expected(const S & value) : parent_variant(std::in_place_index<0>, value) {}

	template <typename S>
	expected(const expected<S, E> & e) : parent_variant(e) {}

	expected(const unexpected<E> & e) : parent_variant(std::in_place_index<1>, e.error()) {}
	expected(unexpected<E> && e) : parent_variant(std::in_place_index<1>, std::move(e.error())) {}

	expected(const E & e, error_tag_t & tag) : parent_variant(std::in_place_index<1>, e) {}
	expected(E && e, error_tag_t & tag) : parent_variant(std::in_place_index<1>, std::move(e)) {}

	explicit operator bool () const { return parent_variant::index() == 0; }

	value_type operator * () && { return std::get<0>(*this); }
	value_type & operator * () & { return std::get<0>(*this); }
	const value_type & operator * () const & { return std::get<0>(*this); }

	value_type * operator -> () { return &std::get<0>(*this); }
	const value_type * operator -> () const { return &std::get<0>(*this); }

	//const E & error() const { if (*this) return E(); return std::get<1>(*this); }
	const E & error() const { return std::get<1>(*this); }
};

using error_t = unexpected<std::string>;
template <typename T, typename E = std::string>
using result_t = expected<T, E>;

static inline error_t error(std::string_view e) { return { std::string(e) }; }

} // namespace tll

#endif//_TLL_UTIL_RESULT_H
