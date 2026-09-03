test_cpp_args = []
if host_machine.system() == 'macos' and gtest.version().version_compare('< 1.18')
  # Workaround gtest implicit conversion bug https://github.com/google/googletest/issues/4762
  test_cpp_args = ['-Wno-character-conversion']
endif

generated128 = custom_target('generated.h',
    output : 'test_generated128.h',
    input : '../python/test/decimal128.yaml',
    command : [python, meson.current_source_dir() / 'generate128.py', '@INPUT@', '@OUTPUT@'],
)

test('test-util', executable('test-util',
	sources: ['test_main.cc', 'test_conv.cc', 'test_util.cc', 'test_ring.cc', 'test_expected.cc', '../src/ring.c', '../src/time.cc', 'test_decimal128.cc', generated128],
	include_directories: include,
	override_options: ['cpp_std=c++2a'],
	dependencies: [gtest, fmt, zlib],
	link_args: cxx_fs_link_args,
        cpp_args: test_cpp_args,
	)
)

test_lib_sources = files('test_main.cc', 'test_config.cc', 'test_logger.cc', 'test_scheme.cc', 'test_scheme_binder.cc', 'test_stat.cc')
if keyutils.found()
  test_lib_sources += 'test_keyring.cc'
endif

test('test-lib', executable('test-lib',
	sources: test_lib_sources,
	cpp_args: ['-DSCHEME_PATH="@0@"'.format(meson.current_source_dir() / 'test_scheme.yaml')] + test_cpp_args,
	dependencies: [gtest, fmt, tll],
	),
	env: ['TLL_SCHEME_PATH=@0@'.format(meson.current_source_dir() / 'scheme')],
)

test('test-channel', executable('test-channel',
	sources: ['test_main.cc', 'test_channel.cc'],
	dependencies: [gtest, fmt, tll],
	link_args: cxx_fs_link_args,
        cpp_args: test_cpp_args,
	)
)

test('test-processor', executable('test-processor',
	sources: ['test_main.cc', 'test_processor.cc'],
	dependencies: [gtest, fmt, tll],
	link_args: cxx_fs_link_args,
        cpp_args: test_cpp_args,
	),
        workdir: meson.current_source_dir() / '..', # For keyring tests
)
