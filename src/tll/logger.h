/*
 * Copyright (c) 2018-2019 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_LOGGER_H
#define _TLL_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

#include <stddef.h>

struct tll_config_t;

typedef enum
{
	TLL_LOGGER_TRACE = 0,
	TLL_LOGGER_DEBUG,
	TLL_LOGGER_INFO,
	TLL_LOGGER_WARNING,
	TLL_LOGGER_ERROR,
	TLL_LOGGER_CRITICAL,
} tll_logger_level_t;

static inline const char * tll_logger_level_name(tll_logger_level_t level)
{
	switch (level) {
	case TLL_LOGGER_TRACE: return "TRACE";
	case TLL_LOGGER_DEBUG: return "DEBUG";
	case TLL_LOGGER_INFO:  return "INFO";
	case TLL_LOGGER_WARNING: return "WARN";
	case TLL_LOGGER_ERROR: return "ERROR";
	case TLL_LOGGER_CRITICAL: return "CRIT";
	}
	return "UNKNOWN";
}

typedef struct {
	tll_logger_level_t level;
} tll_logger_t;

/**
 * Create new logger with name ``name``. If ``len`` is non-negative name can be not NULL terminated.
 * If logger with this name already exists same pointer is obtained (with increased reference count).
 */
tll_logger_t * tll_logger_new(const char * name, int len);

//< Free logger object
void tll_logger_free(tll_logger_t * log);

int tll_logger_config(struct tll_config_t * cfg);

/**
 * Set logging level for specified path
 * If name is empty ("") then default logging level is changed.
 * If name is not empty then new config entry is added.
 * ``subtree`` controls which active loggers are updated with new level:
 * - 0 (false): only logger with exact name
 * - !0 (true): whole subtree with logger names starting with ``name`` is updated
 */
int tll_logger_set(const char * name, int len, tll_logger_level_t level, int subtree);

//< Get logger name
const char * tll_logger_name(const tll_logger_t * log);

//< Log message
int tll_logger_log(tll_logger_t * log, tll_logger_level_t lvl, const char * buf, size_t size);

typedef struct tll_logger_impl_t {
	int (*log)(long long ts, const char * category, tll_logger_level_t level, const char * data, size_t size, void * obj);
	void * (*log_new)(const char * category, struct tll_logger_impl_t * impl);
	void (*log_free)(const char * category, void * obj, struct tll_logger_impl_t * impl);
	void * user;
} tll_logger_impl_t;

int tll_logger_register(tll_logger_impl_t *);

typedef struct {
	char * data;
	size_t size;
	size_t reserve;
} tll_logger_buf_t;

tll_logger_buf_t * tll_logger_tls_buf(void);

#ifdef __cplusplus
} // extern "C"
#endif//__cplusplus

#ifdef __cplusplus

#include <string_view>
#include <fmt/format.h>

namespace tll {

class logger_buf_t : public tll_logger_buf_t
{
 public:
	typedef char value_type;
	typedef char & reference;
	typedef char * pointer;
	typedef const char * const_pointer;
	typedef pointer iterator;
	typedef const_pointer const_iterator;

	size_t size() const { return tll_logger_buf_t::size; }

 	void resize(size_t s)
	{
		if (s > reserve) {
			tll_logger_buf_t::data = (char *) realloc(tll_logger_buf_t::data, s);
			reserve = s;
		}
		tll_logger_buf_t::size = s;
	}

	void push_back(const value_type &v)
	{
		resize(size() + 1);
		data()[size() - 1] = v;
	}

	char * data() { return tll_logger_buf_t::data; }
	const char * data() const { return tll_logger_buf_t::data; }

	iterator begin() { return data(); }
	const_iterator begin() const { return data(); }

	iterator end() { return begin() + size(); }
	const_iterator end() const { return begin() + size(); }
};

class Logger
{
protected:
	tll_logger_t * _log = nullptr;

	logger_buf_t * tls_buf() const { return static_cast<logger_buf_t *>(tll_logger_tls_buf()); }

public:
	Logger(const Logger &rhs) : _log(tll_logger_new(rhs.name(), -1)) {}
	Logger(std::string_view n) : _log(tll_logger_new(n.data(), n.size())) {}
	~Logger() { tll_logger_free(_log); _log = 0; }

	Logger & operator = (Logger rhs)
	{
		std::swap(_log, rhs._log);
		return *this;
	}

	using level_t = tll_logger_level_t;
	static auto constexpr Trace = TLL_LOGGER_TRACE;
	static auto constexpr Debug = TLL_LOGGER_DEBUG;
	static auto constexpr Info = TLL_LOGGER_INFO;
	static auto constexpr Warning = TLL_LOGGER_WARNING;
	static auto constexpr Error = TLL_LOGGER_ERROR;
	static auto constexpr Critical = TLL_LOGGER_CRITICAL;

	static int config(tll_config_t * cfg)
	{
		return tll_logger_config(cfg);
	}

	static int set(std::string_view name, level_t level, bool subtree = false)
	{
		return tll_logger_set(name.data(), name.size(), level, subtree);
	}

	static std::string_view level_name(level_t level)
	{
		return tll_logger_level_name(level);
	}

	const char * name() const { return tll_logger_name(_log); }
	level_t & level() { return _log->level; }
	level_t level() const { return _log->level; }

	template <typename Fmt, typename... Args>
	void log(level_t level, Fmt format, Args && ... args) const
	{
		auto buf = tls_buf();
		if (_log->level > level) return;
		try {
			buf->resize(0);
			fmt::format_to(std::back_inserter(*buf), format, std::forward<Args>(args)...);
		} catch (fmt::format_error &e) {
			buf->resize(0);
			fmt::format_to(std::back_inserter(*buf), "Invalid format {}: {}", format, e.what());
		}
		buf->push_back('\0');
		tll_logger_log(_log, level, buf->data(), buf->size() - 1);
	}

#define DECLARE_LOG(func, level) \
	template <typename Fmt, typename... Args> \
	void func(Fmt format, Args && ... args) const { return log(level, format, std::forward<Args>(args)...); }

	DECLARE_LOG(trace, Trace)
	DECLARE_LOG(debug, Debug)
	DECLARE_LOG(info, Info)
	DECLARE_LOG(warning, Warning)
	DECLARE_LOG(error, Error)
	DECLARE_LOG(critical, Critical)

	template <typename R, typename Fmt, typename... Args>
	R fail(R err, Fmt format, Args && ... args) const
	{
		error(format, std::forward<Args>(args)...);
		return err;
	}
};

} // namespace tll

#endif//__cplusplus

#endif//_TLL_LOGGER_H
