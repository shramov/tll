#ifndef _LOGGER_COMMON_H
#define _LOGGER_COMMON_H

#include "tll/logger.h"
#include "tll/logger/impl.h"
#include "tll/util/refptr.h"
#include "tll/util/time.h"

#include <memory>
#include <mutex>

namespace tll::logger {

struct tll_logger_obj_t : tll::util::refbase_t<tll_logger_obj_t, 0>
{
	const char * name = "";
	void * obj = nullptr;
	std::shared_ptr<tll_logger_impl_t> impl;

	~tll_logger_obj_t()
	{
		if (obj && impl && impl->log_free)
			impl->log_free(impl.get(), name, obj);
	}

	auto log(tll::time_point ts, tll_logger_level_t level, std::string_view body)
	{
		return (*impl->log)(ts.time_since_epoch().count(), name, level, body.data(), body.size(), obj);
	}
};

struct Logger : public tll_logger_t, public tll::util::refbase_t<Logger>
{
	void destroy() {}

	std::mutex lock;
	std::string name;
	tll::util::refptr_t<tll_logger_obj_t> impl;

	auto log(tll::time_point ts, tll_logger_level_t level, std::string_view body)
	{
		std::unique_lock<std::mutex> lck(lock);
		auto ref = impl;
		lck.unlock();

		return ref->log(ts, level, body);
	}
};

} // namespace tll::logger

#endif//_LOGGER_COMMON_H
