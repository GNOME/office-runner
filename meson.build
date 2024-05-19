# SPDX-FileCopyrightText: 2024 Emmanuele Bassi
# SPDX-License-Identifier: GPL-3.0-or-later

project('office-runner', 'c',
  version: '1.0.3',
  license: 'GPL-3.0-or-later',
  default_options: [
    'c_std=gnu99',
    'warning_level=2',
  ],
)

i18n = import('i18n')
gnome = import('gnome')

application_id = 'org.gnome.OfficeRunner'
gettext_package = meson.project_name()

or_bindir = get_option('bindir')
or_datadir = get_option('prefix') / get_option('datadir')
or_localedir = get_option('prefix') / get_option('datadir') / 'locale'
or_pkgdatadir = get_option('prefix') / get_option('datadir') / meson.project_name()

subdir('src')
subdir('po')
subdir('data')

gnome.post_install(
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)

summary({
    'prefix': get_option('prefix'),
    'bindir': or_bindir,
    'datadir': or_datadir,
    'localedir': or_localedir,
    'pkgdatadir': or_pkgdatadir,
  },
  section: 'Directories',
)
