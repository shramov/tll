executable('bench-time', sources: ['time.cc'], dependencies: [fmt, tll])
executable('tll-bench-channel', sources: ['channel.cc'], dependencies: [fmt, tll], install: true)
executable('bench-binder', sources: ['binder.cc'], dependencies: [fmt], include_directories: include)
executable('bench-callback', sources: ['callback.cc'], dependencies: [fmt, tll])
executable('bench-callback-standalone', sources: ['callback-standalone.cc'])
executable('bench-ring', sources: ['ring.cc', '../src/ring.c'], dependencies: [fmt], include_directories: include)
executable('bench-refcount', sources: ['refcount.cc'], dependencies: [fmt, threads], include_directories: include)
executable('bench-stat', sources: ['stat.cc'], dependencies: [fmt, threads], include_directories: include)

custom_target('tll-bench-channel.1'
	, output : 'tll-bench-channel.1'
	, input : 'channel.rst'
	, command : [rst2man, '@INPUT@', '@OUTPUT@']
	, install_dir: get_option('mandir') / 'man1'
	, install: true
)
