// Kleine 3D-Ansicht für die Fahrzeuge des Räumdienstes. Raupe, Pflug und Hubschrauber sind Modelle aus
// Quadern mit gerundeten Kanten, Zylindern, Drehkörpern, Schalen und Prismen. Sie werden um die
// Hochachse gedreht (Gierwinkel), leicht von oben parallel projiziert und mit Tiefenpuffer gerastert:
// weiches, weltfestes Licht von oben links, Glanz je Werkstoff (Lack, Metall, Gummi, Glas), Muster
// (Kettenglieder, Reifenprofil, Kühler …), Schmutz unten, Schnee auf waagerechten Flächen,
// Umgebungsverdeckung in Ecken und eine dunkle Außenkante. Reine Rechnung ohne Qt.
//
// Modellachsen: x nach vorn (Schild/Bug), y nach oben, z zur rechten Fahrzeugseite.
// Gier 0 = Fahrzeug zeigt im Bild nach rechts, π/2 = Front zum Betrachter, π = nach links, 3π/2 = Heck zum Betrachter.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace schnee {

struct P3 { double x = 0, y = 0, z = 0; };
struct P2 { double x = 0, y = 0; };

constexpr int WINKELSTUFEN = 24;          // vorgemalte Ansichten je Umdrehung (15°-Schritte)
constexpr double KAMERA_NEIGUNG = 0.16;   // rad: Blick leicht von oben, damit man Dach und Haube sieht

P2 projizieren(P3 p, double gier);        // Bildpunkt in Einheiten (x rechts, y unten) relativ zum Modellursprung
double tiefe(P3 p, double gier);          // größer = näher am Betrachter
P3 imBildraum(P3 richtung, double gier);  // Richtung im Bildraum (x rechts, y oben, z zum Betrachter)
int winkelstufe(double gier);             // nächste vorgemalte Ansicht 0 … WINKELSTUFEN-1
double stufenWinkel(int stufe);

enum class Stoff : uint8_t { Lack, Metall, Gummi, Matt, Glas, Haut, Tuch, Licht, Scheibe };
enum class Muster : uint8_t { Keins, Kette, Rolle, Reifen, Kuehler, Warnstreifen, Jacke, Gesicht, Schneide, Raupenhaube, HeliRumpf, HeliAusleger, Rotorscheibe, Pritsche };

// Kettenglieder: Farbe an der Stelle u (Bogenlänge am Umlauf) bei Phase 0 … 1 (ein Glied weiter = Phase + 1)
constexpr double KETTEN_TEILUNG = 5.33;
uint32_t kettenMuster(double u, double phase);

struct Teilinfo {
    std::string name;
    Stoff stoff = Stoff::Lack;
    uint32_t farbe = 0xff808080;   // ARGB
    Muster muster = Muster::Keins;
    P3 mitte, halb;                // Quader: Mitte und halbe Kantenlängen (für gerundete Kanten und Muster)
    double rundung = 0;            // Radius der Kantenrundung (nur Licht, die Form bleibt)
    double phase = 0;              // Kette: Anteil eines Glieds; Rolle/Reifen: Drehwinkel in rad
    int teilung = 5;               // Rolle: Zahl der Löcher (bestimmt, nach welchem Drehwinkel sie wieder gleich aussieht)
    double schnee = 1;             // wie viel Schnee auf waagerechten Flächen liegt (0 = keiner)
    double schmutz = 1;            // Verschmutzung nahe am Boden (0 = keine)
    bool beidseitig = false;       // dünne Fläche, von beiden Seiten zu sehen
};

struct Ecke { P3 p, n; double u = 0; };
struct Dreieck { Ecke e[3]; uint16_t teil = 0; };

struct Modell {
    std::vector<Dreieck> dreiecke;
    std::vector<Teilinfo> teile;
    double schnee = 0.6, schmutz = 1; // Vorgaben für neue Teile

    uint16_t teil(const std::string &name, Stoff stoff, uint32_t farbe, Muster muster = Muster::Keins);
    // Quader mit gerundeten Kanten (jeder gerundete Quader braucht ein eigenes Teil)
    void quader(uint16_t t, double x0, double x1, double y0, double y1, double z0, double z1, double rundung = 0);
    // Zylinder oder Kegelstumpf zwischen a und b; u = Winkel
    void rohr(uint16_t t, P3 a, P3 b, double ra, double rb, int ecken = 16, bool deckel = true);
    void zylinder(uint16_t t, P3 a, P3 b, double r, int ecken = 16) { rohr(t, a, b, r, r, ecken, true); }
    // Ellipsoid (auch nur ein Höhenband davon: von/bis als Sinus des Breitengrads, -1 … 1)
    void ellipsoid(uint16_t t, P3 mitte, P3 radien, int ecken = 14, double von = -1, double bis = 1);
    // Schale aus elliptischen Spanten entlang x (Mitte y, Halbachsen y und z), Bogen w0 … w1 um die Längsachse
    // (0 = rechts, π/2 = oben); voller Umfang bekommt vorn und hinten einen Abschluss
    struct Spant { double x, y, ry, rz; };
    void schale(uint16_t t, const std::vector<Spant> &spanten, int ecken = 20, double w0 = 0, double w1 = 6.28318530718);
    // Profil (x, y) entlang z ausgezogen; glatt = runde Übergänge am Profil; u = Bogenlänge am Profil
    void prisma(uint16_t t, const std::vector<P2> &profil, double z0, double z1, bool glatt = false, bool deckel = true);
    // Drehkörper um die z-Achse durch mitte: Profil aus (Radius, z); u = Winkel
    void drehkoerper(uint16_t t, P3 mitte, const std::vector<P2> &profil, int ecken = 24);
    void dreieck(uint16_t t, Ecke a, Ecke b, Ecke c);
    void viereck(uint16_t t, Ecke a, Ecke b, Ecke c, Ecke d) { dreieck(t, a, b, c); dreieck(t, a, c, d); }
};

struct Ansichtsbild {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;     // vormultipliziert, 4 Byte je Pixel
    std::vector<uint16_t> teil;    // je Pixel: vorderstes deckendes Teil + 1 (0 = keins)
    uint8_t alpha(int x, int y) const { return rgba[size_t(4 * (y * w + x) + 3)]; }
};

// Rastert das Modell: pxJeEinheit Bildpunkte je Einheit, der Modellursprung liegt bei (ursprungX, ursprungY) px
Ansichtsbild rendern(const Modell &m, double gier, double pxJeEinheit, int w, int h, double ursprungX, double ursprungY, int abtastung = 3);
// sichtbare Bildpunkte je Teilname (bei 2 px je Einheit): zum Prüfen, was aus welcher Richtung zu sehen ist
std::map<std::string, int> abdeckung(const Modell &m, double gier);

} // namespace schnee
