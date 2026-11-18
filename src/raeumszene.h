// Was vom Räumdienst zu zeichnen ist: eine Liste von Einzelteilen (Sprites aus dem Fahrzeug-Atlas)
// mit Lage, Drehung und Maßstab, dazu die Schneewülste vor den Schilden als kleine Decken.
// Reine Logik ohne Qt: aus dem Zustand des Räumdienstes wird das Bild zusammengesetzt.
#pragma once
#include <cstdint>
#include <vector>
#include "ansicht3d.h"
#include "raeumdienst.h"
#include "schnee.h"

namespace schnee {

// Teile im Fahrzeug-Atlas (fahrzeugbild.cpp malt sie in genau dieser Reihenfolge)
enum class Teil : uint8_t {
    HeliRumpf, HeliRotor0, HeliRotor1, HeliRotor2, Heckrotor0, Heckrotor1, Heckrotor2,
    LichtRot, LichtGruen, LichtWeiss, LichtOrange, Haken, Seil, Schatten,
    RaupeUnterwagen, Kettenglied, Laufrolle, Antriebsrad, Leitrad, RaupeAufbau, RaupeGlas, Fahrer, Arm, RaupeSchild,
    SchneeDach, SchneeHaube, Klecks,
    PflugRumpf, Rad, PflugSchild,
    Rauch, Staub, Ball,
    // vorgemalte 3D-Ansichten fürs Wenden: je Winkelstufe (ansicht3d.h), Raupe und Pflug in zwei Ketten-/Radphasen
    Raupe3D,
    Pflug3D = Raupe3D + 2 * 24,
    Heli3D = Pflug3D + 2 * 24,
    Blase0 = Heli3D + 24, // … Blase0 + spruchAnzahl() - 1
};
static_assert(WINKELSTUFEN == 24, "Teil-Nummern passen zu 24 Winkelstufen");
inline int teilAnzahl() { return int(Teil::Blase0) + spruchAnzahl(); }

struct Zeichenteil {
    uint8_t teil;
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

// zeit: Sekunden einer beliebigen Uhr (Blinken, Rotorbild)
void szeneBauen(const Raeumdienst &rd, const Simulation &sim, double zeit, Szene &aus);

} // namespace schnee
