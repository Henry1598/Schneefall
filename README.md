# Schneefall

Schnee fällt über alle Fenster auf KDE Plasma 6 (Wayland). Fenster sind feste
2D-Flächen: Schnee landet überall, wo ein Fenster von oben getroffen werden kann
(bei Überlappung nur auf dem freien Teil), prallt seitlich ab und fällt nie davor
oder dahinter. Unter Fenstern liegt ein Schneeschatten. Beim Verschieben fährt der
Schnee mit, nur heftiges Ruckeln schüttelt einen Teil ab. Beim Minimieren oder
Schließen rutscht alter Schnee als Schneebrett ab (auch auf das nächste Fenster),
junger rieselt als Wolke herunter. Am unteren Bildschirmrand und auf der Taskleiste
sammelt sich Schnee und schmilzt langsam.

Gebaut für Plasma/KWin 6.8, Qt 6.12, mehrere Monitore mit gemischter Skalierung.

## Starten und Beenden

| Was | Wie |
|---|---|
| an/aus umschalten | `schneefall-umschalten` oder Starter „Schneefall“ im Anwendungsmenü |
| Steuerfenster öffnen | Linksklick auf die Schneeflocke im Systemabschnitt der Taskleiste, oder `~/Schneefall/build/schneefall` ein zweites Mal starten |
| Menü | Rechtsklick auf die Schneeflocke: Schnee an, Steuerfenster, Schnee abschütteln, Beenden |
| Beenden | Menü „Beenden“, Knopf „Beenden“ im Steuerfenster, oder `schneefall-umschalten` |

Es gibt keinen Autostart, solange im Steuerfenster nicht „Mit dem System starten“
angehakt ist. Das Häkchen legt `~/.config/autostart/schneefall.desktop` an,
Entfernen des Häkchens löscht genau diese eine Datei.

Befehle für die Konsole:

```
~/Schneefall/build/schneefall               # starten (läuft es schon: Steuerfenster öffnen)
~/Schneefall/build/schneefall --status      # Zustand, Flockenzahl, Monitore
~/Schneefall/build/schneefall --umschalten  # Schnee an/aus (Programm läuft weiter)
~/Schneefall/build/schneefall --beenden
```

## Steuerfenster

- **Schnee an:** Pause, das Programm läuft weiter (Symbol bleibt im Systemabschnitt).
- **Regler:** Menge, Wind (← links / → rechts), Flockengröße, max. Deckenhöhe (bis 80 px), Fallgeschwindigkeit.
- **Flockendetail:** „Realistisch“ (Standard) zeigt echte sechszählige Schneekristalle mit
  Tiefenwirkung, „Einfach“ alle Flocken als weiche Punkte.
- **Schnee abschütteln:** Alle Schneedecken fallen herunter.
- **Gruppe „Fenster“:**
  - **Fenstermodus:** „Fest“ (Standard, siehe oben), „Dahinter“ (fallender Schnee nur über der
    Arbeitsfläche, ein Teil bleibt auf den Kanten) oder „Davor“ (fällt vor den Fenstern).
  - **Schüttel-Schwelle:** ab welcher Beschleunigung beim Ziehen Schnee abfällt. Wie viel abfällt,
    hängt von der Heftigkeit ab: bei doppelter Schwelle etwa die Hälfte.
  - **Rutsch ab:** War ein Fenster so viele Minuten offen (oder ist seine Decke zu 60 % voll),
    rutscht der Schnee beim Minimieren/Schließen als Brett ab. Sonst rieselt er als Wolke herunter.
  - **Rutschtempo, Wolkendauer, Wolke bleibt liegen (%), Max. Boden/Taskleiste.**
- **Bei Vollbild pausieren:** Ist das aktive Fenster im Vollbild (Spiel, Video), wird der
  Schnee ausgeblendet und angehalten. Das kostet dann keine Leistung.
- **Mit dem System starten:** siehe oben.

Jede Änderung wirkt sofort und wird in `~/.config/schneefall.conf` gespeichert.

## Einstellungen (`~/.config/schneefall.conf`)

Die Datei wird beim ersten Start mit Erklärungen angelegt. Von Hand geänderte Werte
liest das laufende Programm sofort neu ein.

| Schlüssel | Bedeutung | Bereich | Standard |
|---|---|---|---|
| `menge` | Flocken je 10 000 px² | 0 … 10 | 1 |
| `wind` | px/s, negativ = nach links | -200 … 200 | 0 |
| `flockengroesse` | mittlerer Durchmesser in px | 1 … 12 | 3 |
| `max_deckenhoehe` | höchste Schneedecke in px | 2 … 80 | 18 |
| `fallgeschwindigkeit` | px/s | 5 … 300 | 45 |
| `fps` | Bilder pro Sekunde | 10 … 120 | 30 |
| `an` | Schnee an | true/false | true |
| `fenstermodus` | fest, dahinter oder davor | fest/dahinter/davor | fest |
| `schuetteln_schwelle` | Beschleunigung beim Ziehen, ab der Schnee abfällt (px/s²) | 1000 … 100000 | 15000 |
| `rutsch_ab_minuten` | ab diesem Deckenalter Schneebrett statt Wolke | 0 … 120 | 10 |
| `rutsch_tempo` | Fallbeschleunigung des Schneebretts (1 = 1200 px/s²) | 0,2 … 3 | 1 |
| `wolke_dauer` | Sekunden, die sich die Schneewolke hält | 1 … 60 | 8 |
| `wolke_liegen` | Prozent der Wolke, die unten liegen bleiben | 0 … 100 | 40 |
| `boden_max` | höchste Schneedecke auf Boden und Taskleiste in px | 2 … 80 | 18 |
| `vollbild_pausieren` | bei Vollbild pausieren | true/false | true |
| `flockendetail` | Aussehen der Flocken | realistisch/einfach | realistisch |

Der frühere Schlüssel `vor_fenstern` wird nicht mehr gelesen: Beim ersten Start einer neuen
Version wird er entfernt (Modus „fest“), fehlende neue Schlüssel werden ergänzt, alle übrigen
Werte bleiben unverändert.

`flockengroesse` ist die Grundgröße: Bei „realistisch“ sind ferne Flocken etwa so groß wie
eingestellt (weiche Punkte), mittlere 2,5- bis 5-mal und nahe bis 7,5-mal so groß (Kristalle).

Gemessen (4 Monitore, ca. 2000 Flocken, 30 fps): schneefall ca. 3 % eines Kerns bei „realistisch“
(vor den Kristallen 2 %), KWin unverändert ca. 2 Prozentpunkte. `--status` zeigt je Monitor die
Zeit fürs Zusammenstellen und Zeichnen eines Bildes und die Zahl der Ecken.

`fps=60` sieht etwas flüssiger aus, kostet aber etwa doppelt so viel CPU (schneefall + KWin).

## Aussehen

- **Fallende Flocken:** Dendriten, Sternplättchen und Plättchen, prozedural erzeugt (16 Formen,
  jede Flocke dazu mit eigener Drehung, Größe und eigenem Taumeln). Weißer Kern, bläulich
  durchscheinende Kante, Glanz auf den Eisflächen. Die Größe gibt die Tiefe: kleine Flocken
  sind fern (blasse Punkte, langsam), große nah (klare Kristalle, schnell), die allergrößten
  ganz vorn und leicht unscharf.
- **Schneedecken:** geglättete, an den Enden gerundete Oberkante, an freien Fensterenden ein
  kleiner Überhang, oben weiß und nach unten zart blaugrau, feine Körnung und einzelne
  funkelnde Kristalle, weiche Ränder statt Pixelstufen.
- **Abfallender Schnee** (Zerfall, Abschütteln, Abrutschen) fällt als Pulverklümpchen. Im
  Modus „fest“ gleitet abgeschüttelter Schnee erst oben über die nähere Fensterkante.
- **Schneebrett:** ein zusammenhängender Block, der beschleunigt fällt. Beim Aufprall bleibt
  liegen, was aufsetzt (ein wenig stiebt als Pulver auf), überhängende Teile brechen ab und
  fallen weiter. Gezeichnet wie eine Decke, in jedem Bild neu.
- **Schneewolke:** feines Pulver, das langsam rieselt, treibt und sich in seiner letzten
  Sekunde auflöst; der eingestellte Anteil landet als Schnee.
- Die Formen werden einmal je Monitor-Skalierung in einen Textur-Atlas gerastert (7 Größen von
  8 bis 64 px, in Gerätepixeln, also scharf bei 1,5×) und danach nur noch als Sprites gezeichnet.

## Aufbau

- `src/schnee.{h,cpp}`: Simulationskern ohne GUI (Flocken, Decken je Fensterkante,
  Verdeckung nach Stapelreihenfolge, feste Fenster, Mitfahren und Schütteln, Schneebrett und
  Wolke, Abrutschen, Boden mit Schmelzen).
- `src/kristall.{h,cpp}`: Formgenerator für die Schneekristalle, Rasterung und Atlas-Aufteilung (ohne Qt).
- `src/flockenbild.{h,cpp}`: welches Bild, welche Größe, Deckkraft und Drehung eine Flocke bekommt.
- `src/deckenform.{h,cpp}`: Form der Schneedecke fürs Zeichnen (Glättung, runde Enden, Überhang).
- `src/overlay.{h,cpp}`: eine durchsichtige Layer-Shell-Ebene (Overlay) je Monitor.
  Sie hat keine Tastatur und eine leere Eingaberegion, Klicks gehen also durch.
  Gezeichnet wird mit OpenGL in voller Gerätepixel-Auflösung (scharf bei 1,5×).
- `src/app.{h,cpp}`: D-Bus-Dienst `org.schneefall` (Sitzungsbus), Monitore, Takt, Systemabschnitt.
- `src/steuerfenster.{h,cpp}`: Steuerfenster.
- `kwin/`: KWin-Skript `schneefall-fenster`. Es meldet bei jeder Änderung (höchstens alle 15 ms,
  auch während des Ziehens, und alle 2 s) alle Fenster mit Position, Stapelplatz, minimiert,
  Vollbild, aktiv und Dock an `org.schneefall`. Aus den Positionen und der Ankunftszeit misst
  der Kern Geschwindigkeit und Beschleunigung gezogener Fenster (Schütteln).
  Läuft schneefall nicht, gehen die Aufrufe ins Leere.

## Schnittstellen im Kern (für Räumdienst und Schneebälle)

Alle in `schnee::Simulation` (`src/schnee.h`), Koordinaten global logisch wie in KWin.
Mengen sind wie bei `gesamtSchnee()` die Summe der Spaltenhöhen in px.

| Aufruf | Zweck |
|---|---|
| `decken()`, `decke(id)` | Deckenprofil je Fenster: `x`, `y` (Fensteroberkante), `spaltenBreite`, `hoehe[i]` je Spalte, `sichtbar[i]`; Spalte i liegt bei x + i·spaltenBreite |
| `oberkanteBei(id, x)` | Welt-y der Schneeoberfläche an x (NaN außerhalb der Decke) |
| `schneeHinzufuegen(id, x, menge)` | Schnee auf die Spalte bei x legen (verteilt sich danach wie üblich) |
| `schneeEntnehmen(id, x0, x1, menge)` | bis zu `menge` aus [x0, x1) nehmen, z. B. für einen Schneeball; gibt die Menge zurück |
| `abtragenUndFallenLassen(id, x, menge, richtung)` | Schnee bei x abtragen und als Brocken über die linke (−1) oder rechte (+1) Kante fallen lassen |
| `brockenFallenLassen(x, y, breite, menge, vx, vy)` | freier Brocken (Mitte x, Unterseite y), z. B. zerplatzter Schneeball |
| `brocken()`, `fallenderSchnee()` | fallende Brocken; Schnee, der gerade fällt und noch landen wird |

## Bauen und Testen

cmake wird nicht gebraucht, es reicht ein Makefile mit pkg-config.

```
cd ~/Schneefall
~/.local/bin/testlauf 4G 300 make -j8          # Programm bauen
~/.local/bin/testlauf 4G 300 make test         # QtTest: Kern, Aussehen, Steuerfenster + Test des KWin-Skripts (node)
tests/rauchtest.sh                             # Live-Test der D-Bus-Schnittstelle (Sitzung nötig)
make install                                   # schneefall-umschalten + Starter
make kwin-skript                               # KWin-Skript installieren/aktualisieren und einschalten
```

## Deinstallation

```
~/Schneefall/build/schneefall --beenden
kwriteconfig6 --file kwinrc --group Plugins --key schneefall-fensterEnabled --delete
qdbus6 org.kde.KWin /KWin reconfigure
kpackagetool6 --type=KWin/Script -r schneefall-fenster
rm ~/.local/bin/schneefall-umschalten ~/.local/share/applications/schneefall.desktop
rm ~/.config/schneefall.conf ~/.config/autostart/schneefall.desktop   # falls vorhanden
rm -r ~/Schneefall                                                     # Quellen
```

## Bekannte Grenzen

- Schwebende Plasma-Panels: Die Panel-Fläche schließt den Schwebeabstand ein, deshalb
  liegt der Schnee ein paar Pixel über der sichtbaren Leiste.
- Die Fensterkante ist der Fensterrahmen laut KWin. Bei Programmen mit eigenem Rahmen
  und Schattenrand (CSD) kann der Schnee leicht über der sichtbaren Kante liegen.
- Vollbild wird am aktiven Fenster erkannt. Ein Vollbildfenster auf einem anderen
  Monitor, das nicht aktiv ist, pausiert den Schnee nicht.
- Modus „fest“: Gemessen wird die Fläche laut KWin (Rahmen). Schnee auf einem Fenster, das
  teilweise über den Monitorrand hinausgeschoben wird, fällt dort ab und kommt nicht zurück.
- Das Alter einer Decke zählt ab dem Start von Schneefall bzw. ab dem Öffnen/Wiederherstellen
  des Fensters; während einer Pause zählt es nicht weiter.

## Geräusche (Teil C)

Alle Geräusche (Hubschrauber, Dieselmotor, Rückfahrwarner, Schnee-Rutsch, Plumps, Schneeball-Wurf/-Treffer, Murren) werden
zur Laufzeit synthetisiert, es gibt keine Audiodateien. Standard: aus (`ton=aus`, `lautstaerke=50` in `schneefall.conf`);
Schalter „Geräusche“ und Regler „Lautstärke“ im Steuerfenster. Ist der Ton aus oder lange still, wird das Audiogerät geschlossen.

- `src/klang.{h,cpp}`: Klasse `Klang` mit `ereignis()`, `dauerklang()`, `dauerklangStopp()`.
- `make klangprobe`, dann `build/klangprobe` (spielt alles kurz ab), `--wav datei.wav`, `--wav-ordner ordner` (offline rendern), `--cpu` (Last messen).
- Tests: `make test-klang`.
