cython = find_program('cython3', 'cython') # On FreeBSD it's cython, not cython3
python = import('python').find_installation('python3')
pydep = python.dependency()

cython_gen = generator(cython, output: '@BASENAME@.c', arguments: ['@INPUT@', '@EXTRA_ARGS@', '-o', '@OUTPUT@']) #, '-I', meson.current_source_dir()])
cython_gen_api = generator(cython, output: '@BASENAME@_api.h', arguments: ['@INPUT@', '@EXTRA_ARGS@', '-o', '@BUILD_DIR@/@BASENAME@.c'])

pyembed = python.dependency(embed: true)
cython_cflags = []

if python.language_version().version_compare('== 3.8')
  cython_cflags += ['-Wno-deprecated-declarations']
endif

shared_library('tll-python'
	, ['src/channel.c', cython_gen_api.process('tll/channel/context.pyx')]
	, dependencies : [tll, pyembed, meson.get_compiler('c').find_library('dl')]
	, install: true
	)

if get_option('with_cython_build')
	subdir('tll')
	subdir('tll/channel')
	subdir('tll/processor')

	'''
	test('python-test', python
		, args: ['-m', 'pytest', '-vx']
		, workdir: meson.current_source_dir()
		, suite: 'slow'
	)
	'''
endif
