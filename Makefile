# Schneefall – Build ohne cmake (pkg-config + moc)
# Plattformweiche: make PLATTFORM=linux (Standard, KDE Plasma/Wayland) oder make PLATTFORM=windows (MSYS2 UCRT64)
PLATTFORM ?= linux
CXX      ?= g++
CXXFLAGS ?= -O2 -g -Wall -Wextra
CXXFLAGS += -std=c++20 -fPIC -Isrc
# Version: zentral in der Datei VERSION (auch für --version, Steuerfenster, Paket und Installer)
VERSION := $(strip $(shell cat VERSION))
CXXFLAGS += -DSCHNEEFALL_VERSION='"$(VERSION)"'

ifeq ($(PLATTFORM),windows)
EXE        := .exe
QT_LIBEXEC := $(shell pkg-config --variable=libexecdir Qt6Core)
MOC        ?= $(QT_LIBEXEC)/moc
RCC        ?= $(QT_LIBEXEC)/rcc
QT_MODULE  := Qt6Widgets Qt6Gui Qt6OpenGL Qt6Network
# kein LayerShellQt, kein QtDBus; Win32-Bibliotheken für das Windows-Backend (Schritt 3)
LSQ_CFLAGS :=
LSQ_LIBS   := -lgdi32 -ldwmapi -lshcore -ldxgi -luser32 -lshell32 -lopengl32 -ladvapi32
# Steuerkanal: Benutzer-SID (Pipe-Name, Prüfung des Servers)
KANAL_LIBS := -ladvapi32
PLAT_DATEIEN := fensterquelle ebene systemkanal autostart
PLAT_MOC   :=
PLAT_EXTRA_O = $(B)/fensterauswahl.o $(B)/schneefall_rc.o
WINDRES    ?= windres
# Programm ohne Konsolenfenster (Ausgaben von --status gehen in die aufrufende Konsole, siehe main.cpp)
APP_LDFLAGS := -mwindows
# Tests nur für das Windows-Backend
BACKEND_TESTS := test_autostart_win test_pipe_win
else ifeq ($(PLATTFORM),linux)
EXE        :=
MOC        ?= /usr/lib/qt6/moc
RCC        ?= /usr/lib/qt6/rcc
QT_MODULE  := Qt6Widgets Qt6Gui Qt6OpenGL Qt6DBus Qt6Network
LSQ_CFLAGS := -I/usr/include/LayerShellQt
LSQ_LIBS   := -lLayerShellQtInterface
PLAT_DATEIEN := kwinfensterquelle layerebene dbusdienst autostart
PLAT_MOC   := moc_dbusdienst
KANAL_LIBS :=
# Tests nur für das Linux-Backend
BACKEND_TESTS := test_kwinquelle test_autostart
else
$(error PLATTFORM muss linux oder windows sein)
endif

QT_CFLAGS := $(shell pkg-config --cflags $(QT_MODULE))
QT_LIBS   := $(shell pkg-config --libs $(QT_MODULE))
TEST_CFLAGS := $(shell pkg-config --cflags Qt6Test Qt6Core)
TEST_LIBS   := $(shell pkg-config --libs Qt6Test Qt6Core)
NET_CFLAGS := $(shell pkg-config --cflags Qt6Network)
NET_LIBS   := $(shell pkg-config --libs Qt6Network)
MM_CFLAGS := $(shell pkg-config --cflags Qt6Multimedia)
MM_LIBS   := $(shell pkg-config --libs Qt6Multimedia)

B ?= build
SPRUECHE_O := $(B)/sprueche.o $(B)/qrc_sprueche.o
STAT_O := $(B)/statistik.o
RAEUM_O := $(B)/raeumdienst.o $(B)/raeumszene.o $(B)/fahrzeugbild.o $(B)/ansicht3d.o $(B)/fahrzeugmodell.o $(B)/blasen.o
# Plattform-Schicht: Schnittstellen in src/plattform/*.h, plattformneutral pfade/steuerkanal, Backend in src/plattform/$(PLATTFORM)/
PLAT_H := src/plattform/fensterquelle.h src/plattform/ebene.h src/plattform/steuerkanal.h src/plattform/autostart.h src/plattform/pfade.h \
          $(wildcard src/plattform/$(PLATTFORM)/*.h)
PLAT_O := $(B)/pfade.o $(B)/steuerkanal.o $(patsubst %,$(B)/pl_%.o,$(PLAT_DATEIEN) $(PLAT_MOC)) $(PLAT_EXTRA_O)
# alles, was die App (ohne main) braucht
APP_O := $(B)/app.o $(B)/overlay.o $(B)/steuerfenster.o $(B)/moc_app.o $(B)/schnee.o $(B)/einstellungen.o $(B)/kristall.o $(B)/deckenform.o \
         $(B)/flockenbild.o $(B)/bildtakt.o $(B)/klang.o $(SPRUECHE_O) $(RAEUM_O) $(STAT_O) $(PLAT_O)

all: $(B)/schneefall$(EXE)

$(B):
	mkdir -p $(B)

$(B)/schnee.o: src/schnee.cpp src/schnee.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/einstellungen.o: src/einstellungen.cpp src/einstellungen.h src/schnee.h src/raeumdienst.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/kristall.o: src/kristall.cpp src/kristall.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/deckenform.o: src/deckenform.cpp src/deckenform.h src/schnee.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/bildtakt.o: src/bildtakt.cpp src/bildtakt.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/flockenbild.o: src/flockenbild.cpp src/flockenbild.h src/schnee.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/raeumdienst.o: src/raeumdienst.cpp src/raeumdienst.h src/schnee.h src/ansicht3d.h src/fahrzeugmodell.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/klang.o: src/klang.cpp src/klang.h | $(B)
	$(CXX) $(CXXFLAGS) $(MM_CFLAGS) -c $< -o $@

$(B)/pfade.o: src/plattform/pfade.cpp src/plattform/pfade.h | $(B)
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -c $< -o $@

$(B)/steuerkanal.o: src/plattform/steuerkanal.cpp src/plattform/steuerkanal.h | $(B)
	$(CXX) $(CXXFLAGS) $(TEST_CFLAGS) $(NET_CFLAGS) -c $< -o $@

$(B)/pl_%.o: src/plattform/$(PLATTFORM)/%.cpp $(PLAT_H) src/schnee.h | $(B)
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) $(LSQ_CFLAGS) -c $< -o $@

$(B)/moc_dbusdienst.cpp: src/plattform/linux/dbusdienst.h | $(B)
	$(MOC) $< -o $@

$(B)/pl_moc_dbusdienst.o: $(B)/moc_dbusdienst.cpp
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -c $< -o $@

$(B)/moc_app.cpp: src/app.h | $(B)
	$(MOC) $< -o $@

$(B)/%.o: src/%.cpp src/app.h src/overlay.h src/steuerfenster.h src/schnee.h src/einstellungen.h src/kristall.h src/deckenform.h src/flockenbild.h src/bildtakt.h src/klang.h src/raeumdienst.h src/raeumszene.h src/fahrzeugbild.h src/ansicht3d.h src/sprueche.h src/blasen.h src/fahrzeugmodell.h src/statistik.h src/version.h VERSION $(PLAT_H) | $(B)
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -c $< -o $@

$(B)/moc_app.o: $(B)/moc_app.cpp
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -c $< -o $@

# Programm-Symbol und VERSIONINFO (nur Windows); VERSION "1.2.3[-x]" -> 1,2,3,0
VERSION_ZAHL := $(shell echo '$(VERSION)' | sed -E 's/^([0-9]+)\.([0-9]+)\.([0-9]+).*/\1,\2,\3,0/')
$(B)/schneefall_rc.o: pakete/windows/schneefall.rc symbole/schneefall.ico VERSION | $(B)
	$(WINDRES) -DVERSION_TEXT='\"$(VERSION)\"' -DVERSION_ZAHL=$(VERSION_ZAHL) -I. -i $< -o $@

$(B)/schneefall$(EXE): $(B)/main.o $(APP_O)
	$(CXX) $(LDFLAGS) $(APP_LDFLAGS) $^ -o $@ $(QT_LIBS) $(MM_LIBS) $(LSQ_LIBS)

$(B)/test_schnee.moc: tests/test_schnee.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_schnee$(EXE): tests/test_schnee.cpp $(B)/test_schnee.moc $(B)/schnee.o $(B)/einstellungen.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_schnee.cpp $(B)/schnee.o $(B)/einstellungen.o -o $@ $(TEST_LIBS)

$(B)/test_bild.moc: tests/test_bild.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_bild$(EXE): tests/test_bild.cpp $(B)/test_bild.moc src/kristall.h src/deckenform.h src/flockenbild.h $(B)/kristall.o $(B)/deckenform.o $(B)/flockenbild.o $(B)/schnee.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_bild.cpp $(B)/kristall.o $(B)/deckenform.o $(B)/flockenbild.o $(B)/schnee.o -o $@ $(TEST_LIBS)

$(B)/test_bildtakt.moc: tests/test_bildtakt.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_bildtakt$(EXE): tests/test_bildtakt.cpp $(B)/test_bildtakt.moc src/bildtakt.h src/flockenbild.h $(B)/bildtakt.o $(B)/flockenbild.o $(B)/einstellungen.o $(B)/schnee.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_bildtakt.cpp $(B)/bildtakt.o $(B)/flockenbild.o $(B)/einstellungen.o $(B)/schnee.o -o $@ $(TEST_LIBS)

test-bildtakt: $(B)/test_bildtakt$(EXE)
	QT_QPA_PLATFORM=offscreen ./$(B)/test_bildtakt

$(B)/test_steuerfenster.moc: tests/test_steuerfenster.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_steuerfenster$(EXE): tests/test_steuerfenster.cpp tests/fakefensterquelle.h $(B)/test_steuerfenster.moc $(APP_O)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(QT_CFLAGS) $(TEST_CFLAGS) -I$(B) tests/test_steuerfenster.cpp $(APP_O) -o $@ $(QT_LIBS) $(MM_LIBS) $(TEST_LIBS) $(LSQ_LIBS)

# ---- Statistik (Zähler, Speicherung) ----
$(B)/test_statistik.moc: tests/test_statistik.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_statistik$(EXE): tests/test_statistik.cpp $(B)/test_statistik.moc $(B)/statistik.o $(B)/pfade.o $(B)/raeumdienst.o $(B)/schnee.o $(B)/ansicht3d.o $(SPRUECHE_O)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -I$(B) tests/test_statistik.cpp $(B)/statistik.o $(B)/pfade.o $(B)/raeumdienst.o $(B)/schnee.o $(B)/ansicht3d.o $(SPRUECHE_O) -o $@ $(TEST_LIBS)

test-statistik: $(B)/test_statistik$(EXE)
	QT_QPA_PLATFORM=offscreen ./$(B)/test_statistik$(EXE)

# ---- Plattform-Schicht ----
$(B)/test_app.moc: tests/test_app.cpp | $(B)
	$(MOC) $< -o $@

# App mit nachgebildeter Fensterquelle (unabhängig vom Backend)
$(B)/test_app$(EXE): tests/test_app.cpp tests/fakefensterquelle.h $(B)/test_app.moc $(APP_O)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(QT_CFLAGS) $(TEST_CFLAGS) -I$(B) tests/test_app.cpp $(APP_O) -o $@ $(QT_LIBS) $(MM_LIBS) $(TEST_LIBS) $(LSQ_LIBS)

$(B)/test_steuerkanal.moc: tests/test_steuerkanal.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_steuerkanal$(EXE): tests/test_steuerkanal.cpp $(B)/test_steuerkanal.moc $(B)/steuerkanal.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) $(NET_CFLAGS) -I$(B) tests/test_steuerkanal.cpp $(B)/steuerkanal.o -o $@ $(NET_LIBS) $(TEST_LIBS) $(KANAL_LIBS)

# Windows-Fensterquelle, plattformneutraler Teil (Filter, DPI) – wird auf beiden Plattformen getestet
$(B)/fensterauswahl.o: src/plattform/windows/fensterauswahl.cpp src/plattform/windows/fensterauswahl.h src/schnee.h | $(B)
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -c $< -o $@

$(B)/test_fensterauswahl.moc: tests/test_fensterauswahl.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_fensterauswahl$(EXE): tests/test_fensterauswahl.cpp $(B)/test_fensterauswahl.moc $(B)/fensterauswahl.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(QT_CFLAGS) $(TEST_CFLAGS) -I$(B) tests/test_fensterauswahl.cpp $(B)/fensterauswahl.o -o $@ $(QT_LIBS) $(TEST_LIBS)

$(B)/test_kwinquelle.moc: tests/test_kwinquelle.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_kwinquelle: tests/test_kwinquelle.cpp $(B)/test_kwinquelle.moc $(B)/pl_kwinfensterquelle.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -I$(B) tests/test_kwinquelle.cpp $(B)/pl_kwinfensterquelle.o -o $@ $(TEST_LIBS)

$(B)/test_pipe_win.moc: tests/test_pipe_win.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_pipe_win$(EXE): tests/test_pipe_win.cpp $(B)/test_pipe_win.moc $(B)/steuerkanal.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) $(NET_CFLAGS) -I$(B) tests/test_pipe_win.cpp $(B)/steuerkanal.o -o $@ $(NET_LIBS) $(TEST_LIBS) $(KANAL_LIBS)

$(B)/test_autostart_win.moc: tests/test_autostart_win.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_autostart_win$(EXE): tests/test_autostart_win.cpp $(B)/test_autostart_win.moc $(B)/pl_autostart.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -I$(B) tests/test_autostart_win.cpp $(B)/pl_autostart.o -o $@ $(TEST_LIBS)

$(B)/test_autostart.moc: tests/test_autostart.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_autostart: tests/test_autostart.cpp $(B)/test_autostart.moc $(B)/pl_autostart.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -I$(B) tests/test_autostart.cpp $(B)/pl_autostart.o -o $@ $(TEST_LIBS)

test-plattform: $(B)/test_steuerkanal$(EXE) $(B)/test_app$(EXE) $(patsubst %,$(B)/%$(EXE),$(BACKEND_TESTS))
	QT_QPA_PLATFORM=offscreen ./$(B)/test_steuerkanal$(EXE)
	QT_QPA_PLATFORM=offscreen ./$(B)/test_app$(EXE)
	@set -e; for t in $(BACKEND_TESTS); do QT_QPA_PLATFORM=offscreen ./$(B)/$$t; done

# ---- Räumdienst (Teil B) ----
$(B)/ansicht3d.o: src/ansicht3d.cpp src/ansicht3d.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/fahrzeugmodell.o: src/fahrzeugmodell.cpp src/fahrzeugmodell.h src/ansicht3d.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/blasen.o: src/blasen.cpp src/blasen.h | $(B)
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -c $< -o $@

$(B)/raeumszene.o: src/raeumszene.cpp src/raeumszene.h src/raeumdienst.h src/schnee.h src/ansicht3d.h src/fahrzeugmodell.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/fahrzeugbild.o: src/fahrzeugbild.cpp src/fahrzeugbild.h src/raeumszene.h src/raeumdienst.h src/schnee.h src/ansicht3d.h src/fahrzeugmodell.h | $(B)
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -c $< -o $@

$(B)/szenenprobe$(EXE): tools/szenenprobe.cpp $(RAEUM_O) $(B)/schnee.o $(SPRUECHE_O)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(QT_CFLAGS) -Isrc tools/szenenprobe.cpp $(RAEUM_O) $(B)/schnee.o $(SPRUECHE_O) -o $@ $(QT_LIBS)

szenenprobe: $(B)/szenenprobe$(EXE)

$(B)/test_raeumdienst.moc: tests/test_raeumdienst.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_raeumdienst$(EXE): tests/test_raeumdienst.cpp $(B)/test_raeumdienst.moc $(B)/raeumdienst.o $(B)/schnee.o $(B)/ansicht3d.o $(SPRUECHE_O)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_raeumdienst.cpp $(B)/raeumdienst.o $(B)/schnee.o $(B)/ansicht3d.o $(SPRUECHE_O) -o $@ $(TEST_LIBS)

$(B)/test_ansicht.moc: tests/test_ansicht.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_ansicht$(EXE): tests/test_ansicht.cpp $(B)/test_ansicht.moc $(B)/ansicht3d.o $(B)/fahrzeugmodell.o $(B)/raeumszene.o $(B)/raeumdienst.o $(B)/schnee.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_ansicht.cpp $(B)/ansicht3d.o $(B)/fahrzeugmodell.o $(B)/raeumszene.o $(B)/raeumdienst.o $(B)/schnee.o -o $@ $(TEST_LIBS)

$(B)/test_blasen.moc: tests/test_blasen.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_blasen$(EXE): tests/test_blasen.cpp $(B)/test_blasen.moc $(B)/blasen.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(QT_CFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_blasen.cpp $(B)/blasen.o -o $@ $(QT_LIBS) $(TEST_LIBS)

test-raeumdienst: $(B)/test_raeumdienst$(EXE) $(B)/test_ansicht$(EXE) $(B)/test_blasen$(EXE)
	QT_QPA_PLATFORM=offscreen ./$(B)/test_ansicht
	QT_QPA_PLATFORM=offscreen ./$(B)/test_blasen
	QT_QPA_PLATFORM=offscreen ./$(B)/test_raeumdienst

# ---- Geräusche (Teil C) ----
$(B)/test_klang.moc: tests/test_klang.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_klang$(EXE): tests/test_klang.cpp $(B)/test_klang.moc $(B)/klang.o $(B)/einstellungen.o $(B)/schnee.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) $(MM_CFLAGS) -Isrc -I$(B) tests/test_klang.cpp $(B)/klang.o $(B)/einstellungen.o $(B)/schnee.o -o $@ $(TEST_LIBS) $(MM_LIBS)

$(B)/klangprobe$(EXE): tools/klangprobe.cpp $(B)/klang.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(MM_CFLAGS) -Isrc tools/klangprobe.cpp $(B)/klang.o -o $@ $(MM_LIBS)

klangprobe: $(B)/klangprobe$(EXE)

test-klang: $(B)/test_klang$(EXE)
	QT_QPA_PLATFORM=offscreen ./$(B)/test_klang

TESTS := test_klang test_ansicht test_blasen test_raeumdienst test_schnee test_bild test_bildtakt test_steuerfenster test_sprueche \
         test_statistik test_steuerkanal test_app test_fensterauswahl
test: $(patsubst %,$(B)/%$(EXE),$(TESTS)) $(patsubst %,$(B)/%$(EXE),$(BACKEND_TESTS)) $(B)/schneefall$(EXE)
	@test "$$(./$(B)/schneefall$(EXE) --version | tr -d '\r')" = "schneefall $(VERSION)" && echo "PASS: --version = $(VERSION)"
	@./$(B)/schneefall$(EXE) --diagnose-pfade | tr -d '\r' | grep -q '^programmordner=.' && echo "PASS: --diagnose-pfade"
	@set -e; for t in $(TESTS) $(BACKEND_TESTS); do QT_QPA_PLATFORM=offscreen ./$(B)/$$t$(EXE); done
ifeq ($(PLATTFORM),linux)
	node tests/test_kwin.js
endif

# KWin-Skript installieren/aktualisieren und in kwinrc einschalten (ohne KWin-Neustart)
kwin-skript:
	kpackagetool6 --type=KWin/Script -i kwin 2>/dev/null || kpackagetool6 --type=KWin/Script -u kwin
	kwriteconfig6 --file kwinrc --group Plugins --key schneefall-fensterEnabled true
	qdbus6 org.kde.KWin /KWin reconfigure || dbus-send --session --dest=org.kde.KWin --type=method_call /KWin org.kde.KWin.reconfigure

clean:
	rm -rf $(B)

# Windows-Paket (nur in MSYS2 UCRT64): Ordner Schneefall/ und Schneefall-<version>-windows.zip in dist/
paket:
	bash pakete/windows/paket.sh

.PHONY: paket test-plattform test-statistik test-sprueche test-bildtakt szenenprobe test-raeumdienst klangprobe test-klang all test kwin-skript install clean

# Befehl schneefall-umschalten und Desktop-Starter für den aktuellen Benutzer
install: all
	install -Dm755 skripte/schneefall-umschalten $(HOME)/.local/bin/schneefall-umschalten
	install -Dm644 skripte/schneefall.desktop $(HOME)/.local/share/applications/schneefall.desktop
	install -Dm644 symbole/schneefall.svg $(HOME)/.local/share/icons/hicolor/scalable/apps/schneefall.svg

# ---- Sprüche (Fahrer und Pilot) ----

$(B)/sprueche.o: src/sprueche.cpp src/sprueche.h | $(B)
	$(CXX) $(CXXFLAGS) $(TEST_CFLAGS) -c $< -o $@

$(B)/qrc_sprueche.cpp: daten/sprueche.qrc daten/sprueche.tsv | $(B)
	$(RCC) --name sprueche $< -o $@

$(B)/qrc_sprueche.o: $(B)/qrc_sprueche.cpp
	$(CXX) $(CXXFLAGS) $(TEST_CFLAGS) -c $< -o $@

$(B)/test_sprueche.moc: tests/test_sprueche.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_sprueche$(EXE): tests/test_sprueche.cpp $(B)/test_sprueche.moc $(B)/sprueche.o $(B)/qrc_sprueche.o $(B)/einstellungen.o $(B)/schnee.o daten/sprueche.tsv
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -DSPRUECHE_TSV='"$(CURDIR)/daten/sprueche.tsv"' -Isrc -I$(B) tests/test_sprueche.cpp $(B)/sprueche.o $(B)/qrc_sprueche.o $(B)/einstellungen.o $(B)/schnee.o -o $@ $(TEST_LIBS)

test-sprueche: $(B)/test_sprueche$(EXE)
	QT_QPA_PLATFORM=offscreen ./$(B)/test_sprueche
