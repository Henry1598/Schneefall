# Schneefall – Build ohne cmake (pkg-config + moc)
CXX      ?= g++
MOC      ?= /usr/lib/qt6/moc
CXXFLAGS ?= -O2 -g -Wall -Wextra
CXXFLAGS += -std=c++20 -fPIC
QT_CFLAGS := $(shell pkg-config --cflags Qt6Gui Qt6OpenGL Qt6DBus)
QT_LIBS   := $(shell pkg-config --libs Qt6Gui Qt6OpenGL Qt6DBus)
TEST_CFLAGS := $(shell pkg-config --cflags Qt6Test Qt6Core)
TEST_LIBS   := $(shell pkg-config --libs Qt6Test Qt6Core)
LSQ_CFLAGS := -I/usr/include/LayerShellQt
LSQ_LIBS   := -lLayerShellQtInterface

B := build
PREFIX ?= $(HOME)/.local

all: $(B)/schneefall

$(B):
	mkdir -p $(B)

$(B)/schnee.o: src/schnee.cpp src/schnee.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/einstellungen.o: src/einstellungen.cpp src/einstellungen.h src/schnee.h | $(B)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(B)/moc_app.cpp: src/app.h | $(B)
	$(MOC) $< -o $@

$(B)/%.o: src/%.cpp src/app.h src/schnee.h | $(B)
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) $(LSQ_CFLAGS) -c $< -o $@

$(B)/moc_app.o: $(B)/moc_app.cpp
	$(CXX) $(CXXFLAGS) $(QT_CFLAGS) $(LSQ_CFLAGS) -Isrc -c $< -o $@

$(B)/schneefall: $(B)/main.o $(B)/app.o $(B)/overlay.o $(B)/moc_app.o $(B)/schnee.o $(B)/einstellungen.o
	$(CXX) $^ -o $@ $(QT_LIBS) $(LSQ_LIBS)

$(B)/test_schnee.moc: tests/test_schnee.cpp | $(B)
	$(MOC) $< -o $@

$(B)/test_schnee: tests/test_schnee.cpp $(B)/test_schnee.moc $(B)/schnee.o $(B)/einstellungen.o
	$(CXX) $(CXXFLAGS) $(TEST_CFLAGS) -Isrc -I$(B) tests/test_schnee.cpp $(B)/schnee.o $(B)/einstellungen.o -o $@ $(TEST_LIBS)

test: $(B)/test_schnee
	QT_QPA_PLATFORM=offscreen ./$(B)/test_schnee

install: all
	install -Dm755 $(B)/schneefall $(PREFIX)/bin/schneefall

clean:
	rm -rf $(B)

.PHONY: all test install clean
