#ifndef _TLL_COMPAT_FMT_STD_H
#define _TLL_COMPAT_FMT_STD_H

#include <fmt/format.h>

#if FMT_VERSION >= 90000
#include <fmt/std.h>
#else
#include <filesystem>

template <>
struct fmt::formatter<std::filesystem::path> : fmt::formatter<string_view>
{
	template <typename Ctx>
	typename Ctx::iterator format(const std::filesystem::path &p, Ctx &ctx)
#if FMT_VERSION >= 80000
	const
#endif
	{
		return fmt::formatter<string_view>::format(p.string(), ctx);
	}
};
#endif

#endif//_TLL_COMPAT_FMT_STD_H
