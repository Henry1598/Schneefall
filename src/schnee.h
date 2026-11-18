// Simulationskern von Schneefall: reine Logik ohne Qt-GUI.
// Alle Koordinaten sind globale logische Bildschirmkoordinaten (wie in KWin).
#pragma once
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace schnee {

struct Rechteck {
    double x = 0, y = 0, w = 0, h = 0;
    double rechts() const { return x + w; }
    double unten() const { return y + h; }
    bool enthaelt(double px, double py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct Fenster {
    std::string id;
    Rechteck r;
    int stapel = 0;          // höher = weiter oben
    bool minimiert = false;
    bool vollbild = false;
    bool aktiv = false;
    bool dock = false;       // Taskleiste/Panel
};

struct Einstellungen {
    double menge = 1.0;          // Flocken pro 10 000 px² Bildschirmfläche (ungefähr)
    double wind = 0.0;           // px/s, positiv = nach rechts
    double groesse = 3.0;        // mittlerer Flockendurchmesser in px
    double maxHoehe = 18.0;      // maximale Deckenhöhe in px
    double spaltenBreite = 3.0;  // Breite einer Deckenspalte in px
    double fallGeschw = 45.0;    // mittlere Fallgeschwindigkeit px/s
    double schmelzen = 1.5;      // px/s Abbau am unteren Bildschirmrand (bei voller Höhe)
    double ablageJeFlocke = 0.0; // Höhenzuwachs je Flocke; 0 = aus Größe berechnet
};

struct Flocke {
    double x, y;      // Mittelpunkt
    double vx, vy;    // px/s
    double groesse;   // Durchmesser px
    double phase;     // für das Schaukeln
};

// Schneedecke auf einer Kante (Fensteroberkante oder Bildschirmboden)
struct Decke {
    std::string id;          // Fenster-ID oder "boden:<i>"
    double x = 0, y = 0;     // linke Kante, Höhe der Kante
    double spaltenBreite = 3;
    std::vector<float> hoehe;     // Höhe je Spalte
    std::vector<uint8_t> sichtbar; // 1 = Kante sichtbar (nicht verdeckt, auf Bildschirm)
    bool boden = false;
    float abfallLinks = 0, abfallRechts = 0; // gesammelter Überhang, der als Flocke herunterfällt
    double breite() const { return hoehe.size() * spaltenBreite; }
};

// true, wenn das aktive Fenster im Vollbild ist (Spiel, Video …)
bool vollbildAktiv(const std::vector<Fenster> &fenster);

class Simulation {
public:
    explicit Simulation(Einstellungen e = {}, uint32_t saat = 12345);

    void setEinstellungen(const Einstellungen &e);
    const Einstellungen &einstellungen() const { return m_e; }

    void setBildschirme(const std::vector<Rechteck> &b);
    void setFenster(const std::vector<Fenster> &f);

    // Simuliert dt Sekunden. Neue Flocken entstehen nur, wenn erzeugen=true.
    void schritt(double dt, bool erzeugen = true);

    void flockeHinzufuegen(const Flocke &f) { m_flocken.push_back(f); }
    const std::vector<Flocke> &flocken() const { return m_flocken; }
    const std::vector<Decke> &decken() const { return m_decken; }
    const Decke *decke(const std::string &id) const;
    double hoeheBei(const std::string &id, double x) const; // Deckenhöhe an globalem x
    double gesamtSchnee(const std::string &id) const;       // Summe der Höhen

    // Zielanzahl fallender Flocken für die aktuelle Bildschirmfläche
    int zielAnzahl() const;
    void alleFlockenEntfernen() { m_flocken.clear(); }
    void abschuetteln(); // alle Decken zerfallen in Flocken

private:
    void zerfallen(Decke &d, size_t von, size_t bis); // Spalten [von,bis) in Flocken
    void sichtbarkeitBerechnen();
    void nachSichtbarkeitZerfallen();
    void bodenAufbauen();
    void ablegen(Decke &d, size_t spalte, double menge);
    void rutschen(Decke &d, double dt);
    void herunterfallen(Decke &d, size_t spalte, double menge, int richtung);
    Flocke neueFlocke(double x, double y);
    void erzeugen(double dt);
    bool aufBildschirm(double x, double y) const;
    double ablageFuer(double groesse) const;

    Einstellungen m_e;
    std::mt19937 m_rng;
    std::vector<Rechteck> m_bildschirme;
    std::vector<Fenster> m_fenster;
    std::vector<Decke> m_decken;
    std::vector<Flocke> m_flocken;
    double m_erzeugRest = 0;
    struct Strecke { double x0, y0, x1, y1; int seite; }; // seite: 0 oben, -1 links, +1 rechts
    std::vector<Strecke> m_raender;  // Ränder, an denen neue Flocken hereinkommen
};

} // namespace schnee
