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