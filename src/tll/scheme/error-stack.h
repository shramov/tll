// SPDX-License-Identifier: MIT

#ifndef _TLL_SCHEME_ERROR_STACK_H
#define _TLL_SCHEME_ERROR_STACK_H

#include "tll/scheme.h"

#include <variant>
#include <vector>

namespace tll::scheme {

struct ErrorStack
{

	template <typename... Args>
#if FMT_VERSION < 80000
	using format_string = std::string_view;
#else
	using format_string = fmt::format_string<Args...>;
#endif

	/// Error message
	std::string error;
	/// Error stack, field pointer or array index
	std::vector<std::variant<const tll::scheme::Field *, size_t>> error_stack;

	void error_clear()
	{
		error.clear();
		error_stack.clear();
	}

	template <typename R, typename... Args>
	[[nodiscard]]
	R fail(R err, format_string<Args...> format, Args && ... args)
	{
		error = fmt::format(format, std::forward<Args>(args)...);
		error_stack.clear();
		return err;
	}

	template <typename R>
	[[nodiscard]]
	R fail_index(R err, size_t idx)
	{
		error_stack.push_back(idx);
		return err;
	}

	template <typename R>
	[[nodiscard]]
	R fail_field(R err, const tll::scheme::Field * field)
	{
		error_stack.push_back(field);
		return err;
	}

	std::string format_stack() const
	{
		using Field = tll::scheme::Field;
		std::string r;
		for (auto i = error_stack.rbegin(); i != error_stack.rend(); i++) {
			if (std::holds_alternative<size_t>(*i)) {
				r += fmt::format("[{}]", std::get<size_t>(*i));
			} else {
				if (r.size())
					r += ".";
				r += std::get<const Field *>(*i)->name;
			}
		}
		return r;
	}
};

} // namespace tll::scheme

#endif//_TLL_SCHEME_ERROR_STACK_H
