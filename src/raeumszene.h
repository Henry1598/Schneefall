// Was vom Räumdienst zu zeichnen ist: eine Liste von Einzelteilen (Sprites aus dem Fahrzeug-Atlas)
// mit Lage, Drehung und Maßstab, dazu die Schneewülste vor den Schilden als kleine Decken.
// Reine Logik ohne Qt: aus dem Zustand des Räumdienstes wird das Bild zusammengesetzt.
#pragma once
#include <cstdint>
#include <vector>
#include "ansicht3d.h"
#include "fahrzeugmodell.h"
#include "raeumdienst.h"
#include "schnee.h"

namespace schnee {

// Teile im Fahrzeug-Atlas. Die Fahrzeuge bestehen aus Schichten, die je Winkelstufe (ansicht3d.h) vorgemalt sind.
enum class Teil : uint16_t {
    HeliRotor0, HeliRotor1, HeliRotor2, HeliRotor3,
    LichtRot, LichtGruen, LichtWeiss, LichtOrange, Haken, Seil, Schatten,
    Fahrer, Arm, SchneeDach, SchneeHaube, Klecks, Rauch, Staub, Ball,
    RaupeKoerper,                                        // + Winkelstufe
    RaupeLeer = RaupeKoerper + WINKELSTUFEN,             // ohne Fahrer (er lehnt sich heraus): 0 = nach rechts, 1 = nach links
    RaupeKetteR = RaupeLeer + 2,                         // + Winkelstufe * KETTENPHASEN + Phase
    RaupeKetteL = RaupeKetteR + WINKELSTUFEN * KETTENPHASEN,
    RaupeSchild = RaupeKetteL + WINKELSTUFEN * KETTENPHASEN,
    PflugKoerper = RaupeSchild + WINKELSTUFEN,
    PflugLeer = PflugKoerper + WINKELSTUFEN,
    PflugRad = PflugLeer + 2,                            // + Winkelstufe * RADPHASEN + Phase
    PflugSchild = PflugRad + WINKELSTUFEN * RADPHASEN,
    HeliKoerper = PflugSchild + WINKELSTUFEN,
    Blase0 = HeliKoerper + WINKELSTUFEN,                 // … Blase0 + spruchAnzahl() - 1
};
inline int teilAnzahl() { return int(Teil::Blase0) + spruchAnzahl(); }

struct Zeichenteil {
    uint16_t teil;
    float x, y;      // Lage des Drehpunkts (global logisch)
    float winkel;    // rad, im Uhrzeigersinn
    float sx, sy;    // logische px je Einheit des Teils (sx < 0 = gespiegelt)
    float alpha;
};

struct Szene {
    std::vector<Zeichenteil> teile;  // in Zeichenreihenfolge
    std::vector<Decke> haufen;       // Schneewulst vor dem Schild, gezeichnet wie eine Decke
    void leeren() { teile.clear(); haufen.clear(); }
};

// Maßstab der Sprechblasen (bleiben lesbar, wachsen nur wenig mit)
double blasenSkala(double fahrzeugGroesse);

// Ein einzelnes Fahrzeug im Gierwinkel an der Stelle (x, y) – zum Begutachten aller Ansichten.
// art: 0 = Raupe, 1 = Pflug, 2 = Hubschrauber; lauf: Ketten- bzw. Radweg in Einheiten
void szeneAnsicht(int art, double gier, double x, double y, double lauf, Szene &aus);

// zeit: Sekunden einer beliebigen Uhr (Blinken, Rotorbild)
void szeneBauen(const Raeumdienst &rd, const Simulation &sim, double zeit, Szene &aus);

} // namespace schnee
