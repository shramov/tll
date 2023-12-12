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
	, 'mem.cc'
	, 'pub.cc'
	, 'pub-client.cc'
	, 'rate.cc'
	, 'resolve.cc'
	, 'rotate.cc'
	, 'serial.cc'
	, 'stream-client.cc'
	, 'stream-server.cc'
	, 'tcp.cc'
	, 'timeit.cc'
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

mansources = \
	[ 'tcp.rst'
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
