rst2pdf = find_program('rst2pdf', disabler: true, required: false)
rst2html = find_program('rst2html', disabler: true, required: false)

sources = ['logger.rst', 'stat.rst', 'scheme.rst', 'channel.rst', 'processor.rst', 'channels.rst', 'tll.rst']

pdf = []
foreach s : ['tll.rst', 'channels.rst']
	pdf += custom_target(s + '.pdf'
		, output : '@BASENAME@.pdf'
		, input : s
		, command : [rst2pdf, '@INPUT@', '-o', '@OUTPUT@']
		, install_dir : get_option('datadir') / 'doc'
		, install : true
	)
endforeach

html = []
foreach s : sources
	html += custom_target(s + '.html'
		, output : '@BASENAME@.html'
		, input : s
		, command : [rst2html, '--stylesheet', meson.current_source_dir() / 'tll.css', '@INPUT@', '@OUTPUT@']
		, install_dir : get_option('datadir') / 'doc'
		, install : true
	)
endforeach

#rst2pdf = generator(find_program('rst2pdf'), output: '@BASENAME@.pdf', arguments: ['@INPUT@', '-o', '@OUTPUT@'])
#rst2html = generator(find_program('rst2html'), output: '@BASENAME@.html', arguments: ['@INPUT@', '@OUTPUT@'])
#install_data(rst2pdf.process(sources), install_dir : get_option('datadir') / 'doc')
#install_data(rst2html.process(sources), install_dir : get_option('datadir') / 'doc')
#alias_target('docs', pdf, html)
