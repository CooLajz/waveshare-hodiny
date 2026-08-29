# Third-party notices

The MIT license in the repository root applies to the original project code.
The following components and assets retain their own licenses and copyright
notices.

## Runtime and build dependencies

| Component | Version used by the project | License | Source |
| --- | --- | --- | --- |
| Arduino core for ESP32 | 3.0.7 | LGPL-2.1 | https://github.com/espressif/arduino-esp32 |
| LVGL | 8.3.10 | MIT | https://github.com/lvgl/lvgl |
| PNGdec | 1.0.1 | Apache-2.0 | https://github.com/bitbank2/PNGdec |
| Improv Wi-Fi C++ SDK | commit `17898613a1c17062ca5af295ceb639b16b4930bf` | Apache-2.0 | https://github.com/improv-wifi/sdk-cpp |
| ESP Web Tools | 10.4.0 | Apache-2.0 | https://github.com/esphome/esp-web-tools |

The Improv Wi-Fi sources embedded in `WaveshareHodiny/improv.cpp` and
`WaveshareHodiny/improv.h` are derived from the upstream C++ SDK identified
above.

The minified ESP Web Tools browser bundle used by the GitHub Pages installer is
self-hosted in `docs/vendor/esp-web-tools/`. Its upstream Apache-2.0 license is
preserved as `docs/vendor/esp-web-tools/LICENSE`.

## MeteoPlaneRadar by Chiptron.cz

Část implementace meteoradaru a mapových podkladů byla převzata a upravena z
open-source projektu MeteoPlaneRadar:

- autor: Petr / Chiptron.cz,
- zdroj: https://github.com/petus/MeteoPlaneRadar,
- web autora: https://chiptron.cz/,
- licence: MIT.

Copyright (c) 2026 Petr / chiptron.cz

Na převzaté a odvozené části se vztahují podmínky MIT licence. Její úplné
znění je součástí souboru `LICENSE` v kořeni tohoto repozitáře. Děkujeme
autorovi za zveřejnění zdrojového kódu a inspiraci pro integraci radaru ČHMÚ.

## Meteorologická data ČHMÚ

Meteoradar používá radarový kompozit MAX_Z poskytovaný Českým
hydrometeorologickým ústavem. Data nejsou součástí licence zdrojového kódu a
vyžadují uvedení zdroje.

Zdroj: https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png/

## Mapový podklad meteoradaru

Obrys České republiky vychází z dat Natural Earth 1:10m (public domain),
zjednodušených v projektu MeteoPlaneRadar. Souřadnice měst pocházejí z
GeoNames a podléhají licenci CC BY 4.0.

Zdroje: https://www.naturalearthdata.com/ · https://www.geonames.org/ ·
https://github.com/petus/MeteoPlaneRadar

## Fonts and icons

| Asset | License | Source |
| --- | --- | --- |
| Montserrat | SIL Open Font License 1.1 | https://github.com/JulietaUla/Montserrat |
| Barlow | SIL Open Font License 1.1 | https://github.com/jpt/barlow |
| Liberation Sans | SIL Open Font License 1.1 | https://github.com/liberationfonts/liberation-fonts |
| DSEG | SIL Open Font License 1.1 | https://github.com/keshikan/DSEG |
| Doto | SIL Open Font License 1.1 | https://github.com/google/fonts/tree/main/ofl/doto |
| Font Awesome Free | Icons: CC BY 4.0; fonts: SIL OFL 1.1; code: MIT | https://fontawesome.com/license/free |

Generated LVGL font data in `ClockCzechFont*.c` uses Montserrat glyphs.
The selectable clock fonts use glyphs generated from Barlow Bold 1.408,
Liberation Sans Bold 2.1.5, DSEG7 Modern Bold 0.46 and Doto Bold. The exact
source fonts and their licenses are included below `assets/fonts/`.
`ClockIconsFont42.c` uses selected Font Awesome Free glyphs.

## Meteocons

The weather icons embedded in the firmware are derived from Meteocons
version `3.0.0-next.10` by Bas Milius.

The 45 animated GIFs published in `docs/assets/weather-icons/` are generated
from the same upstream package. Their MIT license is also preserved next to
the published assets as `docs/assets/weather-icons/LICENSE.txt`.

Source: https://github.com/basmilius/weather-icons

MIT License

Copyright (c) 2020-2024 Bas Milius

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
