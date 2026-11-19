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

## Download und Installation (Windows)

Fertige Pakete gibt es **nur** unter [github.com/Henry1598/Schneefall/releases](https://github.com/Henry1598/Schneefall/releases)
(nicht von anderen Webseiten, aus Foren oder Messengern):

| Datei | Zweck |
|---|---|
| `Schneefall-<version>-windows-Setup.exe` | Installer (empfohlen), Deutsch und Englisch |
| `Schneefall-<version>-windows.zip` | portabel: entpacken, `Schneefall\schneefall.exe` starten |
| `SHA256SUMS` | Prüfsummen der beiden Dateien |
| `pakete.txt` | alle MSYS2-Pakete (mit Version) der Bauumgebung |

**Systemanforderungen:** Windows 10 ab Version 1903 oder Windows 11, 64 Bit; Grafik mit OpenGL 3.3 oder höher
(jeder gängige Grafiktreiber). Ohne brauchbaren OpenGL-Treiber (VM, Fernsitzung) nimmt Qt automatisch das mitgelieferte
Software-OpenGL (Mesa, `opengl32sw.dll`); das ist langsamer. Auf einer echten GPU wird immer das native OpenGL des Treibers genutzt.

**Reihenfolge beim ersten Start:**

1. Datei nur von der Release-Seite oben laden (`SHA256SUMS` aus demselben Release dazu).
2. Prüfsumme vergleichen. PowerShell, im Download-Ordner, ersetze `<version>` durch die heruntergeladene Version; Ausgabe
   `True` heißt: stimmt mit `SHA256SUMS` überein, bei `False` Datei löschen und nicht starten:

   ```
   (Get-Content .\SHA256SUMS | Select-String 'Schneefall-<version>-windows-Setup.exe').Line.Split(' ')[0] -eq (Get-FileHash .\Schneefall-<version>-windows-Setup.exe -Algorithm SHA256).Hash.ToLower()
   ```

   (für die ZIP entsprechend `Schneefall-<version>-windows.zip`). Ehrlich dazu: Die Prüfsumme zeigt nur, dass die Datei
   unterwegs nicht verändert wurde. Sie beweist nicht, woher sie stammt, denn `SHA256SUMS` liegt am selben Ort.
3. Herkunft prüfen: Die Pakete tragen eine Build-Provenance-Attestation von GitHub Actions (belegt, aus welchem Commit
   und welchem Workflow die Datei gebaut wurde). Mit der [GitHub-CLI](https://cli.github.com/):

   ```
   gh attestation verify .\Schneefall-<version>-windows-Setup.exe --repo Henry1598/Schneefall
   ```

4. Erst dann starten. Die Dateien sind **nicht signiert** (kein Code-Signing-Zertifikat); Windows SmartScreen zeigt deshalb
   „Der Computer wurde durch Windows geschützt“. Nach den Schritten 1 bis 3: **„Weitere Informationen“ → „Trotzdem ausführen“**.
   Der Quelltext liegt in diesem Repository, die Pakete entstehen in der öffentlichen CI (GitHub Actions).

**Lizenzen der Bibliotheken:** Das Paket enthält Qt (LGPL), Mesa, LLVM, FFmpeg und weitere Bibliotheken aus MSYS2. Die Liste
(Datei, Paket, Version, Lizenz) steht in `DRITTANBIETER.txt`, die Lizenztexte im Ordner `lizenzen`. Schneefall selbst: alle Rechte
beim Autor, eine Lizenz folgt ggf.

**Installation:** `Setup.exe` starten. Es ist **kein Administrator** nötig; installiert wird pro Benutzer nach
`%LOCALAPPDATA%\Programs\Schneefall`. Auswählbar sind Desktop-Symbol und „Schneefall mit Windows starten“ (derselbe Autostart wie das
Häkchen „Mit dem System starten“ im Steuerfenster). Startmenü-Einträge: „Schneefall“ und „Schneefall beenden“. Eine laufende
Instanz wird beim Aktualisieren und Deinstallieren automatisch beendet.

**Deinstallation:** Einstellungen → Apps → „Schneefall“ → Deinstallieren (oder Startmenü). Der Deinstaller fragt, ob auch die
Einstellungen und die Statistik (`%APPDATA%\Schneefall`) gelöscht werden sollen; Standard ist **Nein** (behalten). Bei stiller
Deinstallation (`/VERYSILENT`) bleiben sie immer erhalten.

**Version:** `schneefall.exe --version` (aus einer Konsole, am besten mit Umleitung: `schneefall.exe --version > v.txt`) und unten links
im Steuerfenster. Die Version steht zentral in der Datei `VERSION`.

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
~/Schneefall/build/schneefall --abschuetteln
~/Schneefall/build/schneefall --werfen 2000 500 400 -300 150   # Schneeball: x y vx vy Menge
~/Schneefall/build/schneefall --zeitlupe 0.2                   # Zeitlupe (1 = normal)
qdbus6 org.schneefall / org.schneefall.Werfen 2000 500 400 -300 150   # dasselbe über D-Bus
```

### Einzelinstanz und Sicherheit der Befehlswege

- Nur eine Instanz je Benutzer: Sie hält die Sperrdatei `$XDG_RUNTIME_DIR/schneefall.sock.lock`
  (ohne Laufzeitordner: eigener 0700-Ordner `/tmp/schneefall-<uid>/` mit Besitzerprüfung, zuletzt
  `~/.schneefall-<Rechner>.sock`, damit sich Rechner mit gemeinsamem NFS-Home nicht in die Quere kommen)
  (Windows: `%LOCALAPPDATA%\Schneefall\<Pipe-Name>.lock`). Nach einem Absturz wird die Sperre übernommen,
  sobald der alte Prozess nicht mehr lebt; nur wer sie hält, ersetzt einen verwaisten Socket. Lässt sich
  der Steuerkanal nicht öffnen, meldet der Start einen Fehler (statt „läuft schon“). Starten zwei Instanzen
  gleichzeitig, öffnet die zweite nur das Steuerfenster der ersten.
- Windows: Der Pipe-Namensraum ist für alle Benutzer gemeinsam. Schneefall legt die Pipe deshalb selbst als
  erste Instanz an (`FILE_FLAG_FIRST_PIPE_INSTANCE`, `PIPE_REJECT_REMOTE_CLIENTS`), Besitzer und einziger
  Berechtigter ist der eigene Benutzer. Hat ein fremder Prozess den Namen vorher belegt, startet Schneefall
  nicht („Pipe … von fremdem Prozess belegt“), statt ihm Befehle zu schicken. Der Server prüft je Verbindung
  den Benutzer des Clients, der Client vor dem Senden den Besitzer der Pipe. Läuft Schneefall über UAC erhöht,
  trägt die Pipe die hohe Integritätsstufe: Befehle aus einer normalen Konsole kommen dann nicht an (mit
  Hinweis beim Start und klarer Meldung beim Befehl); umgekehrt erreicht eine Administrator-Konsole eine
  normal gestartete Instanz.
- Steuerkanal: Socket nur für den eigenen Benutzer, je Verbindung ein Befehl, höchstens 2 s und
  16 Verbindungen gleichzeitig; Anfragen bis 64 KiB, Antworten bis 4 MiB; der Client wartet insgesamt
  höchstens 3 s. Zahlen müssen endlich sein
  (`nan`/`inf` → „unbekannter Befehl“); Würfe werden auf ±5000 px um die Bildschirme und 2600 px/s
  begrenzt, höchstens 64 Bälle fliegen gleichzeitig, jeder höchstens 20 s.
- Ausgaben von `--status`, `--fensterliste` usw. erscheinen ohne Steuerzeichen (außer Zeilenumbruch/Tab),
  ohne Bidi-/Formatzeichen (z. B. U+202E) und ohne Zeilen-/Absatztrenner.
- D-Bus `org.schneefall` liegt im Sitzungsbus. **Jeder Prozess der Sitzung** kann ihn aufrufen – und den
  Namen auch vorab belegen, bevor Schneefall startet. Dann gilt Schneefall als „läuft schon“, und das
  KWin-Skript schickt seine Fensterliste an den fremden Prozess. Das ist kein Schutz gegen Programme, die
  ohnehin unter dem eigenen Benutzer laufen; die Fensterliste aus `Fenster(json)` wird deshalb streng
  geprüft (höchstens 1 MiB, 1000 Fenster, id bis 128 Zeichen ohne Steuerzeichen, endliche Zahlen,
  Lage ±100000 px, Größe bis 100000 px). Der Kern legt über alle Decken zusammen höchstens 500 000 Spalten
  an; weitere Fenster tragen dann keinen Schnee.
- Autostart: Der Programmpfad steht nach der Desktop-Entry-Spezifikation gequotet in `Exec=` (Leerzeichen,
  `"`, `$`, `` ` `` geprüft mit `gio launch` und `systemd-xdg-autostart-generator`); Pfade mit Steuerzeichen,
  `=`, `%` oder Backslash werden abgelehnt (Hinweis im Steuerfenster), weil diese Starter sie nicht
  zuverlässig zurückübersetzen.

## Steuerfenster

- **Schnee an:** Pause, das Programm läuft weiter (Symbol bleibt im Systemabschnitt).
- **Regler:** Menge, Wind (← links / → rechts), Flockengröße, max. Deckenhöhe (bis 80 px), Fallgeschwindigkeit.
- **Flockendetail:** „Realistisch“ (Standard) zeigt echte sechszählige Schneekristalle mit
  Tiefenwirkung, „Einfach“ alle Flocken als weiche Punkte.
- **Schnee abschütteln:** Alle Schneedecken fallen herunter.
- **Gruppe „Fenster“:**
  - **Bildrate:** 60 Bilder/s (Standard), Auto (so oft wie der Monitor, am flüssigsten), 30, 120 oder
  144 als Obergrenze. Gemessen mit 4 Monitoren (165/144/144/60 Hz), ca. 3 500 Flocken: 30 → 4,5 %,
  60 → 7 %, Auto → 10 % eines Kerns (vorher fester 30-fps-Takt: 4–5 %), KWin je nach Rate +0 bis +4 Punkte.
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
| `bildrate` | Bilder/s je Monitor: `auto` = so oft wie der Monitor (z. B. 144/165 Hz), sonst Obergrenze | auto, 10 … 360 | 60 |
| `fps` | alter fester Takt; wird nur noch für ältere Versionen mitgeschrieben (fehlt `bildrate`, zählt ein selbst gewählter `fps`-Wert, der alte Standard 30 wird zu 60) | 10 … 120 | 30 |
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
   (siehe „Fahrzeuge und Wenden“), andere Seite. Vor dem Schild türmt sich der Wulst; was über die Fensterkante ragt, fällt als
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

### Fahrzeuge und Wenden

Raupe, Pflug und Hubschrauber sind 3D-Modelle (`src/fahrzeugmodell.cpp`): Quader mit gerundeten Kanten,
Zylinder, Drehkörper, Schalen und Prismen. Alle Ansichten – auch die gewohnte Seitenansicht – stammen aus
demselben Modell, es gibt keinen Wechsel zwischen „Seitenbild“ und „Drehbild“ mehr.

- **Raupe (Kettendozer):** Wanne, abgeschrägte Haube mit Lüftungsschlitzen, Kühler, Scheinwerfer, Kabine mit
  Holmen, Scheiben, Spiegeln, Türgriff und Handlauf, Fahrer mit Helm und Warnjacke hinter Glas, Auspuff mit
  Regenklappe, Luftfilter, Gegengewicht mit Warnschraffur und Rücklichtern, Rundumleuchte. Ketten mit
  Trieb-, Leit-, Lauf- und Stützrollen; die Glieder laufen in sechs Phasen je Glied sichtbar in Fahrtrichtung,
  beim Drehen auf der Stelle gegenläufig. Schild mit blanker Arbeitsfläche, Schneidkante, Schubrahmen und
  Hydraulikzylindern; es hebt und senkt sich.
- **Kleiner Pflug:** ein Radfahrzeug (Kommunalschlepper mit Fahrerhaus, Pritsche mit Streuer, Schneeschild
  mit Warnfahnen). Die Räder haben Profil und Felgen und drehen sich mit dem Weg. Er dreht **nicht** auf
  der Stelle, sondern wendet wie ein Auto in Zügen: Bogen vor, umlenken, Bogen zurück (mit Rückfahrwarner),
  Bogen vor – drei Züge, bei wenig Platz fünf oder sieben. Die Vorderräder schlagen sichtbar ein.
- **Hubschrauber:** runder Rumpf, Glaskanzel mit Pilot, Seitenfenster und Türfuge, Triebwerksverkleidung mit
  Lufteinlass und Abgasrohr, Heckausleger mit Kennung, Leitwerk, Höhenflosse mit Endscheiben, Heckrotor,
  Kufen. Der Hauptrotor ist in jeder Gierlage ein Blattkreis mit verwischten Blättern und kippt beim
  Beschleunigen etwas weiter als der Rumpf. Beim Richtungswechsel giert er über Front bzw. Heck.
- **Raupe wendet** auf der Stelle: Schild hoch, 180° in 1,5 bis 2,5 s (je nach Größe, weich an- und
  auslaufend), Motor unter Last, Ruck beim Anfahren und Anhalten, die Ketten wirbeln Schnee auf. Der Drehsinn
  wechselt von Einsatz zu Einsatz (einmal über die Front-, einmal über die Heckansicht). Reicht der Platz
  nicht, rangiert sie vorher zur Mitte. Ein Schneeballtreffer mitten in der Wende zählt, der Fahrer
  beschwert sich aber erst danach.

Technik: `src/ansicht3d.{h,cpp}` rastert die Modelle ohne Qt mit Tiefenpuffer und Überabtastung: weiches,
weltfestes Licht von oben links (dreht nicht mit), Glanz je Werkstoff (Lack, Metall, Gummi, Glas),
durchscheinende Scheiben, Muster (Kettenglieder, Reifenprofil, Kühler, Warnschraffur, Kennung), Schmutz
unten, Schnee auf waagerechten Flächen, Umgebungsverdeckung in Ecken und eine dunkle Außenkante. Jedes
Fahrzeug besteht aus Schichten (Raupe: Körper, zwei Ketten, Schild; Pflug: Körper, vier Räder, Schild), die
je 15° Gierwinkel einmal in den Atlas gemalt und beim Zeichnen nach Tiefe sortiert zusammengesetzt werden.
Der Atlas entsteht im Hintergrund auf mehreren Kernen. Zum Begutachten: `build/szenenprobe <ordner>`
schreibt `ansichten_*.png` (zwölf Gierwinkel je Fahrzeug), `wende_*.png`, `pflug_wende_*.png` und
`heli_giert_*.png`; live hilft `qdbus6 org.schneefall / org.schneefall.Zeitlupe 0.12` (1 = wieder normal).

### Schieben

Der Schnee vor dem Schild staucht sich zu einem wälzenden Wulst mit Buckeln und aufliegenden Brocken, die
langsam nach vorn rollen; das Schild wirft seinen Schatten darauf. Pulver stiebt am Schild und an den
Ketten auf, kleine Bröckchen kullern den Wulst hinunter. Hinter dem Fahrzeug bleibt eine dünne,
festgefahrene Restschicht mit Kettenabdruck liegen (knapp 1 px, sie verblasst beim Abholen bzw. schneit
wieder zu). An der Fensterkante hängt der Schnee erst über und bricht dann in zwei bis vier ungleichen
Schollen ab – mal früher, mal später. Die Schollen drehen sich im Fallen, ziehen eine Pulverfahne, verlieren
Bröckchen und schlagen mit einer Staubwolke auf; dort bildet sich ein Haufen. Die Schneemenge bleibt dabei
erhalten.

### Keine Flocken über maximierten und Vollbild-Fenstern

Im Modus „fest“ tragen Fenster am oberen Monitorrand und Vollbildfenster keinen Schnee; Flocken fallen dort
unsichtbar hindurch. Damit auch kein Flockenrand hervorblitzt (eine Flocke, deren Mittelpunkt knapp über der
Fensterkante oder auf dem Monitor darüber liegt, ragt mit ihrem bis zu 64 px großen Bild hinein), schneidet
der Flocken-Shader diese Fensterflächen pixelgenau aus (`Simulation::flockenfreieFlaechen()`, bis zu acht
Rechtecke je Monitor). Im Modus „dahinter“ gilt das für alle Fenster außer der Taskleiste.

### Was geräumt wird

Geräumt wird nur, was man sieht – das gilt für die automatische Buchung und für „Jetzt räumen“:
nicht bei minimierten oder Vollbildfenstern, nicht bei Fenstern, deren Oberkante am oberen Rand ihres
Monitors liegt (maximiert, höchstens 4 px darunter), nicht bei Kanten, die zu mehr als 40 % verdeckt sind,
und nicht, wenn der längste freie Abschnitt für Fahrzeug und Wende zu kurz ist. Wird ein Fenster während
eines Einsatzes maximiert, ins Vollbild geschaltet oder verdeckt, bricht der Einsatz ab und der Hubschrauber
holt das Fahrzeug ab.

### Sprüche

Fahrer und Pilot haben zusammen gut 1000 Sätze (`daten/sprueche.tsv`, ins Programm eingebettet). Jeder
Treffer wird kommentiert, mit jedem weiteren deutlicher – gezählt über den ganzen Einsatz; wie deutlich
höchstens, bestimmt „Ausdrucksweise“ (`sprueche_max_stufe`, 1 freundlich … 5 richtig vulgär). Der Fahrer
grüßt beim Dienstantritt und beim Anstupsen, schimpft beim Streik und beim beleidigten Abflug. Der Pilot
meldet sich, wenn der Hubschrauber getroffen wird, wenn er den beleidigten Fahrer abholt und beim Abflug.
Bei Ballserien gibt es höchstens alle 1,2 s (Pilot 1,5 s) einen neuen Satz. Die Sprechblasen werden je
Satz einmal gemalt (`src/blasen.{h,cpp}`: Vorrat von sechs Plätzen je Monitor, Zeilenumbruch, scharf je
Skalierung), ploppen auf, stehen je nach Textlänge 2 bis 6,5 s und blenden aus; am rechten Bildrand klappen
sie nach links. Zu jeder Blase gehört ein Murren (nur mit Ton hörbar).

### Schneebälle

Auf Schnee drücken (Decke ab 6 px Höhe) und über den Schnee ziehen formt einen Ball, der dabei
wächst – der Schnee fehlt danach in der Decke. Loslassen wirft ihn mit dem Schwung der Maus; er
fliegt im Bogen, dreht sich und zerplatzt auf der nächsten Decke wieder zu Schnee. Ein Treffer
am Fahrzeug lässt es wackeln, Schnee bleibt kleben. Nach `treffer_bis_streik` Treffern bleibt
es stehen: Der Fahrer steigt aus, steht auf der Kette (beim Pflug auf der Trittstufe), droht mit der Faust und schimpft (Sprechblase, mit Ton
ein Murren). Nach sechs Sekunden räumt er weiter. Zwei weitere Treffer während des Streiks oder
eine zweite Runde – dann fliegt er beleidigt ab und kommt zehn Minuten lang nicht wieder.
Ein Klick auf das Fahrzeug stupst es nur an.

**Eingaberegion:** Die Ebenen nehmen die Maus nur auf Schnee über 6 px Höhe und auf Fahrzeugen
an (Wayland-Eingaberegion, alle halbe Sekunde angepasst), überall sonst gehen Klicks durch wie
bisher. Folge: Ein Klick genau auf einen Schneestreifen (z. B. die untersten Pixel über der
Taskleiste, wenn dort Schnee liegt) landet bei Schneefall und nicht im Fenster darunter. Stört
das, „Schnee anfassbar“ abschalten.

## Statistik

Schneefall zählt mit (Linux und Windows gleich, das Modul `src/statistik.{h,cpp}` ist reines Qt Core):

| Zähler | Wann |
|---|---|
| Einsätze gestartet / abgeschlossen / abgebrochen (davon Taskleisten-Pflug) | Abgeschlossen = Fahrzeug regulär abgeholt; abgebrochen = Fenster minimiert/geschlossen/verdeckt oder beleidigter Abflug |
| Geräumt (Schneemenge) | was das Fahrzeug über die Kante schiebt, in Einheiten wie die Schneehöhe (Tooltip am Wert „Geräumt“) |
| Schneebälle geworfen, Treffer Fahrzeug, Treffer Hubschrauber, daneben | daneben = Ball landet auf einem Fenster oder verlässt den Bildschirm |
| Trefferquote | Treffer / (Treffer + daneben) in Prozent; Bälle noch in der Luft zählen nicht |
| Gestreikt, beleidigte Abflüge, Sprüche gesagt | je Ereignis |

Anzeige: Gruppe „Statistik“ im Steuerfenster (live, einmal je Sekunde), Knopf „Statistik zurücksetzen“
(mit Rückfrage), Tooltip der Schneeflocke im Systemabschnitt („Schneefall – 12× geräumt, 37 Treffer, 3 Streiks“)
und eine Zeile `statistik=geraeumt:12,gestartet:…` am Ende von `--status`.
Gespeichert wird in `~/.config/schneefall-statistik.conf` (neben `schneefall.conf`; unter Windows im
entsprechenden Konfigurationsordner), höchstens alle 30 s und beim Beenden, atomar. Eine fehlende oder
kaputte Datei ergibt eine Zählung ab null; die Datei löschen setzt die Statistik ebenfalls zurück.

## Aussehen

- **Räumdienst:** siehe „Fahrzeuge und Wenden“. `src/fahrzeugbild.cpp` baut den Atlas je
  Monitor-Skalierung und Fahrzeuggröße, `src/raeumszene.cpp` setzt die Schichten in jedem Bild zusammen.
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
- `src/statistik.{h,cpp}`: Zähler und Speicherung der Statistik (nur Qt Core); der Räumdienst legt
  `Statistikereignis`se ab (`statistikAbholen()`), `App` verbucht sie je Schritt.
- `src/ansicht3d.{h,cpp}`, `src/fahrzeugmodell.{h,cpp}`: 3D-Modelle der Fahrzeuge und ihr Rasterer (ohne Qt).
- `src/raeumszene.{h,cpp}`: setzt aus dem Zustand das Bild zusammen (Liste von Einzelteilen mit
  Lage, Drehung, Maßstab; Wulst als Decke). Ohne Qt.
- `src/fahrzeugbild.{h,cpp}`: baut den Fahrzeug-Atlas (3D-Ansichten, dazu Rotor, Lichter, Sprechblasen mit QPainter).
- `src/overlay.{h,cpp}`: eine durchsichtige Ebene (Overlay) je Monitor, plattformneutral gezeichnet;
  Fensterart, Eingaberegion und Takt kommen aus `plattform::Ebene` (Linux: Layer-Shell).
  Sie hat keine Tastatur; die Eingaberegion ist leer oder umfasst nur anfassbaren Schnee und
  Fahrzeuge, alle anderen Klicks gehen durch.
  Gezeichnet wird mit OpenGL in voller Gerätepixel-Auflösung (scharf bei 1,5×).
- `src/bildtakt.{h,cpp}`: Bildzeiten-Messung, Teiler für die Obergrenze, gemeinsamer Taktstand (ohne Qt).
- `src/app.{h,cpp}`: Befehle (`App::befehl`), Monitore, Takt, Systemabschnitt; Fenster kommen aus einer
  `plattform::Fensterquelle`.
- `src/plattform/`: Plattform-Schicht (für die Windows-Version). Schnittstellen:
  - `fensterquelle.h` – Fensterliste (Linux: `linux/kwinfensterquelle` – das KWin-Skript über D-Bus),
  - `ebene.h` – Overlay-Fenster je Monitor (Linux: `linux/layerebene.cpp` – LayerShellQt),
  - `steuerkanal.{h,cpp}` – Einzelinstanz und Befehle über `QLocalSocket`
    (Linux: `QLocalServer` auf `$XDG_RUNTIME_DIR/schneefall.sock`, Windows: eigener abgesicherter Pipe-Server,
    benannte Pipe je Benutzer-SID und Sitzung) mit
    Sperrdatei für die Einzelinstanz; dazu der Systemkanal
    (Linux: `linux/dbusdienst` – D-Bus `org.schneefall` wie bisher, das KWin-Skript braucht ihn),
  - `autostart.h` – Linux `.desktop`, Windows Registry `HKCU\…\Run`,
  - `pfade.{h,cpp}` – Ablage der Einstellungen (Linux `~/.config/schneefall.conf`, Windows `%APPDATA%\Schneefall\`).
  - `windows/`: Windows-Backend:
    - `ebene.cpp` – je Monitor ein randloses, durchsichtiges Topmost-Fenster (Layered + DWM-Blur mit leerer
      Region für Alpha je Pixel), nicht in Taskleiste/Alt-Tab, Per-Monitor-DPI v2. Durchklicken über
      `WS_EX_TRANSPARENT`, das 60-mal je Sekunde nach der Mausposition umgeschaltet wird (nur über anfassbarem
      Schnee/Fahrzeugen klickbar). Takt: ein Thread je Monitor wartet auf das VBlank (`IDXGIOutput::WaitForVBlank`,
      sonst `DwmFlush`, zuletzt Schlafen) und zählt den Teiler selbst.
    - `fensterquelle.cpp` – `EnumWindows` + `SetWinEventHook` (außerhalb des Prozesses), Rahmen über
      `DWMWA_EXTENDED_FRAME_BOUNDS`, verhüllte Fenster (virtuelle Desktops, UWP) über `DWMWA_CLOAKED`.
      Gemeldet wird nur bei Änderung, gebündelt nach höchstens 15 ms, dazu ein Abgleich je Sekunde.
    - `fensterauswahl.{h,cpp}` – plattformneutral und getestet: Filter (Shell-, Werkzeug-, eigene Fenster),
      Stapel, Vollbild, Taskleisten als Dock (auch automatisch ausgeblendet), minimierte Fenster mit letzter
      Lage, Umrechnung physischer in logische Koordinaten je Monitor (wie Qt bei gemischter Skalierung).
    - `autostart.cpp` – Registry `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.
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
~/.local/bin/testlauf 4G 300 make -j8          # Programm bauen (PLATTFORM=linux ist Standard; PLATTFORM=windows für MSYS2)
~/.local/bin/testlauf 3G 300 make test-plattform    # nur Steuerkanal, App mit nachgebildeter Fensterquelle, KWin-Liste, Autostart
~/.local/bin/testlauf 4G 300 make test         # QtTest: Klang, Räumdienst, Kern, Aussehen, Bildtakt, Steuerfenster + Test des KWin-Skripts (node)
~/.local/bin/testlauf 3G 300 make test-bildtakt # nur Bildzeiten, Teiler, Fortschreiben der Flocken, Bildrate
~/.local/bin/testlauf 4G 300 make test-raeumdienst   # nur Räumdienst, Schneebälle und 3D-Ansicht
tests/rauchtest.sh                             # Live-Test der D-Bus-Schnittstelle (Sitzung nötig)
make install                                   # schneefall-umschalten + Starter
make kwin-skript                               # KWin-Skript installieren/aktualisieren und einschalten
```

Windows (MSYS2 UCRT64, Qt 6 aus `mingw-w64-ucrt-x86_64-qt6-*`):

```
mingw32-make PLATTFORM=windows B=build-win -j8        # schneefall.exe (Windows-Programm ohne Konsolenfenster)
mingw32-make PLATTFORM=windows B=build-win -j8 test   # alle Tests, dazu Autostart (Registry) und Pipe-Sicherheit
```

- `schneefall.exe` ist ein Windows-Programm (`-mwindows`). Aus einer Konsole gestartet, schreibt es Ausgaben
  von `--status` usw. dorthin (bzw. in eine Umleitung); aus dem Explorer gestartet, erscheinen Meldungen als Fenster.
  Weil die Konsole nicht auf ein Windows-Programm wartet, für Ausgaben am besten umleiten
  (`schneefall.exe --status > status.txt`) oder `start /wait` nutzen.
- Ohne GPU-Treiber (z. B. VM) braucht Qt Software-OpenGL: Mesa (`mingw-w64-ucrt-x86_64-mesa`), dessen
  `opengl32.dll` als `opengl32sw.dll` neben `schneefall.exe`. Schneefall setzt `GALLIUM_DRIVER=llvmpipe`
  als Vorgabe (der D3D12-Treiber zeigt in Layered-Fenstern nichts). Fehlt OpenGL ganz, startet Schneefall
  ohne Schnee-Ebene und meldet das.
- Einstellungen und Statistik: `%APPDATA%\Schneefall\`, Sperrdatei `%LOCALAPPDATA%\Schneefall\`.

Windows-Paket und Installer (in MSYS2 UCRT64; zusätzlich `mingw-w64-ucrt-x86_64-{mesa,ntldd}` und `zip`; Inno Setup 6 für den Installer):

```
mingw32-make PLATTFORM=windows paket     # Release-Build, windeployqt, DLLs per ntldd -> dist/Schneefall/ + dist/Schneefall-<version>-windows.zip
bash pakete/windows/installer.sh         # dist/Schneefall-<version>-windows-Setup.exe + dist/SHA256SUMS (pakete/windows/schneefall.iss)
```

- `pakete/windows/paket.sh` ermittelt die nötigen ucrt64-DLLs mit `ntldd` (Runden, bis nichts Neues kommt, nichts geraten) und bricht ab, wenn eine DLL fehlt
  oder `--version` nicht zur Datei `VERSION` passt. Die Mesa-DLL kommt nur als `opengl32sw.dll` ins Paket (Rückfall); eine
  `opengl32.dll` im Ordner würde den Grafiktreiber verdecken und wird deshalb nie beigelegt (das Skript prüft das).
- Release: Datei `VERSION` anpassen, Tag `vX.Y.Z` setzen und pushen; `.github/workflows/ci.yml` baut, testet und veröffentlicht Setup.exe,
  ZIP und `SHA256SUMS` (der Tag muss zu `VERSION` passen). Push und Pull Requests führen Linux- und Windows-Tests aus.

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
- Windows: Das Durchklicken wird 60-mal je Sekunde nach der Mausposition umgeschaltet. Ein sehr schneller
  Klick direkt nach dem Bewegen vom Schnee weg kann deshalb selten noch beim Schnee landen.
- Windows: Ohne Grafiktreiber (Software-OpenGL, z. B. in einer VM) sind Bildrate und CPU-Last nicht
  aussagekräftig.

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

## Sprüche (Fahrer und Hubschrauberpilot)

`daten/sprueche.tsv` enthält über 1000 Sätze (`sprecher<TAB>anlass<TAB>stufe<TAB>text`), von freundlich (Stufe 1) bis richtig vulgär (Stufe 5).
Das Modul `src/sprueche.{h,cpp}` wählt zufällig ohne Wiederholung der letzten 50 Sätze; die Stufe steigt mit der Trefferzahl und ist durch
`sprueche_max_stufe` (Steuerfenster: „Ausdrucksweise“, Standard 3 = frech) begrenzt. Angebunden im Räumdienst, siehe „Sprüche“ weiter oben.
Tests: `make test-sprueche`.
