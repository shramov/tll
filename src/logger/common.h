#ifndef _LOGGER_COMMON_H
#define _LOGGER_COMMON_H

#include "tll/logger.h"
#include "tll/logger/impl.h"
#include "tll/util/refptr.h"
#include "tll/util/time.h"

#include <mutex>

namespace tll::logger {

struct tll_logger_obj_t : tll::util::refbase_t<tll_logger_obj_t, 0>
{
	const char * name = "";
	void * obj = nullptr;
	tll_logger_impl_t * impl = nullptr;

	~tll_logger_obj_t()
	{
		if (obj && impl && impl->log_free)
			impl->log_free(impl, name, obj);
	}

	auto log(tll::time_point ts, tll_logger_level_t level, std::string_view body)
	{
		return (*impl->log)(ts.time_since_epoch().count(), name, level, body.data(), body.size(), obj);
	}
};

struct Logger : public tll_logger_t, public tll::util::refbase_t<Logger>
{
	void destroy();

	std::mutex lock;
	std::string name;
	tll::util::refptr_t<tll_logger_obj_t> impl;
};

} // namespace tll::logger

#endif//_LOGGER_COMMON_H
