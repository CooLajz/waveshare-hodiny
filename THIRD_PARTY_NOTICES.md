# Third-party notices

The MIT license in the repository root applies to the original project code.
The following components and assets retain their own licenses and copyright
notices.

## Runtime and build dependencies

| Component | Version used by the project | License | Source |
| --- | --- | --- | --- |
| Arduino core for ESP32 | 3.0.2 | LGPL-2.1 | https://github.com/espressif/arduino-esp32 |
| LVGL | 8.3.10 | MIT | https://github.com/lvgl/lvgl |
| Improv Wi-Fi C++ SDK | commit `17898613a1c17062ca5af295ceb639b16b4930bf` | Apache-2.0 | https://github.com/improv-wifi/sdk-cpp |
| ESP Web Tools | 10.4.0 | Apache-2.0 | https://github.com/esphome/esp-web-tools |

The Improv Wi-Fi sources embedded in `WaveshareHodiny/improv.cpp` and
`WaveshareHodiny/improv.h` are derived from the upstream C++ SDK identified
above.

The minified ESP Web Tools browser bundle used by the GitHub Pages installer is
self-hosted in `docs/vendor/esp-web-tools/`. Its upstream Apache-2.0 license is
preserved as `docs/vendor/esp-web-tools/LICENSE`.

## Fonts and icons

| Asset | License | Source |
| --- | --- | --- |
| Montserrat | SIL Open Font License 1.1 | https://github.com/JulietaUla/Montserrat |
| Liberation Sans | SIL Open Font License 1.1 | https://github.com/liberationfonts/liberation-fonts |
| Font Awesome Free | Icons: CC BY 4.0; fonts: SIL OFL 1.1; code: MIT | https://fontawesome.com/license/free |

Generated LVGL font data in `ClockCzechFont*.c` uses Montserrat glyphs.
`ClockTimeFont110.c` uses glyphs generated from Liberation Sans Bold 2.1.5.
The exact source font and its license are included in
`assets/fonts/liberation-sans/`.
`ClockIconsFont42.c` uses selected Font Awesome Free glyphs.

## Meteocons

The weather icons embedded in the firmware are derived from Meteocons
version `3.0.0-next.10` by Bas Milius.

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
