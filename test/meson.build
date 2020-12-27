fs_link_args = []
cxx = meson.get_compiler('cpp')
if not cxx.has_header('filesystem') or (cxx.get_id() == 'gcc' and cxx.version().version_compare('< 9.0'))
	fs_link_args = ['-lstdc++fs']
	#add_project_link_arguments('-lstdc++fs', language: 'cpp')
endif

test('test-util', executable('test-util',
	sources: ['test_main.cc', 'test_conv.cc', 'test_util.cc', 'test_ring.cc', '../src/ring.c', '../src/time.cc'],
	include_directories: include,
	dependencies: [gtest, fmt, zlib],
	)
)

test('test-lib', executable('test-lib',
	sources: ['test_main.cc', 'test_config.cc', 'test_logger.cc', 'test_scheme.cc', 'test_stat.cc'],
	cpp_args: '-DSCHEME_PATH="@0@"'.format(meson.current_source_dir() / 'test_scheme.yaml'),
	dependencies: [gtest, fmt, tll],
	)
)

test('test-channel', executable('test-channel',
	sources: ['test_main.cc', 'test_channel.cc'],
	dependencies: [gtest, fmt, tll],
	link_args: fs_link_args,
	)
)

test('test-processor', executable('test-processor',
	sources: ['test_main.cc', 'test_processor.cc'],
	dependencies: [gtest, fmt, tll],
	link_args: fs_link_args,
	)
)
