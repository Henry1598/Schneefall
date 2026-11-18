#include "raeumszene.h"
#include <algorithm>
#include <cmath>
#include "fahrzeugmodell.h"

namespace schnee {
namespace {

// Lage eines Körpers: Teile werden in seinen Einheiten angegeben (x nach vorn, y nach unten)
struct Lage {
    double x, y, winkel;
    double sx, sy; // px je Einheit; sx < 0 = gespiegelt
    std::vector<Zeichenteil> *ziel;
    void teil(Teil t, double lx, double ly, double lokalWinkel = 0, double fx = 1, double fy = 1, double alpha = 1) const {
        if (alpha <= 0) return;
        const double c = std::cos(winkel), s = std::sin(winkel);
        const double px = lx * sx, py = ly * sy;
        const double spiegel = sx < 0 ? -1 : 1;
        ziel->push_back({uint16_t(t), float(x + c * px - s * py), float(y + s * px + c * py), float(winkel + spiegel * lokalWinkel),
                         float(sx * fx), float(sy * fy), float(alpha)});
    }
    // Unterkörper an lokaler Stelle mit eigener Drehung
    Lage kind(double lx, double ly, double lokalWinkel) const {
        const double c = std::cos(winkel), s = std::sin(winkel);
        const double px = lx * sx, py = ly * sy;
        return {x + c * px - s * py, y + s * px + c * py, winkel + (sx < 0 ? -1 : 1) * lokalWinkel, sx, sy, ziel};
    }
};


void sprechblase(const Fahrzeug &f, double G, std::vector<Zeichenteil> &z) {
    if (f.spruch < 0 || f.blaseRest <= 0) return;
    const double B = blasenSkala(G);
    const double alpha = std::clamp(f.blaseRest / 0.3, 0.0, 1.0);
    const double hoehe = massVon(f.art).hoehe;
    // ploppt auf: kurz etwas größer
    z.push_back({uint16_t(int(Teil::Blase0) + f.spruch), float(f.x + 8 * G), float(f.y - (hoehe + 6) * G), 0, float(B), float(B), float(alpha)});
}

// Schicht eines Fahrzeugs mit ihrer Tiefe (größer = näher): gezeichnet wird von hinten nach vorn
struct Schicht { double tiefe; int art; double x, y; int teil; };

bool dreht(const Fahrzeug &f) { return f.phase == FahrPhase::Wenden && f.dreht; }
double gierVon(const Fahrzeug &f) { return dreht(f) ? f.gier : (f.blick > 0 ? 0.0 : 3.14159265359); }
int phaseVon(double anteil, int phasen) {
    const double b = anteil - std::floor(anteil);
    return std::min(phasen - 1, int(b * phasen));
}

// Gemeinsames für Raupe und Pflug: Fahrer lehnt sich beim Streik aus dem Seitenfenster, Leuchte, Schnee, Kleckse
void zubehoer(const Fahrzeug &f, const Lage &boden, double gier, P3 fenster, P3 leuchte, double mass, double halbeBreite) {
    const bool raupe = f.art == Fahrzeugart::Raupe;
    auto bei = [&](P3 p) { return projizieren(p, gier); };
    if (f.raus > 0.05 && !dreht(f)) {
        // Seitenansicht: er hängt bis zur Hüfte aus dem Fenster (schaut in Fahrtrichtung)
        const P2 q = projizieren(fenster, 0);
        const Lage gespiegelt{boden.x, boden.y, boden.winkel, boden.sy * f.blick, boden.sy, boden.ziel};
        Lage mensch = gespiegelt.kind(q.x - 1.5 * f.raus, q.y - 1.5 * f.raus, -0.3 * f.raus);
        mensch.sx *= mass;
        mensch.sy *= mass;
        mensch.teil(Teil::Fahrer, 0, 0);
        mensch.teil(Teil::Arm, -1.5, -10.5, (2.55 + 0.45 * std::sin(f.faust)) * f.raus, 1, 1, std::min(1.0, f.raus * 2));
    }
    if (f.schneeDach > 0.05 && raupe) {
        const P2 d = bei(RAUPE_DACH);
        const double breit = (30 * std::abs(std::cos(gier)) + 23 * std::abs(std::sin(gier))) / 30;
        boden.teil(Teil::SchneeDach, d.x, d.y + 0.6, 0, breit, 0.3 + 0.7 * f.schneeDach);
    }
    const P2 l = bei(leuchte);
    const double dreh = f.leuchte * 7.5;
    const double hell = 0.3 + 0.7 * std::pow(std::max(0.0, std::sin(dreh)), 2);
    boden.teil(Teil::LichtOrange, l.x, l.y, 0, (raupe ? 0.55 : 0.45) + 0.7 * std::abs(std::cos(dreh * 0.5)), raupe ? 0.8 : 0.65, hell);
    for (const Klecks &k : f.kleckse) {
        const double seite = std::sin(gier) >= 0 || std::cos(gier) > 0 ? halbeBreite : -halbeBreite;
        const P2 q = bei({double(k.x), double(-k.y), seite});
        boden.teil(Teil::Klecks, q.x, q.y, k.x, k.groesse / 15, k.groesse / 15, 0.95);
    }
}

void raupe(const Fahrzeug &f, double G, std::vector<Zeichenteil> &z) {
    const int stufe = winkelstufe(gierVon(f));
    const double gier = stufenWinkel(stufe), c = std::cos(gier), s = std::sin(gier);
    const Lage boden{f.x, f.y, f.neigung + f.wackeln * 0.5, G, G, &z};
    if (f.phase != FahrPhase::AmSeil) boden.teil(Teil::Schatten, 0, 0.5, 0, 0.55 + 0.7 * std::abs(c), 0.6, 0.3);
    // Ketten: beim Fahren laufen beide gleich, beim Drehen auf der Stelle gegenläufig
    const double weg = f.kette / G / KETTEN_TEILUNG, dreh = f.drehweg / G / KETTEN_TEILUNG;
    const int rechts = int(Teil::RaupeKetteR) + stufe * KETTENPHASEN + phaseVon(weg + dreh, KETTENPHASEN);
    const int links = int(Teil::RaupeKetteL) + stufe * KETTENPHASEN + phaseVon(weg - dreh, KETTENPHASEN);
    const bool leer = f.raus > 0.05 && !dreht(f);
    const int koerper = leer ? int(Teil::RaupeLeer) + (f.blick > 0 ? 0 : 1) : int(Teil::RaupeKoerper) + stufe;
    Schicht schichten[4] = {{12.25 * c, 0, 0, 0, rechts}, {-12.25 * c, 0, 0, 0, links}, {0.01, 1, 0, 0, koerper},
                            {37 * s + 13 * std::abs(c), 2, 0, 0, int(Teil::RaupeSchild) + stufe}};
    std::sort(std::begin(schichten), std::end(schichten), [](const Schicht &a, const Schicht &b) { return a.tiefe < b.tiefe; });
    for (const Schicht &sch : schichten) {
        if (sch.art == 1) {
            // Aufbau federt und nickt auf dem Fahrwerk
            const Lage auf = boden.kind(0, f.hub * 0.5 / G, f.nicken * c);
            auf.teil(Teil(sch.teil), 0, 0);
        } else if (sch.art == 2) {
            boden.teil(Teil(sch.teil), 0, -RAUPE_SCHILDHUB * f.schild);
        } else {
            boden.teil(Teil(sch.teil), 0, 0);
        }
    }
    zubehoer(f, boden, gier, RAUPE_FENSTER, RAUPE_LEUCHTE, 1.0, 10.5);
}

void pflug(const Fahrzeug &f, double G, std::vector<Zeichenteil> &z) {
    const int stufe = winkelstufe(gierVon(f));
    const double gier = stufenWinkel(stufe), c = std::cos(gier), s = std::sin(gier);
    // beim Wenden fährt er einen Bogen: ein Stück zum Betrachter hin bzw. von ihm weg
    const Lage boden{f.x, f.y + f.tiefeZ * std::sin(KAMERA_NEIGUNG), f.neigung + f.wackeln * 0.5, G, G, &z};
    if (f.phase != FahrPhase::AmSeil) boden.teil(Teil::Schatten, 0, 0.5, 0, 0.4 + 0.45 * std::abs(c), 0.5, 0.3);
    const bool leer = f.raus > 0.05 && !dreht(f);
    Schicht schichten[6];
    int n = 0;
    // vier Räder: drehen sich mit dem Weg, die vorderen schlagen beim Lenken ein
    const double umdrehung = f.kette / G / PFLUG_RADHOEHE / (3.14159265359 / 3);
    for (double x : {-PFLUG_RADSTAND / 2, PFLUG_RADSTAND / 2})
        for (double seite : {-PFLUG_SPUR, PFLUG_SPUR}) {
            const P2 q = projizieren({x, PFLUG_RADHOEHE, seite}, gier);
            const int radStufe = winkelstufe(gier + (x > 0 ? f.lenk : 0.0));
            schichten[n++] = {x * s + seite * c, 0, q.x, q.y, int(Teil::PflugRad) + radStufe * RADPHASEN + phaseVon(umdrehung, RADPHASEN)};
        }
    schichten[n++] = {0.01, 1, 0, 0, leer ? int(Teil::PflugLeer) + (f.blick > 0 ? 0 : 1) : int(Teil::PflugKoerper) + stufe};
    schichten[n++] = {26 * s + 9.5 * std::abs(c), 2, 0, 0, int(Teil::PflugSchild) + stufe};
    std::sort(schichten, schichten + n, [](const Schicht &a, const Schicht &b) { return a.tiefe < b.tiefe; });
    for (int i = 0; i < n; ++i) {
        const Schicht &sch = schichten[i];
        if (sch.art == 1) boden.kind(0, f.hub * 0.5 / G, f.nicken * c).teil(Teil(sch.teil), 0, 0);
        else if (sch.art == 2) boden.teil(Teil(sch.teil), 0, -PFLUG_SCHILDHUB * f.schild);
        else boden.teil(Teil(sch.teil), sch.x, sch.y);
    }
    zubehoer(f, boden, gier, PFLUG_FENSTER, PFLUG_LEUCHTE, 0.74, 8.4);
}

void seilMalen(const Einsatz &e, double G, std::vector<Zeichenteil> &z) {
    // gespannt gerade, sonst hängt es leicht durch
    const double x0 = e.heli.x, y0 = e.heli.y, x1 = e.seil.x, y1 = e.seil.y;
    const double dist = std::hypot(x1 - x0, y1 - y0);
    const double durch = std::max(0.0, e.seil.laenge - dist) * 0.6;
    const int n = durch > 1 ? 6 : 1;
    double ax = x0, ay = y0;
    for (int i = 1; i <= n; ++i) {
        const double t = double(i) / n;
        const double bx = x0 + (x1 - x0) * t - e.seil.vx * 0.01 * 4 * t * (1 - t);
        const double by = y0 + (y1 - y0) * t + durch * 4 * t * (1 - t);
        const double l = std::hypot(bx - ax, by - ay);
        z.push_back({uint16_t(Teil::Seil), float((ax + bx) / 2), float((ay + by) / 2), float(std::atan2(by - ay, bx - ax)),
                     float((l + 0.6) / 4), float(std::max(1.2, 1.5 * G) / 2), 1});
        ax = bx; ay = by;
    }
    const double wi = std::atan2(x0 - x1, y1 - y0); // Haken hängt in Seilrichtung
    z.push_back({uint16_t(Teil::Haken), float(x1), float(y1), float(-wi), float(G), float(G), 1});
}

void heli(const Einsatz &e, const Simulation &sim, double G, double zeit, std::vector<Zeichenteil> &z) {
    const Hubschrauber &h = e.heli;
    const double wackel = h.stoss * std::sin(zeit * 30);
    // Schatten auf dem Schnee darunter
    const double oben = e.kanteY - std::max(0.0, sim.hoeheBei(e.id, h.x));
    const double hoch = oben - h.y;
    if (hoch > 0 && hoch < 420 * G && sim.decke(e.id)) {
        const double naehe = 1 - hoch / (420 * G);
        const double lang = 0.45 + 0.55 * std::abs(std::cos(h.gier));
        z.push_back({uint16_t(Teil::Schatten), float(h.x), float(oben + 1), 0, float(G * (1.2 + 1.0 * naehe) * lang), float(G * 0.6), float(0.34 * naehe)});
    }
    // Rumpf in der Ansicht zum Gierwinkel; die Neigung (Nicken) sieht man nur von der Seite voll
    const int stufe = winkelstufe(h.gier);
    const double gier = stufenWinkel(stufe);
    const double nick = h.neigung * std::cos(gier);
    const Lage l{h.x, h.y, nick + wackel, G, G, &z};
    l.teil(Teil(int(Teil::HeliKoerper) + stufe), 0, 0);
    // Rotor: Blattkreis über dem Mast, kippt beim Beschleunigen etwas weiter als der Rumpf
    const P2 r = projizieren(HELI_ROTOR, gier);
    l.teil(Teil(int(Teil::HeliRotor0) + int(zeit * 30) % 4), r.x, r.y, nick * 0.6);
    // Lichter: rotes Blinklicht am Bauch, weißer Doppelblitz am Leitwerk, Positionslicht der zugewandten Seite
    auto licht = [&](Teil t, P3 p, double gr, double a) { const P2 q = projizieren(p, gier); l.teil(t, q.x, q.y, 0, gr, gr, a); };
    if (std::fmod(zeit, 1.0) < 0.12) licht(Teil::LichtRot, HELI_BLINKLICHT, 1.1, 1);
    const double t2 = std::fmod(zeit + 0.4, 1.3);
    if (t2 < 0.05 || (t2 > 0.12 && t2 < 0.17)) licht(Teil::LichtWeiss, HELI_HECKBLITZ, 1.3, 1);
    if (tiefe(HELI_GRUEN, gier) > 2) licht(Teil::LichtGruen, HELI_GRUEN, 0.5, 0.9);
    if (tiefe(HELI_ROT, gier) > 2) licht(Teil::LichtRot, HELI_ROT, 0.5, 0.9);
}

} // namespace

double blasenSkala(double g) { return std::clamp(g, 1.0, 1.5); }

void szeneAnsicht(int art, double gier, double x, double y, double lauf, Szene &aus) {
    if (art == 2) {
        Einsatz e;
        e.heli.x = x; e.heli.y = y; e.heli.gier = gier;
        e.kanteY = -1e9;
        Simulation leer;
        heli(e, leer, 1.0, lauf, aus.teile);
        return;
    }
    Fahrzeug f;
    f.art = art == 0 ? Fahrzeugart::Raupe : Fahrzeugart::Pflug;
    f.phase = FahrPhase::Wenden;
    f.dreht = true;
    f.gier = gier;
    f.x = x; f.y = y;
    f.kette = lauf * 4;
    f.lenk = art == 1 ? 0.5 : 0;
    f.schild = 0;
    if (art == 0) raupe(f, 1.0, aus.teile); else pflug(f, 1.0, aus.teile);
}

void szeneBauen(const Raeumdienst &rd, const Simulation &sim, double zeit, Szene &aus) {
    aus.leeren();
    const double G = rd.einstellungen().fahrzeugGroesse;
    std::vector<Zeichenteil> &z = aus.teile;
    for (const Einsatz &e : rd.einsaetze()) {
        const Fahrzeug &f = e.fz;
        // Wulst vor dem Schild: sitzt auf dem liegenden Schnee, läuft nach vorn aus
        const Decke *d = sim.decke(e.id);
        const double sb = sim.einstellungen().spaltenBreite;
        const double flaeche = f.wulst * sb;
        if (d && flaeche > 1 && f.phase != FahrPhase::AmSeil) {
            const double vorn = f.x + f.blick * massVon(f.art).schildVorn * G - f.blick * 1.5 * G;
            const double WG = wulstMass(f.art, G);
            const double laenge = wulstLaenge(flaeche, WG);
            // die ganze Decke noch einmal mit aufgesetztem Wulst: so stimmen Enden und Überhang mit der echten Decke überein
            Decke h;
            h.x = d->x; h.y = d->y; h.spaltenBreite = d->spaltenBreite;
            h.hoehe = d->hoehe;
            h.sichtbar = d->sichtbar;
            bool etwas = false;
            const long i0 = std::max(0L, long((std::min(vorn, vorn + f.blick * laenge) - d->x) / sb) - 1);
            const long i1 = std::min(long(h.hoehe.size()), long((std::max(vorn, vorn + f.blick * laenge) - d->x) / sb) + 2);
            for (long i = i0; i < i1; ++i) {
                const double x = d->x + (i + 0.5) * sb, ab = (x - vorn) * f.blick;
                if (ab < 0 || x < e.segA || x > e.segB || !h.sichtbar[size_t(i)]) continue;
                // aufgeworfener Schnee ist klumpig: kleine Buckel, die mit dem Schild mitwandern
                const double buckel = 1 + 0.05 * std::sin(ab * 0.21 + f.kette * 0.04) + 0.06 * std::sin(ab * 0.083 + 1.3);
                h.hoehe[size_t(i)] += float(wulstProfil(flaeche, WG, ab) * buckel);
                etwas = true;
            }
            if (etwas) aus.haufen.push_back(std::move(h));
        }
        const bool heliVorn = f.phase == FahrPhase::AmSeil;
        if (!heliVorn) heli(e, sim, G, zeit, z);
        if (f.art == Fahrzeugart::Raupe) raupe(f, G, z); else pflug(f, G, z);
        seilMalen(e, G, z);
        if (heliVorn) heli(e, sim, G, zeit, z);
        sprechblase(f, G, z);
    }
    for (const Staub &s : rd.staub()) {
        const float t = s.alter / s.dauer;
        const float gr = s.groesse * (s.art == 1 ? 1.0f + 1.6f * t : 0.8f + 0.9f * t) / 8;
        const float a = (s.art == 1 ? 0.5f : 0.85f) * (1 - t) * std::min(1.0f, s.alter * 12 + 0.3f);
        z.push_back({uint16_t(s.art == 1 ? Teil::Rauch : Teil::Staub), s.x, s.y, 0, gr, gr, a});
    }
    auto ball = [&](const Schneeball &b) {
        const float s = float(b.radius() / 16);
        z.push_back({uint16_t(Teil::Ball), float(b.x), float(b.y), float(b.winkel), s, s, 1});
    };
    for (const Schneeball &b : rd.baelle()) ball(b);
    if (const Schneeball *b = rd.ballInHand()) ball(*b);
}

} // namespace schnee
