# Převod Meteocons pro LVGL

Tento projekt používá animované Meteocons jako GIFy o rozměru přesně
84 × 84 px. Firmware obsahuje pouze metadata a mapování ikon, které skutečně
používá. Veřejný release proto publikuje jen uzavřenou sadu uvedenou ve
firmwarovém allowlistu, nikoli kompletní pracovní mirror knihovny.

Statický fallback je samostatná vestavěná sada 15 ikon ve stylu Monochrome.
Používá stejné podmínky a stejné mapování jako animace, ale vzniká výhradně
z `svg-static/monochrome`. Animované SVG se nesmí použít jako zdroj statického
snímku.

## Zásadní pravidla

- Zdroj musí být oficiální balíček `@meteocons/svg` ve stejné verzi, jakou
  používá web [meteocons.com](https://meteocons.com/icons/). Nepoužívat starý
  balíček `@meteocons/lottie` ani převod přes Lottie JSON.
- SVG rasterizovat přímo v cílovém rozlišení **84 × 84 px**. Nikdy nevytvářet
  mezisnímky 128 × 128 px a následně je nezmenšovat; druhé převzorkování
  poškozuje hrany animovaných SVG masek a na displeji vznikají průniky paprsků
  přes mrak.
- Statické SVG se rovněž rasterizuje přímo do **84 × 84 px** v Chromiu.
  Výsledné PNG se již nezmenšuje a generátor vestavěných LVGL dat odmítne
  vstup s jinými rozměry.
- Animace se vzorkuje v Chromiu přes SVG/SMIL. Pro každý z 90 snímků se SVG
  pozastaví a nastaví se přesný čas pomocí `SVGSVGElement.setCurrentTime()`.
- Monochrome se převádí na bílou s alfa kanálem, aby jej firmware mohl
  přebarvit. Flat a Line si ponechávají zdrojové barvy.
- Každá publikovaná verze assetů je neměnná. Oprava vždy dostane nový suffix
  `lvgl.N`; existující verze se nepřepisuje.
- Každá zveřejněná sada musí obsahovat pouze soubory používané danou verzí
  firmware a ke každému souboru pravdivou velikost a SHA-256.

## Závislosti

- Python 3 s Pillow,
- Node.js,
- Playwright a lokální Google Chrome,
- `zip` a `curl` pro lokální kontrolu výsledného balíčku.

Používané projektové nástroje:

- `tools/render_meteocons_svg.mjs` — přesné vykreslení SVG/SMIL,
- `tools/render_meteocons_static.mjs` — přímé vykreslení `svg-static` do
  84 × 84 px; odmítne vstup obsahující SVG animační elementy,
- `tools/build_meteocons_assets.py` — vytvoření GIFů a úplného manifestu,
- `tools/split_meteocons_asset_package.py` — rozdělení podle stylu,
- `tools/generate_weather_animation_metadata.py` — metadata používaných ikon
  pro firmware,
- `tools/prepare_public_weather_assets.py` — výběr pouze firmwarového
  allowlistu, úplná kontrola velikostí a SHA-256 a příprava GitHub Pages.

## 1. Ověření a stažení zdroje

Nejprve na webu nebo z HTML konkrétní ikony ověř aktuální CDN verzi. Následně
ověř, že stejná verze existuje v oficiálním npm balíčku:

```sh
npm view @meteocons/svg versions --json
npm view @meteocons/svg@<VERZE> dist.tarball version --json
```

Stáhni tarball z hodnoty `dist.tarball`. Po rozbalení musí styly
`monochrome`, `flat` a `line` obsahovat stejnou množinu SVG. Skript tuto
podmínku kontroluje automaticky.

Pro vestavěný Monochrome fallback použij přesně tyto versionované zdroje:

```text
https://cdn.meteocons.com/3.0.0-next.10/svg-static/monochrome/<ikona>.svg
```

Uzavřená sada ikon je `clear-day`, `clear-night`, `mostly-clear-day`,
`mostly-clear-night`, `partly-cloudy-day`, `partly-cloudy-night`,
`overcast-day`, `overcast-night`, `overcast`, `drizzle`, `rain`, `sleet`,
`snow`, `mist` a `thunderstorms`. Každý soubor vykresli
`tools/render_meteocons_static.mjs` přímo na 84 × 84 px a výsledný PNG předej
`tools/generate_openweather_icons.swift`. Tento generátor nepřijímá SVG ani
PNG jiné velikosti, aby nemohlo dojít k druhému zmenšení.

## 2. Povinný vizuální vzorek

Před kompletním během převeď ve všech třech stylech alespoň
`partly-cloudy-day`. Tato ikona současně testuje:

- SVG masku mraku,
- rotaci paprsků,
- společný pohyb masky a mraku,
- zachování barev Flat a Line,
- přebarvitelný Monochrome.

Zkontroluj animaci, nikoli jen první snímek. Paprsky nesmějí procházet plochou
mraku ani mizet v odkryté části. Teprve po tomto ověření spusť celou dávku.

## 3. Sestavení kompletní knihovny

Příklad pro verzi `3.0.0-next.10` a interní revizi `lvgl.2`:

```sh
python3 tools/build_meteocons_assets.py \
  --package /tmp/meteocons-svg-3.0.0-next.10.tgz \
  --playwright /absolutni/cesta/k/playwright/index.mjs \
  --license /tmp/meteocons/package/LICENSE \
  --output /tmp/meteocons-3.0.0-next.10-lvgl.2 \
  --asset-version 3.0.0-next.10-lvgl.2 \
  --base-url https://example.invalid/releases \
  --workers 8
```

Skript musí skončit počtem `počet ikon × 3` GIFů. Pro verzi
`3.0.0-next.10` je očekáváno 519 ikon na styl, tedy 1557 GIFů.

## 4. Rozdělení a kontrola

```sh
python3 tools/split_meteocons_asset_package.py \
  --package /tmp/meteocons-3.0.0-next.10-lvgl.2/package \
  --output /tmp/meteocons-3.0.0-next.10-lvgl.2/upload \
  --version 3.0.0-next.10-lvgl.2
```

Každý styl zabal samostatně tak, aby `asset-manifest.json`, `LICENSE.txt` a
používané GIFy byly přímo v kořeni ZIPu. Z kompletní pracovní knihovny vyber
jen klíče uvedené ve `WeatherAnimationService.cpp`. Před zveřejněním proveď
`unzip -t` a u všech souborů porovnej velikost a SHA-256 s allowlistem. Nestačí
ověřit pouze počet položek nebo několik vzorků.

Veřejný Pages adresář připrav přímo z ověřeného kompletního balíčku:

```sh
python3 tools/prepare_public_weather_assets.py \
  --package /tmp/meteocons-3.0.0-next.10-lvgl.2/package
```

Skript odmítne chybějící nebo pozměněný GIF a publikuje právě 45 používaných
GIFů spolu s per-style manifesty a upstream MIT licencí.

## 5. Přepojení firmwaru

1. V `tools/generate_weather_animation_metadata.py` nastav nový prefix verze
   a předej veřejnou adresu parametrem `--base-url`.
2. V `WaveshareHodiny/WeatherAnimationService.cpp` změň samostatné verze pro
   Monochrome, Flat a Line.
3. Přegeneruj allowlist používaných ikon a beze změny přenes velikosti a
   SHA-256 do `ASSETS`.
4. Ověř, že největší používaný GIF nepřekračuje `MAX_ASSET_SIZE`.
5. Spusť `git diff --check`, `./build.sh` a nahraj vývojový build přes
   `./upload.sh <port>`.

## 6. Fyzické přijetí změny

Build ani náhled GIFu v počítači nejsou konečný důkaz. Na připojeném displeji
ověř:

- Monochrome, Flat i Line,
- přechod mezi statickou a animovanou ikonou,
- Partly Cloudy v několika fázích celé animace,
- denní i noční režim,
- změnu barvy pouze u Monochrome,
- žádný bílý čtverec, zbytky předchozího snímku ani porušenou masku.

Za opravenou považuj knihovnu až po optickém potvrzení na fyzickém displeji.

## Ověřená referenční konfigurace

- Upstream: `@meteocons/svg` `3.0.0-next.10`
- Asset revize: `3.0.0-next.10-lvgl.2`
- Počet: 519 ikon × 3 styly = 1557 GIFů
- Rozměr: 84 × 84 px
- Časování: 90 snímků, 70 ms na snímek, nekonečná smyčka
- Fyzicky ověřeno: Partly Cloudy je opticky výrazně lepší a maska mraku je
  správná.
