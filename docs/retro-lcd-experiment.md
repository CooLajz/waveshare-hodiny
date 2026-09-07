# Retro LCD – vývojový experiment

Výchozí čistý stav: tag `snapshot/retro-lcd-before-2026-09-07`, commit
`ffe4d166d276861e96f2aa6a4d9c5cda80b16f92`.

Retro LCD je třetí typ hodin ve webovém i dotykovém nastavení. Výběr na webu
nejprve zobrazí náhled; tlačítko Uložit jej uloží i pro další restart.
Vývojové USB příkazy `RETRO` / `RETROOFF` zůstávají pro dočasný test vrstvy.
Restart vždy obnoví uložený typ hodin.

- Čas a datum vycházejí ze stejného lokálního času jako původní ciferník.
- Číslice a den jsou kreslené sedmi a čtrnácti segmenty, včetně zhasnutého podkresu.
- Levá a pravá hodnota LCD si nezávisle vybírají levou, pravou, spodní A nebo spodní B hodnotu, včetně názvu, jednotky a přesnosti.
- Přímo v nastavení Retro LCD mají levá a pravá strana volbu 1–4 míst před desetinnou tečkou
  (znaménko má vlastní úzký segment navíc), výchozí 3 pro A a 4 pro B. Desetinná místa
  se přidávají podle stávající přesnosti. Neobsazené pozice zůstávají zhasnuté,
  při nedostupnosti nebo přesahu po zaokrouhlení se zobrazí pomlčky. Pozice
  i velikost číslic jsou pevné pro zvolenou konfiguraci, jednotka neposkakuje.
- Pozadí a popředí mají samostatné barevné volby v sekci Displej. Podkres
  je odvozený z těchto barev a má posuvník 0–50 % (výchozí 5 %); nula jej skryje. Tenké znaky si zachovávají jemnější podkres. Nastavení je v samostatných klíčích vzhledu,
  beze změny binárního schématu hlavní konfigurace; zahrnuje se do záloh.
- Ukazatel s nastavitelnými 5–50 dílky (výchozí 10) ve vyhrazené šířce má vlastní výběr ze čtyř existujících hodnot a možnost vypnutí
  (výchozí). Minimum a maximum se nastavují pouze ve webu; maximum musí být větší.
  Dílky vyjadřují polohu hodnoty v rozsahu a za jeho hranicemi zůstávají prázdné/plné.
  Pod ukazatelem je jen název s jednotkou v závorce, bez číselného rozsahu a aktuálního
  čísla. Chybějící údaj se označí pomlčkami a žádný dílek nesvítí.
- Noční vzhled respektuje stávající nastavení: červená varianta pouze při
  červeném nočním režimu, při volbě „jen jas“ zůstává světlé LCD.
- Náhled nepotřebuje vlastní framebuffer. Sekundy překreslují pouze svou oblast.

## English

Retro LCD is selectable in web and touch settings. Web selection previews it;
Save persists the choice across restart. USB `RETRO` / `RETROOFF` remain
temporary development overrides; restart restores the saved face.
It draws seven/fourteen-segment glyphs, local time/date and two live values independently mapped from left, right, bottom A or bottom B.
The centered indicator has 5–50 segments (default 10) and independently selects one of the four existing values or
is disabled (default). A finite minimum and maximum define its range; maximum must
exceed minimum. Values outside the range clamp to empty/full. The face shows only
the name and unit below the blocks, without numeric limits or the current value.
Missing data adds dashes to the caption and leaves all blocks inactive.
Each LCD side has 1–4 positions in the Retro LCD settings before the decimal point,
with a separate narrow minus segment (defaults 3/4). Existing decimal precision adds fractional
positions. Overflow and missing values display dashes instead of truncated
numbers. Digit and unit positions stay fixed for a given configuration.
Background/foreground colors and digit counts are stored in separate
appearance keys and included in backups. Inactive segment intensity ranges from 0–50% (default 5%); zero hides it and thin glyphs retain reduced contrast. Existing night-mode policy is
respected. No additional framebuffer is allocated.

Dílky i mezery mají vždy shodnou celočíselnou šířku; zbytek prostoru se rozdělí na okraje.
Segments and gaps always have identical whole-pixel widths; remaining space becomes centered margins.

Po uvolnění posuvníku počtu dílků se ihned použije dočasný náhled; tlačítko Uložit jej zachová po restartu.
Releasing the segment-count slider previews the change immediately; Save persists it across restart.

Znaménko mínus nemá podkres; rezervované místo udržuje číslice a jednotku na místě.
The minus sign has no inactive trace; its reserved space keeps digits and units stationary.
