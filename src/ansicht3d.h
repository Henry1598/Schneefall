// Kleine 3D-Ansicht für das Wenden: Fahrzeuge sind einfache Körper (Quader, Prismen, Schalen),
// die um die Hochachse gedreht (Gierwinkel) und schräg von oben parallel projiziert werden.
// Das Licht ist weltfest (von oben links vorn) und dreht sich nicht mit dem Fahrzeug.
// Reine Rechnung ohne Qt; gemalt wird in fahrzeugbild.cpp.
//
// Modellachsen: x nach vorn (Schild/Bug), y nach oben, z zur rechten Fahrzeugseite.
// Gier 0 = Fahrzeug zeigt im Bild nach rechts, π/2 = Front zum Betrachter, π = nach links, 3π/2 = Heck zum Betrachter.
#pragma once
#include <cstdint>
#include <vector>

namespace schnee {

struct P3 { double x = 0, y = 0, z = 0; };
struct P2 { double x = 0, y = 0; };

constexpr int WINKELSTUFEN = 24;          // vorgemalte Ansichten je Umdrehung (15°-Schritte)
constexpr double KAMERA_NEIGUNG = 0.16;   // rad: Blick leicht von oben, damit man Dach und Haube sieht

// Bildpunkt in Einheiten (x rechts, y unten) relativ zum Modellursprung
P2 projizieren(P3 p, double gier);
// größer = näher am Betrachter
double tiefe(P3 p, double gier);
// Richtung im Bildraum (x rechts, y oben, z zum Betrachter)
P3 imBildraum(P3 richtung, double gier);
// Helligkeit einer Fläche mit dieser Normalen im Bildraum; 1 = Fläche schaut zum Betrachter
double helligkeit(P3 normaleBild);
int winkelstufe(double gier);              // nächste vorgemalte Ansicht 0 … WINKELSTUFEN-1
double stufenWinkel(int stufe);

struct Flaeche {
    std::vector<P3> ecken;     // von außen gesehen gegen den Uhrzeigersinn
    uint32_t farbe = 0xff808080; // ARGB
    int textur = -1;           // -1 = einfarbig, sonst Seitenbild: Bildpunkt (u, v) = (x + tu, tv - y)
    double tu = 0, tv = 0;
    double vor = 0;            // Aufkleber: so weit vor der Trägerfläche (nur für die Reihenfolge)
    bool beidseitig = false;   // dünne Scheibe: von beiden Seiten sichtbar
};

P3 normale(const Flaeche &f);

struct Modell {
    std::vector<Flaeche> flaechen;
    // Quader; gibt den Index der ersten seiner sechs Flächen zurück (Reihenfolge: +x, -x, +y, -y, +z, -z)
    size_t quader(double x0, double x1, double y0, double y1, double z0, double z1, uint32_t farbe);
    // Profil (x, y) entlang z ausgezogen: erst die Mantelflächen in Profilreihenfolge, dann Deckel +z, Deckel -z
    size_t prisma(const std::vector<P2> &profil, double z0, double z1, uint32_t mantel, uint32_t deckel);
    // Schale aus elliptischen Spanten entlang x: je Spant Mitte y, Halbachsen y und z
    struct Spant { double x, y, ry, rz; };
    size_t schale(const std::vector<Spant> &spanten, int ecken, uint32_t farbe);
    // Rechteck in einer Ebene x = fest (Front/Heck) oder y = fest (Dach) als Aufkleber
    void aufkleberX(double x, double y0, double y1, double z0, double z1, bool nachVorn, uint32_t farbe, double vor = 0.2);
};

struct Sichtflaeche {
    std::vector<P2> ecken;
    double hell = 1;           // Helligkeit (weltfestes Licht)
    double tiefe = 0;
    const Flaeche *quelle = nullptr;
};
// sichtbare Flächen (Rückseiten entfernt), von hinten nach vorn sortiert
std::vector<Sichtflaeche> ansicht(const Modell &m, double gier);

} // namespace schnee
