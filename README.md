# Waveshare Hodiny

🇬🇧 **[English documentation](README.en.md)**

Český informační dashboard pro kulatý dotykový displej
[Waveshare ESP32-S3-Touch-LCD-2.1](https://www.waveshare.com/esp32-s3-touch-lcd-2.1.htm)
s rozlišením 480 × 480 px. Zobrazuje čas, datum, počasí, teploty, další
měřené hodnoty a srážkový radar ČHMÚ. Jako zdroj hodnot lze použít Open-Meteo
bez účtu nebo Home Assistant. Vzhled, zdroje dat, poloha, radar, jas, animace
i aktualizace se nastavují z webového rozhraní bez úpravy zdrojového kódu.

<p align="center">
  <a href="https://coolajz.github.io/waveshare-hodiny/">
    <img src="https://img.shields.io/badge/Nainstalovat_firmware_z_prohl%C3%AD%C5%BEe%C4%8De-00BBD4?style=for-the-badge&amp;logo=googlechrome&amp;logoColor=white" alt="Nainstalovat firmware z prohlížeče" height="46">
  </a>
</p>

<p align="center">
  <strong>Jednoduchá instalace přes USB bez stahování souborů.</strong><br>
  Otevřete instalační stránku v desktopovém Chromu nebo Edge, připojte displej a pokračujte podle průvodce.
</p>

<p align="center">
  <img src="screenshots/dashboard.png" alt="Hlavní obrazovka Waveshare Hodiny v denním režimu" width="46%">
  <img src="screenshots/dashboard-night.png" alt="Hlavní obrazovka Waveshare Hodiny v červeném nočním režimu" width="46%">
</p>

<p align="center">
  <img src="screenshots/dashboard-analog.png" alt="Analogový ciferník Waveshare Hodiny v denním režimu" width="30%">
  <img src="screenshots/dashboard.png" alt="Digitální ciferník Waveshare Hodiny v denním režimu" width="30%">
  <img src="screenshots/dashboard-radar.png" alt="Meteoradar ČHMÚ na displeji Waveshare Hodiny" width="30%">
</p>

V denním režimu mají místnosti a hodnoty vlastní barvy. Volitelný červený
noční vzhled sjednotí dashboard i meteoradar do odstínů červené a sníží jas,
aby displej v noci nerušil. Vedle klasického digitálního rozložení lze zvolit
také analogový ciferník s vlastním barevným tónem a volitelnými akcenty na
pozicích 12, 3, 6 a 9 hodin.

## Co firmware umí

- digitální hodiny s fonty Barlow, Liberation Sans, LCD DSEG nebo Doto, nebo
  analogový ciferník s nastavitelným tónem a volitelnými hlavními akcenty,
- české nebo anglické datum v několika formátech a volitelný vteřinový prstenec,
- synchronizaci času přes NTP a české časové pásmo včetně letního času,
- dvě místnosti s vlastním názvem, teplotou, ikonou a barvou,
- animované i statické ikony počasí založené na Meteocons,
- srážkový radar ČHMÚ s mapou České republiky, městy a 1 až 15 snímky,
- rozsahy 25, 50, 100 a 200 km nebo celou ČR ovládané svislým gestem swipe,
- červenou noční paletu radaru se zachováním rozlišení intenzity srážek,
- volitelné automatické střídání hodin a radaru se samostatnou dobou zobrazení,
- dvě další měřené veličiny, například CO₂, VOC, vlhkost, tlak nebo baterii,
- vlastní jednotky, počet desetinných míst a plynulé barevné škály,
- denní a noční jas s ručním přepínáním nebo automatikou podle Open-Meteo či entity slunce,
- tři efekty vteřin: klasické tečky, plynulou čáru a kometu,
- webovou konfiguraci s volitelným heslem, export a import zálohy a bezpečný restart,
- samostatnou živou diagnostiku hardwaru, paměti, sítě, Home Assistantu a radaru,
- prvotní nastavení Wi-Fi přes Improv Serial,
- A/B OTA aktualizace se zachováním Wi-Fi a konfigurace,
- ovládací API pro Home Assistant chráněné náhodným secretem,
- základní nastavení také přímo na dotykovém displeji.

## Potřebný hardware

Firmware je určený výhradně pro **Waveshare ESP32-S3-Touch-LCD-2.1** s
480 × 480 px displejem a 16MiB flash. Konfigurace pinů, displeje ST7701,
dotyku CST820, PSRAM a partition table odpovídá této konkrétní desce.

Desku můžete zakoupit u českých prodejců:

<p align="center">
  <a href="https://pajenicko.cz/waveshare-esp32-s3-touch-lcd-2.1-s-kulatym-ips-lcd-dotykovym-displejem"><img src="docs/assets/retailers/pajenicko.png" alt="Koupit podporovanou desku na Pájeníčko.cz" height="60"></a>&nbsp;&nbsp;&nbsp;&nbsp;
  <a href="https://www.laskakit.cz/waveshare-esp32-s3-round-2-1--480--480-ips-touch-wifi-modul/"><img src="docs/assets/retailers/laskakit.png" alt="Koupit podporovanou desku na LaskaKit" height="60"></a>
</p>

Nepoužívejte tento binární obraz na jiném modelu jen proto, že také obsahuje
ESP32-S3. Odlišný pinout nebo flash layout může zabránit startu zařízení.

## Instalace pro běžného uživatele

### Instalace z prohlížeče

Veřejná [instalační stránka na GitHub Pages](https://coolajz.github.io/waveshare-hodiny/)
umožňuje nahrát stabilní release přímo z desktopového Chromu nebo Edge přes
USB. Instalační tlačítko se zpřístupní, jakmile je na GitHubu dostupný veřejný
stabilní release se zkontrolovaným čtyřdílným factory balíčkem.

Do té doby lze použít release balíček s manifestem v
[ESP Web Tools](https://web.esphome.io/) nebo firmware sestavit ze zdrojů
podle kapitoly [Sestavení ze zdrojů](#sestavení-ze-zdrojů). Factory instalace
vyžaduje všechny části a přesné offsety uvedené v release `manifest.json`;
samostatný aplikační `.ota.bin` není factory obraz.

### Nastavení Wi-Fi

Veřejný release neobsahuje přednastavené Wi-Fi údaje. Po instalaci připoj
zařízení jedním z jeho USB-C konektorů a použij Improv Serial v instalační
stránce. Zadané SSID a heslo se uloží do NVS a po restartu zůstanou zachované.

Deska má USB–UART konektor přes CH343P a nativní USB konektor ESP32-S3.
Produkční firmware obsluhuje Improv Serial na obou konektorech.

## První spuštění

1. Nainstaluj firmware a nastav Wi-Fi přes Improv Serial.
2. Počkej na připojení; na displeji se zobrazí IP adresa a stavové ikony.
3. Otevři `http://waveshare-hodiny.local/`. Pokud mDNS v síti nefunguje,
   použij IP adresu z nastavení na displeji.
4. V záložce **Zdroj a poloha** vyber Open-Meteo nebo Home Assistant a vyhledej
   město. Poloha je společná pro počasí Open-Meteo i meteoradar.
5. Při použití Home Assistantu zadej jeho adresu a long-lived access token a
   tlačítkem **Otestovat připojení** ověř spojení.
6. Uprav vzhled, radar a jas a zvol **Uložit změny**.

Nová konfigurace používá Open-Meteo, polohu Brno a pohled meteoradaru na celou
Českou republiku. Home Assistant není pro základní provoz povinný.

## Zdroje dat

### Open-Meteo

Open-Meteo je výchozí zdroj a nevyžaduje účet ani token. Poskytuje aktuální
počasí a čtyři konfigurovatelné hodnoty. Vybrané město a jeho GPS souřadnice
současně určují střed lokálních pohledů meteoradaru ČHMÚ.

### Home Assistant

Firmware čte jednotlivé entity přes REST API Home Assistantu. Nepotřebuje
MQTT, vlastní integraci ani administrátorský účet.

### Vytvoření tokenu

V Home Assistantu otevři svůj uživatelský profil, sekci **Long-lived access
tokens**, vytvoř nový token pro hodiny a vlož jej do webové konfigurace.
Použij účet pouze s oprávněními, která zařízení skutečně potřebuje.

Token se po uložení už do webové stránky neposílá a nelze jej z ní přečíst;
lze jej pouze nahradit. Při testu se uložený token znovu použije jen pro přesně
stejnou uloženou adresu Home Assistantu. Pokud adresu změníš, musíš zadat také
nový token.

Firmware podporuje lokální HTTP i HTTPS servery s vlastním nebo neplatným
certifikátem. U HTTPS spojení s Home Assistantem proto v současnosti neověřuje
certifikát serveru. Tato volba usnadňuje domácí instalace, ale nechrání token
před aktivním útočníkem v síti. Používej firmware pouze v důvěryhodné LAN.

### Doporučené entity

| Údaj | Příklad entity | Poznámka |
| --- | --- | --- |
| Počasí | `weather.domov` | Textový stav HA nebo podporovaný číselný kód |
| Slunce | `sun.sun` | Řídí automatický denní/noční režim |
| Venkovní teplota | `sensor.venkovni_teplota` | Libovolný číselný senzor |
| Pokojová teplota | `sensor.obyvak_teplota` | Libovolný číselný senzor |
| Hodnota A/B | `sensor.obyvak_co2` | CO₂, VOC, PM, vlhkost, tlak a další |

ID entit se zadávají ručně. Nedostupná nebo neplatná hodnota se na displeji
zobrazí jako `--`.

## Webová konfigurace

<p align="center">
  <img src="screenshots/web-configuration.png" alt="Webová konfigurace Home Assistantu a entit" width="920">
</p>

Web umožňuje nastavit:

- jazyk zařízení; dokud není uložená volba, displej používá češtinu a při
  prvním otevření webu se uloží čeština pro prohlížeče `cs`/`sk`, jinak
  angličtina; další návštěvy už respektují uložené nastavení zařízení a jazyk
  lze kdykoli přepnout vlajkami v pevném horním pruhu webu,
- zdroj dat Open-Meteo nebo Home Assistant a společnou polohu zařízení,
- Home Assistant URL, token, entitu počasí a entitu slunce,
- levou a pravou místnost včetně názvu, teploty, ikony a barev,
- styl animovaných ikon `Monochrome`, `Flat` nebo `Line`,
- meteoradar ČHMÚ s obrysem ČR, městy, pohledy 25, 50, 100, 200 km nebo celá ČR a volbou 1 až 15 snímků,
- měřené hodnoty A a B, jednotky, přesnost a barevné škály,
- barvu hodin, data a obou částí vteřinového efektu,
- denní/noční jas, ruční nebo automatický režim a automatické střídání hodin s radarem,
- automatické OTA aktualizace a režim webového serveru,
- volitelné heslo webového nastavení,
- export/import zálohy, restart, ovládání podsvícení a živou diagnostiku.

### Meteoradar ČHMÚ

Radar používá výhradně otevřená data radarového kompozitu MAX_Z Českého
hydrometeorologického ústavu. Nabízí pohledy 25, 50, 100 a 200 km kolem
uložené GPS polohy a přehled celé České republiky. Mapový podklad obsahuje
obrys státu a města přizpůsobená jednotlivým rozsahům.

Meteoradar je dostupný pouze pro polohy, které vyhledávání Open-Meteo označí
kódem země `CZ`. U lokality mimo Českou republiku firmware radar nespouští,
nestahuje jeho data na pozadí, nereaguje na radarová gesta a automatické
střídání obrazovek vypne. Počasí Open-Meteo i Home Assistant zůstávají bez
tohoto omezení.

Počet snímků lze nastavit od 1 do 15. Jeden snímek znamená statický radar;
vyšší počet vytvoří animaci od nejstaršího snímku k nejnovějšímu. Po posledním
snímku následuje nastavitelná pauza 0 až 30 sekund; výchozí hodnota je 5 sekund.
Čas posledního, tedy nejaktuálnějšího snímku je v denním režimu zvýrazněný
jasně zeleně. Decentní pruh pod popisem ukazuje průběh animace a během kompletní
přípravy prázdné cache je červený. Nová data se kontrolují v pevných
pětiminutových slotech přibližně minutu po čase publikace ČHMÚ.

Při červeném nočním vzhledu se mapový podklad, města, poloha, čas i jednotlivé
stupně odrazivosti převedou do odstínů červené. Jas jednotlivých stupňů dál
vyjadřuje intenzitu srážek a nejslabší odrazy mají zachované čitelné minimum.
Čas nejaktuálnějšího snímku má stejnou červenou jako ostatní text. Převod
probíhá z připravené cache, takže přepnutí vzhledu nevyvolá nové stahování ani
přípravu animace.

Tlačítka rozsahů na webu mění právě zobrazený pohled okamžitě. Modrá označuje
aktuální rozsah na hodinách a žlutá uložený výchozí rozsah. Do trvalé
konfigurace se změna zapíše až tlačítkem **Uložit změny**. Rozsah zvolený
dotykem na displeji zůstává pouze do restartu; po něm se obnoví hodnota
naposledy uložená přes web.

Automatické střídání hodin a radaru je ve výchozím stavu vypnuté. Po zapnutí
lze nastavit samostatnou dobu zobrazení hodin a radaru. Nastavený čas radaru
je minimální: rozběhnutý animační cyklus se vždy dokončí včetně závěrečné
pauzy, takže přechod zpět na hodiny nepřeruší animaci uprostřed. Po restartu
se příprava cache spustí na pozadí až po připojení Wi-Fi a synchronizaci času.
První automatický přechod na radar počká na kompletní animaci; další přechody
ji proto zobrazí okamžitě od nejstaršího snímku. Je-li automatické střídání
vypnuté, firmware radar na pozadí nestahuje a načítání začne až při ručním
otevření.

### Barevné prahy měřených hodnot

Každá měřená hodnota může mít až deset dvojic **hodnota → barva**. Firmware
mezi sousedními body plynule interpoluje, takže změna barvy na displeji není
omezena jen na několik tvrdých stavů. Škály jsou nezávislé: například VOC může
používat jiné hranice než CO₂.

<p align="center">
  <img src="screenshots/web-color-scales.png" alt="Nastavení barevných prahů VOC" width="49%">
  <img src="screenshots/web-color-scales-b.png" alt="Nastavení barevných prahů CO2" width="49%">
</p>

### Jas, denní/noční režim a vteřiny

Denní i noční jas se nastavují samostatně. Automatika používá východ a západ
slunce s volitelným ranním a večerním offsetem. Volitelná entita světla může v
nočním čase dočasně aktivovat denní vzhled. Volba **Vzhled nočního režimu** je
dostupná i při vypnuté automatice, protože stejný červený vzhled lze zapnout
ručně krátkým dotykem na hodinách i meteoradaru. Samostatně lze nastavit také
barvu hodin, data, typ vteřinového efektu, velikost a jas jeho aktivní i
neaktivní části.

<p align="center">
  <img src="screenshots/web-display-settings.png" alt="Nastavení jasu, denního a nočního režimu a vteřin" width="920">
</p>

Konfigurační web je ve výchozím režimu **Vždy zapnutý**. Lze jej přepnout na
deset minut po startu nebo aktivaci z displeje, případně jej úplně vypnout.
Provozuj jej jen v důvěryhodné síti; aktivní web signalizuje ikona ozubeného
kola na dashboardu.

Webové nastavení lze chránit heslem o délce 6 až 20 znaků. Stav bez hesla je
v záložce **Systém** označený červeně, aktivní ochrana zeleně. Heslo je uložené
v zařízení jako odvozený hash, nelze je zpětně zobrazit a není součástí
exportované zálohy.

### Diagnostika

V záložce **Systém** je odkaz na samostatnou stránku `/diagnostics`, která se
otevře v novém panelu. Bez dalších měření na pozadí zobrazuje aktuální a
minimální volnou interní RAM a PSRAM, procesor, velikost flash, důvod restartu,
Wi-Fi, IP adresu a skutečnou frekvenci pixel clocku displeje. Dále ukazuje stav
Home Assistantu a Open-Meteo a u radaru vybrané město, GPS, rozsah, počet
připravených snímků, jejich časové rozpětí, poslední úspěšnou aktualizaci,
další plánovanou kontrolu, HTTP stav a právě zpracovávaný soubor.

### Záloha konfigurace

Exportovaná JSON záloha obsahuje vzhled a ID entit, ale neobsahuje Home
Assistant token, heslo webu ani secret ovládacího API. Po importu proto může
být nutné citlivé hodnoty zadat znovu. Restart zařízení uložené nastavení
nemaže.

## Nastavení na displeji

Nastavení otevře dlouhý stisk kdekoliv na hodinách i meteoradaru. Mezi hodinami
a meteoradarem přepne vodorovné gesto swipe doleva nebo doprava. Protože jsou
obrazovky pouze dvě, oba směry vždy zobrazí druhou obrazovku.

Na radaru swipe nahoru pohled přiblíží a swipe dolů jej oddálí. Změna provedená
na displeji je dočasná a nezapisuje se do flash.

| Obrazovka a gesto | Výsledek |
| --- | --- |
| Hodiny: swipe doleva nebo doprava | Otevře meteoradar |
| Meteoradar: swipe doleva nebo doprava | Vrátí hodiny |
| Hodiny nebo meteoradar: dlouhý stisk kdekoliv | Otevře nastavení |
| Hodiny nebo meteoradar: krátký dotyk při vypnuté automatice den/noc | Přepne denní a noční režim |
| Meteoradar: swipe nahoru | Přiblíží rozsah |
| Meteoradar: swipe dolů | Oddálí rozsah |

Nastavení má tři stránky. Velká tlačítka se šipkami je přepínají; gesto swipe
se nepoužívá.

<p align="center">
  <img src="screenshots/device-settings.png" alt="První stránka nastavení denního a nočního jasu" width="31%">
  <img src="screenshots/device-settings-2.png" alt="Druhá stránka nastavení vteřin a animovaných ikon" width="31%">
  <img src="screenshots/device-settings-3.png" alt="Třetí stránka nastavení webu a OTA" width="31%">
</p>

První stránka ovládá denní a noční jas a automatický režim. Druhá přepíná
vteřiny, jejich efekt a animované ikony. Třetí řídí režim webového serveru a
ruční kontrolu OTA. IP adresa je na veřejném snímku záměrně skrytá. Krátký
dotyk hodin i meteoradaru při vypnuté automatice přepíná denní a noční režim.

## Animované Meteocons

Statické monochromatické ikony jsou uložené přímo ve firmware. Volitelné
animované ikony veřejného buildu se stahují z GitHub Pages a ukládají do
lokální cache. V nočním režimu se vždy použije monochromatický styl, aby ikony
respektovaly červené noční zobrazení.

V `docs/assets/weather-icons/` je pouze 45 GIFů používaných firmwarovým
allowlistem: 15 stavů pro každý ze stylů Monochrome, Flat a Line. Každý veřejný
manifest obsahuje skutečnou velikost a SHA-256 souboru; kompletní pracovní
mirror 1557 ikon v repozitáři není. Postup reprodukovatelného vytvoření je v
[`METEOCONS_ASSET_PIPELINE.md`](METEOCONS_ASSET_PIPELINE.md).

## OTA aktualizace

Release firmware používá A/B layout se dvěma stejně velkými 6MiB aplikačními
oddíly. Veřejný build čte statická metadata a OTA obraz pouze z GitHub Pages;
interní vývojový profil může dál používat Firmware Hub. Nová aplikace se
zapisuje do neaktivního slotu. Před aktivací se ověří:

- HTTPS spojení a povolený release origin,
- HTTP status a deklarovaná velikost,
- skutečný počet přijatých bajtů,
- SHA-256 obrazu,
- rodina čipu ESP32-S3,
- kapacita neaktivního aplikačního oddílu.

Při chybě zůstane aktivní stávající firmware. Wi-Fi a konfigurace v NVS a
`clockcfg` se při běžné OTA aktualizaci zachovají. Factory instalace nebo
vymazání celé flash je jiná operace a může uživatelská data odstranit.

Verze 1.6.0 podporuje jedinou historickou migraci konfigurace z veřejné verze
1.5.5. Zachová dosavadní zdroj dat, Home Assistant, entity, vzhled a další
uložené hodnoty a doplní nové radarové volby. U migrovaného zařízení se radar
nastaví na celou ČR, 6 snímků a automatické střídání zůstane vypnuté. Starší
vývojové meziverze nejsou samostatně podporované migračními kroky. Přechod z
1.5.5 na 1.6.0 byl ověřen skutečnou A/B OTA aktualizací včetně zachování
uložené konfigurace.

Automatické OTA aktualizace jsou po čisté instalaci vypnuté. Po zapnutí ve
webu firmware nejvýše jednou denně po 4:10 lokálního času zkontroluje novou
SemVer a případně ji nainstaluje. Stejnou cestu používá ruční aktualizace.

## Ovládací API pro Home Assistant

Web zobrazuje URL ovládacího endpointu obsahující náhodný 128bitový secret.
Pomocí REST příkazů lze aktualizovat data, zapnout či vypnout podsvícení nebo
vyvolat další podporované akce. URL považuj za přihlašovací údaj: nevkládej ji
do screenshotů, veřejných logů ani Git repozitáře.

Secret je uložený v zařízení, ověřuje se konstantním časem a není součástí
exportované zálohy. Přesný tvar endpointů a příklady požadavků jsou zobrazené
přímo v aktuálním webovém rozhraní firmware.

## Sestavení ze zdrojů

### Závislosti

Ověřený toolchain používá:

- Arduino CLI,
- Arduino ESP32 core `3.0.7`,
- LVGL `8.3.10`,
- PNGdec `1.0.1`,
- Python 3 pro generátory a release balíček.

Na macOS lze závislosti nainstalovat například takto:

```sh
arduino-cli core install esp32:esp32@3.0.7 --config-file arduino-cli.yaml
arduino-cli lib install lvgl@8.3.10 --config-file arduino-cli.yaml
arduino-cli lib install PNGdec@1.0.1 --config-file arduino-cli.yaml
```

Přenositelná konfigurace Arduino CLI je v `arduino-cli.yaml`. Lokální
ignorovaný soubor `WaveshareHodiny/local/arduino-cli.yaml` ji může přepsat.

### Vývojový build

```sh
./build.sh
./upload.sh
```

Volitelný port lze předat explicitně:

```sh
./upload.sh /dev/cu.usbmodemXXXXXXXX
```

Vývojový build se ukládá do `build/waveshare-hodiny-develop/`, podporuje USB
diagnostiku a screenshoty a úmyslně neinstaluje OTA release. Bez `.env` se
stále sestaví, pouze nemá vývojové výchozí Wi-Fi a HA hodnoty.

### Volitelná lokální `.env`

`.env` je celý ignorovaný Gitem a není pro sestavení povinný. Generátor
podporuje tyto lokální proměnné:

```dotenv
WIFI_SSID=
WIFI_PASSWORD=
HOME_ASSISTANT_URL=
HOME_ASSISTANT_TOKEN=
HA_ENTITY_WEATHER_CODE=
HA_ENTITY_OUTSIDE_TEMPERATURE=
HA_ENTITY_ROOM_TEMPERATURE=
HA_ENTITY_ROOM_CO2=
HA_ENTITY_ROOM_HUMIDITY=
HA_ENTITY_SUN=
FIRMWARE_SERVER_URL=
FIRMWARE_PROJECT_SLUG=
```

Skutečné hodnoty nikdy necommituj. Generované headery se ukládají pouze do
ignorovaného adresáře `WaveshareHodiny/local/`.

### Release build

Verzi zvol jako platný SemVer 2.0.0:

```sh
./build-release.sh 1.0.0
```

Výsledek je v `build/waveshare-hodiny-release/1.0.0/`. Adresář `package/`
obsahuje instalační části pro ESP Web Tools a právě jeden samostatný
`.ota.bin`. Release build neobsahuje lokální Wi-Fi ani Home Assistant údaje.

Tento výchozí příkaz zachovává interní profil z lokální `.env`. Veřejný profil
pro GitHub Pages lze lokálně pouze sestavit takto:

```sh
RELEASE_CHANNEL=public ./build-release.sh 1.0.0
```

Jeho výsledek je v `build/waveshare-hodiny-release/1.0.0-public/` a kromě
factory částí obsahuje také statická `ota.json` metadata. Nepoužívá `.env`,
lokální Wi-Fi, Home Assistant údaje ani klíč Firmware Hubu.

Žádný lokální build nic nepublikuje. Ruční GitHub Actions workflow **Public
firmware release** vyžaduje konkrétní stabilní SemVer a má samostatný přepínač
pro vytvoření neměnného GitHub Release. Bez něj pouze sestaví a zkontroluje
dočasný artifact. Pages z nejnovějšího stabilního GitHub Release přebírá čtyři
factory části, instalační manifest, samostatný OTA obraz a jeho metadata.

## Screenshot displeje přes USB

Vývojový firmware umí odeslat RGB565 framebuffer příkazem `SCREENSHOT`.
Pomocný nástroj jej převede na transparentní kruhové PNG 480 × 480 px:

```sh
./capture-screenshot.sh --output screenshots/latest.png
./capture-screenshot.sh --settings --output screenshots/settings.png
./capture-screenshot.sh --settings-page 2 --output screenshots/settings-2.png
./capture-screenshot.sh --night --output screenshots/night.png
```

Pokud je připojeno více zařízení, předej `--port`. Nástroj používá pyserial
3.5 z lokálního ignorovaného adresáře `.arduino/python`.

## Struktura repozitáře

```text
WaveshareHodiny/        Arduino sketch a firmware
assets/                 Zdrojové assety použité generátory
docs/assets/            Jen veřejně používané animované GIFy a manifesty
screenshots/            Veřejné obrázky dokumentace
tools/                  Build, test a asset utility
WaveshareHodiny/partitions.csv
                        Vlastní 16MiB A/B partition table
build.sh                Vývojový build
build-release.sh        Oddělený release build
upload.sh               USB upload vývojového buildu
```

## Řešení problémů

### `waveshare-hodiny.local` se neotevře

- ověř ikonu Wi-Fi na displeji,
- použij IP adresu z nastavení zařízení,
- pokud je zvolený časově omezený nebo vypnutý režim webu, otevři nastavení
  dlouhým stiskem kdekoliv na hodinách nebo meteoradaru,
- zkontroluj, že klient i zařízení jsou ve stejné dosažitelné síti.

Samostatná stránka `http://<IP-adresa>/diagnostics` zůstává dostupná i při
zamčeném konfiguračním webu.

### Home Assistant test selže

- URL musí obsahovat `http://` nebo `https://`,
- ověř token a přesná ID entit,
- při změně URL zadej také nový token,
- zkontroluj firewall mezi IoT sítí a Home Assistantem.

### Hodnota zůstává `--`

Otevři v Home Assistantu **Vývojářské nástroje → Stavy** a ověř, že entita
existuje a její stav je číselný nebo podporovaný stav počasí.

### OTA aktualizace není dostupná

Vývojový build OTA neinstaluje. U release buildu ověř připojení k internetu,
synchronizovaný čas a dostupnost nakonfigurovaného HTTPS release serveru.
Veřejný build používá `https://coolajz.github.io/waveshare-hodiny/firmware/`;
interní profil může používat jiný server z lokální `.env`.

### Zařízení se neobjeví na USB

Vyzkoušej oba USB-C konektory a datový kabel. Pro první factory instalaci může
být nutné uvést ESP32-S3 do bootloaderu podle dokumentace Waveshare.

## Bezpečnost a soukromí

- žádné Wi-Fi heslo ani HA token není součástí veřejného release,
- secrets, lokální buildy a generované headery jsou ignorované Gitem,
- HA token se po uložení neposílá zpět do prohlížeče,
- konfigurační web lze chránit heslem; bez nastaveného hesla patří pouze do
  důvěryhodné LAN,
- veřejná diagnostika nezobrazuje hesla, tokeny ani secret ovládacího API,
- HA HTTPS aktuálně toleruje neověřený/self-signed certifikát,
- OTA používá samostatná přísnější ověření TLS, originu, velikosti a SHA-256,
- ovládací API URL obsahuje secret a nesmí se zveřejňovat.

Před nahlášením bezpečnostního problému nezveřejňuj funkční token, Wi-Fi heslo
ani ovládací URL v issue.

## Poděkování

Při implementaci meteoradaru jsem využil a pro potřeby tohoto firmware
přizpůsobil část kódu z open-source projektu
[MeteoPlaneRadar](https://github.com/petus/MeteoPlaneRadar), který vyvíjí
Petr z [Chiptron.cz](https://chiptron.cz/). Děkuji za zveřejnění projektu,
praktickou ukázku práce s radarovými daty ČHMÚ a mapové podklady, na kterých
jsem mohl tuto integraci postavit.

## Licence

Původní kód projektu je dostupný pod [MIT licencí](LICENSE). Firmware používá
knihovny, fonty a grafické assety s vlastními licencemi; jejich autoři,
licence a zdrojové odkazy jsou uvedené v
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). MIT licence projektu jejich
původní licenční podmínky nenahrazuje.
