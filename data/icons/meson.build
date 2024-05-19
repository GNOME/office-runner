# SPDX-FileCopyrightText: 2024 Emmanuele Bassi
# SPDX-License-Identifier: GPL-3.0-or-later
sizes = [
  '16x16',
  '22x22',
  '24x24',
  '32x32',
  '48x48',
  '256x256',
]

icon_png = application_id + '.png'
hicolor_dir = or_datadir / 'icons' / 'hicolor'

foreach size: sizes
  install_data(size / icon_png, install_dir: hicolor_dir / size / 'apps')
endforeach
