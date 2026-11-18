# Schneefall – Build ohne cmake (pkg-config + moc)
CXX      ?= g++
MOC      ?= /usr/lib/qt6/moc
CXXFLAGS ?= -O2 -g -Wall -Wextra
CXXFLAGS += -std=c++20 -fPIC
QT_CFLAGS := $(shell pkg-config --cflags Qt6Widgets Qt6Gui Qt6OpenGL Qt6DBus)
QT_LIBS   := $(shell pkg-config --libs Qt6Widgets Qt6Gui Qt6OpenGL Qt6DBus)
TEST_CFLAGS := $(shell pkg-config --cflags Qt6Test Qt6Core)
TEST_LIBS   := $(shell pkg-config --libs Qt6Test Qt6Core)
MM_CFLAGS := $(shell pkg-config --cflags Qt6Multimedia)
MM_LIBS   := $(shell pkg-config --libs Qt6Multimedia)
LSQ_CFLAGS := -I/usr/include/LayerShellQt
LSQ_LIBS   := -lLayerShellQtInterface

B ?= build
RAEUM_O := $(B)/raeumdienst.o $(B)/raeumszene.o $(B)/fahrzeugbild.o $(B)/ansicht3d.o $(B)/fahrzeugmodell.o

all: $(B)/schneefall

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

$(B)/raeumdienst.o: src/raeumdienst.cpp src/raeumdienst.h src/schnee.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/klang.o: src/klang.cpp src/klang.h | $(B)
	$(CXX) $(CXXFLAGS) $(MM_CFLAGS) -c $< -o $@

$(B)/moc_app.cpp: src/app.h | $(B)
	$(MOC) $< -o $@

$(B)/%.o: src/%.cpp src/app.h src/overlay.h src/steuerfenster.h src/schnee.h src/einstellungen.h src/kristall.h src/deckenform.h src/flockenbild.h src/bildtakt.h src/klang.h src/raeumdienst.h src/raeumszene.h src/fahrzeugbild.h src/ansicht3d.h | $(B)
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) $(LSQ_CFLAGS) -c $< -o $@

$(B)/moc_app.o: $(B)/moc_app.cpp
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) $(LSQ_CFLAGS) -Isrc -c $< -o $@

$(B)/schneefall: $(B)/main.o $(B)/app.o $(B)/overlay.o $(B)/steuerfenster.o $(B)/moc_app.o $(B)/schnee.o $(B)/einstellungen.o $(B)/kristall.o $(B)/deckenform.o $(B)/flockenbild.o $(B)/bildtakt.o $(B)/klang.o $(RAEUM_O)
	$(CXX) $(LDFLAGS) $^ -o $@ $(QT_LIBS) $(MM_LIBS) $(LSQ_LIBS)

$(B)/test_schnee.moc: tests/test_schnee.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_schnee: tests/test_schnee.cpp $(B)/test_schnee.moc $(B)/schnee.o $(B)/einstellungen.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_schnee.cpp $(B)/schnee.o $(B)/einstellungen.o -o $@ $(TEST_LIBS)

$(B)/test_bild.moc: tests/test_bild.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_bild: tests/test_bild.cpp $(B)/test_bild.moc src/kristall.h src/deckenform.h src/flockenbild.h $(B)/kristall.o $(B)/deckenform.o $(B)/flockenbild.o $(B)/schnee.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_bild.cpp $(B)/kristall.o $(B)/deckenform.o $(B)/flockenbild.o $(B)/schnee.o -o $@ $(TEST_LIBS)

$(B)/test_bildtakt.moc: tests/test_bildtakt.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_bildtakt: tests/test_bildtakt.cpp $(B)/test_bildtakt.moc src/bildtakt.h src/flockenbild.h $(B)/bildtakt.o $(B)/flockenbild.o $(B)/einstellungen.o $(B)/schnee.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_bildtakt.cpp $(B)/bildtakt.o $(B)/flockenbild.o $(B)/einstellungen.o $(B)/schnee.o -o $@ $(TEST_LIBS)

test-bildtakt: $(B)/test_bildtakt
	QT_QPA_PLATFORM=offscreen ./$(B)/test_bildtakt

$(B)/test_steuerfenster.moc: tests/test_steuerfenster.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_steuerfenster: tests/test_steuerfenster.cpp $(B)/test_steuerfenster.moc $(B)/app.o $(B)/overlay.o $(B)/steuerfenster.o $(B)/moc_app.o $(B)/schnee.o $(B)/einstellungen.o $(B)/kristall.o $(B)/deckenform.o $(B)/flockenbild.o $(B)/bildtakt.o $(B)/klang.o $(RAEUM_O)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(QT_CFLAGS) $(TEST_CFLAGS) $(LSQ_CFLAGS) -Isrc -I$(B) tests/test_steuerfenster.cpp $(B)/app.o $(B)/overlay.o $(B)/steuerfenster.o $(B)/moc_app.o $(B)/schnee.o $(B)/einstellungen.o $(B)/kristall.o $(B)/deckenform.o $(B)/flockenbild.o $(B)/bildtakt.o $(B)/klang.o $(RAEUM_O) -o $@ $(QT_LIBS) $(MM_LIBS) $(TEST_LIBS) $(LSQ_LIBS)

# ---- Räumdienst (Teil B) ----
$(B)/ansicht3d.o: src/ansicht3d.cpp src/ansicht3d.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/fahrzeugmodell.o: src/fahrzeugmodell.cpp src/fahrzeugmodell.h src/ansicht3d.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/raeumszene.o: src/raeumszene.cpp src/raeumszene.h src/raeumdienst.h src/schnee.h src/ansicht3d.h src/fahrzeugmodell.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/fahrzeugbild.o: src/fahrzeugbild.cpp src/fahrzeugbild.h src/raeumszene.h src/raeumdienst.h src/schnee.h src/ansicht3d.h src/fahrzeugmodell.h | $(B)
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) -c $< -o $@

$(B)/szenenprobe: tools/szenenprobe.cpp $(RAEUM_O) $(B)/schnee.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(QT_CFLAGS) -Isrc tools/szenenprobe.cpp $(RAEUM_O) $(B)/schnee.o -o $@ $(QT_LIBS)

szenenprobe: $(B)/szenenprobe

$(B)/test_raeumdienst.moc: tests/test_raeumdienst.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_raeumdienst: tests/test_raeumdienst.cpp $(B)/test_raeumdienst.moc $(B)/raeumdienst.o $(B)/schnee.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_raeumdienst.cpp $(B)/raeumdienst.o $(B)/schnee.o -o $@ $(TEST_LIBS)

$(B)/test_ansicht.moc: tests/test_ansicht.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_ansicht: tests/test_ansicht.cpp $(B)/test_ansicht.moc $(B)/ansicht3d.o $(B)/fahrzeugmodell.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_ansicht.cpp $(B)/ansicht3d.o $(B)/fahrzeugmodell.o -o $@ $(TEST_LIBS)

test-raeumdienst: $(B)/test_raeumdienst $(B)/test_ansicht
	QT_QPA_PLATFORM=offscreen ./$(B)/test_ansicht
	QT_QPA_PLATFORM=offscreen ./$(B)/test_raeumdienst

# ---- Geräusche (Teil C) ----
$(B)/test_klang.moc: tests/test_klang.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_klang: tests/test_klang.cpp $(B)/test_klang.moc $(B)/klang.o $(B)/einstellungen.o $(B)/schnee.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) $(MM_CFLAGS) -Isrc -I$(B) tests/test_klang.cpp $(B)/klang.o $(B)/einstellungen.o $(B)/schnee.o -o $@ $(TEST_LIBS) $(MM_LIBS)

$(B)/klangprobe: tools/klangprobe.cpp $(B)/klang.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(MM_CFLAGS) -Isrc tools/klangprobe.cpp $(B)/klang.o -o $@ $(MM_LIBS)

klangprobe: $(B)/klangprobe

test-klang: $(B)/test_klang
	QT_QPA_PLATFORM=offscreen ./$(B)/test_klang

test: $(B)/test_klang $(B)/test_ansicht $(B)/test_raeumdienst $(B)/test_schnee $(B)/test_bild $(B)/test_bildtakt $(B)/test_steuerfenster
	QT_QPA_PLATFORM=offscreen ./$(B)/test_klang
	QT_QPA_PLATFORM=offscreen ./$(B)/test_ansicht
	QT_QPA_PLATFORM=offscreen ./$(B)/test_raeumdienst
	QT_QPA_PLATFORM=offscreen ./$(B)/test_schnee
	QT_QPA_PLATFORM=offscreen ./$(B)/test_bild
	QT_QPA_PLATFORM=offscreen ./$(B)/test_bildtakt
	QT_QPA_PLATFORM=offscreen ./$(B)/test_steuerfenster
	node tests/test_kwin.js

# KWin-Skript installieren/aktualisieren und in kwinrc einschalten (ohne KWin-Neustart)
kwin-skript:
	kpackagetool6 --type=KWin/Script -i kwin 2>/dev/null || kpackagetool6 --type=KWin/Script -u kwin
	kwriteconfig6 --file kwinrc --group Plugins --key schneefall-fensterEnabled true
	qdbus6 org.kde.KWin /KWin reconfigure || dbus-send --session --dest=org.kde.KWin --type=method_call /KWin org.kde.KWin.reconfigure

clean:
	rm -rf $(B)

.PHONY: test-bildtakt szenenprobe test-raeumdienst klangprobe test-klang all test kwin-skript install clean

# Befehl schneefall-umschalten und Desktop-Starter für den aktuellen Benutzer
install: all
	install -Dm755 skripte/schneefall-umschalten $(HOME)/.local/bin/schneefall-umschalten
	install -Dm644 skripte/schneefall.desktop $(HOME)/.local/share/applications/schneefall.desktop
