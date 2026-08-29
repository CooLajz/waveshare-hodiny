# Oznámení o komponentách třetích stran

Na původní kód projektu se vztahuje licence MIT uvedená v kořeni repozitáře.
Následující komponenty a podklady si zachovávají vlastní licence a autorská
oznámení.

## Běhové a sestavovací závislosti

| Komponenta | Verze použitá v projektu | Licence | Zdroj |
| --- | --- | --- | --- |
| Arduino core pro ESP32 | 3.0.7 | LGPL-2.1 | https://github.com/espressif/arduino-esp32 |
| LVGL | 8.3.10 | MIT | https://github.com/lvgl/lvgl |
| PNGdec | 1.0.1 | Apache-2.0 | https://github.com/bitbank2/PNGdec |
| Improv Wi-Fi C++ SDK | revize `17898613a1c17062ca5af295ceb639b16b4930bf` | Apache-2.0 | https://github.com/improv-wifi/sdk-cpp |
| ESP Web Tools | 10.4.0 | Apache-2.0 | https://github.com/esphome/esp-web-tools |

Zdrojové soubory Improv Wi-Fi vložené v `WaveshareHodiny/improv.cpp` a
`WaveshareHodiny/improv.h` jsou odvozené z výše uvedeného původního C++ SDK.

Minifikovaný prohlížečový balíček ESP Web Tools používaný instalátorem na
GitHub Pages je hostovaný přímo v `docs/vendor/esp-web-tools/`. Jeho původní
licence Apache-2.0 je zachována v `docs/vendor/esp-web-tools/LICENSE`.

## MeteoPlaneRadar od Chiptron.cz

Část implementace meteoradaru a mapových podkladů byla převzata a upravena z
projektu MeteoPlaneRadar s otevřeným zdrojovým kódem:

- autor: Petr / Chiptron.cz,
- zdroj: https://github.com/petus/MeteoPlaneRadar,
- web autora: https://chiptron.cz/,
- licence: MIT.

Copyright (c) 2026 Petr / chiptron.cz

Na převzaté a odvozené části se vztahují podmínky MIT licence. Její úplné
znění je součástí souboru `LICENSE` v kořeni tohoto repozitáře. Děkuji
autorovi za zveřejnění zdrojového kódu a inspiraci pro integraci radaru ČHMÚ.

## Meteorologická data ČHMÚ

Meteoradar používá radarový kompozit MAX_Z poskytovaný Českým
hydrometeorologickým ústavem. Data nejsou součástí licence zdrojového kódu a
vyžadují uvedení zdroje.

Zdroj: https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png/

## Mapový podklad meteoradaru

Obrys České republiky vychází z volně použitelných dat Natural Earth 1:10m,
zjednodušených v projektu MeteoPlaneRadar. Souřadnice měst pocházejí z
GeoNames a podléhají licenci CC BY 4.0.

Zdroje: https://www.naturalearthdata.com/ · https://www.geonames.org/ ·
https://github.com/petus/MeteoPlaneRadar

## Písma a ikony

| Podklad | Licence | Zdroj |
| --- | --- | --- |
| Montserrat | SIL Open Font License 1.1 | https://github.com/JulietaUla/Montserrat |
| Barlow | SIL Open Font License 1.1 | https://github.com/jpt/barlow |
| Liberation Sans | SIL Open Font License 1.1 | https://github.com/liberationfonts/liberation-fonts |
| DSEG | SIL Open Font License 1.1 | https://github.com/keshikan/DSEG |
| Doto | SIL Open Font License 1.1 | https://github.com/google/fonts/tree/main/ofl/doto |
| Font Awesome Free | ikony: CC BY 4.0; písma: SIL OFL 1.1; kód: MIT | https://fontawesome.com/license/free |

Vygenerovaná data písem LVGL v `ClockCzechFont*.c` používají znaky písma
Montserrat. Volitelná písma hodin používají znaky vygenerované z Barlow Bold
1.408, Liberation Sans Bold 2.1.5, DSEG7 Modern Bold 0.46 a Doto Bold. Přesné
zdrojové soubory písem a jejich licence jsou uložené v `assets/fonts/`.
`ClockIconsFont42.c` používá vybrané znaky Font Awesome Free.

## Meteocons

Ikony počasí vložené ve firmware jsou odvozené z Meteocons ve verzi
`3.0.0-next.10` od Base Miliuse.

Všech 45 animovaných obrázků GIF publikovaných v
`docs/assets/weather-icons/` je vygenerováno ze stejného původního balíčku.
Jejich licence MIT je zachována také vedle publikovaných podkladů v
`docs/assets/weather-icons/LICENSE.txt`.

Zdroj: https://github.com/basmilius/weather-icons

### Původní znění licence MIT

Následující právně závazné znění licence je záměrně ponecháno v původní
angličtině:

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
