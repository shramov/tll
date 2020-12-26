cython = find_program('cython3', 'cython') # On FreeBSD it's cython, not cython3
python = import('python').find_installation('python3')
pydep = python.dependency()

cython_gen = generator(cython, output: '@BASENAME@.c', arguments: ['@INPUT@', '@EXTRA_ARGS@', '-o', '@OUTPUT@']) #, '-I', meson.current_source_dir()])
cython_gen_api = generator(cython, output: '@BASENAME@_api.h', arguments: ['@INPUT@', '@EXTRA_ARGS@', '-o', '@BUILD_DIR@/@BASENAME@.c'])

# Replace with python.dependency(embed: true) on meson >= 0.53
if python.language_version().version_compare('< 3.8')
	pyembed = dependency('python3', required: true)
else
	pyembed = dependency('python3-embed', required: true)
endif

shared_library('tll-channel-python'
	, ['src/channel.c', cython_gen_api.process('tll/channel/context.pyx')]
	, dependencies : [tll, pyembed, meson.get_compiler('c').find_library('dl')]
	, install: true
	)
