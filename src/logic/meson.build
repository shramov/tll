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

shared_library('tll-logic-control'
	, ['control.cc']
	, dependencies : [fmt, tll, rapidjson]
	, install: true
	)
