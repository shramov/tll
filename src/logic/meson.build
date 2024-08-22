logic_scheme = files(
	[ 'control.yaml'
	, 'stat.yaml'
	, 'resolve.yaml'
	, 'quantile.yaml'
	])

install_data(logic_scheme, install_dir: get_option('datadir') / 'tll/scheme/tll/logic')

shared_library('tll-logic-forward'
	, ['forward.cc']
	, dependencies : [fmt, tll]
	, install: true
	)

shared_library('tll-logic-stat'
	, ['stat.cc', 'quantile.cc']
	, dependencies : [fmt, tll]
	, install: true
	)

if rapidjson.found()
  shared_library('tll-logic-control'
                , ['control.cc']
                , dependencies : [fmt, tll, rapidjson]
                , install: true
                )
endif

shared_library('tll-logic-rtt'
	, ['rtt.cc']
	, dependencies : [fmt, tll]
	, install: true
	)

mansources = [
	'forward.rst',
	'rtt.rst',
	'stat.rst',
	'quantile.rst',
	]

foreach f : mansources
	custom_target('logic-man-@0@'.format(f)
		, input: f
		, output : 'tll-logic-@BASENAME@.7'
		, command : [rst2man, '@INPUT@', '@OUTPUT@']
		, install_dir: get_option('mandir') / 'man7'
		, install: true
	)
endforeach
