python = import('python').find_installation('python3')

generated128 = custom_target('generated.h',
    output : 'test_generated128.h',
    input : '../python/test/decimal128.yaml',
    command : [python, meson.current_source_dir() / 'generate128.py', '@INPUT@', '@OUTPUT@'],
)

test('test-util', executable('test-util',
	sources: ['test_main.cc', 'test_conv.cc', 'test_util.cc', 'test_ring.cc', '../src/ring.c', '../src/time.cc', 'test_decimal128.cc', generated128],
	include_directories: include,
	dependencies: [gtest, fmt, zlib],
	link_args: cxx_fs_link_args,
	)
)

test('test-lib', executable('test-lib',
	sources: ['test_main.cc', 'test_config.cc', 'test_logger.cc', 'test_scheme.cc', 'test_stat.cc'],
	cpp_args: '-DSCHEME_PATH="@0@"'.format(meson.current_source_dir() / 'test_scheme.yaml'),
	dependencies: [gtest, fmt, tll],
	),
	env: ['TLL_SCHEME_PATH=@0@'.format(meson.current_source_dir() / 'scheme')],
)

test('test-channel', executable('test-channel',
	sources: ['test_main.cc', 'test_channel.cc'],
	dependencies: [gtest, fmt, tll],
	link_args: cxx_fs_link_args,
	)
)

test('test-processor', executable('test-processor',
	sources: ['test_main.cc', 'test_processor.cc'],
	dependencies: [gtest, fmt, tll],
	link_args: cxx_fs_link_args,
	)
)
