project('tll', 'c', 'cpp'
	, version: '0.0.0'
	, default_options : ['cpp_std=c++17', 'werror=true', 'optimization=2']
	, meson_version: '>= 0.49'
	)

cc = meson.get_compiler('c')
if cc.get_id() == 'gcc' and cc.version().version_compare('< 8.0')
	add_project_arguments('-Wno-unused-variable', language: 'cpp')
endif

zlib = dependency('zlib')
yaml = dependency('yaml-0.1')
fmt = dependency('fmt')
pthread = cc.find_library('pthread', required: false)

include = include_directories('src')

subdir('src')

gtest = dependency('gtest', required: get_option('with_gtest'), disabler: true)
if not get_option('with_gtest').disabled() and gtest.found()
	subdir('test')
endif
