// Simulationskern von Schneefall: reine Logik ohne Qt-GUI.
// Alle Koordinaten sind globale logische Bildschirmkoordinaten (wie in KWin).
//
// Schneemengen („menge“) werden wie gesamtSchnee() gemessen: Summe der Spaltenhöhen in px
// (eine Flocke legt ablageFuer(groesse) auf ihre Spalte). Schnittstellen für Räumdienst und
// Schneebälle (Teil B): decke()/decken() (Profil je Fenster), oberkanteBei(), schneeHinzufuegen(),
// schneeEntnehmen(), abtragenUndFallenLassen(), brockenFallenLassen(), brocken(), fallenderSchnee().
#pragma once
#include <cstdint>
#include <limits>
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

// Wie fallender Schnee und Fenster zueinander stehen
enum class Fenstermodus : uint8_t {
    Fest,     // Fenster sind feste 2D-Flächen: Schnee landet auf jeder sichtbaren Oberkante, prallt seitlich ab, nie im Fenster
    Dahinter, // Schnee fällt scheinbar hinter den Fenstern (im Fenster nicht gezeichnet), ein Teil bleibt auf den Kanten
    Davor,    // Schnee fällt vor den Fenstern, ein Teil bleibt auf den Kanten
};

struct Einstellungen {
    double menge = 1.0;          // Flocken pro 10 000 px² Bildschirmfläche (ungefähr)
    double wind = 0.0;           // px/s, positiv = nach rechts
    double groesse = 3.0;        // mittlerer Flockendurchmesser in px
    double maxHoehe = 18.0;      // maximale Deckenhöhe in px
    double spaltenBreite = 3.0;  // Breite einer Deckenspalte in px
    double fallGeschw = 45.0;    // mittlere Fallgeschwindigkeit px/s
    double schmelzen = 0.01;     // px/s Abbau am unteren Bildschirmrand (×(0,2 + Höhe/maxHöhe))
    double ablageJeFlocke = 0.0; // Höhenzuwachs je Flocke; 0 = aus Größe berechnet
    Fenstermodus modus = Fenstermodus::Fest;
    double fangAnteil = 0.4;     // nur davor/dahinter: Anteil der Flocken, die auf einer Fensterkante liegen bleiben
    double bodenMax = 18.0;      // höchste Schneedecke auf Boden und Taskleiste in px
    double schuettelnSchwelle = 15000; // px/s²: ab dieser Beschleunigung eines gezogenen Fensters fällt Schnee ab
    double rutschAbMinuten = 10; // ab diesem Deckenalter rutscht beim Minimieren/Schließen ein Schneebrett ab (sonst Wolke)
    double rutschTempo = 1.0;    // Fallbeschleunigung des Schneebretts (1 = 1200 px/s²)
    double wolkeDauer = 8;       // s, so lange hält sich die Schneewolke
    double wolkeLiegen = 0.4;    // Anteil der Wolke, der unten liegen bleibt (Rest löst sich auf)
};

enum class Flockenart : uint8_t {
    Kristall, // fällt vom Himmel
    Pulver,   // von einer Schneedecke abgefallen (Zerfall, Abschütteln, Abrutschen, Wolke)
};

struct Flocke {
    double x, y;      // Mittelpunkt
    double vx, vy;    // px/s
    double groesse;   // Durchmesser px
    double phase;     // für das Schaukeln
    float fang = 1.0f;  // Wahrscheinlichkeit, auf einer Fensterkante liegen zu bleiben (Boden/Taskleiste: immer)
    uint32_t los = 0;   // Zufallszahl der Flocke; entscheidet je Kante fest, ob sie fängt
    uint32_t herkunft = 0; // Schlüssel der Decke, aus der sie zerfallen ist (dort nie wieder landen)
    Flockenart art = Flockenart::Kristall; // nur fürs Aussehen
    float leben = -1;   // > 0: Wolkenstaub, löst sich nach so vielen Sekunden auf (landet nie); < 0: normal
    bool hinten = false; // fest: liegt hinter einem Fenster (wurde verdeckt), bis sie unter ihm herauskommt
};

// So schnell läuft die Schaukel-Phase einer Flocke (rad/s); Drehung und Taumeln hängen an ihr.
constexpr double PHASENTEMPO = 1.7;
// waagrechtes Tempo einer Flocke samt Schaukeln (px/s)
double flockeTempoX(const Flocke &f);
// eine Flocke frei weiterbewegen (ohne Landen und Fenster): Phase, Schaukeln, Fall
void flockeBewegen(Flocke &f, double dt);

// Schneebrett/Brocken: zusammenhängender Block, der schnell fällt und beim Aufprall zerbricht
struct Brocken {
    double x = 0, y = 0;        // linke Kante, Unterseite
    double spaltenBreite = 3;
    std::vector<float> hoehe;   // Höhe je Spalte (Summe = Menge)
    double vx = 0, vy = 0;
    uint32_t herkunft = 0;      // Schlüssel der Decke, aus der er kommt (dort landet er nie)
    double breite() const { return hoehe.size() * spaltenBreite; }
    double menge() const { double m = 0; for (float h : hoehe) m += h; return m; }
};

// Schneedecke auf einer Kante (Fensteroberkante oder Bildschirmboden)
struct Decke {
    std::string id;          // Fenster-ID oder "boden:<i>"
    double x = 0, y = 0;     // linke Kante, Höhe der Kante
    double spaltenBreite = 3;
    std::vector<float> hoehe;     // Höhe je Spalte
    std::vector<uint8_t> sichtbar; // 1 = Kante sichtbar (nicht verdeckt, auf Bildschirm)
    bool boden = false;
    bool dock = false;       // Taskleiste: fängt wie der Boden alle Flocken
    uint32_t schluessel = 0;      // aus id, nie 0
    float abfallLinks = 0, abfallRechts = 0; // gesammelter Überhang, der als Flocke herunterfällt
    double geboren = 0;      // Simulationszeit, seit der das Fenster Schnee trägt (Alter fürs Minimieren)
    double fensterBreite = 0; // Fensterbreite in px (Spalten = floor(Breite / Spaltenbreite))
    // Bewegung beim Ziehen (geglättet, px/s und px/s²) und Schüttelvorgang
    double vx = 0, vy = 0, ax = 0, ay = 0;
    double tLetzt = 0, offenX = 0, offenY = 0;
    double schuettelnSeit = -1, schuettelnMenge = 0, schuettelnMax = 0;
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
    // zeitpunkt: wann die Fensterliste galt, in Simulationszeit (zeit() + Zeit seit dem letzten Schritt);
    // daraus werden Geschwindigkeit und Beschleunigung gezogener Fenster gemessen. NaN = zeit().
    void setFenster(const std::vector<Fenster> &f, double zeitpunkt = std::numeric_limits<double>::quiet_NaN());

    // Simuliert dt Sekunden. Neue Flocken entstehen nur, wenn erzeugen=true.
    void schritt(double dt, bool erzeugen = true);

    void flockeHinzufuegen(const Flocke &f) { m_flocken.push_back(f); }
    const std::vector<Flocke> &flocken() const { return m_flocken; }
    const std::vector<Decke> &decken() const { return m_decken; }
    // Zählt hoch, sobald sich Decken sprunghaft ändern (Fenster, Monitore, Zerfall) – die Ebene
    // berechnet die Deckenform dann sofort neu statt nur alle paar Bilder.
    uint64_t deckenStand() const { return m_deckenStand; }
    const Decke *decke(const std::string &id) const;
    double hoeheBei(const std::string &id, double x) const; // Deckenhöhe an globalem x
    double gesamtSchnee(const std::string &id) const;       // Summe der Höhen

    // Zielanzahl fallender Flocken für die aktuelle Bildschirmfläche
    int zielAnzahl() const;
    // Soll diese Flocke gezeichnet werden? (fest/dahinter: nicht in Fenstern)
    bool flockeZeichnen(const Flocke &f) const;
    void alleFlockenEntfernen() { m_flocken.clear(); }
    void abschuetteln(); // alle Decken zerfallen in Flocken
    void vorfuellen();   // Flocken gleichmäßig über alle Monitore verteilen (Start)
    double zeit() const { return m_zeit; } // Simulationszeit in s

    // ---- Schnittstellen für Räumdienst und Schneebälle (Teil B) ----
    const std::vector<Brocken> &brocken() const { return m_brocken; }
    // Oberkante des Schnees (Welt-y) an globalem x; NaN, wenn x nicht über der Decke liegt
    double oberkanteBei(const std::string &id, double x) const;
    // legt menge auf die Spalte bei x (rutscht danach wie üblich); gibt zurück, was angenommen wurde
    double schneeHinzufuegen(const std::string &id, double x, double menge);
    // nimmt bis zu menge aus den sichtbaren Spalten in [x0, x1) (von oben, gleichmäßig); gibt die entnommene Menge zurück
    double schneeEntnehmen(const std::string &id, double x0, double x1, double menge);
    // nimmt bis zu menge von der Spalte bei x (und Nachbarn) und lässt sie als Brocken über die
    // linke (richtung < 0) oder rechte (> 0) Fensterkante fallen; gibt die abgetragene Menge zurück
    double abtragenUndFallenLassen(const std::string &id, double x, double menge, int richtung);
    // freier Brocken: Mitte x, Unterseite y, Breite in px, Menge, Anfangstempo
    void brockenFallenLassen(double x, double y, double breite, double menge, double vx = 0, double vy = 0, uint32_t herkunft = 0);
    // Schnee, der gerade fällt und noch landen wird (Flocken ohne Wolkenstaub + Brocken)
    double fallenderSchnee() const;
    // Menge, die eine Flocke dieser Größe beim Landen ablegt (für aufgewirbelten Schnee)
    double ablageJeFlocke(double groesse) const { return ablageFuer(groesse); }
    const std::vector<Rechteck> &bildschirme() const { return m_bildschirme; }
    const std::vector<Fenster> &fenster() const { return m_fenster; }
    // Oberkante am oberen Rand seines Monitors (maximiert): auch der Räumdienst fährt dort nicht
    bool amOberenRand(const Fenster &w) const;
    double maxHoeheVon(const Decke &d) const { return maxFuer(d); }

private:
    // Spalten [von,bis) (davon „anteil“) in Flocken; seitwaerts: rutschen erst über die nähere Kante
    void zerfallen(Decke &d, size_t von, size_t bis, double anteil = 1.0, bool seitwaerts = false);
    void bewegungMessen(Decke &d, double dx, double dy, double t);
    void verschwinden(Decke &d);                      // Fenster weg: Rutsch oder Wolke
    void wolke(Decke &d);
    void rutsch(Decke &d);
    void schuetteln(Decke &d, double dt);
    void brockenSchritt(double dt);
    const Fenster *fensterBei(double x, double y) const; // oberstes Fenster an dieser Stelle (fest)
    bool imSchatten(double x, double y) const;           // in oder senkrecht unter einem Fenster (fest)
    bool ohneDecke(const Fenster &w) const;              // Vollbild oder (fest) am oberen Rand: trägt keinen Schnee
    bool durchlaessig(const Fenster &w) const;           // fest: Schnee fällt davor, statt anzustoßen
    void festKollision(Flocke &f, double x0, double y0, bool &weg);
    double maxFuer(const Decke &d) const { return d.boden || d.dock ? m_e.bodenMax : m_e.maxHoehe; }
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
    std::vector<uint32_t> m_fensterSchluessel; // je Fenster der Schlüssel seiner Decke
    std::vector<Decke> m_decken;
    std::vector<Flocke> m_flocken;
    std::vector<Brocken> m_brocken;
    double m_erzeugRest = 0;
    double m_zeit = 0;
    uint64_t m_deckenStand = 0;
    bool m_fensterBekannt = false; // erste Fensterliste von KWin schon da (vorher vorgefüllte Flocken liegen auch in Fenstern)
    struct Strecke { double x0, y0, x1, y1; int seite; }; // seite: 0 oben, -1 links, +1 rechts
    std::vector<Strecke> m_raender;  // Ränder, an denen neue Flocken hereinkommen
};

} // namespace schnee
