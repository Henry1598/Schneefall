# Schneefall

Schnee fällt über alle Fenster auf KDE Plasma 6 (Wayland). Er bleibt auf den
sichtbaren Fensteroberkanten und auf der Taskleiste liegen und rutscht an den
Fensterenden ab. Wird ein Fenster verschoben, geschlossen, minimiert oder
verdeckt, fällt sein Schnee herunter. Am unteren Bildschirmrand sammelt sich
Schnee und schmilzt langsam.

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
- **Regler:** Menge, Wind (← links / → rechts), Flockengröße, max. Deckenhöhe, Fallgeschwindigkeit.
- **Schnee abschütteln:** Alle Schneedecken fallen herunter.
- **Schnee fällt vor Fenstern:** Aus = fallender Schnee ist nur über der Arbeitsfläche
  zu sehen, als fiele er hinter den Fenstern. Die Decken auf den Fensterkanten bleiben sichtbar.
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
| `vor_fenstern` | Schnee fällt vor den Fenstern | true/false | true |
| `vollbild_pausieren` | bei Vollbild pausieren | true/false | true |

`fps=60` sieht etwas flüssiger aus, kostet aber etwa doppelt so viel CPU (schneefall + KWin).

## Aufbau

- `src/schnee.{h,cpp}`: Simulationskern ohne GUI (Flocken, Decken je Fensterkante,
  Verdeckung nach Stapelreihenfolge, Abrutschen, Zerfall, Boden mit Schmelzen).
- `src/overlay.{h,cpp}`: eine durchsichtige Layer-Shell-Ebene (Overlay) je Monitor.
  Sie hat keine Tastatur und eine leere Eingaberegion, Klicks gehen also durch.
  Gezeichnet wird mit OpenGL in voller Gerätepixel-Auflösung (scharf bei 1,5×).
- `src/app.{h,cpp}`: D-Bus-Dienst `org.schneefall` (Sitzungsbus), Monitore, Takt, Systemabschnitt.
- `src/steuerfenster.{h,cpp}`: Steuerfenster.
- `kwin/`: KWin-Skript `schneefall-fenster`. Es meldet bei jeder Änderung (und alle 2 s)
  alle Fenster mit Position, Stapelplatz, minimiert, Vollbild, aktiv und Dock an `org.schneefall`.
  Läuft schneefall nicht, gehen die Aufrufe ins Leere.

## Bauen und Testen

cmake wird nicht gebraucht, es reicht ein Makefile mit pkg-config.

```
cd ~/Schneefall
~/.local/bin/testlauf 4G 300 make -j8          # Programm bauen
~/.local/bin/testlauf 4G 300 make test         # QtTest-Kern + Test des KWin-Skripts (node)
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
