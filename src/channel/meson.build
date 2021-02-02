channel_deps = [meson.get_compiler('c').find_library('dl')]
channel_sources = files(
	[ 'impl.c'
	, 'context.cc'
	, 'ipc.cc'
	, 'log.cc'
	, 'mem.cc'
	, 'serial.cc'
	, 'tcp.cc'
	, 'timer.cc'
	, 'zero.cc'
	]
)

if curl.found()
	channel_deps += [curl]
	channel_sources += files('curl.cc')
endif

channel_sources += configure_file(output : 'channels.h', configuration : configuration_data({
	'WITH_CURL': curl.found(),
}))
