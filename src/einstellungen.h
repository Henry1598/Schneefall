// Liest ~/.config/schneefall.conf (einfaches key=value, # Kommentare)
#pragma once
#include <string>
#include "schnee.h"

namespace schnee {
struct Programmeinstellungen {
    Einstellungen sim;
    int fps = 60;
    bool an = true;                 // Schnee sichtbar (Pause = false)
    bool vollbildPausieren = true;  // bei aktivem Vollbildfenster ausblenden
};
// Wertet den Dateiinhalt aus; unbekannte Schlüssel/kaputte Werte werden ignoriert.
Programmeinstellungen einstellungenAusText(const std::string &text);
std::string einstellungenAlsText(const Programmeinstellungen &p);
}
