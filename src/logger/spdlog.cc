#include "tll/logger.h"
#include "tll/logger/impl.h"
#include "tll/config.h"
#include "tll/util/size.h"
#include "tll/util/refptr.h"

#include "logger/spdlog.h"
#include "logger/util.h"

#include <cassert>

#define SPDLOG_HEADER_ONLY
#undef SPDLOG_COMPILED_LIB
#ifndef SPDLOG_FMT_EXTERNAL
# define SPDLOG_FMT_EXTERNAL
#endif

#define SPDLOG_LEVEL_NAMES { "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL", "OFF" }

#include <spdlog/sinks/sink.h>
#include <spdlog/spdlog.h>

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/syslog_sink.h>

namespace {
spdlog::level::level_enum tll2spdlog(tll_logger_level_t l)
{
	switch (l) {
	case tll::logger::Trace: return spdlog::level::trace;
	case tll::logger::Debug: return spdlog::level::debug;
	case tll::logger::Info: return spdlog::level::info;
	case tll::logger::Warning: return spdlog::level::warn;
	case tll::logger::Error: return spdlog::level::err;
	case tll::logger::Critical: return spdlog::level::critical;
	}
	return spdlog::level::debug;
}
}

#if SPDLOG_VERSION > 10500

static inline uint64_t _gettid()
{
#ifdef __APPLE__
	uint64_t tid = 0;
	pthread_threadid_np(nullptr, &tid);
	return tid;
#elif defined(__FreeBSD__)
	return pthread_getthreadid_np();
#else
	return gettid();
#endif
}

struct thread_name_flag final : public spdlog::custom_flag_formatter
{
	static thread_local std::string _name;

	static std::string_view name()
	{
		if (_name.size() != 0)
			return _name;

#ifndef __APPLE__
		char buf[256];
		if (pthread_getname_np(pthread_self(), buf, sizeof(buf)) == 0) {
			_name = buf;
			return _name;
		}
#endif
		_name = tll::conv::to_string(_gettid());
		return _name;
	}

	static void reset(const char * name) {
		if (name == nullptr)
			_name.clear();
		else
			_name = name;
	}

	void format(const spdlog::details::log_msg &, const std::tm &, spdlog::memory_buf_t &dest) override
	{
		auto v = name();
		dest.append(v.data(), v.data() + v.size());
	}

	std::unique_ptr<custom_flag_formatter> clone() const override
	{
		return spdlog::details::make_unique<thread_name_flag>();
	}
};

thread_local std::string thread_name_flag::_name;
#endif

void spdlog_thread_name_set(const char * name)
{
#if SPDLOG_VERSION > 10500
	thread_name_flag::reset(name);
#endif
}

struct sink_t
{
	spdlog::level::level_enum level = tll2spdlog(tll::Logger::Trace);
	spdlog::level::level_enum flush_level = tll2spdlog(tll::Logger::Info);
	std::unique_ptr<spdlog::sinks::sink> sink;

	static constexpr std::string_view default_format = "%^%Y-%m-%d %H:%M:%S.%e %l %n%$: %v";

	static sink_t make_default(std::string_view format = "")
	{
		if (format.empty())
			format = default_format;
		sink_t sink;
		sink.sink.reset(new spdlog::sinks::ansicolor_stderr_sink_mt);
		sink.set_pattern(format);
		return sink;
	}

	void set_pattern(std::string_view format)
	{
#if SPDLOG_VERSION > 10500
		auto formatter = std::make_unique<spdlog::pattern_formatter>();
		formatter->add_flag<thread_name_flag>('t').set_pattern(std::string(format));
		sink->set_formatter(std::move(formatter));
#else
		sink->set_pattern(std::string(format));
#endif
	}
};

struct node_t : public tll::util::refbase_t<node_t, 0>
{
	node_t * parent = nullptr;
	std::vector<tll::util::refptr_t<node_t>> children;
	std::vector<sink_t> sinks;

	std::string prefix;
	bool additivity = false;

	node_t * find(std::string_view name)
	{
		auto tail = name.substr(prefix.size());
		if (tail.size() == 0)
			return this;
		for (auto &c : children) {
			auto ctail = c->prefix.substr(prefix.size());
			if (ctail.size() == 0) continue;
			const bool wildcard = ctail.back() == '*';
			if (wildcard)
				ctail = ctail.substr(0, ctail.size() - 1);
			if (tail.size() < ctail.size()) continue;
			if (tail.substr(0, ctail.size()) != ctail) continue;
			if (!wildcard && tail.size() > ctail.size() && tail[ctail.size()] != '.') continue;
			return c->find(name);
		}
		return this;
	}

	void finalize()
	{
		std::sort(children.begin(), children.end(), [](auto &l, auto &r) { return l->prefix > r->prefix; });
		for (auto & c: children) {
			c->parent = this;
			c->finalize();
		}
	}

	void log(spdlog::details::log_msg &msg)
	{
		for (auto & s : sinks) {
			if (s.level <= msg.level) {
				s.sink->log(msg);
				if (s.flush_level <= msg.level)
					s.sink->flush();
			}
		}
		if (additivity && parent)
			return parent->log(msg);
	}
};

struct spdlog_impl_t : public tll_logger_impl_t
{
	std::vector<sink_t> sinks;
	tll::util::refptr_t<node_t> root = { nullptr };

	spdlog_impl_t() {
		auto impl = (tll_logger_impl_t *) this;
		*impl = {};
		impl->log = _log;
		impl->log_new = [](tll_logger_impl_t * impl, const char * category) { return static_cast<spdlog_impl_t *>(impl)->log_new(category); };
		impl->log_free = [](tll_logger_impl_t * impl, const char * category, void * obj) { return static_cast<spdlog_impl_t *>(impl)->log_free(category, obj); };
		impl->configure = [](tll_logger_impl_t * impl, const tll_config_t * cfg) { return static_cast<spdlog_impl_t *>(impl)->configure(cfg); };
		//impl->init = [](tll_logger_impl_t * impl) { return static_cast<spdlog_impl_t *>(impl)->init(); };
		impl->release = [](tll_logger_impl_t * impl) { return static_cast<spdlog_impl_t *>(impl)->release(); };
		init();
	}

	static int _log(long long ns, const char * category, tll_logger_level_t level, const char * data, size_t size, void * obj)
	{
		using namespace std::chrono;
		using namespace spdlog;

		log_clock::time_point ts(duration_cast<log_clock::time_point::duration>(nanoseconds(ns)));

		auto node = static_cast<node_t *>(obj);
		std::string_view name(category);
#if SPDLOG_VERSION >= 10800
		spdlog::details::log_msg msg(ts, spdlog::source_loc {}, name, tll2spdlog(level), std::string_view(data, size));
#elif SPDLOG_VERSION >= 10500
		spdlog::details::log_msg msg(spdlog::source_loc {}, name, tll2spdlog(level), spdlog::string_view_t(data, size));
#else
		std::string name_str(name);
		spdlog::details::log_msg msg(spdlog::source_loc {}, &name_str, tll2spdlog(level), spdlog::string_view_t(data, size));
#endif
		node->log(msg);
		return 0;
	}

	int configure(const tll_config_t * _cfg)
	{
		init();

		tll::ConstConfig cfg(_cfg);
		tll::Logger log("tll.logger.spdlog");

		std::string format { cfg.get("format").value_or(sink_t::default_format) };

		tll::util::refptr_t<node_t> result = new node_t();
		//std::vector<sink_t> result;

		for (auto & [_, c] : cfg.browse("spdlog.sinks.*", true)) {
			sink_t sink;

			auto type = c.get("type");
			if (!type) continue;

			auto reader = tll::make_props_reader(tll::make_props_chain(c, cfg.sub("spdlog.defaults." + std::string(*type))));

			sink.level = tll2spdlog(reader.getT("level", tll::Logger::Trace));
			sink.flush_level = tll2spdlog(reader.getT("flush-level", tll::Logger::Info));

			try {
				if (*type == "console") {
					sink.sink.reset(new spdlog::sinks::ansicolor_stderr_sink_mt);
				} else if (*type == "file") {
					auto filename = reader.getT<std::string>("filename");
					auto truncate = reader.getT("truncate", false);
					if (!reader)
						return log.fail(EINVAL, "Invalid parameters for sink {}: {}", *type, reader.error());
					sink.sink.reset(new spdlog::sinks::basic_file_sink_mt(filename, truncate));
				} else if (*type == "daily-file") {
					auto filename = reader.getT<std::string>("filename");
					auto hour = reader.getT<>("rotate-hour", 0u);
					auto minute = reader.getT<>("rotate-minute", 0u);
					auto truncate = reader.getT("truncate", false);
					auto max_files = reader.getT("max-files", 5u);
					if (!reader)
						return log.fail(EINVAL, "Invalid parameters for sink {}: {}", *type, reader.error());
#if SPDLOG_VERSION < 10500
					sink.sink.reset(new spdlog::sinks::daily_file_sink_mt(filename, hour, minute, truncate));
					(void) max_files;
#else
					sink.sink.reset(new spdlog::sinks::daily_file_sink_mt(filename, hour, minute, truncate, max_files));
#endif
				} else if (*type == "rotating-file") {
					auto filename = reader.getT<std::string>("filename");
					auto max_size = reader.getT<tll::util::Size>("max-size", 64 * 1024 * 1024);
					auto max_files = reader.getT("max-files", 5u);
					auto rotate_on_open = reader.getT("rotate-on-open", false);
					if (!reader)
						return log.fail(EINVAL, "Invalid parameters for sink {}: {}", *type, reader.error());
#if SPDLOG_VERSION < 10500
					sink.sink.reset(new spdlog::sinks::rotating_file_sink_mt(filename, max_size, max_files));
					(void) rotate_on_open;
#else
					sink.sink.reset(new spdlog::sinks::rotating_file_sink_mt(filename, max_size, max_files, rotate_on_open));
#endif
				} else if (*type == "syslog") {
					auto ident = reader.getT<std::string>("ident", "");
					if (!reader)
						return log.fail(EINVAL, "Invalid parameters for sink {}: {}", *type, reader.error());
#if SPDLOG_VERSION < 10500
					sink.sink.reset(new spdlog::sinks::syslog_sink_mt(ident, LOG_PID, 0));
#else
					sink.sink.reset(new spdlog::sinks::syslog_sink_mt(ident, LOG_PID, 0, false));
#endif
				} else {
					log.error("Unknown sink type {}", *type);
					continue;
				}
			} catch (std::exception &e) {
				return log.fail(EINVAL, "Failed to create sink {}: {}", *type, e.what());
			}

			sink.set_pattern(reader.getT("format", format));

			auto prefix = reader.getT<std::string>("prefix", "");
			auto additivity = reader.getT("additivity", false);

			if (!reader)
				return log.fail(EINVAL, "Invalid parameters for spdlog sink {}: {}", *type, reader.error());

			auto node = result->find(prefix);
			if (node->prefix != prefix) {
				auto child = new node_t();
				child->prefix = prefix;
				child->additivity = additivity;
				for (auto & c : node->children) {
					if (!c) continue;
					if (c->prefix.size() <= prefix.size()) continue;
					if (c->prefix.substr(0, prefix.size()) != prefix) continue;
					if (c->prefix[prefix.size()] != '.') continue;
					child->children.push_back(c.get());
					c.reset(nullptr);
				}
				auto end = std::remove_if(node->children.begin(), node->children.end(), [](auto c) { return !c; });
				node->children.resize(end - node->children.begin());
				node->children.push_back(child);
				node = child;
			}
			node->sinks.push_back(std::move(sink));
		}

		result->finalize();

		if (!result->sinks.size())
			result->sinks.push_back(sink_t::make_default(format));

		std::swap(root, result);
		return 0;
	}


	void * log_new(const char * category)
	{
		auto node = root->find(category);
		node->ref();
		return node;
	}

	void log_free(const char * category, void * obj)
	{
		auto node = static_cast<node_t *>(obj);
		node->unref();
	}

	void init()
	{
		auto node = new node_t();
		node->sinks.push_back(sink_t::make_default());
		root.reset(node);
	}

	void release()
	{
		root.reset(nullptr);
	}
};


tll_logger_impl_t * spdlog_impl()
{
	static spdlog_impl_t impl;
	return &impl;
}
