# SPDX-FileCopyrightText: 2024 Emmanuele Bassi
# SPDX-License-Identifier: GPL-3.0-or-later
icons = [
  { 'size': 'scalable', 'icon': application_id + '.svg' },
  { 'size': 'symbolic', 'icon': application_id + '-symbolic.svg' },
]

hicolor_dir = or_datadir / 'icons' / 'hicolor'

foreach icon: icons
  size = icon.get('size')
  file = size / 'apps' / icon.get('icon')
  install_data(file, install_dir: hicolor_dir / size / 'apps')
endforeach
