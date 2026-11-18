# Merge-Hinweise: Sprüche (Branch `sprueche`)

Fertig und eigenständig: `daten/sprueche.tsv` (1006 Sprüche), `src/sprueche.{h,cpp}`, Test `tests/test_sprueche.cpp`,
Einstellung `sprueche_max_stufe` (`einstellungen.*`) und Auswahlfeld „Ausdrucksweise“ im Steuerfenster (`steuerfenster.*`).
**Noch nicht angebunden**: `raeumdienst`/`raeumszene`/`fahrzeugbild` benutzen weiter die 13 festen Sprüche.
Grund: Die Sprechblasen sind vorgemalte Atlas-Texturen (`Teil::Blase0 + spruchNr`, `fahrzeugAtlasErzeugen()`); mit 1006 Sätzen
geht das nicht 1:1 (zu viel Atlas). Das muss zusammen mit dem Grafik-Umbau gelöst werden.

## Schnittstelle
```cpp
#include "sprueche.h"
auto &sp = schnee::Sprueche::instanz();          // lädt einmal (Ressource :/daten/sprueche.tsv, sonst Ersatzsätze)
sp.setzeMaxStufe(p.spruecheMaxStufe);            // bei jeder Einstellungsänderung (App::einstellungenAendern)
QString t = sp.waehleBeiTreffer(Sprecher::Fahrer, Anlass::Treffer, f.trefferGesamt); // Stufe = min(Treffer, Max)
QString g = sp.waehle(Sprecher::Fahrer, Anlass::Gruss, 1);                          // feste Stufe
```
Anlässe: `Treffer` (jeder Treffer), `Streik`, `BeleidigtAbflug`, `Gruss` (Fahrer); `HeliTreffer`, `HeliAbholen`, `HeliAbflug` (Pilot).
Ohne Wiederholung der letzten 50 Sätze je Sprecher (`SPRUCH_FENSTER`). Programm-Link: `$(SPRUECHE_O)` (sprueche.o + qrc_sprueche.o).

## Aufgaben beim Merge
1. **Sprechblase mit freiem Text.** Vorschlag: statt `Blase0 + i` (je Satz eine Textur) einen kleinen Pool von z. B. 6 Blasen-Slots im Atlas
   (`Teil::Blase0 … Blase5`), die bei jedem neuen Satz neu gemalt werden (QPainter wie in `fahrzeugbild.cpp`, Schleife über `spruchText(i)`),
   oder die Blase als eigenes Overlay-Element mit `QStaticText`. `Fahrzeug::spruch` (int) wird dann ein Slot-Index plus Text.
   Blasenbreite je Text (`horizontalAdvance + 22`); Sätze sind ≤ 70 Zeichen.
2. **Fahrer** (`raeumdienst.cpp`): `sagen(f, von, anzahl, dauer)` ersetzen:
   - `treffer(Einsatz&, Schneeball&)`, Zweig „weitere Treffer im Streik“ (`sagen(f, 0, 8, 3.5)`) → `Anlass::Treffer`;
     zusätzlich bei **jedem** Treffer eine kurze Blase (`Anlass::Treffer`, Stufe aus Gesamt-Trefferzahl dieses Einsatzes – `f.treffer` wird bei Streik auf 0 gesetzt, besser einen eigenen Zähler `f.trefferGesamt` führen).
   - `streikPruefen`: `sagen(f, 8, 3, 3.0)` → `Anlass::BeleidigtAbflug`, `sagen(f, 0, 8, 5.0)` → `Anlass::Streik`.
   - Anstupsen (`sagen(e.fz, 11, 2, 2.0)`) und Start/Ende → `Anlass::Gruss` (Stufe 1–2).
3. **Heli-Treffererkennung / Pilot**: Treffer auf den Hubschrauber wird in `Raeumdienst::ballSchritt` schon erkannt
   (Block `if (std::abs(b.x - e.heli.x) <= 50 * G + r * 0.5 && …)`, dort `e.heli.stoss += …`, `klang(Geraeusch::Treffer, …)`).
   Dort ergänzen: `++e.heliTreffer;` und `Pilot-Blase = waehleBeiTreffer(Sprecher::Pilot, Anlass::HeliTreffer, e.heliTreffer)`.
   `Hubschrauber` hat noch kein Blasenfeld: `spruch`/`blaseRest` ergänzen (oder gemeinsame Struct `Sprechblase`), Zeichnen analog
   zu `sprechblase()` in `raeumszene.cpp` (Position über dem Rumpf, `heli.y - ~30 * G`); Blase nur zeigen, solange kein Fahrer-Spruch den Platz belegt.
   Max. eine Pilot-Blase pro ~1,5 s (Treffer-Entprellung), sonst wird es bei Ballserien unlesbar.
4. **Pilot beim Abholen/Abflug**: `HeliAbholen` beim Anflug zum beleidigten Fahrer (`e.beleidigt`, Phase Beleidigt → Heli holt ab),
   `HeliAbflug` beim Start mit Fahrzeug/Fahrer.
5. **`App`**: `Sprueche::instanz().setzeMaxStufe(m_einst.spruecheMaxStufe)` in `App::starten()` und `App::einstellungenAendern()`.
6. Tests nachziehen: `tests/test_raeumdienst.cpp` (Anzahl `spruchAnzahl()` ist heute 13, `tests/test_bild.cpp` prüft ggf. die Blasen-Atlasgröße).

## Konfliktstellen beim Merge (klein gehalten)
- `Makefile`: `SPRUECHE_O` in den Linkzeilen von `schneefall` und `test_steuerfenster`, `src/sprueche.h` in der `%.o`-Abhängigkeitsliste, neuer Block „Sprüche“ am Ende, `test`-Ziel.
- `src/einstellungen.{h,cpp}`: Feld `spruecheMaxStufe`, Schlüssel `sprueche_max_stufe` (auch in `einstellungenBrauchtErgaenzung`).
- `src/steuerfenster.{h,cpp}`: `m_ausdruck` (QComboBox „Ausdrucksweise“) am Ende der Gruppe „Räumdienst“ (Zeile 8, Stretch jetzt Zeile 9).
- `tests/test_steuerfenster.cpp`: Block „Ausdrucksweise“ + `sprueche_max_stufe=5` in der Dateiprüfung.
- Die Datei `daten/sprueche.tsv` ist reiner Text: Sprüche ändern = Datei bearbeiten, `make` bettet sie neu ein; `make test-sprueche` prüft Dubletten, Längen, Abdeckung.
