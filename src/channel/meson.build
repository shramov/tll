channel_deps = [meson.get_compiler('c').find_library('dl'), lz4, rapidjson]
channel_sources = files(
	[ 'impl.c'
	, 'blocks.cc'
	, 'context.cc'
	, 'direct.cc'
	, 'ipc.cc'
	, 'file.cc'
	, 'framed.cc'
	, 'log.cc'
	, 'lz4.cc'
	, 'lz4block.cc'
	, 'mem.cc'
	, 'pub.cc'
	, 'pub-client.cc'
	, 'pub-mem.cc'
	, 'rate.cc'
	, 'reopen.cc'
	, 'resolve.cc'
	, 'rotate.cc'
	, 'serial.cc'
	, 'stream-client.cc'
	, 'stream-server.cc'
	, 'tcp.cc'
	, 'timeit.cc'
	, 'timeline.cc'
	, 'timer.cc'
	, 'udp.cc'
	, 'yaml.cc'
	, 'zero.cc'
	]
)

channel_sources += configure_file(output : 'channels.h', configuration : configuration_data({
	'WITH_RAPIDJSON': rapidjson.found()
}))

if rapidjson.found()
	channel_sources += files(['json.cc'])
endif

mansources = [
	'blocks.rst',
	'common.rst',
	'direct.rst',
	'file.rst',
	'ipc.rst',
	'lz4.rst',
	'mem.rst',
	'null.rst',
	'pub-mem.rst',
	'pub-tcp.rst',
	'random.rst',
	'rate.rst',
	'rotate.rst',
	'serial.rst',
	'stream-server.rst',
	'stream-client.rst',
	'tcp.rst',
	'timeit.rst',
	'timeline.rst',
	'timer.rst',
	'udp.rst',
	'yaml.rst',
	'zero.rst',
	]

foreach f : mansources
	custom_target('channel-man-@0@'.format(f)
		, input: f
		, output : 'tll-channel-@BASENAME@.7'
		, command : [rst2man, '@INPUT@', '@OUTPUT@']
		, install_dir: get_option('mandir') / 'man7'
		, install: true
	)
endforeach
