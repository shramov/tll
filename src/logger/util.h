#ifndef _LOGGER_UTIL_H
#define _LOGGER_UTIL_H

#include "tll/logger.h"
#include "tll/util/conv.h"

constexpr bool icmp(char a, char b)
{
	constexpr char shift = 'a' - 'A';
	if ('a' <= a && a <= 'z') a -= shift;
	if ('a' <= b && b <= 'z') b -= shift;
	return a == b;
}

constexpr bool stricmp(std::string_view a, std::string_view b)
{
	if (a.size() != b.size())
		return false;
	for (auto i = 0u; i < a.size(); i++) {
		if (!icmp(a[i], b[i])) return false;
	}
	return true;
}

template <>
struct tll::conv::parse<tll_logger_level_t>
{
        static result_t<tll_logger_level_t> to_any(std::string_view level)
	{
		if (stricmp(level, "trace")) return tll::Logger::Trace;
		else if (stricmp(level, "debug")) return tll::Logger::Debug;
		else if (stricmp(level, "info")) return tll::Logger::Info;
		else if (stricmp(level, "warning")) return tll::Logger::Warning;
		else if (stricmp(level, "warn")) return tll::Logger::Warning;
		else if (stricmp(level, "error")) return tll::Logger::Error;
		else if (stricmp(level, "critical")) return tll::Logger::Critical;
		else if (stricmp(level, "crit")) return tll::Logger::Critical;
		return tll::error("Invalid level name: " + std::string(level));
	}
};

#endif//_LOGGER_UTIL_H
