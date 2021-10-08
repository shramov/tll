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
	, 'udp.cc'
	, 'yaml.cc'
	, 'zero.cc'
	]
)

channel_sources += configure_file(output : 'channels.h', configuration : configuration_data({
}))
