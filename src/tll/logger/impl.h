#ifndef _TLL_LOGGER_IMPL_H
#define _TLL_LOGGER_IMPL_H

#include "tll/logger.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Custom logger implementation
typedef struct tll_logger_impl_t {
	/// Log message, obj is data returned from log_new
	int (*log)(long long ts, const char * category, tll_logger_level_t level, const char * data, size_t size, void * obj);
	/// New logger is created, return logger specific data
	void * (*log_new)(struct tll_logger_impl_t * impl, const char * category);
	/// Logger instance is removed, free impl specific data (returned by log_new)
	void (*log_free)(struct tll_logger_impl_t * impl, const char * category, void * obj);
	/// Logger configuration is performed, same config object is passed to impl
	int (*configure)(struct tll_logger_impl_t * impl, const tll_config_t * config);
	/// Another logger implementation is registered (or program is exiting)
	void (*release)(struct tll_logger_impl_t * impl);
	/// User data
	void * user;
} tll_logger_impl_t;

/// Replace current logger implementation
int tll_logger_register(tll_logger_impl_t *);

#ifdef __cplusplus
} // extern "C"
#endif

#endif//_TLL_LOGGER_IMPL_H
