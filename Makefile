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

B := build

all: $(B)/schneefall

$(B):
	mkdir -p $(B)

$(B)/schnee.o: src/schnee.cpp src/schnee.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/einstellungen.o: src/einstellungen.cpp src/einstellungen.h src/schnee.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/kristall.o: src/kristall.cpp src/kristall.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/deckenform.o: src/deckenform.cpp src/deckenform.h src/schnee.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/flockenbild.o: src/flockenbild.cpp src/flockenbild.h src/schnee.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/klang.o: src/klang.cpp src/klang.h | $(B)
	$(CXX) $(CXXFLAGS) $(MM_CFLAGS) -c $< -o $@

$(B)/moc_app.cpp: src/app.h | $(B)
	$(MOC) $< -o $@

$(B)/%.o: src/%.cpp src/app.h src/overlay.h src/steuerfenster.h src/schnee.h src/einstellungen.h src/kristall.h src/deckenform.h src/flockenbild.h src/klang.h | $(B)
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) $(LSQ_CFLAGS) -c $< -o $@

$(B)/moc_app.o: $(B)/moc_app.cpp
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) $(LSQ_CFLAGS) -Isrc -c $< -o $@

$(B)/schneefall: $(B)/main.o $(B)/app.o $(B)/overlay.o $(B)/steuerfenster.o $(B)/moc_app.o $(B)/schnee.o $(B)/einstellungen.o $(B)/kristall.o $(B)/deckenform.o $(B)/flockenbild.o $(B)/klang.o
	$(CXX) $(LDFLAGS) $^ -o $@ $(QT_LIBS) $(MM_LIBS) $(LSQ_LIBS)

$(B)/test_schnee.moc: tests/test_schnee.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_schnee: tests/test_schnee.cpp $(B)/test_schnee.moc $(B)/schnee.o $(B)/einstellungen.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_schnee.cpp $(B)/schnee.o $(B)/einstellungen.o -o $@ $(TEST_LIBS)

$(B)/test_bild.moc: tests/test_bild.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_bild: tests/test_bild.cpp $(B)/test_bild.moc src/kristall.h src/deckenform.h src/flockenbild.h $(B)/kristall.o $(B)/deckenform.o $(B)/flockenbild.o $(B)/schnee.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_bild.cpp $(B)/kristall.o $(B)/deckenform.o $(B)/flockenbild.o $(B)/schnee.o -o $@ $(TEST_LIBS)

$(B)/test_steuerfenster.moc: tests/test_steuerfenster.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_steuerfenster: tests/test_steuerfenster.cpp $(B)/test_steuerfenster.moc $(B)/app.o $(B)/overlay.o $(B)/steuerfenster.o $(B)/moc_app.o $(B)/schnee.o $(B)/einstellungen.o $(B)/kristall.o $(B)/deckenform.o $(B)/flockenbild.o $(B)/klang.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(QT_CFLAGS) $(TEST_CFLAGS) $(LSQ_CFLAGS) -Isrc -I$(B) tests/test_steuerfenster.cpp $(B)/app.o $(B)/overlay.o $(B)/steuerfenster.o $(B)/moc_app.o $(B)/schnee.o $(B)/einstellungen.o $(B)/kristall.o $(B)/deckenform.o $(B)/flockenbild.o $(B)/klang.o -o $@ $(QT_LIBS) $(MM_LIBS) $(TEST_LIBS) $(LSQ_LIBS)

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

test: $(B)/test_klang $(B)/test_schnee $(B)/test_bild $(B)/test_steuerfenster
	QT_QPA_PLATFORM=offscreen ./$(B)/test_klang
	QT_QPA_PLATFORM=offscreen ./$(B)/test_schnee
	QT_QPA_PLATFORM=offscreen ./$(B)/test_bild
	QT_QPA_PLATFORM=offscreen ./$(B)/test_steuerfenster
	node tests/test_kwin.js

# KWin-Skript installieren/aktualisieren und in kwinrc einschalten (ohne KWin-Neustart)
kwin-skript:
	kpackagetool6 --type=KWin/Script -i kwin 2>/dev/null || kpackagetool6 --type=KWin/Script -u kwin
	kwriteconfig6 --file kwinrc --group Plugins --key schneefall-fensterEnabled true
	qdbus6 org.kde.KWin /KWin reconfigure || dbus-send --session --dest=org.kde.KWin --type=method_call /KWin org.kde.KWin.reconfigure

clean:
	rm -rf $(B)

.PHONY: klangprobe test-klang all test kwin-skript install clean

# Befehl schneefall-umschalten und Desktop-Starter für den aktuellen Benutzer
install: all
	install -Dm755 skripte/schneefall-umschalten $(HOME)/.local/bin/schneefall-umschalten
	install -Dm644 skripte/schneefall.desktop $(HOME)/.local/share/applications/schneefall.desktop
