#include "raeumszene.h"
#include <algorithm>
#include <cmath>

namespace schnee {
namespace {

// Lage eines Körpers: Teile werden in seinen Einheiten angegeben (x nach vorn, y nach unten)
struct Lage {
    double x, y, winkel;
    double sx, sy; // px je Einheit; sx < 0 = gespiegelt
    std::vector<Zeichenteil> *ziel;
    void teil(Teil t, double lx, double ly, double lokalWinkel = 0, double fx = 1, double fy = 1, double alpha = 1) const {
        const double c = std::cos(winkel), s = std::sin(winkel);
        const double px = lx * sx, py = ly * sy;
        const double spiegel = sx < 0 ? -1 : 1;
        ziel->push_back({uint8_t(t), float(x + c * px - s * py), float(y + s * px + c * py), float(winkel + spiegel * lokalWinkel),
                         float(sx * fx), float(sy * fy), float(alpha)});
    }
    // Unterkörper an lokaler Stelle mit eigener Drehung
    Lage kind(double lx, double ly, double lokalWinkel) const {
        const double c = std::cos(winkel), s = std::sin(winkel);
        const double px = lx * sx, py = ly * sy;
        return {x + c * px - s * py, y + s * px + c * py, winkel + (sx < 0 ? -1 : 1) * lokalWinkel, sx, sy, ziel};
    }
};

double mindestens(double v, double m) { return std::abs(v) < m ? (v < 0 ? -m : m) : v; }

void sprechblase(const Fahrzeug &f, double G, std::vector<Zeichenteil> &z) {
    if (f.spruch < 0 || f.blaseRest <= 0) return;
    const double B = blasenSkala(G);
    const double alpha = std::clamp(f.blaseRest / 0.3, 0.0, 1.0);
    const double hoehe = massVon(f.art).hoehe;
    // ploppt auf: kurz etwas größer
    z.push_back({uint8_t(int(Teil::Blase0) + f.spruch), float(f.x + 8 * G), float(f.y - (hoehe + 6) * G), 0, float(B), float(B), float(alpha)});
}

void raupe(const Fahrzeug &f, double G, std::vector<Zeichenteil> &z) {
    const double w = mindestens(f.wende, 0.12);
    const Lage boden{f.x, f.y, f.neigung + f.wackeln * 0.5, G * w, G, &z};
    if (f.phase != FahrPhase::AmSeil) boden.teil(Teil::Schatten, 0, 0.5, 0, 1.25, 0.55, 0.3);
    boden.teil(Teil::RaupeUnterwagen, 0, 0);
    // Räder drehen passend zum gefahrenen Weg (in Einheiten)
    const double weg = f.kette / G;
    boden.teil(Teil::Antriebsrad, -23, -9, weg / 7);
    boden.teil(Teil::Leitrad, 22, -8.5, weg / 6.5);
    for (double x : {-13.0, -5.5, 2.0, 9.5}) boden.teil(Teil::Laufrolle, x, -4.6, weg / 3.5);
    // Kette: Glieder laufen um (unten rückwärts, oben vorwärts)
    const double r1 = 8, r2 = 7.6, gerade = 45, b1 = 3.14159265 * r1, b2 = 3.14159265 * r2, umfang = 2 * gerade + b1 + b2;
    const int glieder = 26;
    for (int i = 0; i < glieder; ++i) {
        double s = std::fmod(i * umfang / glieder + weg, umfang);
        if (s < 0) s += umfang;
        double x, y, wi;
        if (s < gerade) { x = 22 - s; y = -1.2; wi = 0; }
        else if (s < gerade + b1) { const double a = (s - gerade) / r1; x = -23 - std::sin(a) * r1; y = -9.2 + std::cos(a) * r1; wi = a; }
        else if (s < 2 * gerade + b1) { const double t = (s - gerade - b1) / gerade; x = -23 + t * gerade; y = -17.2 + t * 1.0; wi = 3.14159265 + 0.022; }
        else { const double a = (s - 2 * gerade - b1) / r2; x = 22 + std::sin(a) * r2; y = -8.6 - std::cos(a) * r2; wi = 3.14159265 + a; }
        boden.teil(Teil::Kettenglied, x, y, wi);
    }
    // Aufbau federt und nickt auf dem Fahrwerk
    const Lage auf = boden.kind(0, -14 + f.hub * 0.5 / G, f.nicken);
    auf.teil(Teil::RaupeAufbau, 0, 0);
    // Fahrer sitzt hinter Scheibe und Tür; beim Streik lehnt er sich bis zur Hüfte heraus
    const bool draussen = f.raus > 0.3;
    const Lage fahrer = auf.kind(-10 - 4 * f.raus, -8 - 3 * f.raus, -0.3 * f.raus);
    auto fahrerMalen = [&] {
        fahrer.teil(Teil::Fahrer, 0, 0);
        if (f.raus > 0.15) fahrer.teil(Teil::Arm, -1.5, -10.5, (2.55 + 0.45 * std::sin(f.faust)) * f.raus, 1, 1, std::min(1.0, f.raus * 2));
    };
    if (!draussen) fahrerMalen();
    auf.teil(Teil::RaupeGlas, -22, -30);
    if (draussen) fahrerMalen();
    if (f.schneeDach > 0.05) {
        auf.teil(Teil::SchneeDach, -10, -30.6, 0, 1, 0.35 + 0.65 * f.schneeDach);
        auf.teil(Teil::SchneeHaube, 14, -16.6, 0.05, 1, 0.3 + 0.7 * f.schneeDach);
    }
    // Schild hebt und senkt sich am Schubarm
    boden.teil(Teil::RaupeSchild, 19, -8, -0.42 * f.schild);
    // Rundumleuchte: dreht sich, das Licht schwillt an und ab
    const double dreh = f.leuchte * 7.5;
    const double hell = 0.3 + 0.7 * std::pow(std::max(0.0, std::sin(dreh)), 2);
    auf.teil(Teil::LichtOrange, -16, -33.5, 0, 0.55 + 0.75 * std::abs(std::cos(dreh * 0.5)), 0.8, hell);
    for (const Klecks &k : f.kleckse) boden.teil(Teil::Klecks, k.x, k.y, k.x, k.groesse / 10, k.groesse / 10, 0.95);
}

void pflug(const Fahrzeug &f, double G, std::vector<Zeichenteil> &z) {
    const double w = mindestens(f.wende, 0.12);
    const Lage boden{f.x, f.y, f.neigung + f.wackeln * 0.5, G * w, G, &z};
    if (f.phase != FahrPhase::AmSeil) boden.teil(Teil::Schatten, 0, 0.5, 0, 0.8, 0.45, 0.3);
    const double weg = f.kette / G;
    const Lage auf = boden.kind(0, -3 + f.hub * 0.5 / G, f.nicken);
    auf.teil(Teil::PflugRumpf, 0, 0);
    const Lage fahrer = auf.kind(-5, -9 - 3.5 * f.raus, -0.15 * f.raus);
    fahrer.teil(Teil::Fahrer, 0, 0, 0, 0.9, 0.9);
    if (f.raus > 0.15) fahrer.teil(Teil::Arm, -1.5, -10, (2.55 + 0.45 * std::sin(f.faust)) * f.raus, 0.9, 0.9, std::min(1.0, f.raus * 2));
    boden.teil(Teil::Rad, -11, -5.8, weg / 5.8);
    boden.teil(Teil::Rad, 11, -5.8, weg / 5.8);
    boden.teil(Teil::PflugSchild, 13, -5, -0.4 * f.schild);
    if (f.schneeDach > 0.05) auf.teil(Teil::SchneeHaube, 9, -12.8, 0.03, 0.6, 0.3 + 0.7 * f.schneeDach);
    const double dreh = f.leuchte * 7.5;
    const double hell = 0.3 + 0.7 * std::pow(std::max(0.0, std::sin(dreh)), 2);
    auf.teil(Teil::LichtOrange, -11, -23.5, 0, 0.45 + 0.6 * std::abs(std::cos(dreh * 0.5)), 0.65, hell);
    for (const Klecks &k : f.kleckse) boden.teil(Teil::Klecks, k.x, k.y, k.x, k.groesse / 10, k.groesse / 10, 0.95);
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
        z.push_back({uint8_t(Teil::Seil), float((ax + bx) / 2), float((ay + by) / 2), float(std::atan2(by - ay, bx - ax)),
                     float((l + 0.6) / 4), float(std::max(1.2, 1.5 * G) / 2), 1});
        ax = bx; ay = by;
    }
    const double wi = std::atan2(x0 - x1, y1 - y0); // Haken hängt in Seilrichtung
    z.push_back({uint8_t(Teil::Haken), float(x1), float(y1), float(-wi), float(G), float(G), 1});
}

void heli(const Einsatz &e, const Simulation &sim, double G, double zeit, std::vector<Zeichenteil> &z) {
    const Hubschrauber &h = e.heli;
    const double b = mindestens(h.blick, 0.14);
    const double wackel = h.stoss * std::sin(zeit * 30);
    const Lage l{h.x, h.y, h.neigung + wackel, G * b, G, &z};
    // Schatten auf dem Schnee darunter
    const double oben = e.kanteY - std::max(0.0, sim.hoeheBei(e.id, h.x));
    const double hoch = oben - h.y;
    if (hoch > 0 && hoch < 420 * G && sim.decke(e.id)) {
        const double naehe = 1 - hoch / (420 * G);
        z.push_back({uint8_t(Teil::Schatten), float(h.x), float(oben + 1), 0, float(G * (1.2 + 1.0 * naehe)), float(G * 0.6), float(0.34 * naehe)});
    }
    const int bild = int(zeit * 30) % 3;
    l.teil(Teil(int(Teil::Heckrotor0) + bild), -82, -31, 0, 1 / std::abs(b) * 0.9, 1);
    l.teil(Teil::HeliRumpf, 0, 0);
    // Rotorscheibe bleibt beim Wenden voll breit
    l.teil(Teil(int(Teil::HeliRotor0) + bild), -4, -42.5, 0, 1 / std::abs(b), 1);
    // Positionslichter: grün am Bug, rotes Blinklicht unten, weißer Doppelblitz am Heck
    l.teil(Teil::LichtGruen, 50, -14, 0, 0.5, 0.5, 0.9);
    const double t1 = std::fmod(zeit, 1.0);
    if (t1 < 0.12) l.teil(Teil::LichtRot, -22, 0.5, 0, 1.1, 1.1, 1);
    const double t2 = std::fmod(zeit + 0.4, 1.3);
    if (t2 < 0.05 || (t2 > 0.12 && t2 < 0.17)) l.teil(Teil::LichtWeiss, -86, -41, 0, 1.3, 1.3, 1);
}

} // namespace

double blasenSkala(double g) { return std::clamp(g, 1.0, 1.5); }

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
        z.push_back({uint8_t(s.art == 1 ? Teil::Rauch : Teil::Staub), s.x, s.y, 0, gr, gr, a});
    }
    auto ball = [&](const Schneeball &b) {
        const float s = float(b.radius() / 16);
        z.push_back({uint8_t(Teil::Ball), float(b.x), float(b.y), float(b.winkel), s, s, 1});
    };
    for (const Schneeball &b : rd.baelle()) ball(b);
    if (const Schneeball *b = rd.ballInHand()) ball(*b);
}

} // namespace schnee
