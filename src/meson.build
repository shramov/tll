sources = []
sources += ['config/config.cc', 'config/yaml.cc']
sources += ['scheme.cc']
sources += ['stat.cc']
sources += ['time.cc']
sources += ['ring.c']
sources += ['decimal128.cc']


subdir('logger')
subdir('channel')
subdir('processor')

sources += channel_sources
sources += processor_sources

sources += configure_file(output : 'scheme-config.h', configuration : configuration_data({
	'DATADIR': '"@0@"'.format(get_option('prefix') / get_option('datadir')),
	'WITH_RHASH': rhash.found(),
}))


if host_machine.system() in ['linux', 'freebsd']
  version_script = ['-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), 'ld.script')]
else
  version_script = []
endif

tll_lib = shared_library('tll',
                        sources,
                        include_directories : include,
                        dependencies : [fmt, rhash, threads, yaml, zlib] + channel_deps,
			version: '0', soversion: '0',
			link_with: tll_logger,
			link_args: version_script + cxx_fs_link_args,
			link_depends: 'ld.script',
                        install : true)

tll = declare_dependency(include_directories: include, link_with: tll_lib)

executable('tll-processor'
	, ['processor/main.cc']
	, dependencies: [fmt, threads, tll]
	, install: true
	)

pkg = import('pkgconfig')
pkg.generate(tll_lib, requires: 'fmt') # meson > 0.46

install_subdir('tll', install_dir : get_option('includedir'))

subdir('logic')
