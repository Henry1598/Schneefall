# Schneefall

Schnee fällt über alle Fenster auf KDE Plasma 6 (Wayland). Fenster sind feste
2D-Flächen: Schnee landet überall, wo ein Fenster von oben getroffen werden kann
(bei Überlappung nur auf dem freien Teil), prallt seitlich ab und fällt nie davor
oder dahinter. Unter Fenstern liegt ein Schneeschatten. Beim Verschieben fährt der
Schnee mit, nur heftiges Ruckeln schüttelt einen Teil ab. Beim Minimieren oder
Schließen rutscht alter Schnee als Schneebrett ab (auch auf das nächste Fenster),
junger rieselt als Wolke herunter. Am unteren Bildschirmrand und auf der Taskleiste
sammelt sich Schnee und schmilzt langsam.

Wird es auf einem Fenster zu viel, kommt der **Räumdienst**: Ein Hubschrauber fliegt eine
Pistenraupe am Seil ein, setzt sie auf dem Fenster ab, sie schiebt den Schnee über die Kante
nach unten und wird wieder abgeholt. Taskleiste und Boden räumt ein kleiner Pflug. Aus dem
Schnee lassen sich mit der Maus **Schneebälle** formen und werfen – trifft man das Fahrzeug
zu oft, streikt der Fahrer.

Gebaut für Plasma/KWin 6.8, Qt 6.12, mehrere Monitore mit gemischter Skalierung.

## Starten und Beenden

| Was | Wie |
|---|---|
| an/aus umschalten | `schneefall-umschalten` oder Starter „Schneefall“ im Anwendungsmenü |
| Steuerfenster öffnen | Linksklick auf die Schneeflocke im Systemabschnitt der Taskleiste, oder `~/Schneefall/build/schneefall` ein zweites Mal starten |
| Menü | Rechtsklick auf die Schneeflocke: Schnee an, Steuerfenster, Schnee abschütteln, Jetzt räumen, Beenden |
| Beenden | Menü „Beenden“, Knopf „Beenden“ im Steuerfenster, oder `schneefall-umschalten` |

Es gibt keinen Autostart, solange im Steuerfenster nicht „Mit dem System starten“
angehakt ist. Das Häkchen legt `~/.config/autostart/schneefall.desktop` an,
Entfernen des Häkchens löscht genau diese eine Datei.

Befehle für die Konsole:

```
~/Schneefall/build/schneefall               # starten (läuft es schon: Steuerfenster öffnen)
~/Schneefall/build/schneefall --status      # Zustand, Flockenzahl, Monitore
~/Schneefall/build/schneefall --umschalten  # Schnee an/aus (Programm läuft weiter)
~/Schneefall/build/schneefall --raeumen     # „Jetzt räumen“: Einsatz am Fenster mit dem meisten Schnee
~/Schneefall/build/schneefall --beenden
qdbus6 org.schneefall / org.schneefall.Werfen 2000 500 400 -300 150   # Schneeball: x y vx vy Menge
```

## Steuerfenster

- **Schnee an:** Pause, das Programm läuft weiter (Symbol bleibt im Systemabschnitt).
- **Regler:** Menge, Wind (← links / → rechts), Flockengröße, max. Deckenhöhe (bis 80 px), Fallgeschwindigkeit.
- **Flockendetail:** „Realistisch“ (Standard) zeigt echte sechszählige Schneekristalle mit
  Tiefenwirkung, „Einfach“ alle Flocken als weiche Punkte.
- **Schnee abschütteln:** Alle Schneedecken fallen herunter.
- **Gruppe „Fenster“:**
  - **Bildrate:** Auto (so oft wie der Monitor, am flüssigsten), 30, 60, 120 oder 144 Bilder/s als
  Obergrenze (spart CPU bei schneefall und KWin).
- **Fenstermodus:** „Fest“ (Standard, siehe oben), „Dahinter“ (fallender Schnee nur über der
    Arbeitsfläche, ein Teil bleibt auf den Kanten) oder „Davor“ (fällt vor den Fenstern).
  - **Schüttel-Schwelle:** ab welcher Beschleunigung beim Ziehen Schnee abfällt. Wie viel abfällt,
    hängt von der Heftigkeit ab: bei doppelter Schwelle etwa die Hälfte.
  - **Rutsch ab:** War ein Fenster so viele Minuten offen (oder ist seine Decke zu 60 % voll),
    rutscht der Schnee beim Minimieren/Schließen als Brett ab. Sonst rieselt er als Wolke herunter.
  - **Rutschtempo, Wolkendauer, Wolke bleibt liegen (%), Max. Boden/Taskleiste.**
- **Gruppe „Räumdienst“:**
  - **Bei viel Schnee von selbst räumen:** Erreicht eine Decke „Räumen ab“ Prozent der Höchstmenge
    (max. Deckenhöhe bzw. Max. Boden/Taskleiste), wird ein Einsatz gebucht. Es läuft immer nur die
    eingestellte Zahl von Einsätzen zugleich, weitere Fenster warten der Reihe nach.
  - **Fahrzeuggröße, Einsätze zugleich, Treffer bis Streik.**
  - **Taskleiste und Boden räumen:** dort kommt der kleine Pflug (auch er wird eingeflogen).
  - **Schnee anfassbar:** siehe „Schneebälle“. Aus = Schneefall nimmt nie einen Mausklick an.
  - **Jetzt räumen:** sofort ein Einsatz am Fenster mit dem meisten Schnee (auch im Menü der
    Schneeflocke und per `--raeumen`).
- **Geräusche, Lautstärke:** siehe unten (Standard aus).
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
| `bildrate` | Bilder/s je Monitor: `auto` = so oft wie der Monitor (z. B. 144/165 Hz), sonst Obergrenze | auto, 10 … 360 | auto |
| `fps` | alter fester Takt; wird nur noch für ältere Versionen mitgeschrieben (fehlt `bildrate`, zählt ein selbst gewählter `fps`-Wert, der alte Standard 30 wird zu `auto`) | 10 … 120 | 30 |
| `an` | Schnee an | true/false | true |
| `fenstermodus` | fest, dahinter oder davor | fest/dahinter/davor | fest |
| `schuetteln_schwelle` | Beschleunigung beim Ziehen, ab der Schnee abfällt (px/s²) | 1000 … 100000 | 15000 |
| `rutsch_ab_minuten` | ab diesem Deckenalter Schneebrett statt Wolke | 0 … 120 | 10 |
| `rutsch_tempo` | Fallbeschleunigung des Schneebretts (1 = 1200 px/s²) | 0,2 … 3 | 1 |
| `wolke_dauer` | Sekunden, die sich die Schneewolke hält | 1 … 60 | 8 |
| `wolke_liegen` | Prozent der Wolke, die unten liegen bleiben | 0 … 100 | 40 |
| `boden_max` | höchste Schneedecke auf Boden und Taskleiste in px | 2 … 80 | 18 |
| `vollbild_pausieren` | bei Vollbild pausieren | true/false | true |
| `ton`, `lautstaerke` | Geräusche an, Lautstärke | true/false, 0 … 100 | false, 50 |
| `raeumdienst` | von selbst räumen | true/false | true |
| `raeumen_ab` | Prozent der Höchstmenge, ab denen geräumt wird | 30 … 100 | 80 |
| `fahrzeug_groesse` | Größe von Hubschrauber und Fahrzeugen | 0,5 … 3 | 1 |
| `einsaetze_gleichzeitig` | Einsätze zugleich, der Rest wartet | 1 … 3 | 1 |
| `taskleiste_raeumen` | kleiner Pflug für Taskleiste und Boden | true/false | true |
| `treffer_bis_streik` | Schneeball-Treffer, bis der Fahrer streikt | 1 … 10 | 3 |
| `schnee_anfassbar` | Schneebälle mit der Maus formen | true/false | true |
| `flockendetail` | Aussehen der Flocken | realistisch/einfach | realistisch |

Der frühere Schlüssel `vor_fenstern` wird nicht mehr gelesen: Beim ersten Start einer neuen
Version wird er entfernt (Modus „fest“), fehlende neue Schlüssel werden ergänzt, alle übrigen
Werte bleiben unverändert.

`flockengroesse` ist die Grundgröße: Bei „realistisch“ sind ferne Flocken etwa so groß wie
eingestellt (weiche Punkte), mittlere 2,5- bis 5-mal und nahe bis 7,5-mal so groß (Kristalle).

`--status` zeigt je Monitor die Zeit fürs Zusammenstellen und Zeichnen eines Bildes, die Zahl der
Ecken und die **Bildzeiten**: Monitorrate (`hz`), Teiler (jedes wievielte Monitorbild gezeichnet
wird), Soll-Abstand, gemessene Bilder/s, mittlerer Abstand, Jitter (Standardabweichung), p50/p95/p99/max,
verpasste Bilder (Abstand über 1,5 × Soll) und die Verteilung der Abstände in Vielfachen des Solls.
Dazu oben `schritte_je_s` (Simulationsschritte) und `flockenaufbau_je_s`.

### Takt (Bildrate)

Gezeichnet wird im Takt des Compositors (Wayland-Frame-Callbacks, `requestUpdate()` nach jedem Bild),
je Monitor mit dessen eigener Rate. Bei einer Obergrenze zeichnet die Ebene gleichmäßig jedes n-te
Monitorbild (165 Hz mit Grenze 60 → jedes 3., also 55 Bilder/s, alle gleich lang) auf einem festen
Raster, das an einem Frame-Callback ausgerichtet ist – kein Wechsel zwischen kurzen und langen Bildern.

Die Simulation rechnet in Ruhe 30 Schritte je Sekunde. Die Flocken baut die CPU nur 30-mal je
Sekunde zusammen (mit Tempo, Drehtempo und Taumeltempo je Flocke); der Vertex-Shader schreibt sie bis
zum jeweiligen Monitorbild fort. Dadurch bewegen und drehen sie sich in jedem Bild stufenlos, die
CPU-Last hängt kaum an der Bildrate. Bewegt sich etwas, das der Shader nicht fortschreiben kann
(Fenster ziehen, Schneebretter, Räumdienst, Schneebälle, Maus gedrückt), rechnet die Simulation in
jedem Bild.

## Räumdienst

Ablauf eines Einsatzes (Zustandsautomat in `src/raeumdienst.cpp`, vom Zeichnen getrennt):

1. **Anflug:** Der Hubschrauber kommt vom näheren Rand der Arbeitsfläche, das Fahrzeug hängt am
   Seil und pendelt (gedämpftes Pendel). Er neigt sich mit der Beschleunigung.
2. **Schweben und Absenken:** Über der Fenstermitte lässt die Winde das Fahrzeug herunter. Der
   Abwind bläst Schnee unter dem Hubschrauber zur Seite und wirbelt Staub auf.
3. **Aufsetzen und Ausklinken:** Das Fahrzeug federt ein, der Schnee darunter wird zur Seite
   gedrückt. Der Hubschrauber zieht den Haken ein und wartet seitlich.
4. **Räumen:** Schild senken, bis zur Kante schieben, zurücksetzen (mit Rückfahrwarner), wenden
   (siehe „Wenden“), andere Seite. Vor dem Schild türmt sich der Wulst; was über die Fensterkante ragt, fällt als
   Brocken hinunter und landet auf dem Fenster darunter oder dem Boden. Ist der Wulst voll,
   rutscht der Überschuss am Schild vorbei und fällt vorn über die Kante. Liegt danach noch
   merklich Schnee, gibt es bis zu zwei weitere Durchgänge.
5. **Abholen:** Der Hubschrauber kommt zurück, lässt den Haken herunter, hängt das Fahrzeug an
   und fliegt hinaus. Danach wird dasselbe Fenster frühestens nach 45 s wieder gebucht.

- Wird das Fenster verschoben, fährt das Fahrzeug mit. Wird es **minimiert oder geschlossen**,
  ist das Seil sofort dran und der Hubschrauber zieht das Fahrzeug weg; der Wulst fällt herunter.
- **Pause und Vollbild:** Der Einsatz bleibt stehen (und verstummt) und läuft danach weiter.
- Fenster, auf deren freie Oberkante das Fahrzeug nicht passt, werden nicht geräumt.
- Die Schneemenge bleibt erhalten: Was geräumt wird, fällt nach unten. Nur am Bildschirmboden
  ist der Schnee weg („vom Bildschirmrand geschoben“).

### Wenden

Das Fahrzeug klappt nicht um, sondern dreht wie ein Kettenfahrzeug auf der Stelle: Schild hoch,
dann 180° um die Hochachse in 1,5 bis 2,5 s (je nach Fahrzeuggröße, weich an- und auslaufend).
Zu sehen sind echte Zwischenansichten – Seite, schräg, Front (Schild in voller Breite) bzw. Heck
(Rücklichter, Auspuff), schräg, andere Seite; die Länge verkürzt sich dabei. Der Drehsinn wechselt
von Einsatz zu Einsatz (einmal über die Front-, einmal über die Heckansicht). Dabei laufen die
Kettenglieder um, der Motor arbeitet (mehr Rauch, mit Ton lauter), das Fahrzeug ruckt beim Anfahren
und Anhalten und die Ketten wirbeln Schnee auf. Würde es beim Drehen über die Kante des befahrbaren
Abschnitts ragen, rangiert es vorher ein Stück zur Mitte. Ein Schneeballtreffer mitten in der
Drehung zählt, der Fahrer beschwert sich aber erst danach. Der kleine Pflug dreht genauso, der
Hubschrauber giert beim Richtungswechsel über die Front- bzw. Heckansicht.

Technik: `src/ansicht3d.{h,cpp}` (Parallelprojektion mit Gierwinkel, Blick leicht von oben,
weltfestes Licht von oben links, Sichtbarkeit und Tiefenordnung) und `src/fahrzeugmodell.{h,cpp}`
(Raupe, Pflug und Hubschrauber aus Quadern, Prismen und Schalen). Die Seitenflächen tragen die
gemalten Seitenbilder als Textur, deshalb gleicht die Ansicht bei 0° dem gewohnten Bild.
`fahrzeugbild.cpp` malt je Fahrzeug 24 Ansichten (15°-Schritte, Raupe und Pflug in zwei
Kettenphasen) einmal in den Atlas. Zum Begutachten: `build/szenenprobe` schreibt
`ansichten_*.png`, `wende_*.png`, `heli_giert_*.png` und `pflug_wende_*.png`; live hilft
`qdbus6 org.schneefall / org.schneefall.Zeitlupe 0.12` (1 = wieder normal).

### Schneebälle

Auf Schnee drücken (Decke ab 6 px Höhe) und über den Schnee ziehen formt einen Ball, der dabei
wächst – der Schnee fehlt danach in der Decke. Loslassen wirft ihn mit dem Schwung der Maus; er
fliegt im Bogen, dreht sich und zerplatzt auf der nächsten Decke wieder zu Schnee. Ein Treffer
am Fahrzeug lässt es wackeln, Schnee bleibt kleben. Nach `treffer_bis_streik` Treffern bleibt
es stehen: Der Fahrer lehnt sich heraus, droht mit der Faust und schimpft (Sprechblase, mit Ton
ein Murren). Nach sechs Sekunden räumt er weiter. Zwei weitere Treffer während des Streiks oder
eine zweite Runde – dann fliegt er beleidigt ab und kommt zehn Minuten lang nicht wieder.
Ein Klick auf das Fahrzeug stupst es nur an.

**Eingaberegion:** Die Ebenen nehmen die Maus nur auf Schnee über 6 px Höhe und auf Fahrzeugen
an (Wayland-Eingaberegion, alle halbe Sekunde angepasst), überall sonst gehen Klicks durch wie
bisher. Folge: Ein Klick genau auf einen Schneestreifen (z. B. die untersten Pixel über der
Taskleiste, wenn dort Schnee liegt) landet bei Schneefall und nicht im Fenster darunter. Stört
das, „Schnee anfassbar“ abschalten.

## Aussehen

- **Räumdienst:** Hubschrauber (Glaskanzel mit Reflex und Pilot, verwischte Rotorscheibe,
  Heckrotor, Kufen, Positionslichter und Blitzer, Schatten auf dem Schnee), Pistenraupe
  (umlaufende Kettenglieder und drehende Rollen passend zum Tempo, Schild am Schubarm, Kabine mit
  Fahrer, Rundumleuchte, Auspuffrauch je nach Last, Schnee auf Dach und Haube, Federung) und der
  kleine Pflug sind mehrteilige Vektormodelle. `src/fahrzeugbild.cpp` malt die Einzelteile einmal
  je Monitor-Skalierung und Fahrzeuggröße in einen Atlas (scharf bei 1,5×), `src/raeumszene.cpp`
  setzt sie in jedem Bild zusammen. Zum Begutachten ohne Bildschirm: `make szenenprobe`, dann
  `build/szenenprobe <ordner> [dpr] [größe]`.

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
- `src/raeumdienst.{h,cpp}`: Räumdienst und Schneebälle ohne GUI (Einsatz-Zustandsautomat,
  Warteschlange, Seilpendel, Hubschrauber, Fahrzeug, Wulst, Bälle, Treffer und Streik,
  Eingabeflächen, Klangereignisse).
- `src/ansicht3d.{h,cpp}`, `src/fahrzeugmodell.{h,cpp}`: 3D-Ansichten fürs Wenden (ohne Qt).
- `src/raeumszene.{h,cpp}`: setzt aus dem Zustand das Bild zusammen (Liste von Einzelteilen mit
  Lage, Drehung, Maßstab; Wulst als Decke). Ohne Qt.
- `src/fahrzeugbild.{h,cpp}`: malt den Fahrzeug-Atlas mit QPainter.
- `src/overlay.{h,cpp}`: eine durchsichtige Layer-Shell-Ebene (Overlay) je Monitor.
  Sie hat keine Tastatur; die Eingaberegion ist leer oder umfasst nur anfassbaren Schnee und
  Fahrzeuge, alle anderen Klicks gehen durch.
  Gezeichnet wird mit OpenGL in voller Gerätepixel-Auflösung (scharf bei 1,5×).
- `src/bildtakt.{h,cpp}`: Bildzeiten-Messung, Teiler für die Obergrenze, gemeinsamer Taktstand (ohne Qt).
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
~/.local/bin/testlauf 4G 300 make test         # QtTest: Klang, Räumdienst, Kern, Aussehen, Bildtakt, Steuerfenster + Test des KWin-Skripts (node)
~/.local/bin/testlauf 3G 300 make test-bildtakt # nur Bildzeiten, Teiler, Fortschreiben der Flocken, Bildrate
~/.local/bin/testlauf 4G 300 make test-raeumdienst   # nur Räumdienst, Schneebälle und 3D-Ansicht
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

### Fenster am oberen Rand und Vollbild

Im Modus „fest“ tragen Fenster, deren Oberkante am oberen Rand ihres Monitors liegt (maximiert oder
hochgezogen, bis 4 px darunter), und Vollbildfenster keine Schneedecke – sonst blitzte oben ein
Streifen hervor, auch auf einem Monitor darüber. Der Schnee fällt sichtbar davor bis zum unteren
Rand bzw. zur Taskleiste und sammelt sich dort (bis `boden_max`). Fenster dahinter bekommen dort
keinen Schnee. Wird ein Fenster mit Schnee maximiert oder an den oberen Rand geschoben, fällt seine
Decke ab (Brett oder Wolke). Ist „Bei Vollbild pausieren“ an, pausiert der Schnee bei Vollbild wie bisher.

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

- Räumdienst: Der Hubschrauber fliegt in globalen Koordinaten; über Lücken zwischen
  unterschiedlich hohen Monitoren ist er kurz nicht zu sehen. Liegt ein Fenster dicht unter dem
  oberen Bildschirmrand, schwebt er teilweise außerhalb des Bildes.
- Viel Schnee, der auf einen Fleck fällt (Abwurf an der Kante), überschreitet dort die
  Höchstmenge des unteren Fensters; der Überschuss fällt wie üblich an dessen Rand weiter.
- Mausklicks auf anfassbaren Schnee erreichen das Fenster darunter nicht (siehe „Eingaberegion“).

## Geräusche (Teil C)

Alle Geräusche (Hubschrauber, Dieselmotor, Rückfahrwarner, Schnee-Rutsch, Plumps, Schneeball-Wurf/-Treffer, Murren) werden
zur Laufzeit synthetisiert, es gibt keine Audiodateien. Standard: aus (`ton=aus`, `lautstaerke=50` in `schneefall.conf`);
Schalter „Geräusche“ und Regler „Lautstärke“ im Steuerfenster. Ist der Ton aus oder lange still, wird das Audiogerät geschlossen.

- `src/klang.{h,cpp}`: Klasse `Klang` mit `ereignis()`, `dauerklang()`, `dauerklangStopp()`.
- Der Räumdienst meldet Rotor, Motor (Last = Schnee vor dem Schild) und Rückfahrwarner als
  Dauerklänge, dazu Plumps (Aufsetzen, Ball auf Schnee), Rutsch (Abwurf an der Kante), Wurf,
  Treffer und Murren (Streik). Bei Pause oder Vollbild werden alle Dauerklänge gestoppt.
- `make klangprobe`, dann `build/klangprobe` (spielt alles kurz ab), `--wav datei.wav`, `--wav-ordner ordner` (offline rendern), `--cpu` (Last messen).
- Tests: `make test-klang`.
