// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_COMPAT_EXPECTED_H
#define _TLL_COMPAT_EXPECTED_H

#include <variant>

/*
 * Naive implementation of std::expected https://wg21.link/P0323
 */

namespace tll::compat {

// Workround 'alias template deduction' error
namespace expected_ns {

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

template <typename T, typename E>
class expected
{
	std::variant<T, E> _data;
 public:
	typedef E error_type;
	typedef T value_type;

	constexpr expected(const T & value) : _data(std::in_place_index<0>, value) {}
	constexpr expected(T && value) : _data(std::in_place_index<0>, std::move(value)) {}

	template <typename S>
	constexpr expected(const S & value) : _data(std::in_place_index<0>, value) {}

	template <typename SE>
	constexpr expected(const unexpected<SE> & e) : _data(std::in_place_index<1>, e.error()) {}

	template <typename SE>
	constexpr expected(unexpected<SE> && e) : _data(std::in_place_index<1>, std::move(e.error())) {}

	constexpr bool has_value() const { return _data.index() == 0; }
	explicit constexpr operator bool () const { return has_value(); }

	constexpr value_type && operator * () && { return std::get<0>(std::move(_data)); }
	constexpr value_type & operator * () & { return std::get<0>(_data); }
	constexpr const value_type & operator * () const & { return std::get<0>(_data); }

	constexpr value_type * operator -> () { return &std::get<0>(_data); }
	constexpr const value_type * operator -> () const { return &std::get<0>(_data); }

	template <typename U>
	constexpr T value_or(U && default_value) const &
	{
		if (*this) return **this; else return static_cast<T>(std::forward<U>(default_value));
	}

	template <typename U>
	constexpr T value_or(U && default_value) &&
	{
		if (*this) return std::move(**this); else return static_cast<T>(std::forward<U>(default_value));
	}

	constexpr const E & error() const & noexcept { return std::get<1>(_data); }
	constexpr E & error() & noexcept { return std::get<1>(_data); }
	constexpr E && error() && noexcept { return std::get<1>(std::move(_data)); }
};

template <typename E>
class expected<void, E>
{
	std::variant<std::monostate, E> _data;
 public:
	typedef E error_type;
	typedef void value_type;

	constexpr expected() : _data(std::in_place_index<0>) {}

	template <typename SE>
	constexpr expected(const unexpected<SE> & e) : _data(std::in_place_index<1>, e.error()) {}
	template <typename SE>
	constexpr expected(unexpected<SE> && e) : _data(std::in_place_index<1>, std::move(e.error())) {}

	constexpr bool has_value() const { return _data.index() == 0; }
	explicit constexpr operator bool () const { return has_value(); }

	constexpr const E & error() const & noexcept { return std::get<1>(*this); }
	constexpr E & error() & noexcept { return std::get<1>(_data); }
	constexpr E && error() && noexcept { return std::get<1>(std::move(_data)); }
};

} // namespace expected_ns

using namespace expected_ns;

} // namespace tll::compat

#endif//_TLL_COMPAT_EXPECTED_H

