#include "raeumdienst.h"
#include <algorithm>
#include <cmath>

namespace schnee {
namespace {
constexpr double SCHWERKRAFT = 900;    // px/s² für Seil und Bälle
constexpr double SEIL_FLUG = 46;       // Seillänge im Flug (Einheiten)
constexpr double BALL_MAX = 220;       // größte Ballmenge
constexpr double ANFASS_HOEHE = 6;     // ab dieser Deckenhöhe ist Schnee anfassbar
double vorzeichen(double v) { return v < 0 ? -1.0 : 1.0; }

const char *const SPRUECHE[] = {
    "Hey! Ich arbeite hier!",
    "Noch einer und ich fahr heim!",
    "Wer war das?!",
    "So nicht, Freundchen!",
    "Ich räum hier DEINEN Schnee weg!",
    "Lass das! Das Ding ist frisch gewaschen!",
    "Streik! Ich mach erst mal Pause.",
    "Das melde ich der Gewerkschaft!",
    "Mir reicht's! Ich fahr heim!",
    "Räum deinen Schnee doch selber!",
    "Feierabend. Tschüss!",
    "Moin!",
    "Nicht anfassen, bitte.",
};
} // namespace

int spruchAnzahl() { return int(sizeof SPRUECHE / sizeof *SPRUECHE); }
const char *spruchText(int i) { return i >= 0 && i < spruchAnzahl() ? SPRUECHE[i] : ""; }

double wulstMass(Fahrzeugart a, double g) { return a == Fahrzeugart::Pflug ? 0.55 * g : g; }

Fahrzeugmass massVon(Fahrzeugart a) {
    if (a == Fahrzeugart::Raupe) return {32, 44, 39, 36, 22};
    return {19, 27, 24, 23, 14};
}

double Schneeball::radius() const { return std::clamp(1.15 * std::sqrt(std::max(0.0, menge) * 3.0 / 3.14159265), 3.0, 18.0); }

// ---- Wulst: flacher Keil vor dem Schild, bis hmax hoch, dann nur noch länger ----
static double wulstMaxHoehe(double g) { return 26 * g; }
double wulstHoehe(double a, double g) { return std::min(wulstMaxHoehe(g), std::sqrt(std::max(0.0, a) / 2)); }
double wulstLaenge(double a, double g) {
    const double hm = wulstMaxHoehe(g);
    return a <= 2 * hm * hm ? 3.2 * std::sqrt(std::max(0.0, a) / 2) : 1.6 * a / hm;
}
double wulstFlaecheFuerLaenge(double l, double g) {
    const double hm = wulstMaxHoehe(g);
    if (l <= 0) return 0;
    if (l <= 3.2 * hm) return 2 * (l / 3.2) * (l / 3.2);
    return hm * l / 1.6;
}
double wulstKapazitaet(double g) { return wulstMaxHoehe(g) * 300 * g / 1.6; }
double wulstProfil(double a, double g, double s) {
    const double l = wulstLaenge(a, g);
    if (a <= 0 || s < 0 || s >= l) return 0;
    return wulstHoehe(a, g) * std::pow(1 - s / l, 0.6);
}

// ---- Seilpendel (positionsbasiert: unbedingt stabil, Seil zieht nur) ----
void Seilpendel::haengen(double ankerX, double ankerY) {
    ax = ankerX; ay = ankerY;
    x = ankerX; y = ankerY + laenge;
    vx = vy = 0;
}

void Seilpendel::schritt(double dt, double ankerX, double ankerY) {
    if (dt <= 0) return;
    const int n = std::max(1, int(std::ceil(dt * 240)));
    const double h = dt / n;
    const double ax0 = ax, ay0 = ay;
    for (int i = 1; i <= n; ++i) {
        const double px = ax0 + (ankerX - ax0) * i / n, py = ay0 + (ankerY - ay0) * i / n;
        const double daempf = std::exp(-daempfung * h);
        vy += SCHWERKRAFT * h;
        vx *= daempf; vy *= daempf;
        const double xa = x, ya = y;
        x += vx * h; y += vy * h;
        const double dx = x - px, dy = y - py, dist = std::hypot(dx, dy);
        if (dist > laenge && dist > 1e-9) {
            x = px + dx / dist * laenge;
            y = py + dy / dist * laenge;
        }
        vx = (x - xa) / h; vy = (y - ya) / h;
        // das Seil gibt keine Energie: nach dem Zurückholen höchstens so schnell wie der Anker plus Fallgeschwindigkeit
        const double grenze = 3000;
        const double v = std::hypot(vx, vy);
        if (v > grenze) { vx *= grenze / v; vy *= grenze / v; }
    }
    ax = ankerX; ay = ankerY;
}

double Seilpendel::winkel() const { return std::atan2(x - ax, std::max(1e-6, y - ay)); }

// ---- Hubschrauber: fliegt weich zum Ziel, neigt sich mit der Beschleunigung ----
void Hubschrauber::schritt(double dt, double maxV, double maxA) {
    unruhe += dt;
    rotor += dt * 38;
    stoss *= std::exp(-3 * dt);
    const double zx = zielX + 3 * std::sin(unruhe * 1.3) + 2 * std::sin(unruhe * 2.9 + 1);
    const double zy = zielY + 2.5 * std::sin(unruhe * 1.7 + 0.5);
    const double wx = std::clamp(1.8 * (zx - x), -maxV, maxV), wy = std::clamp(1.8 * (zy - y), -maxV * 0.6, maxV * 0.6);
    const double bx = std::clamp((wx - vx) * 3, -maxA, maxA), by = std::clamp((wy - vy) * 3, -maxA, maxA);
    vx += bx * dt; vy += by * dt;
    x += vx * dt; y += vy * dt;
    const double zielNeigung = std::clamp(bx / maxA * 0.2 + vx / maxV * 0.1, -0.3, 0.3);
    neigung += (zielNeigung - neigung) * std::min(1.0, 4 * dt);
    // dreht die Nase nur zum Ziel, wenn es weiter weg ist (kein Hin und Her beim Abbremsen und Schweben)
    const double zielBlick = zielX - x > 130 ? 1 : zielX - x < -130 ? -1 : (blick < 0 ? -1 : 1);
    blick += std::clamp(zielBlick - blick, -3 * dt, 3 * dt);
}

Raeumdienst::Raeumdienst(Simulation &sim, uint32_t saat) : m_sim(sim), m_rng(saat) {}

double Raeumdienst::fuellstand(const Decke &d) const {
    std::vector<float> h;
    for (size_t i = 0; i < d.hoehe.size(); ++i)
        if (d.sichtbar[i]) h.push_back(d.hoehe[i]);
    if (h.empty()) return 0;
    const size_t k = std::min(h.size() - 1, size_t(h.size() * 0.9));
    std::nth_element(h.begin(), h.begin() + long(k), h.end());
    return h[k] / m_sim.maxHoeheVon(d);
}

// zusammenhängender sichtbarer Lauf der Decke um x (Welt-x-Grenzen)
bool Raeumdienst::abschnittUm(const Decke &d, double x, double &a, double &b) const {
    const double rel = (x - d.x) / d.spaltenBreite;
    if (rel < 0 || size_t(rel) >= d.hoehe.size() || !d.sichtbar[size_t(rel)]) return false;
    size_t i0 = size_t(rel), i1 = i0;
    while (i0 > 0 && d.sichtbar[i0 - 1]) --i0;
    while (i1 + 1 < d.hoehe.size() && d.sichtbar[i1 + 1]) ++i1;
    a = d.x + i0 * d.spaltenBreite;
    b = d.x + (i1 + 1) * d.spaltenBreite;
    return true;
}

bool Raeumdienst::buchen(const std::string &id, bool vorne) {
    const Decke *d = m_sim.decke(id);
    if (!d) return false;
    for (const Einsatz &e : m_einsaetze) if (e.id == id) return false;
    if (std::find(m_schlange.begin(), m_schlange.end(), id) != m_schlange.end()) return false;
    if (vorne) m_schlange.push_front(id); else m_schlange.push_back(id);
    return true;
}

std::string Raeumdienst::jetztRaeumen() {
    const Decke *beste = nullptr;
    double hoechste = 1.5; // darunter lohnt kein Einsatz
    for (const Decke &d : m_sim.decken()) {
        if ((d.boden || d.dock) && !m_e.taskleiste) continue;
        bool laeuft = false;
        for (const Einsatz &e : m_einsaetze) laeuft |= e.id == d.id;
        if (laeuft) continue;
        const double h = fuellstand(d) * m_sim.maxHoeheVon(d) * (d.boden || d.dock ? 0.5 : 1.0); // Fenster zuerst
        if (h > hoechste) { hoechste = h; beste = &d; }
    }
    if (!beste) return {};
    const std::string id = beste->id;
    m_schlange.erase(std::remove(m_schlange.begin(), m_schlange.end(), id), m_schlange.end());
    if (int(m_einsaetze.size()) < std::max(1, m_e.gleichzeitig)) {
        if (!starten(id)) return {};
    } else {
        m_schlange.push_front(id);
    }
    return id;
}

bool Raeumdienst::starten(const std::string &id) {
    const Decke *d = m_sim.decke(id);
    if (!d || m_sim.bildschirme().empty()) return false;
    const double G = g(), sb = d->spaltenBreite;
    Einsatz e;
    e.fz.art = d->boden || d->dock ? Fahrzeugart::Pflug : Fahrzeugart::Raupe;
    const Fahrzeugmass m = massVon(e.fz.art);
    // sichtbarer Lauf mit dem meisten Schnee, auf den das Fahrzeug passt
    const double mindest = (2 * m.halb + 16) * G;
    double besteMenge = -1;
    for (size_t i = 0; i < d->hoehe.size();) {
        if (!d->sichtbar[i]) { ++i; continue; }
        size_t j = i;
        double summe = 0;
        while (j < d->hoehe.size() && d->sichtbar[j]) summe += d->hoehe[j++];
        if ((j - i) * sb >= mindest && summe > besteMenge) {
            besteMenge = summe;
            e.segA = d->x + i * sb;
            e.segB = d->x + j * sb;
        }
        i = j;
    }
    if (besteMenge < 0) return false;
    e.nr = m_naechsteNr++;
    e.id = id;
    e.schluessel = d->schluessel;
    e.boden = d->boden;
    e.kanteY = d->y;
    e.deckX = d->x;
    const double landX = (e.segA + e.segB) / 2;
    e.fz.mitte = landX - d->x;
    double links = 1e300, rechts = -1e300;
    for (const Rechteck &b : m_sim.bildschirme()) { links = std::min(links, b.x); rechts = std::max(rechts, b.rechts()); }
    e.ausgang = landX - links < rechts - landX ? -1 : 1;
    e.warteSeite = -e.ausgang; // wartet auf der Seite mit mehr Platz
    const double fzH = m.hoehe * G, flug = SEIL_FLUG * G;
    e.heli.x = e.ausgang < 0 ? links - 140 * G : rechts + 140 * G;
    e.heli.y = d->y - fzH - flug - 25 * G - 70;
    e.heli.blick = -e.ausgang;
    e.heli.zielX = landX;
    e.heli.zielY = d->y - fzH - flug - 25 * G;
    e.seil.laenge = flug;
    e.seil.haengen(e.heli.x, e.heli.y);
    e.fz.blick = e.fz.richtung = 1;
    e.fz.wende = 1;
    e.fz.x = e.seil.x;
    e.fz.y = e.seil.y + fzH;
    m_einsaetze.push_back(std::move(e));
    return true;
}

void Raeumdienst::klang(Geraeusch a, double x, double y, double staerke) {
    m_klaenge.push_back({Klangereignis::Einmal, a, 0, x, y, std::clamp(staerke, 0.0, 2.0)});
}

void Raeumdienst::dauer(const Einsatz &e, int kanal, Geraeusch a, double x, double y, double last) {
    const int id = e.nr * 4 + kanal;
    m_klaenge.push_back({Klangereignis::Dauer, a, id, x, y, std::clamp(last, 0.0, 1.0)});
    m_dauerNeu.push_back(id);
}

std::vector<Klangereignis> Raeumdienst::klaengeAbholen() {
    std::vector<Klangereignis> k;
    k.swap(m_klaenge);
    return k;
}

void Raeumdienst::klaengeAnhalten() {
    for (int id : m_dauerAn) m_klaenge.push_back({Klangereignis::Stopp, Geraeusch::Rotor, id, 0, 0, 0});
    m_dauerAn.clear();
}

double Raeumdienst::getrageneMenge() const {
    double m = m_inHand ? m_hand.menge : 0;
    for (const Schneeball &b : m_baelle) m += b.menge;
    for (const Einsatz &e : m_einsaetze) {
        m += e.fz.wulst + e.fz.kippRest;
        for (const Klecks &k : e.fz.kleckse) m += k.menge;
    }
    return m;
}

void Raeumdienst::sagen(Fahrzeug &f, int von, int anzahl, double dauer) {
    f.spruch = von + (m_spruchNr++ % anzahl);
    f.blaseRest = dauer;
}

void Raeumdienst::staubWolke(double x, double y, int anzahl, double tempo, double groesse, uint8_t art) {
    for (int i = 0; i < anzahl && m_staub.size() < 500; ++i) {
        const double w = zufall() * 6.2831853, v = tempo * (0.25 + 0.75 * zufall());
        Staub s;
        s.x = float(x + (zufall() - 0.5) * groesse);
        s.y = float(y + (zufall() - 0.5) * groesse);
        s.vx = float(std::cos(w) * v);
        s.vy = float(std::sin(w) * v * 0.7 - tempo * 0.3);
        s.alter = 0;
        s.dauer = float(0.5 + 0.6 * zufall());
        s.groesse = float(groesse * (0.5 + 0.8 * zufall()));
        s.art = art;
        m_staub.push_back(s);
    }
}

// Schnee als Pulverklümpchen fallen lassen (Menge bleibt erhalten: der Rest geht auf die Decke oder fällt als Brocken)
void Raeumdienst::pulver(double x, double y, double menge, double tempo, double vx0, double vy0, const std::string &rest) {
    if (menge <= 0) return;
    const double portion = m_sim.ablageJeFlocke(m_sim.einstellungen().groesse);
    int n = portion > 0 ? int(menge / portion) : 0;
    n = std::min(n, 50);
    for (int i = 0; i < n; ++i) {
        Flocke f{x + (zufall() - 0.5) * 8, y + (zufall() - 0.5) * 8, vx0 + (zufall() - 0.5) * tempo, vy0 + 25 + zufall() * tempo * 0.6,
                 m_sim.einstellungen().groesse, zufall() * 6.2831853};
        f.fang = 1.0f;
        f.art = Flockenart::Pulver;
        f.los = uint32_t(m_rng());
        m_sim.flockeHinzufuegen(f);
    }
    double uebrig = menge - n * portion;
    if (uebrig > 1e-9 && !rest.empty()) uebrig -= m_sim.schneeHinzufuegen(rest, x, uebrig);
    if (uebrig > 1e-9) m_sim.brockenFallenLassen(x, y, m_sim.einstellungen().spaltenBreite * 2, uebrig, vx0 * 0.3, 20);
}

// Schnee über die Kante werfen: sammelt kleine Mengen und lässt sie als Brocken fallen
// seitlich = der Wulst ist voll: der Überschuss rutscht am Schild vorbei und fällt vorn über die Fensterkante
void Raeumdienst::abwerfen(Einsatz &e, double menge, bool alles, bool seitlich) {
    Fahrzeug &f = e.fz;
    f.kippRest += menge;
    const double sb = m_sim.einstellungen().spaltenBreite;
    if (f.kippRest <= 0 || (!alles && f.kippRest < 26)) return;
    const double m = f.kippRest;
    f.kippRest = 0;
    const int r = f.richtung;
    const double kante = seitlich ? f.x + r * (massVon(f.art).schildVorn + 3) * g() : (r > 0 ? e.segB : e.segA);
    if (e.boden) {
        // am Bildschirmrand: der Schnee ist weg, nur etwas Staub bleibt in der Luft
        staubWolke(kante, e.kanteY - 6, 4, 60, 8);
    } else {
        const double breite = std::clamp(m / 5 * sb, 2 * sb, 12 * sb);
        if (seitlich) m_sim.brockenFallenLassen(kante, e.kanteY - 2, std::min(breite, 5 * sb), m, r * std::abs(f.v) * 0.6, 25, e.schluessel);
        else m_sim.brockenFallenLassen(kante + r * (breite / 2 + 1), e.kanteY, breite, m, r * (25 + std::abs(f.v) * 0.5), 10, e.schluessel);
        staubWolke(kante + r * 4, e.kanteY - 4, 4, 70, 7);
    }
    if (e.rutschKlang <= 0) {
        klang(Geraeusch::Rutsch, kante, e.kanteY, std::clamp(m / 60, 0.3, 1.5));
        e.rutschKlang = 0.5;
    }
}

// Rotorabwind: bläst Schnee unter dem Hubschrauber zur Seite und wirbelt Staub auf
void Raeumdienst::abwind(Einsatz &e, double dt) {
    const Decke *d = m_sim.decke(e.id);
    if (!d) return;
    const double hoehe = e.kanteY - e.heli.y;
    if (hoehe < 0 || hoehe > 320 * g()) return;
    e.abwindRest += dt;
    const double sb = d->spaltenBreite;
    while (e.abwindRest > 0.06) {
        e.abwindRest -= 0.06;
        const double seite = zufall() < 0.5 ? -1 : 1;
        const double x = e.heli.x + seite * zufall() * 60 * g();
        const double h = m_sim.hoeheBei(e.id, x);
        if (h < 1) continue;
        const double weg = m_sim.schneeEntnehmen(e.id, x - sb, x + sb, std::min(1.2, h * 0.3));
        if (weg <= 0) continue;
        const double ziel = x + seite * (40 + 70 * zufall()) * g();
        const double an = m_sim.schneeHinzufuegen(e.id, ziel, weg);
        if (an < weg) pulver(ziel, e.kanteY - 2, weg - an, 20, seite * 8, 0, e.id); // über die Kante geblasen
        const double oben = e.kanteY - h;
        Staub s{float(x), float(oben - 1), float(seite * (90 + 160 * zufall())), float(-30 - 70 * zufall()), 0, float(0.5 + 0.5 * zufall()),
                float(5 + 6 * zufall()), 0};
        if (m_staub.size() < 500) m_staub.push_back(s);
    }
}

void Raeumdienst::abbrechen(Einsatz &e, bool deckeWeg) {
    if (e.abbruch) return;
    e.abbruch = true;
    Fahrzeug &f = e.fz;
    const double G = g();
    const Fahrzeugmass m = massVon(f.art);
    const double last = f.wulst + f.kippRest;
    if (last > 0) {
        // was vor dem Schild lag, fällt herunter (bzw. bleibt liegen, wenn die Decke noch da ist)
        const double sb = m_sim.einstellungen().spaltenBreite;
        m_sim.brockenFallenLassen(f.x + f.blick * (m.schildVorn + 8) * G, f.y - 1, 8 * sb, last, 0, 0, deckeWeg ? e.schluessel : 0);
        f.wulst = f.kippRest = 0;
    }
    if (e.phase >= Phase::Abflug) return;
    if (e.angehaengt && f.phase == FahrPhase::AmSeil) {
        e.phase = Phase::Abflug;
    } else if (deckeWeg) {
        // Fenster weg: Seil sofort dran und hoch
        e.eilig = true;
        e.angehaengt = true;
        f.phase = FahrPhase::AmSeil;
        f.v = 0;
        f.spruch = -1;
        f.raus = 0;
        e.seil.x = f.x;
        e.seil.y = f.y - m.hoehe * G;
        e.seil.vx = e.seil.vy = 0;
        e.seil.laenge = std::hypot(e.seil.x - e.heli.x, e.seil.y - e.heli.y);
        e.phase = Phase::Abflug;
    } else {
        if (f.phase != FahrPhase::Beleidigt) f.phase = FahrPhase::Bereit;
        e.phase = Phase::Rueckkehr;
    }
    e.t = 0;
}

void Raeumdienst::fahrzeugSchritt(Einsatz &e, const Decke &d, double dt) {
    Fahrzeug &f = e.fz;
    const double G = g(), sb = d.spaltenBreite;
    const Fahrzeugmass m = massVon(f.art);
    f.t += dt;
    f.y = d.y;
    double a, b;
    if (!abschnittUm(d, f.x, a, b)) { abbrechen(e, false); return; }
    e.segA = a; e.segB = b;
    f.mitte = std::clamp(f.mitte, a - d.x + std::min(m.halb * G, (b - a) / 2), b - d.x - std::min(m.halb * G, (b - a) / 2));
    const double mitteX = d.x + f.mitte;
    const double vSchub = 80 * std::sqrt(G), vRueck = 130 * std::sqrt(G);
    const double WG = wulstMass(f.art, G);
    const double kap = wulstKapazitaet(WG) / sb;
    const int r = f.richtung;
    double zielV = 0, zielSchild = 1, zielLast = 0.12, zielRaus = 0;
    const double vorn0 = f.x + r * m.schildVorn * G;
    const double ende = r > 0 ? b : a;

    switch (f.phase) {
    case FahrPhase::AmSeil: return;
    case FahrPhase::Aufgesetzt:
        if (e.phase == Phase::Raeumen && f.t > 0.3) { f.phase = f.blick != r ? FahrPhase::Wenden : FahrPhase::Senken; f.t = 0; }
        break;
    case FahrPhase::Wenden:
        f.wende += std::clamp(double(r) - f.wende, -2.5 * dt, 2.5 * dt);
        if (std::abs(f.wende - r) < 1e-3) { f.blick = r; f.wende = r; f.phase = FahrPhase::Senken; f.t = 0; }
        break;
    case FahrPhase::Senken:
        zielSchild = 0;
        if (f.schild < 0.03) { f.phase = FahrPhase::Schieben; f.t = 0; }
        break;
    case FahrPhase::Schieben: {
        const double rest = (ende + r * 2 - vorn0) * r;
        zielV = r * std::min(vSchub * (1 - 0.4 * f.last), std::sqrt(2 * 200 * std::max(0.0, rest)) + 5);
        zielSchild = 0;
        zielLast = 0.3 + 0.7 * std::min(1.0, f.wulst / kap);
        if (rest <= 0.5) { f.phase = FahrPhase::Kippen; f.t = 0; zielV = 0; }
        break;
    }
    case FahrPhase::Kippen:
        zielSchild = 0;
        if (f.t > 0.15 && f.wulst + f.kippRest > 0) { abwerfen(e, f.wulst, true); f.wulst = 0; }
        if (f.t > 0.5) {
            // liegt in dieser Bahn noch merklich Schnee? Dann noch ein Durchgang (höchstens drei)
            double summe = 0;
            int anzahl = 0;
            for (double x = std::min(mitteX, ende) + sb / 2; x < std::max(mitteX, ende); x += sb) { summe += m_sim.hoeheBei(e.id, x); ++anzahl; }
            f.nochmal = anzahl > 0 && summe / anzahl > 1.5 && f.durchgang < 2;
            if (f.nochmal) ++f.durchgang; else { ++f.seite; f.durchgang = 0; }
            f.phase = FahrPhase::Zurueck; f.t = 0;
        }
        break;
    case FahrPhase::Zurueck: {
        const double weg = mitteX - f.x;
        zielV = vorzeichen(weg) * std::min(vRueck, std::sqrt(2 * 200 * std::abs(weg)) + 5);
        zielLast = 0.35;
        if (std::abs(weg) < 1.5) {
            zielV = 0; f.v = 0; f.t = 0;
            if (f.nochmal) f.phase = FahrPhase::Senken;
            else if (f.seite < 2) { f.richtung = -r; f.phase = FahrPhase::Wenden; }
            else f.phase = FahrPhase::Bereit;
        } else if (std::abs(f.v) > 5) {
            dauer(e, 2, Geraeusch::Rueckfahrwarner, f.x, f.y, 1);
        }
        break;
    }
    case FahrPhase::Bereit: break;
    case FahrPhase::Streik:
        zielRaus = f.t < 5.3 ? 1 : 0;
        zielSchild = f.schild;
        f.faust += dt * 11;
        if (f.t >= 6) { f.phase = f.vorStreik; f.t = 0; f.spruch = -1; }
        break;
    case FahrPhase::Beleidigt:
        zielRaus = f.t < 2.2 ? 1 : 0;
        f.faust += dt * 14;
        if (f.t >= 3) { f.phase = FahrPhase::Bereit; f.t = 0; abbrechen(e, false); }
        break;
    }

    // Fahren mit begrenzter Beschleunigung, Federung nickt mit
    const double dv = std::clamp(zielV - f.v, -260 * dt, 260 * dt);
    const double beschl = dt > 0 ? dv / dt : 0;
    f.v += dv;
    f.x += f.v * dt;
    f.kette += f.v * f.blick * dt;
    f.nickV += (-120 * f.nicken - 13 * f.nickV - beschl * f.blick * 0.024) * dt;
    f.nicken += f.nickV * dt;
    f.hubV += (-300 * f.hub - 16 * f.hubV) * dt;
    f.hub += f.hubV * dt;
    f.wackelV += (-180 * f.wackeln - 7 * f.wackelV) * dt;
    f.wackeln += f.wackelV * dt;
    f.schild += std::clamp(zielSchild - f.schild, -2.5 * dt, 2.5 * dt);
    f.raus += std::clamp(zielRaus - f.raus, -3 * dt, 3 * dt);
    f.last += (zielLast - f.last) * std::min(1.0, 3 * dt);
    f.leuchte += dt;
    f.schneeDach = std::min(1.0, f.schneeDach + dt * 0.004 * m_sim.einstellungen().menge);
    if (f.blaseRest > 0 && (f.blaseRest -= dt) <= 0) f.spruch = -1;

    if (f.phase == FahrPhase::Schieben) {
        // alles zwischen alter und neuer Schildkante wandert in den Wulst (solange Platz ist)
        const double vorn1 = f.x + r * m.schildVorn * G;
        if (vorn1 != vorn0) f.wulst += m_sim.schneeEntnehmen(e.id, std::min(vorn0, vorn1), std::max(vorn0, vorn1), 1e9);
        // mehr, als sich vor dem Schild türmen kann, rutscht seitlich weg und fällt vorn über die Kante
        if (f.wulst > kap) { abwerfen(e, f.wulst - kap, false, true); f.wulst = kap; }
        // was über die Kante ragt, kippt hinunter
        const double platz = std::max(0.0, (ende - vorn1) * r);
        const double passt = wulstFlaecheFuerLaenge(platz, WG) / sb;
        if (f.wulst > passt) { abwerfen(e, f.wulst - passt, false); f.wulst = passt; }
    }
    // klebende Schneebälle fallen nach einer Weile ab
    for (size_t i = 0; i < f.kleckse.size();) {
        if (m_sim.zeit() - f.kleckse[i].seit > 25) {
            m_sim.brockenFallenLassen(f.x + f.kleckse[i].x * G, f.y - 1, 2 * sb, f.kleckse[i].menge, 0, 0, 0);
            f.kleckse.erase(f.kleckse.begin() + long(i));
        } else ++i;
    }
    // Auspuffrauch je nach Last
    f.rauchRest += dt * (3 + 16 * f.last);
    while (f.rauchRest >= 1) {
        f.rauchRest -= 1;
        const bool raupe = f.art == Fahrzeugart::Raupe;
        const double ax = f.x + f.blick * (raupe ? 9 : -12) * G, ay = f.y - (raupe ? 41 : 20) * G;
        Staub s{float(ax), float(ay), float(-f.v * 0.2 + (zufall() - 0.5) * 12 + m_sim.einstellungen().wind * 0.3),
                float(-28 - 30 * f.last - 10 * zufall()), 0, float(0.7 + 0.6 * zufall()), float((3 + 3 * f.last + 2 * zufall()) * G), 1};
        if (m_staub.size() < 500) m_staub.push_back(s);
    }
    dauer(e, 1, Geraeusch::Motor, f.x, f.y, f.last);
}

void Raeumdienst::einsatzSchritt(Einsatz &e, double dt) {
    const double G = g();
    Fahrzeug &f = e.fz;
    const Fahrzeugmass m = massVon(f.art);
    const double fzH = m.hoehe * G, flug = SEIL_FLUG * G;
    const Decke *d = m_sim.decke(e.id);
    e.t += dt;
    e.rutschKlang -= dt;
    if (!d && !e.abbruch) abbrechen(e, true);
    if (d) {
        // Fenster verschoben: Fahrzeug fährt mit
        if (f.phase != FahrPhase::AmSeil) f.x += d->x - e.deckX;
        e.deckX = d->x;
        e.kanteY = d->y;
    }
    const double landX = d ? d->x + f.mitte : f.x;
    const double schwebeY = e.kanteY - fzH - flug - 25 * G;
    double maxV = 260 * std::sqrt(G), maxA = 220;
    bool tief = false; // Hubschrauber dicht über der Decke: Abwind
    double links = 1e300, rechts = -1e300;
    for (const Rechteck &b : m_sim.bildschirme()) { links = std::min(links, b.x); rechts = std::max(rechts, b.rechts()); }

    switch (e.phase) {
    case Phase::Anflug:
        e.heli.zielX = landX; e.heli.zielY = schwebeY;
        if (std::hypot(e.heli.x - landX, e.heli.y - schwebeY) < 9 && std::hypot(e.heli.vx, e.heli.vy) < 30) { e.phase = Phase::Schweben; e.t = 0; }
        break;
    case Phase::Schweben:
        e.heli.zielX = landX; e.heli.zielY = schwebeY;
        tief = true;
        if (e.t > 1.0) { e.phase = Phase::Absenken; e.t = 0; }
        break;
    case Phase::Absenken:
        e.heli.zielX = landX; e.heli.zielY = schwebeY;
        tief = true;
        e.seil.laenge += 45 * G * dt;
        break;
    case Phase::Ausklinken:
        tief = true;
        e.heli.zielX = landX; e.heli.zielY = schwebeY;
        if (e.t > 0.5 && e.angehaengt) { e.angehaengt = false; }
        if (!e.angehaengt) e.seil.laenge = std::max(flug, e.seil.laenge - 60 * G * dt);
        if (e.t > 1.3) { e.phase = Phase::Raeumen; e.t = 0; }
        break;
    case Phase::Raeumen:
        e.seil.laenge = std::max(flug, e.seil.laenge - 60 * G * dt);
        e.heli.zielX = std::clamp(landX + e.warteSeite * 170 * G, links + 60, rechts - 60) + 25 * std::sin(e.t * 0.9);
        e.heli.zielY = schwebeY - 60 * G + 10 * std::sin(e.t * 0.5);
        if (f.phase == FahrPhase::Bereit) { e.phase = Phase::Rueckkehr; e.t = 0; }
        break;
    case Phase::Rueckkehr:
        e.heli.zielX = f.x; e.heli.zielY = f.y - fzH - flug - 25 * G;
        if (std::hypot(e.heli.x - e.heli.zielX, e.heli.y - e.heli.zielY) < 9 && std::hypot(e.heli.vx, e.heli.vy) < 30
            && (f.phase == FahrPhase::Bereit || !d)) { e.phase = Phase::Einhaken; e.t = 0; }
        break;
    case Phase::Einhaken: {
        tief = true;
        e.heli.zielX = f.x; e.heli.zielY = f.y - fzH - flug - 25 * G;
        e.seil.laenge += 60 * G * dt;
        const double oeseX = f.x, oeseY = f.y - fzH;
        if (std::hypot(e.seil.x - oeseX, e.seil.y - oeseY) < 5 * G || e.seil.y >= oeseY) {
            e.angehaengt = true;
            f.phase = FahrPhase::AmSeil;
            e.seil.x = oeseX; e.seil.y = oeseY; e.seil.vx = e.seil.vy = 0;
            e.seil.laenge = std::hypot(oeseX - e.heli.x, oeseY - e.heli.y);
            klang(Geraeusch::Plumps, f.x, f.y, 0.3);
            e.phase = Phase::Anheben; e.t = 0;
        }
        break;
    }
    case Phase::Anheben:
        tief = true;
        e.heli.zielY = f.y - fzH - flug - 25 * G - 30 * G;
        e.seil.laenge = std::max(flug, e.seil.laenge - 40 * G * dt);
        if (e.seil.laenge <= flug + 0.5) { e.phase = Phase::Abflug; e.t = 0; }
        break;
    case Phase::Abflug:
        if (e.eilig) maxV *= 1.6;
        e.seil.laenge = std::max(flug, e.seil.laenge - (e.eilig ? 170 : 40) * G * dt);
        e.heli.zielX = e.ausgang < 0 ? links - 400 * G : rechts + 400 * G;
        if (e.t < dt * 1.5) e.heli.zielY = e.heli.y - (e.eilig ? 110 : 50) * G;
        if (e.heli.x < links - 220 * G || e.heli.x > rechts + 220 * G) e.phase = Phase::Fertig;
        break;
    case Phase::Fertig: return;
    }

    e.heli.schritt(dt, maxV, maxA);
    e.seil.schritt(dt, e.heli.x, e.heli.y);
    if (tief) abwind(e, dt);

    if (f.phase == FahrPhase::AmSeil) {
        // hängt an der Öse, schwingt mit dem Seil (etwas träger)
        const double phi = e.seil.winkel() * 0.6;
        f.neigung = -phi;
        f.x = e.seil.x + std::sin(phi) * fzH;
        f.y = e.seil.y + std::cos(phi) * fzH;
        f.leuchte += dt;
        f.wackelV += (-180 * f.wackeln - 7 * f.wackelV) * dt;
        f.wackeln += f.wackelV * dt;
        if (e.phase == Phase::Absenken && d && f.y >= d->y) {
            // aufgesetzt: Schnee unter dem Fahrzeug wird zur Seite gedrückt
            f.phase = FahrPhase::Aufgesetzt;
            f.t = 0;
            f.neigung = 0;
            f.y = d->y;
            f.mitte = f.x - d->x;
            f.hubV = 70;
            f.nickV = 0.5;
            const double halb = m.halb * G, sb = d->spaltenBreite;
            double weg = m_sim.schneeEntnehmen(e.id, f.x - halb, f.x + halb, 1e9);
            for (int k = 0; k < 8 && weg > 0; ++k) {
                const double teil = weg / (8 - k);
                const double xk = f.x + (k % 2 ? 1 : -1) * (halb + sb * (1.5 + k / 2));
                const double platz = std::max(0.0, m_sim.maxHoeheVon(*d) - m_sim.hoeheBei(e.id, xk));
                const double an = m_sim.schneeHinzufuegen(e.id, xk, std::min(teil, platz));
                weg -= teil;
                f.wulst += teil - an; // kein Platz daneben: liegt dann vor dem Schild
            }
            staubWolke(f.x - halb, f.y - 3, 8, 110, 8);
            staubWolke(f.x + halb, f.y - 3, 8, 110, 8);
            klang(Geraeusch::Plumps, f.x, f.y, 1.2);
            e.phase = Phase::Ausklinken;
            e.t = 0;
        }
    } else {
        f.neigung = 0;
        if (d) fahrzeugSchritt(e, *d, dt);
        if (e.angehaengt && f.phase != FahrPhase::AmSeil) {
            // steht schon, hängt aber noch am Haken: Seil bleibt an der Öse
            e.seil.x = f.x; e.seil.y = f.y - fzH; e.seil.vx = e.seil.vy = 0;
            e.seil.laenge = std::max(e.seil.laenge, std::hypot(e.seil.x - e.heli.x, e.seil.y - e.heli.y));
        }
    }
    const double tempo = std::hypot(e.heli.vx, e.heli.vy);
    dauer(e, 0, Geraeusch::Rotor, e.heli.x, e.heli.y, std::clamp(0.35 + tempo / 500 + (e.angehaengt ? 0.2 : 0), 0.0, 1.0));
}

void Raeumdienst::schritt(double dt) {
    m_dauerNeu.clear();
    // von selbst buchen: einmal je Sekunde reicht
    m_pruefRest -= dt;
    if (m_pruefRest <= 0) {
        m_pruefRest = 1.0;
        if (m_e.an) {
            for (const Decke &d : m_sim.decken()) {
                if ((d.boden || d.dock) && !m_e.taskleiste) continue;
                auto sperre = m_sperre.find(d.id);
                if (sperre != m_sperre.end()) {
                    if (sperre->second > m_sim.zeit()) continue;
                    m_sperre.erase(sperre);
                }
                if (fuellstand(d) >= m_e.raeumenAb) buchen(d.id);
            }
        }
    }
    while (!m_schlange.empty() && int(m_einsaetze.size()) < std::max(1, m_e.gleichzeitig)) {
        const std::string id = m_schlange.front();
        m_schlange.pop_front();
        if (!starten(id)) m_sperre[id] = m_sim.zeit() + 30; // passt gerade nicht: später wieder prüfen
    }
    for (Einsatz &e : m_einsaetze) einsatzSchritt(e, dt);
    for (size_t i = 0; i < m_einsaetze.size();) {
        if (m_einsaetze[i].phase == Phase::Fertig) {
            const Einsatz &e = m_einsaetze[i];
            m_sperre[e.id] = m_sim.zeit() + (e.beleidigt ? 600 : 45);
            m_einsaetze.erase(m_einsaetze.begin() + long(i));
        } else ++i;
    }
    ballSchritt(dt);
    // Staub und Rauch
    const double wind = m_sim.einstellungen().wind;
    for (size_t i = 0; i < m_staub.size();) {
        Staub &s = m_staub[i];
        s.alter += float(dt);
        if (s.alter >= s.dauer) { s = m_staub.back(); m_staub.pop_back(); continue; }
        const float brems = float(std::exp(-2.5 * dt));
        s.vx = s.vx * brems + float(wind * 0.4 * dt);
        s.vy = s.vy * brems + float((s.art == 0 ? 70 : -22) * dt);
        s.x += s.vx * float(dt);
        s.y += s.vy * float(dt);
        ++i;
    }
    // Dauerklänge, die nicht mehr gebraucht werden, beenden
    for (int id : m_dauerAn)
        if (std::find(m_dauerNeu.begin(), m_dauerNeu.end(), id) == m_dauerNeu.end())
            m_klaenge.push_back({Klangereignis::Stopp, Geraeusch::Rotor, id, 0, 0, 0});
    m_dauerAn = m_dauerNeu;
}

// ---- Schneebälle ----
bool Raeumdienst::mausDruck(double x, double y, double t) {
    if (m_inHand) return true;
    for (const Decke &d : m_sim.decken()) {
        const double rel = (x - d.x) / d.spaltenBreite;
        if (rel < 0 || size_t(rel) >= d.hoehe.size() || !d.sichtbar[size_t(rel)]) continue;
        const double h = d.hoehe[size_t(rel)];
        if (h <= ANFASS_HOEHE || y < d.y - h - 6 || y > d.y + 3) continue;
        m_inHand = true;
        m_hand = Schneeball{};
        m_hand.x = x; m_hand.y = y;
        m_hand.menge = m_sim.schneeEntnehmen(d.id, x - 6, x + 6, 8);
        m_proben.clear();
        m_proben.push_back({x, y, t});
        return true;
    }
    for (Einsatz &e : m_einsaetze) {
        const Fahrzeugmass m = massVon(e.fz.art);
        if (std::abs(x - e.fz.x) <= m.trefferB * g() && y <= e.fz.y && y >= e.fz.y - 2 * m.trefferH * g()) {
            // angestupst: wackelt und grüßt
            e.fz.wackelV += 1.2;
            if (e.fz.spruch < 0 && e.fz.phase != FahrPhase::AmSeil) sagen(e.fz, 11, 2, 2.0);
            return true;
        }
    }
    return false;
}

void Raeumdienst::mausBewegt(double x, double y, double t) {
    if (!m_inHand) return;
    const double dx = x - m_hand.x, dy = y - m_hand.y, weg = std::hypot(dx, dy);
    const double r = m_hand.radius();
    m_hand.winkel += dx / std::max(4.0, r);
    m_hand.x = x; m_hand.y = y;
    m_proben.push_back({x, y, t});
    if (m_proben.size() > 12) m_proben.erase(m_proben.begin());
    if (weg <= 0 || m_hand.menge >= BALL_MAX) return;
    // rollt über Schnee: nimmt ihn auf und wächst
    for (const Decke &d : m_sim.decken()) {
        const double rel = (x - d.x) / d.spaltenBreite;
        if (rel < 0 || size_t(rel) >= d.hoehe.size() || !d.sichtbar[size_t(rel)]) continue;
        const double h = d.hoehe[size_t(rel)];
        if (h < 1 || y < d.y - h - 12 || y > d.y + 5) continue;
        m_hand.menge += m_sim.schneeEntnehmen(d.id, x - r - 2, x + r + 2, std::min(BALL_MAX - m_hand.menge, weg * 1.2));
        break;
    }
}

void Raeumdienst::mausLos(double x, double y, double t) {
    if (!m_inHand) return;
    m_inHand = false;
    Schneeball b = m_hand;
    b.x = x; b.y = y;
    // Wurftempo: Mausweg der letzten 120 ms
    for (const Probe &p : m_proben) {
        if (t - p.t <= 0.12 && t - p.t > 0.008) {
            b.vx = (x - p.x) / (t - p.t);
            b.vy = (y - p.y) / (t - p.t);
            break;
        }
    }
    werfen(b.x, b.y, b.vx, b.vy, b.menge);
    m_baelle.back().winkel = b.winkel;
}

void Raeumdienst::werfen(double x, double y, double vx, double vy, double menge) {
    Schneeball b;
    b.x = x; b.y = y; b.menge = std::clamp(menge, 0.0, BALL_MAX);
    const double tempo = std::hypot(vx, vy);
    if (tempo > 2600) { vx *= 2600 / tempo; vy *= 2600 / tempo; }
    b.vx = vx; b.vy = vy;
    b.drall = vx / std::max(4.0, b.radius()) * 0.5 + (zufall() - 0.5) * 4;
    m_baelle.push_back(b);
    if (tempo > 120) klang(Geraeusch::Wurf, x, y, std::clamp(tempo / 1200, 0.3, 2.0));
}

void Raeumdienst::platzen(const Schneeball &b, const std::string &deckenId, double anteilLiegen) {
    const double r = b.radius();
    double liegen = b.menge * anteilLiegen, rest = b.menge - liegen;
    for (int k = -3; k <= 3 && !deckenId.empty(); ++k) {
        const double an = m_sim.schneeHinzufuegen(deckenId, b.x + k * r * 0.5, liegen / 7);
        rest += liegen / 7 - an;
    }
    if (deckenId.empty()) rest = b.menge;
    pulver(b.x, b.y - 2, rest, 70, b.vx * 0.05, 0, deckenId);
    staubWolke(b.x, b.y, 8 + int(b.menge / 15), 150, r);
}

void Raeumdienst::treffer(Einsatz &e, const Schneeball &b) {
    Fahrzeug &f = e.fz;
    const double G = g();
    const Fahrzeugmass m = massVon(f.art);
    klang(Geraeusch::Treffer, b.x, b.y, std::clamp(b.menge / 100, 0.4, 2.0));
    // ein Teil bleibt kleben, der Rest zerplatzt zu Staub und Pulver
    const double klebt = b.menge * 0.3;
    Klecks k{float(std::clamp((b.x - f.x) / G, -m.trefferB, m.trefferB)), float(std::clamp((b.y - f.y) / G, -2 * m.trefferH, -3.0)),
             float(b.radius() * 0.9 / G), float(klebt), m_sim.zeit()};
    if (f.kleckse.size() >= 8) {
        m_sim.brockenFallenLassen(f.x, f.y - 1, 6, f.kleckse.front().menge);
        f.kleckse.erase(f.kleckse.begin());
    }
    f.kleckse.push_back(k);
    Schneeball rest = b;
    rest.menge = b.menge - klebt;
    platzen(rest, {}, 0);
    f.wackelV += vorzeichen(b.vx) * (1.5 + b.menge / 100);
    f.schneeDach *= 0.6;
    if (f.phase == FahrPhase::AmSeil) { e.seil.vx += std::clamp(b.vx * 0.08, -120.0, 120.0); return; }
    if (e.abbruch || f.phase == FahrPhase::Beleidigt || e.phase < Phase::Ausklinken || e.phase > Phase::Raeumen) return;
    bool beleidigt = false;
    if (f.phase == FahrPhase::Streik) {
        // weitere Treffer während er sich beschwert
        beleidigt = ++f.trefferImStreik >= 2;
        if (!beleidigt) { sagen(f, 0, 8, 3.5); f.t = std::min(f.t, 2.5); klang(Geraeusch::Murren, f.x, f.y, 1.3); }
    } else if (++f.treffer >= std::max(1, m_e.trefferBisStreik)) {
        f.treffer = 0;
        beleidigt = ++f.streiks >= 2;
        if (!beleidigt) {
            f.vorStreik = f.phase;
            f.phase = FahrPhase::Streik;
            f.t = 0;
            f.trefferImStreik = 0;
            sagen(f, 0, 8, 5.0);
            klang(Geraeusch::Murren, f.x, f.y, 1.0);
        }
    }
    if (beleidigt) {
        f.phase = FahrPhase::Beleidigt;
        f.t = 0;
        e.beleidigt = true;
        sagen(f, 8, 3, 3.0);
        klang(Geraeusch::Murren, f.x, f.y, 1.8);
    }
}

void Raeumdienst::ballSchritt(double dt) {
    if (m_baelle.empty()) return;
    double unten = -1e300, links = 1e300, rechts = -1e300;
    for (const Rechteck &b : m_sim.bildschirme()) { unten = std::max(unten, b.unten()); links = std::min(links, b.x); rechts = std::max(rechts, b.rechts()); }
    const double G = g();
    std::vector<Schneeball> bleiben;
    for (Schneeball b : m_baelle) {
        const double r = b.radius();
        const int n = std::max(1, int(std::ceil(std::hypot(b.vx, b.vy) * dt / 5)));
        const double h = dt / n;
        bool weg = false;
        for (int i = 0; i < n && !weg; ++i) {
            const double y0 = b.y;
            b.vy += 1300 * h;
            const double brems = std::exp(-0.25 * h);
            b.vx *= brems; b.vy *= brems;
            b.x += b.vx * h; b.y += b.vy * h;
            b.winkel += b.drall * h;
            // Fahrzeug oder Hubschrauber getroffen?
            for (Einsatz &e : m_einsaetze) {
                const Fahrzeugmass m = massVon(e.fz.art);
                const double mx = e.fz.x, my = e.fz.y - m.trefferH * G;
                if (std::abs(b.x - mx) <= m.trefferB * G + r * 0.5 && std::abs(b.y - my) <= m.trefferH * G + r * 0.5) {
                    treffer(e, b);
                    weg = true;
                    break;
                }
                if (std::abs(b.x - e.heli.x) <= 50 * G + r * 0.5 && std::abs(b.y - (e.heli.y - 22 * G)) <= 20 * G + r * 0.5) {
                    platzen(b, {}, 0);
                    klang(Geraeusch::Treffer, b.x, b.y, 0.6);
                    e.heli.stoss += vorzeichen(b.vx) * 0.25;
                    e.heli.vx += std::clamp(b.vx * 0.05, -60.0, 60.0);
                    weg = true;
                    break;
                }
            }
            if (weg) break;
            // von oben auf eine Decke gefallen: zerplatzt dort zu Schnee
            if (b.vy > 0) {
                for (const Decke &d : m_sim.decken()) {
                    const double rel = (b.x - d.x) / d.spaltenBreite;
                    if (rel < 0 || size_t(rel) >= d.hoehe.size() || !d.sichtbar[size_t(rel)]) continue;
                    const double oben = d.y - d.hoehe[size_t(rel)];
                    if (y0 + r <= d.y + 4 && b.y + r >= oben) {
                        platzen(b, d.id, 0.75);
                        klang(Geraeusch::Plumps, b.x, b.y, std::clamp(b.menge / 150, 0.2, 1.0));
                        weg = true;
                        break;
                    }
                }
            }
            if (b.y > unten + 60 || b.x < links - 300 || b.x > rechts + 300) weg = true;
        }
        if (!weg) bleiben.push_back(b);
    }
    m_baelle.swap(bleiben);
}

std::vector<Rechteck> Raeumdienst::eingabeRechtecke() const {
    std::vector<Rechteck> r;
    if (!m_e.anfassbar) return r;
    for (const Decke &d : m_sim.decken()) {
        const size_t n = d.hoehe.size();
        for (size_t i = 0; i < n;) {
            if (!d.sichtbar[i] || d.hoehe[i] <= ANFASS_HOEHE) { ++i; continue; }
            size_t j = i;
            float hoch = 0;
            while (j < n && d.sichtbar[j] && d.hoehe[j] > ANFASS_HOEHE) hoch = std::max(hoch, d.hoehe[j++]);
            r.push_back({d.x + i * d.spaltenBreite, d.y - hoch - 2, (j - i) * d.spaltenBreite, hoch + 2.0});
            i = j;
        }
    }
    for (const Einsatz &e : m_einsaetze) {
        const Fahrzeugmass m = massVon(e.fz.art);
        r.push_back({e.fz.x - m.trefferB * g(), e.fz.y - 2 * m.trefferH * g(), 2 * m.trefferB * g(), 2 * m.trefferH * g()});
    }
    return r;
}

} // namespace schnee
