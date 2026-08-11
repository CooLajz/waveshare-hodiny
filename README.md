# Waveshare Hodiny

Konfigurovatelný dashboard pro kulatý displej Waveshare ESP32-S3-Touch-LCD-2.1
s rozlišením 480 × 480 px.

Arduino sketch a všechny projektové zdrojové soubory jsou v adresáři
`WaveshareHodiny/`. Hlavním vstupem je `WaveshareHodiny.ino`.

Zobrazuje čas, datum, venkovní a pokojovou teplotu, stav počasí, dvě volitelné
veličiny a stav připojení k Home Assistantu. Nedostupné hodnoty se zobrazují
jako `--`.

Čas a datum se po připojení k Wi-Fi synchronizují přes NTP. Firmware používá
české časové pásmo včetně automatického přechodu mezi zimním a letním časem.
Veřejný release neobsahuje žádné Wi-Fi ani Home Assistant přihlašovací údaje.
Wi-Fi se po instalaci nastavuje přes Improv Serial a Home Assistant přes webovou
konfiguraci. Lokální vývojový profil může volitelně načíst hodnoty ze souboru
`.env`, který je celý ignorovaný Gitem.

## Webová konfigurace

Po připojení k Wi-Fi je konfigurace dostupná na
`http://waveshare-hodiny.local/` nebo na IP adrese zařízení. Web zatím není
chráněný heslem. Umožňuje nastavit:

- adresu Home Assistantu a long-lived access token,
- nezávislý název, teplotní entitu, ikonu a barvu levé i pravé místnosti;
  dynamickou ikonu počasí lze zvolit na jedné nebo na obou stranách,
- globální entity počasí a slunce a vlastní barvu hodin i data,
- dynamickou barevnou škálu až s deseti body pro měřenou hodnotu A i B;
  mezi body se barvy plynule prolínají,
- dvě konfigurovatelná pole s předvolbou veličiny nebo vlastním názvem,
  jednotkou a vždy volitelným počtem desetinných míst,
- denní a noční jas a automatické přepínání podle entity slunce.

Z webu lze zařízení také bezpečně restartovat bez vymazání uloženého nastavení.
Sekce Firmware zobrazuje aktuální a serverovou verzi, umožňuje ruční kontrolu
a potvrzenou instalaci nové verze. Automatickou aktualizaci lze vypnout; ve
výchozím stavu release firmware jednou denně po 4:10 zkontroluje nakonfigurovaný
release server a dostupnou novější verzi nainstaluje.

Webový server běží deset minut po startu zařízení. Úspěšné uložení na webu nebo
v nastavení přímo na hodinách obnoví celý desetiminutový interval. Pokud už web
neběží, otevření nastavení na hodinách jej znovu spustí. Aktivní web signalizuje
ikonu webu ve vycentrované skupině stavových ikon na dashboardu.

Token se po uložení už do webové stránky neposílá. Lze jej pouze nahradit novou
hodnotou. Nastavení je uložené v samostatném NVS oddílu `clockcfg`, takže přežije
běžné nahrání nové aplikace. Vymazání celé flash nebo nahrání úplného 16MB
factory obrazu smaže i konfiguraci.

Test připojení znovu použije uložený token pouze pro stejnou uloženou adresu
Home Assistantu. Při změně adresy je nutné zadat také nový token; uložený token
se na jiný server nikdy neodešle.

Vývojový build nadále používá hodnoty z lokálního `.env` jako výchozí hodnoty,
pokud odpovídající položka ještě není uložená. Hodnota tokenu z `.env` se při
uložení jiné konfigurace automaticky nezapíše do NVS.

Dlouhý stisk dashboardu otevře nastavení denního a nočního jasu. Posuvníky
podsvícení průběžně náhledově mění a tlačítko `ULOŽIT` nastavení trvale uloží
do interní paměti ESP32. Přepínač `AUTOMATICKY DEN/NOC` řídí režim podle
`sun.sun`; při vypnuté automatice se režim přepíná krátkým dotykem dashboardu.
Pod IP adresou se zobrazuje aktuální verze firmware. Otevření nastavení spustí
kontrolu release serveru; pokud je dostupná novější verze, údaj o firmware se
zbarví červeně, jinak zůstává ve stejné tlumené barvě jako IP adresa.

## Sestavení a nahrání vývojové verze

Ověřený lokální toolchain používá Arduino ESP32 core `3.0.2` a LVGL `8.3.10`.
Závislosti lze přes Arduino CLI nainstalovat například takto:

```sh
arduino-cli core install esp32:esp32@3.0.2 --config-file arduino-cli.yaml
arduino-cli lib install lvgl@8.3.10 --config-file arduino-cli.yaml
```

Bez lokálního `.env` se projekt stále sestaví; výsledný development build pouze
nemá přednastavenou Wi-Fi. Pro běžnou instalaci s nastavením Wi-Fi přes Improv
Serial použij release build.

```sh
./build.sh
./upload.sh
```

Vývojové artefakty se exportují odděleně do
`build/waveshare-hodiny-develop/`. Upload používá odpovídající interní build
adresář a neprohledává společný kořen `build/`, takže jej neovlivní staré nebo
jinak pojmenované binární soubory.

Vývojový profil se nadále nahrává pouze přes USB, používá lokální `.env`, běží
na 921600 baud a zachovává příkazy pro screenshot i přepínání obrazovek. OTA
instalace je v něm úmyslně zakázaná, aby serverový release nenahradil pracovní
vývojovou verzi.

## OTA a release build

Flash je rozdělená na dva 6MiB aplikační sloty `app0` a `app1`. Aktualizace se
stahuje přes HTTPS do neaktivního slotu, kontroluje deklarovanou velikost a
SHA-256 a teprve potom aktivuje nový obraz. Samostatné oddíly `clockcfg` a NVS
s Wi-Fi údaji zůstávají při běžné OTA aktualizaci zachované.

Automatické OTA aktualizace jsou po čisté instalaci vypnuté. Uživatel je může
zapnout ve webovém nastavení; uložená volba je součástí konfigurace v oddílu
`clockcfg` a přežije běžné nahrání aplikace i OTA aktualizaci.

Lokální release build bez publikování lze vytvořit s explicitní SemVer:

```sh
./build-release.sh 1.2.0
```

Výstupní balíček obsahuje instalační části s přesnými offsety pro ESP Web Tools
a samostatný aplikační `.ota.bin` pro aktualizaci běžícího zařízení. Release
neobsahuje lokální Wi-Fi ani Home Assistant údaje; první Wi-Fi se nastaví přes
Improv Serial a uloží do NVS.

OTA zdroj se do release buildu předává pouze lokální ignorovanou konfigurací.
Bez nakonfigurovaného zdroje firmware žádný obraz nestáhne ani nezapíše.

Volitelně lze předat jiný sériový port:

```sh
./upload.sh /dev/cu.usbmodemXXXXXXXX
```

Na macOS lze sestavení i nahrání provést dvojklikem na soubor
`flash-latest.command`. Skript vždy sestaví aktuální zdrojový kód, automaticky
vybere jediný připojený displej a nahraje do něj výsledný firmware.

## Screenshot přes USB

Firmware přijímá přes sériový port příkaz `SCREENSHOT` a odešle aktuální
RGB565 framebuffer. Pomocný skript data převede na kruhové PNG 480 × 480 px:

```sh
./capture-screenshot.sh
```

Výsledek se uloží jako `screenshots/latest.png`. Skript potřebuje pyserial 3.5.
Lokální Arduino CLI může použít ignorovanou konfiguraci
`WaveshareHodiny/local/arduino-cli.yaml`. Bez ní build používá přenositelnou
projektovou konfiguraci `arduino-cli.yaml` a standardní adresáře Arduino CLI.

Přesný a fyzicky ověřený postup převodu kompletní knihovny animovaných
Meteocons pro LVGL je v dokumentu
[`METEOCONS_ASSET_PIPELINE.md`](METEOCONS_ASSET_PIPELINE.md).

Obrazovku nastavení lze pro kontrolu vyfotit bez ručního dlouhého stisku:

```sh
./capture-screenshot.sh --settings --output screenshots/settings.png
```
