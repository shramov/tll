ver = meson.project_version().split('.')
version_sources = configure_file(
  input: '../version.h.in',
  output : 'version.h',
  install_dir : get_option('includedir') / 'tll',
  configuration : configuration_data({
    'MAJOR': ver.get(0),
    'MINOR': ver.get(1),
    'PATCH': ver.get(2).split('-').get(0),
    'STRING': meson.project_version(),
}))
