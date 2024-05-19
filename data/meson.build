# SPDX-FileCopyrightText: 2024 Emmanuele Bassi
# SPDX-License-Identifier: GPL-3.0-or-later

subdir('ui')
subdir('sounds')
subdir('icons')

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