// Liest ~/.config/schneefall.conf (einfaches key=value, # Kommentare)
#pragma once
#include <string>
#include "schnee.h"

namespace schnee {
struct Programmeinstellungen {
    Einstellungen sim;
    int fps = 30;  // 60 kostet etwa doppelt so viel CPU (schneefall + KWin)
    bool an = true;                 // Schnee sichtbar (Pause = false)
    bool vollbildPausieren = true;  // bei aktivem Vollbildfenster ausblenden
    bool realistisch = true;        // Flockendetail: true = Schneekristalle mit Tiefenwirkung, false = einfache weiche Punkte
    bool ton = false;               // Geräusche (Standard aus)
    int lautstaerke = 50;           // 0 … 100
};
// Wertet den Dateiinhalt aus; unbekannte Schlüssel/kaputte Werte werden ignoriert.
Programmeinstellungen einstellungenAusText(const std::string &text);
std::string einstellungenAlsText(const Programmeinstellungen &p);
}
