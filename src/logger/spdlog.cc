#include "tll/logger.h"
#include "tll/logger/impl.h"
#include "tll/config.h"
#include "tll/util/size.h"

#include "logger/spdlog.h"
#include "logger/util.h"

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

struct sink_t
{
	std::string prefix;
	tll_logger_level_t level = tll::Logger::Debug;
	std::unique_ptr<spdlog::sinks::sink> sink;

	bool match(std::string_view name, tll_logger_level_t l)
	{
		if (level > l)
			return false; 
		if (prefix.size() == 0)
			return true;
		if (name.size() < prefix.size())
			return false;
		if (name.substr(0, prefix.size()) != prefix)
			return false;
		if (name.size() > prefix.size() && name[prefix.size()] != '.')
			return false;
		return true;
	}
};

struct spdlog_impl_t : public tll_logger_impl_t
{
	std::vector<sink_t> sinks;

	static constexpr std::string_view default_format = "%^%Y-%m-%d %H:%M:%S.%e %l %n%$: %v";

	spdlog_impl_t() {
		auto impl = (tll_logger_impl_t *) this;
		*impl = {};
		impl->log = _log;
		impl->log_new = [](tll_logger_impl_t * impl, const char * category) { return static_cast<spdlog_impl_t *>(impl)->log_new(category); };
		impl->log_free = [](tll_logger_impl_t * impl, const char * category, void * obj) { return static_cast<spdlog_impl_t *>(impl)->log_free(category, obj); };
		impl->configure = [](tll_logger_impl_t * impl, const tll_config_t * cfg) { return static_cast<spdlog_impl_t *>(impl)->configure(cfg); };
		impl->release = [](tll_logger_impl_t * impl) { return static_cast<spdlog_impl_t *>(impl)->release(); };
	}

	static int _log(long long ns, const char * category, tll_logger_level_t level, const char * data, size_t size, void * obj)
	{
		using namespace std::chrono;
		using namespace spdlog;

		log_clock::time_point ts(duration_cast<log_clock::time_point::duration>(nanoseconds(ns)));

		auto self = static_cast<spdlog_impl_t *>(obj);
		std::string_view name(category);
		spdlog::details::log_msg msg(ts, {}, name, tll2spdlog(level), std::string_view(data, size));
		for (auto & s : self->sinks)
			if (s.match(name, level))
				s.sink->log(msg);
		return 0;
	}

	int configure(const tll_config_t * _cfg)
	{
		tll::ConstConfig cfg(_cfg);
		tll::Logger log("tll.logger.spdlog");

		if (sinks.empty()) {
			sink_t sink;
			sink.sink.reset(new spdlog::sinks::ansicolor_stderr_sink_mt);
			sink.sink->set_pattern(std::string(default_format));
			sinks.push_back(std::move(sink));
		}

		std::string format { cfg.get("format").value_or(default_format) };

		std::vector<sink_t> result;

		for (auto & [_, c] : cfg.browse("spdlog.sinks.*", true)) {
			sink_t sink;

			auto type = c.get("type");
			if (!type) continue;

			auto reader = tll::PropsReaderT(c);

			auto ls = c.get("level");
			if (ls && ls->size()) {
				auto level = level_from_str(*ls);
				if (!level)
					return log.fail(EINVAL, "Invalid level name for sink {}: {}", *type, *ls);
				sink.level = *level;
			}

			sink.prefix = reader.getT<std::string>("prefix", "");

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
					sink.sink.reset(new spdlog::sinks::daily_file_sink_mt(filename, hour, minute, truncate, max_files));
				} else if (*type == "rotating-file") {
					auto filename = reader.getT<std::string>("filename");
					auto max_size = reader.getT<tll::util::Size>("max-size", 64 * 1024 * 1024);
					auto max_files = reader.getT("max-files", 5u);
					auto rotate_on_open = reader.getT("rotate-on-open", false);
					if (!reader)
						return log.fail(EINVAL, "Invalid parameters for sink {}: {}", *type, reader.error());
					sink.sink.reset(new spdlog::sinks::rotating_file_sink_mt(filename, max_size, max_files, rotate_on_open));
				} else if (*type == "syslog") {
					auto ident = reader.getT<std::string>("ident", "");
					if (!reader)
						return log.fail(EINVAL, "Invalid parameters for sink {}: {}", *type, reader.error());
					sink.sink.reset(new spdlog::sinks::syslog_sink_mt(ident, LOG_PID, 0, false));
				} else {
					log.error("Unknown sink type {}", *type);
					continue;
				}
			} catch (std::exception &e) {
				return log.fail(EINVAL, "Failed to create sink {}: {}", *type, e.what());
			}

			sink.sink->set_pattern(reader.getT("format", format));
			result.push_back(std::move(sink));
		}

		std::swap(sinks, result);
		return 0;
	}


	void * log_new(const char * category)
	{
		return this;
	}

	void log_free(const char * category, void * obj)
	{
	}

	void release()
	{
		sinks.clear();
	}
};


tll_logger_impl_t * spdlog_impl()
{
	static spdlog_impl_t impl;
	return &impl;
}
