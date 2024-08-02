#ifndef _TLL_SPDLOG_H
#define _TLL_SPDLOG_H

#include "tll/logger/impl.h"

tll_logger_impl_t * spdlog_impl();

void spdlog_thread_name_set(const char * name);

#endif//_TLL_SPDLOG_H
