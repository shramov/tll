/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/config.h"
#include "tll/logger.h"
#include "tll/logger/impl.h"
#include "tll/util/refptr.h"
#include "tll/util/string.h"
#include "tll/util/time.h"

#include <atomic>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>

#include <stdio.h>
#include <stdarg.h>

#include "logger/config.h"

#include "logger/util.h"

#ifdef WITH_SPDLOG
#include "logger/spdlog.h"
#endif

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
};

struct Logger : public tll_logger_t, public tll::util::refbase_t<Logger>
{
	void destroy();

	std::mutex lock;
	std::string name;
	tll::util::refptr_t<tll_logger_obj_t> impl;
};

struct logger_context_t
{
	std::shared_mutex lock;
	typedef std::unique_lock<std::shared_mutex> wlock_t;
	typedef std::shared_lock<std::shared_mutex> rlock_t;

	std::map<std::string_view, Logger *, std::less<>> _loggers;
	std::map<std::string, tll_logger_level_t, std::less<>> _levels_prefix;
	std::map<std::string, tll_logger_level_t, std::less<>> _levels;
	tll_logger_level_t _default = tll::Logger::Debug;

	static tll_logger_impl_t stdio;
	tll_logger_impl_t * impl = &stdio;

	Logger * init(std::string_view name)
	{
		{
			rlock_t lck(lock);
			auto it = _loggers.find(name);
			if (it != _loggers.end())
				return it->second->ref();
		}

		auto l = new Logger;
		l->name = name;
		l->level = _default;
		l->impl.reset(impl_new_obj(l->name, impl));

		{
			rlock_t lck(lock);
			auto last = _levels.end();
			for (auto s : split<'.'>(name)) {
				auto full = std::string_view(name.begin(), s.end() - name.begin());
				auto li = _levels.find(full);
				if (li != _levels.end()) {
					l->level = li->second;
					last = li;
				} else if (!_levels_prefix.empty()) {
					li = _levels_prefix.upper_bound(full);
					if (li != _levels_prefix.begin()) {
						if (last == _levels.end() || (--li)->first.size() > last->first.size())
							l->level = (--li)->second;
					}
				}
			}
		}

		{
			wlock_t lck(lock);
			auto r = _loggers.emplace(l->name, l);
			if (!r.second) {
				delete l;
				return r.first->second->ref();
			}
		}

		return l;
	}

	void free(Logger * log)
	{
		//printf("Del logger %s %p\n", log->name.c_str(), log);
		if (log->refcnt() != 0) return;

		{
			wlock_t l(lock);
			auto it = _loggers.find(log->name);
			if (it != _loggers.end())
				_loggers.erase(it);
		}

		log->impl.reset(nullptr);
		delete log;
	}

	int set(std::string_view path, tll_logger_level_t level, bool subtree)
	{
		//fmt::print("Set level {}: {}\n", path, tll_logger_level_name(level));
		if (path == "" || path == "*") {
			_default = level;
			return 0;
		}

		auto prefix = (path.back() == '*');
		if (prefix) {
			path = path.substr(0, path.size() - 1);
			//fmt::print("Set new prefix mask: '{}' {}\n", path, level);
			subtree = true;
		}

		{
			wlock_t lck(lock);
			if (prefix)
				_levels_prefix[std::string(path)] = level;
			else
				_levels[std::string(path)] = level;
		}

		rlock_t lck(lock);
		if (subtree) {
			for (auto it = _loggers.lower_bound(path); it != _loggers.end() && it->first.substr(0, path.size()) == path; it++)
				it->second->level = level;
		} else {
			auto it = _loggers.find(path);
			if (it != _loggers.end())
				it->second->level = level;
		}

		return 0;
	}

	tll_logger_obj_t * impl_new_obj(std::string_view name, tll_logger_impl_t * impl)
	{
		auto r = new tll_logger_obj_t;
		r->name = name.data();
		r->impl = impl;
		if (impl->log_new)
			r->obj = (impl->log_new)(impl, name.data());
		return r;
	}

	int set_impl(tll_logger_impl_t *impl)
	{
		if (impl == nullptr)
			impl = &tll::logger::logger_context_t::stdio;

		std::list<tll::util::refptr_t<Logger>> loggers;

		tll_logger_impl_t * old = nullptr;
		{
			rlock_t lck(lock);
			old = this->impl;
			this->impl = impl;
			for (auto & i : _loggers)
				loggers.emplace_back(i.second);
		}

		for (auto & l : loggers) {
			tll::util::refptr_t<tll_logger_obj_t> ref(impl_new_obj(l->name, impl));

			{
				std::lock_guard<std::mutex> lck(l->lock);
				std::swap(l->impl, ref);
			}
		}
		if (old && old->release)
			(old->release)(old);
		return 0;
	}

	int configure(const tll::ConstConfig &cfg)
	{
		auto levels = cfg.sub("levels");
		if (levels) {
			for (auto & [k, v] : levels->browse("**")) {
				if (!v.value()) continue;
				auto level = tll::conv::to_any<tll_logger_level_t>(*v.get());
				if (level)
					set(k, *level, true);
			}
		}

		auto type = cfg.get("type");
		if (!type) {
#ifdef WITH_SPDLOG
		} else if (*type == "spdlog") {
			set_impl(spdlog_impl());
#endif
		}

		if (impl && impl->configure)
			return (impl->configure)(impl, cfg);
		// TODO: Reconfigure all loggers
		return 0;
	}

	static int stdio_log(long long ts, const char * category, tll_logger_level_t level, const char * data, size_t size, void * obj)
	{
		fmt::print(stderr, "{:<5}: {}: {}\n", tll_logger_level_name(level), category, std::string_view(data, size));
		return 0;
	}
} context;

tll_logger_impl_t logger_context_t::stdio = { logger_context_t::stdio_log };

void Logger::destroy()
{
	context.free(this);
}

} // namespace tll::logger

tll_logger_t * tll_logger_new(const char * name, int len)
{
	return tll::logger::context.init(tll::string_view_from_c(name, len));
}

tll_logger_t * tll_logger_copy(const tll_logger_t * log)
{
	if (!log) return nullptr;
	return const_cast<tll::logger::Logger *>(static_cast<const tll::logger::Logger *>(log))->ref();
}

void tll_logger_free(tll_logger_t * log)
{
	if (!log) return;
	static_cast<tll::logger::Logger *>(log)->unref();
}

int tll_logger_config(const struct tll_config_t * _cfg)
{
	if (!_cfg)
		return 0;
	return tll::logger::context.configure(tll::ConstConfig(_cfg));
}

int tll_logger_set(const char * name, int len, tll_logger_level_t level, int subtree)
{
	return tll::logger::context.set(tll::string_view_from_c(name, len), level, subtree);
}

int tll_logger_register(tll_logger_impl_t *impl)
{
	return tll::logger::context.set_impl(impl);
}

const char * tll_logger_name(const tll_logger_t * log)
{
	return static_cast<const tll::logger::Logger *>(log)->name.c_str();
}

int tll_logger_log(tll_logger_t * l, tll_logger_level_t level, const char * buf, size_t len)
{
	auto log = static_cast<tll::logger::Logger *>(l);
	if (log->level > level) return 0;

	std::unique_lock<std::mutex> lck(log->lock);
	auto impl = log->impl;
	lck.unlock();

	auto ts = tll::time::now();

	return (*impl->impl->log)(ts.time_since_epoch().count(), impl->name, level, buf, len, impl->obj);
}

tll_logger_buf_t * tll_logger_tls_buf()
{
	struct buf_t : public tll_logger_buf_t
	{
		buf_t() { data = nullptr; size = reserve = 0; }
		~buf_t() { if (data) free(data); data = nullptr; }
	};

	static thread_local buf_t buf;
	return &buf;
}

namespace {
int bufprintf(tll::logger::tls_buf_t * buf, const char * fmt, va_list va)
{
	if (buf->size() == 0)
		buf->resize(1024);

	va_list vac;
	va_copy(vac, va);

	auto r = vsnprintf(buf->data(), buf->size(), fmt, vac); // When buffer is small 'va' is consumed, use copy
	if (r >= (int) buf->size()) {
		buf->resize(r + 1);
		r = vsnprintf(buf->data(), buf->size(), fmt, va);
	}
	return r;
};
}

int tll_logger_printf(tll_logger_t * l, tll_logger_level_t level, const char * fmt, ...)
{
	if (!l || l->level > level) return 0;

	auto buf = static_cast<tll::logger::tls_buf_t *>(tll_logger_tls_buf());

	va_list va;
	va_start(va, fmt);
	auto r = bufprintf(buf, fmt, va);
	if (r < 0)
		return -1;

	return tll_logger_log(l, level, buf->data(), r);
}
