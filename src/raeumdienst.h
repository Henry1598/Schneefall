// Räumdienst und Schneebälle: reine Logik ohne Qt (Zustandsautomat, Seilpendel, Fahrzeug, Bälle).
// Ein Einsatz: Hubschrauber fliegt ein Räumfahrzeug am Seil ein, setzt es auf dem Fenster ab, das
// Fahrzeug schiebt den Schnee in Bahnen über die Kante, der Hubschrauber holt es wieder ab.
// Koordinaten global logisch wie in schnee.h; Mengen wie gesamtSchnee() (Summe der Spaltenhöhen).
#pragma once
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>
#include "schnee.h"

namespace schnee {

struct RaeumEinstellungen {
    bool an = true;               // Einsätze von selbst buchen
    double raeumenAb = 0.8;       // Anteil der Höchstmenge, ab dem gebucht wird
    double fahrzeugGroesse = 1.0; // 0,5 … 3
    int gleichzeitig = 1;         // Einsätze zugleich (1 … 3)
    bool taskleiste = true;       // kleiner Pflug für Taskleiste und Boden
    int trefferBisStreik = 3;
    bool anfassbar = true;        // Schnee mit der Maus formen (Eingaberegion)
};

// Geräusche, Reihenfolge wie KlangArt in klang.h
enum class Geraeusch { Rotor, Motor, Rueckfahrwarner, Rutsch, Plumps, Wurf, Treffer, Murren };
struct Klangereignis {
    enum Typ { Einmal, Dauer, Stopp } typ = Einmal;
    Geraeusch art = Geraeusch::Plumps;
    int id = 0;          // nur Dauer/Stopp
    double x = 0, y = 0;
    double staerke = 1;  // Einmal: Stärke 0..2, Dauer: Last 0..1
};

// Wer spricht und aus welchem Anlass (Reihenfolge wie Sprecher/Anlass in sprueche.h)
enum class Redner { Fahrer, Pilot };
enum class Redeanlass { Treffer, Streik, BeleidigtAbflug, Gruss, HeliTreffer, HeliAbholen, HeliAbflug };
// Liefert den Satz: trefferzahl bestimmt die Ausdrucksweise (1 = freundlich, mit jedem Treffer deutlicher).
// Das Programm hängt hier die 1000 Sprüche an (sprueche.h); ohne Quelle gibt es ein paar eingebaute Sätze.
using Spruchquelle = std::function<std::string(Redner, Redeanlass, int trefferzahl)>;

struct Sprechblase {
    std::string text;         // leer = keine Blase
    double rest = 0;          // s, bis sie verschwindet
    double dauer = 0;         // gesamte Anzeigedauer (für das Ein- und Ausblenden)
    Redeanlass anlass = Redeanlass::Gruss;
    bool sichtbar() const { return rest > 0 && !text.empty(); }
};
// Anzeigedauer nach Textlänge (Lesezeit), 2 … 6,5 s
double blasenDauer(const std::string &text);

// Last am Seil: Punktmasse an einem Seil fester Länge (zieht nur, schiebt nicht), gedämpft.
struct Seilpendel {
    double x = 0, y = 0, vx = 0, vy = 0; // Last
    double ax = 0, ay = 0;               // Aufhängung (zuletzt)
    double laenge = 40;
    double daempfung = 0.9;              // 1/s
    void haengen(double ankerX, double ankerY); // ruhig senkrecht darunter
    void schritt(double dt, double ankerX, double ankerY);
    double winkel() const;               // Auslenkung aus der Senkrechten in rad (rechts positiv)
};

struct Hubschrauber {
    double x = 0, y = 0, vx = 0, vy = 0; // Bezugspunkt: Lasthaken-Aufhängung unter dem Rumpf
    double zielX = 0, zielY = 0;
    double neigung = 0;   // rad im Bild (Uhrzeigersinn): Nase senkt sich in Beschleunigungsrichtung
    double gier = 0;      // rad: 0 = Bug nach rechts, π = nach links; dreht über die Front- bzw. Heckansicht
    int blickSoll = 1;    // wohin der Bug zeigen soll (+1 rechts, -1 links)
    double rotor = 0;     // Phase
    double unruhe = 0;    // Zeit für die Schwebe-Unruhe
    double stoss = 0;     // Wackeln nach Balltreffer
    Sprechblase blase;    // der Pilot sagt etwas
    void schritt(double dt, double maxV, double maxA);
};

enum class Fahrzeugart : uint8_t { Raupe, Pflug };
enum class FahrPhase : uint8_t { AmSeil, Aufgesetzt, Wenden, Senken, Schieben, Kippen, Zurueck, Bereit, Streik, Beleidigt };

struct Fahrzeugmass { double halb, hoehe, schildVorn, trefferB, trefferH; };
Fahrzeugmass massVon(Fahrzeugart a); // in Einheiten (px bei Größe 1)

struct Klecks { float x, y, groesse, menge; double seit; }; // klebender Schnee in Einheiten ab Fahrzeugmitte unten: x nach vorn, y nach unten

struct Fahrzeug {
    Fahrzeugart art = Fahrzeugart::Raupe;
    FahrPhase phase = FahrPhase::AmSeil, vorStreik = FahrPhase::Schieben;
    double x = 0, y = 0;      // Mitte unten (Aufstandsfläche)
    double v = 0;             // px/s
    double neigung = 0;       // am Seil: Pendelwinkel
    int blick = 1;            // Schild zeigt nach rechts (+1) oder links (-1)
    double gier = 0;          // rad: 0 = Schild rechts, π = links; beim Wenden dreht das Fahrzeug auf der Stelle
    double wendeT = 0;        // 0 … 1 Fortschritt der Drehung
    double wendeVon = 0;      // Gierwinkel beim Beginn der Drehung
    bool dreht = false;       // Phase Wenden: die eigentliche Drehung läuft (vorher wird ggf. rangiert)
    double drehweg = 0;       // Raupe: Kettenweg aus dem Drehen auf der Stelle (rechte Kette +, linke -)
    // Radpflug: wendet wie ein Auto in mehreren Zügen (vor, zurück, vor) mit eingeschlagenen Vorderrädern
    double lenk = 0;          // Lenkwinkel der Vorderräder in rad (+ = Bug dreht zum Betrachter)
    double tiefeZ = 0;        // Versatz zum Betrachter hin (px), entsteht beim Bogenfahren
    int wendeZuege = 3, wendeZug = 0, wendeErster = 1;   // Zahl der Züge, laufender Zug, erster Zug vorwärts (+1) oder rückwärts (-1)
    int richtung = 1;         // Bahnrichtung
    int seite = 0, durchgang = 0;
    bool nochmal = false;
    double mitte = 0;         // Absetzpunkt relativ zur Deckenkante x
    double t = 0;             // Zeit in der Phase
    double kette = 0;         // Weg in Blickrichtung (Kettenglieder, Rollen)
    double schild = 1;        // 0 = unten, 1 = oben
    double nicken = 0, nickV = 0, hub = 0, hubV = 0, wackeln = 0, wackelV = 0;
    double wulst = 0;         // Schnee vor dem Schild
    double kippRest = 0;      // gesammelter Abwurf bis zum nächsten Brocken (seitlich am vollen Schild)
    double ueberhang = 0;     // Schnee, der an der Fensterkante überhängt, bis er abbricht
    double bruchAb = 40;      // bei dieser Menge bricht der Überhang (wechselt, damit die Schollen unregelmäßig sind)
    double broeselRest = 0, pulverRest = 0;
    double last = 0;          // Motorlast 0..1
    double leuchte = 0;       // Phase der Rundumleuchte
    double schneeDach = 0;    // 0..1 liegengebliebener Schnee
    double rauchRest = 0, staubRest = 0;
    bool streikFaellig = false; // Treffergrenze erreicht, Streik beginnt (nach einer laufenden Drehung)
    int treffer = 0, streiks = 0, trefferImStreik = 0;
    double raus = 0;          // 0 = Fahrer sitzt, 1 = lehnt sich raus
    double faust = 0;         // Phase der drohenden Faust
    Sprechblase blase;        // der Fahrer sagt etwas
    int trefferGesamt = 0;    // alle Treffer dieses Einsatzes: je mehr, desto deutlicher wird er
    std::vector<Klecks> kleckse;
};

enum class Phase : uint8_t { Anflug, Schweben, Absenken, Ausklinken, Raeumen, Rueckkehr, Einhaken, Anheben, Abflug, Fertig };

struct Einsatz {
    int nr = 0;
    std::string id;           // Decke
    Phase phase = Phase::Anflug;
    double t = 0;             // Zeit in der Phase
    Hubschrauber heli;
    Seilpendel seil;          // Last = Fahrzeug-Öse (angehängt) oder Haken
    bool angehaengt = true;
    bool abbruch = false, eilig = false, beleidigt = false;
    int heliTreffer = 0;       // Schneebälle auf den Hubschrauber
    Fahrzeug fz;
    double segA = 0, segB = 0; // befahrbarer Abschnitt (Welt-x)
    double kanteY = 0;         // Fensteroberkante
    double deckX = 0;          // linke Deckenkante zuletzt (Mitfahren beim Verschieben)
    uint32_t schluessel = 0;
    bool boden = false;
    double abwindRest = 0, warteSeite = 1, rutschKlang = 0;
    double sichtRest = 0;      // bis zur nächsten Prüfung, ob die Kante noch zu sehen ist
    double spurA = 0, spurB = 0; // geräumter Streifen (relativ zur linken Deckenkante): dort bleibt die Kettenspur
    bool spur = false;
    struct Fallstueck { double x, y, menge; };
    std::vector<Fallstueck> fallend;   // abgeworfene Schollen im letzten Schritt (für Staub beim Aufprall)
    int ausgang = 1;           // Weltseite für den Abflug
};

struct Schneeball {
    double x = 0, y = 0, vx = 0, vy = 0;
    double menge = 0, winkel = 0, drall = 0;
    double radius() const;
};

struct Staub { float x, y, vx, vy, alter, dauer, groesse; uint8_t art; }; // art 0 = Schneestaub, 1 = Rauch, 2 = Schneebröckchen (kullert, fällt)

// Form des Schneewulstes vor dem Schild (Fläche in px², g = Fahrzeuggröße)
double wulstHoehe(double flaeche, double g);
double wulstLaenge(double flaeche, double g);
double wulstFlaecheFuerLaenge(double laenge, double g);
double wulstKapazitaet(double g);   // px²
// Maßstab des Wulstes: der kleine Pflug schiebt weniger vor sich her
double wulstMass(Fahrzeugart a, double fahrzeugGroesse);
// Dauer einer halben Drehung auf der Stelle in Sekunden (größere Fahrzeuge drehen langsamer)
double wendeDauer(double fahrzeugGroesse);
// so viel Platz braucht das Fahrzeug beim Drehen nach jeder Seite (px)
double wendeRadius(Fahrzeugart a, double fahrzeugGroesse);
// Profil: Höhe im Abstand s vor dem Schild
double wulstProfil(double flaeche, double g, double s);


class Raeumdienst {
public:
    explicit Raeumdienst(Simulation &sim, uint32_t saat = 777);
    void setEinstellungen(const RaeumEinstellungen &e) { m_e = e; }
    void setSpruchquelle(Spruchquelle q) { m_sprueche = std::move(q); }
    const RaeumEinstellungen &einstellungen() const { return m_e; }

    void schritt(double dt);
    // bucht einen Einsatz für diese Decke (hinten in die Warteschlange); false, wenn schon gebucht/aktiv oder ungeeignet
    bool buchen(const std::string &id, bool vorne = false);
    // „Jetzt räumen“: sofort am Fenster mit dem meisten Schnee; gibt dessen ID zurück ("" = nichts zu räumen)
    std::string jetztRaeumen();
    // Ist die Kante dieser Decke für einen Einsatz geeignet? Nicht bei Vollbild, minimiert, Oberkante am oberen
    // Monitorrand (maximiert), größtenteils verdeckt oder ohne freien Abschnitt für Fahrzeug und Wende.
    bool raeumbar(const Decke &d) const;
    // Anteil der Höchstmenge (90. Perzentil der sichtbaren Spalten)
    double fuellstand(const Decke &d) const;

    const std::vector<Einsatz> &einsaetze() const { return m_einsaetze; }
    const std::deque<std::string> &warteschlange() const { return m_schlange; }
    const std::vector<Schneeball> &baelle() const { return m_baelle; }
    const Schneeball *ballInHand() const { return m_inHand ? &m_hand : nullptr; }
    const std::vector<Staub> &staub() const { return m_staub; }
    bool etwasLos() const { return !m_einsaetze.empty() || !m_baelle.empty() || m_inHand || !m_staub.empty(); }

    // Maus (t in Sekunden, beliebige Uhr). mausDruck gibt zurück, ob Schnee oder ein Fahrzeug getroffen wurde.
    bool mausDruck(double x, double y, double t);
    void mausBewegt(double x, double y, double t);
    void mausLos(double x, double y, double t);
    void werfen(double x, double y, double vx, double vy, double menge);
    // Flächen, in denen das Overlay Mausklicks annimmt (Schnee über 6 px, Fahrzeuge); leer = alles klickt durch
    std::vector<Rechteck> eingabeRechtecke() const;

    std::vector<Klangereignis> klaengeAbholen();
    void klaengeAnhalten(); // Pause: alle Dauerklänge stoppen
    // Schnee, den der Räumdienst gerade trägt (Wulst, Abwurfrest, Kleckse, Bälle)
    double getrageneMenge() const;

private:
    bool starten(const std::string &id);
    void ueberhangBrechen(Einsatz &e);
    void schollenSchritt(Einsatz &e, double dt);
    bool abschnittUm(const Decke &d, double x, double &a, double &b) const;
    void einsatzSchritt(Einsatz &e, double dt);
    void fahrzeugSchritt(Einsatz &e, const Decke &d, double dt);
    void abbrechen(Einsatz &e, bool deckeWeg);
    void abwerfen(Einsatz &e, double menge, bool alles, bool seitlich = false);
    void abwind(Einsatz &e, double dt);
    void ballSchritt(double dt);
    void platzen(const Schneeball &b, const std::string &deckenId, double anteilLiegen);
    void treffer(Einsatz &e, const Schneeball &b);
    void streikPruefen(Einsatz &e);
    void pulver(double x, double y, double menge, double tempo, double vx0, double vy0, const std::string &rest);
    void staubWolke(double x, double y, int anzahl, double tempo, double groesse, uint8_t art = 0);
    void klang(Geraeusch a, double x, double y, double staerke = 1);
    void dauer(const Einsatz &e, int kanal, Geraeusch a, double x, double y, double last);
    // neue Sprechblase (mit Murren); entprellt = nur, wenn die letzte Blase schon eine Weile steht
    bool sagen(Sprechblase &b, Redner wer, Redeanlass anlass, int trefferzahl, double x, double y, double entprellt = 0);
    double zufall() { return m_u(m_rng); }
    double g() const { return m_e.fahrzeugGroesse; }

    Simulation &m_sim;
    RaeumEinstellungen m_e;
    std::mt19937 m_rng;
    std::uniform_real_distribution<double> m_u{0, 1};
    std::vector<Einsatz> m_einsaetze;
    std::deque<std::string> m_schlange;
    std::map<std::string, double> m_sperre; // Decke → Simulationszeit, bis zu der nicht neu gebucht wird
    std::vector<Schneeball> m_baelle;
    Schneeball m_hand;
    bool m_inHand = false;
    struct Probe { double x, y, t; };
    std::vector<Probe> m_proben;
    std::vector<Staub> m_staub;
    std::vector<Klangereignis> m_klaenge;
    std::vector<int> m_dauerAn, m_dauerNeu; // laufende Dauerklang-IDs
    double m_pruefRest = 0;
    int m_naechsteNr = 1, m_spruchNr = 0;
    Spruchquelle m_sprueche;
};

} // namespace schnee
