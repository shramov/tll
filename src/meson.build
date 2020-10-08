sources = []
sources += ['config/config.cc', 'config/yaml.cc']
sources += ['scheme.cc']
sources += ['logger/logger.cc']
sources += ['processor/loop.cc']
sources += ['stat.cc']
sources += ['ring.c']

subdir('channel')

sources += channel_sources

if host_machine.system() == 'linux'
  version_script = '-Wl,--version-script,@0@/@1@'.format(meson.current_source_dir(), 'ld.script')
else
  version_script = ''
endif

tll_lib = shared_library('tll',
                        sources,
                        include_directories : include,
                        dependencies : [fmt, pthread, yaml, zlib] + channel_deps,
			version: '0', soversion: '0',
			link_args: version_script,
			link_depends: 'ld.script',
                        install : true)

tll = declare_dependency(include_directories: include, link_with: tll_lib)

pkg = import('pkgconfig')
pkg.generate(tll_lib) # meson > 0.46

install_subdir('tll', install_dir : get_option('includedir'))
