extensions = ['common', 'channel', 'context', 'base', 'reopen']
foreach pyx : extensions
	pyx_so = python.extension_module( pyx
		, cython_gen.process(pyx + '.pyx')
		, c_args : cython_cflags
		, dependencies : [pydep, tll]
		, install: false
	)
endforeach
