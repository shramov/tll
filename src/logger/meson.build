logger_sources = files(['logger.cc'])
logger_deps = []

if spdlog.found()
	logger_sources += files(['spdlog.cc'])
	logger_deps += [spdlog]
endif

logger_sources += configure_file(output : 'config.h', configuration : configuration_data({
	'WITH_SPDLOG': spdlog.found(),
}))

tll_logger = static_library('tll-logger',
	logger_sources,
	include_directories: include,
	dependencies: [fmt, spdlog, threads],
	install: false,
)
