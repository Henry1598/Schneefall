# Schneefall – Build ohne cmake (pkg-config + moc)
CXX      ?= g++
MOC      ?= /usr/lib/qt6/moc
CXXFLAGS ?= -O2 -g -Wall -Wextra
CXXFLAGS += -std=c++20 -fPIC
QT_CFLAGS := $(shell pkg-config --cflags Qt6Widgets Qt6Gui Qt6OpenGL Qt6DBus)
QT_LIBS   := $(shell pkg-config --libs Qt6Widgets Qt6Gui Qt6OpenGL Qt6DBus)
TEST_CFLAGS := $(shell pkg-config --cflags Qt6Test Qt6Core)
TEST_LIBS   := $(shell pkg-config --libs Qt6Test Qt6Core)
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

$(B)/moc_app.cpp: src/app.h | $(B)
	$(MOC) $< -o $@

$(B)/%.o: src/%.cpp src/app.h src/overlay.h src/steuerfenster.h src/schnee.h src/einstellungen.h | $(B)
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) $(LSQ_CFLAGS) -c $< -o $@

$(B)/moc_app.o: $(B)/moc_app.cpp
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) $(LSQ_CFLAGS) -Isrc -c $< -o $@

$(B)/schneefall: $(B)/main.o $(B)/app.o $(B)/overlay.o $(B)/steuerfenster.o $(B)/moc_app.o $(B)/schnee.o $(B)/einstellungen.o
	$(CXX) $^ -o $@ $(QT_LIBS) $(LSQ_LIBS)

$(B)/test_schnee.moc: tests/test_schnee.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_schnee: tests/test_schnee.cpp $(B)/test_schnee.moc $(B)/schnee.o $(B)/einstellungen.o
	$(CXX) $(CXXFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_schnee.cpp $(B)/schnee.o $(B)/einstellungen.o -o $@ $(TEST_LIBS)

test: $(B)/test_schnee
	QT_QPA_PLATFORM=offscreen ./$(B)/test_schnee
	node tests/test_kwin.js

# KWin-Skript installieren/aktualisieren und in kwinrc einschalten (ohne KWin-Neustart)
kwin-skript:
	kpackagetool6 --type=KWin/Script -i kwin 2>/dev/null || kpackagetool6 --type=KWin/Script -u kwin
	kwriteconfig6 --file kwinrc --group Plugins --key schneefall-fensterEnabled true
	qdbus6 org.kde.KWin /KWin reconfigure || dbus-send --session --dest=org.kde.KWin --type=method_call /KWin org.kde.KWin.reconfigure

clean:
	rm -rf $(B)

.PHONY: all test kwin-skript install clean

# Befehl schneefall-umschalten und Desktop-Starter für den aktuellen Benutzer
install: all
	install -Dm755 skripte/schneefall-umschalten $(HOME)/.local/bin/schneefall-umschalten
	install -Dm644 skripte/schneefall.desktop $(HOME)/.local/share/applications/schneefall.desktop
