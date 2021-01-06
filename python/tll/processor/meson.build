extensions = ['loop', 'processor']
foreach pyx : extensions
	pyx_so = python.extension_module( pyx
		, cython_gen.process(pyx + '.pyx')
		, dependencies : [pydep, tll]
		, install: false
	)
endforeach

