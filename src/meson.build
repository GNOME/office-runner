# SPDX-FileCopyrightText: 2024 Emmanuele Bassi
# SPDX-License-Identifier: GPL-3.0-or-later

config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', gettext_package)
configure_file(output: 'config.h', configuration: config_h)

cc = meson.get_compiler('c')

executable('office-runner',
  sources: ['office-runner.c'],
  dependencies: [
    dependency('gtk+-3.0', version: '>=3.8'),
    cc.find_library('m', required: false),
  ],
  c_args: [
    '-DGNOMELOCALEDIR="@0@"'.format(or_localedir),
    '-DPKGDATADIR="@0@"'.format(or_pkgdatadir),
    '-DAPPLICATION_ID="@0@"'.format(application_id),
  ],
  install: true,
  install_dir: or_bindir,
)

desktop_file = i18n.merge_file(
  input: 'org.gnome.OfficeRunner.desktop.in',
  output: '@0@.desktop'.format(application_id),
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: or_datadir / 'applications',
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('validate-desktop',
    desktop_utils,
    args: [desktop_file],
    suite: ['lint'],
  )
endif

appstream_file = i18n.merge_file(
  input: 'org.gnome.OfficeRunner.appdata.xml.in',
  output: '@0@.appdata.xml'.format(application_id),
  po_dir: '../po',
  install: true,
  install_dir: or_datadir / 'metainfo',
)

install_data([
    'office-runner.ui',
    'jump.wav',
    'pause.wav',
  ],
  install_dir: or_pkgdatadir,
)
