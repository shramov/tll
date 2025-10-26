processor_sources = files(
	[ 'api.cc'
	, 'chains.cc'
	, 'deps.cc'
	, 'loop.cc'
	, 'processor.cc'
	, 'worker.cc'
	]
)

custom_target('channel-man-processor'
        , input: 'processor.rst'
        , output : 'tll-@BASENAME@.1'
        , command : [rst2man, '@INPUT@', '@OUTPUT@']
        , install_dir: get_option('mandir') / 'man1'
        , install: true
)
