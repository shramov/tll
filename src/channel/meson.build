channel_deps = [meson.get_compiler('c').find_library('dl'), lz4]
channel_sources = files(
	[ 'impl.c'
	, 'context.cc'
	, 'ipc.cc'
	, 'file.cc'
	, 'framed.cc'
	, 'log.cc'
	, 'lz4.cc'
	, 'mem.cc'
	, 'pub.cc'
	, 'pub-client.cc'
	, 'serial.cc'
	, 'tcp.cc'
	, 'timeit.cc'
	, 'timer.cc'
	, 'udp.cc'
	, 'yaml.cc'
	, 'zero.cc'
	]
)

channel_sources += configure_file(output : 'channels.h', configuration : configuration_data({
}))
