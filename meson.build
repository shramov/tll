project('tll', 'c', 'cpp'
	, version: '0.2.1'
	, default_options : ['cpp_std=c++17', 'werror=true', 'optimization=2']
	, meson_version: '>= 0.53'
	)

cc = meson.get_compiler('c')
if cc.get_id() == 'gcc' and cc.version().version_compare('< 8.0')
	add_project_arguments('-Wno-unused-variable', language: 'cpp')
endif

if host_machine.system() == 'freebsd'
  # On FreeBSD 12.3 fmt can not format normal (non class) enums
  add_project_arguments('-DFMT_DEPRECATED=', language: 'cpp')
endif

cxx_fs_link_args = []
cxx = meson.get_compiler('cpp')
if not cxx.has_header('filesystem') or (cxx.get_id() == 'gcc' and cxx.version().version_compare('< 9.0'))
	cxx_fs_link_args = ['-lstdc++fs']
endif

zlib = dependency('zlib')
lz4 = dependency('liblz4', version: '>=1.9')
yaml = dependency('yaml-0.1')
fmt = dependency('fmt')
spdlog = dependency('spdlog', version: '>=1.0', required: false)
rapidjson = dependency('RapidJSON', required: false)
rhash = dependency('librhash', required: false)
threads = dependency('threads')

include = include_directories('src')

rst2man = find_program('rst2man', disabler: true, required: false)

subdir('src')
subdir('python')

gtest = dependency('gtest', required: get_option('with_gtest'), disabler: true)
if not get_option('with_gtest').disabled() and gtest.found()
	subdir('test')
endif

subdir('doc')
subdir('bench')
