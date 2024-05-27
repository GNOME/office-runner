# SPDX-FileCopyrightText: 2024 Emmanuele Bassi
# SPDX-License-Identifier: GPL-3.0-or-later
sizes = [
  'scalable',
  'symbolic'
]

icon_svg = application_id + '.svg'
hicolor_dir = or_datadir / 'icons' / 'hicolor'

foreach size: sizes
  install_data(size / icon_svg, install_dir: hicolor_dir / size / 'apps')
endforeach
