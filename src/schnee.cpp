#include "schnee.h"
#include <algorithm>
#include <cmath>

namespace schnee {

Simulation::Simulation(Einstellungen e, uint32_t saat) : m_e(e), m_rng(saat) {}

void Simulation::setEinstellungen(const Einstellungen &e) { m_e = e; }

namespace {
uint32_t schluesselVon(const std::string &id) {
    uint32_t h = 2166136261u; // FNV-1a
    for (unsigned char c : id) { h ^= c; h *= 16777619u; }
    return h ? h : 1;
}
using Sichtbarkeit = std::vector<std::pair<std::string, std::vector<uint8_t>>>;
Sichtbarkeit merken(const std::vector<Decke> &decken) {
    Sichtbarkeit v;
    for (const Decke &d : decken) v.emplace_back(d.id, d.sichtbar);
    return v;
}
}

void Simulation::nachSichtbarkeitZerfallen() {
    auto vorher = merken(m_decken);
    sichtbarkeitBerechnen();
    for (Decke &d : m_decken)
        for (auto &[id, sicht] : vorher)
            if (id == d.id)
                for (size_t i = 0; i < d.hoehe.size() && i < sicht.size(); ++i)
                    if (sicht[i] && !d.sichtbar[i]) zerfallen(d, i, i + 1);
}

void Simulation::setBildschirme(const std::vector<Rechteck> &b) {
    ++m_deckenStand;
    m_bildschirme = b;
    // Bodendecken neu aufbauen (Monitore geändert)
    for (Decke &d : m_decken)
        if (d.boden) zerfallen(d, 0, d.hoehe.size());
    m_decken.erase(std::remove_if(m_decken.begin(), m_decken.end(), [](const Decke &d) { return d.boden; }), m_decken.end());
    bodenAufbauen();
    nachSichtbarkeitZerfallen();
}

void Simulation::bodenAufbauen() {
    m_raender.clear();
    const double sb = m_e.spaltenBreite;
    for (size_t i = 0; i < m_bildschirme.size(); ++i) {
        const Rechteck &b = m_bildschirme[i];
        Decke d;
        d.id = "boden:" + std::to_string(i);
        d.schluessel = schluesselVon(d.id);
        d.boden = true;
        d.x = b.x;
        d.y = b.unten();
        d.spaltenBreite = sb;
        size_t n = std::max<size_t>(1, size_t(std::floor(b.w / sb)));
        d.hoehe.assign(n, 0.f);
        d.sichtbar.assign(n, 1);
        m_decken.push_back(std::move(d));
        // freie Ränder (kein Nachbarmonitor dahinter) in 8-px-Stücken sammeln
        const double stueck = 8;
        for (double x = b.x; x < b.rechts(); x += stueck) {
            double x1 = std::min(x + stueck, b.rechts());
            if (!aufBildschirm((x + x1) / 2, b.y - 1)) {
                if (!m_raender.empty() && m_raender.back().seite == 0 && m_raender.back().x1 == x && m_raender.back().y0 == b.y)
                    m_raender.back().x1 = x1;
                else
                    m_raender.push_back({x, b.y, x1, b.y, 0});
            }
        }
        for (int seite : {-1, 1}) {
            double sx = seite < 0 ? b.x : b.rechts();
            for (double y = b.y; y < b.unten(); y += stueck) {
                double y1 = std::min(y + stueck, b.unten());
                double px = seite < 0 ? sx - 1 : sx + 1;
                if (!aufBildschirm(px, (y + y1) / 2)) {
                    if (!m_raender.empty() && m_raender.back().seite == seite && m_raender.back().y1 == y && m_raender.back().x0 == sx)
                        m_raender.back().y1 = y1;
                    else
                        m_raender.push_back({sx, y, sx, y1, seite});
                }
            }
        }
    }
}

void Simulation::erzeugen(double dt) {
    std::uniform_real_distribution<double> u(0, 1);
    const double dichte = m_e.menge / 10000.0; // Flocken je px²
    double rate = 0;                            // Flocken je Sekunde
    std::vector<double> anteil;
    for (const Strecke &r : m_raender) {
        double laenge = r.seite == 0 ? r.x1 - r.x0 : r.y1 - r.y0;
        double fluss = 0;
        if (r.seite == 0) fluss = m_e.fallGeschw;
        else if (r.seite * m_e.wind < 0) fluss = std::abs(m_e.wind); // Wind weht von dieser Seite herein
        anteil.push_back(dichte * fluss * laenge);
        rate += anteil.back();
    }
    if (rate <= 0) return;
    m_erzeugRest += rate * dt;
    int anzahl = int(m_erzeugRest);
    m_erzeugRest -= anzahl;
    anzahl = std::min(anzahl, 200);
    for (int k = 0; k < anzahl; ++k) {
        double wahl = u(m_rng) * rate;
        size_t i = 0;
        while (i + 1 < anteil.size() && wahl > anteil[i]) { wahl -= anteil[i]; ++i; }
        const Strecke &r = m_raender[i];
        Flocke f = neueFlocke(0, 0);
        if (r.seite == 0) {
            f.x = r.x0 + u(m_rng) * (r.x1 - r.x0);
            f.y = r.y0 - f.groesse;
        } else {
            f.x = r.x0 - r.seite * f.groesse;
            f.y = r.y0 + u(m_rng) * (r.y1 - r.y0);
        }
        if (m_e.modus == Fenstermodus::Fest && fensterBei(f.x, f.y)) continue; // Fenster ragt über den Rand
        m_flocken.push_back(f);
    }
}

int Simulation::zielAnzahl() const {
    double flaeche = 0;
    for (const Rechteck &b : m_bildschirme) flaeche += b.w * b.h;
    return int(flaeche * m_e.menge / 10000.0);
}

void Simulation::setFenster(const std::vector<Fenster> &f, double zeitpunkt) {
    ++m_deckenStand;
    m_fenster = f;
    m_fensterSchluessel.clear();
    for (const Fenster &w : m_fenster) m_fensterSchluessel.push_back(schluesselVon(w.id));
    if (std::isnan(zeitpunkt)) zeitpunkt = m_zeit;
    auto finde = [&](const std::string &id) -> const Fenster * {
        for (const Fenster &w : m_fenster)
            if (w.id == id) return &w;
        return nullptr;
    };
    auto traegt = [](const Fenster *w) { return w && !w->minimiert && !w->vollbild && w->r.w >= 1; };
    // bestehende Decken mitnehmen (Schnee fährt mit dem Fenster) oder bei verschwundenem Fenster abgehen lassen
    for (Decke &d : m_decken) {
        if (d.boden) continue;
        const Fenster *w = finde(d.id);
        if (!traegt(w)) {
            verschwinden(d);
            d.hoehe.clear();
            continue;
        }
        const size_t alt = d.hoehe.size();
        const size_t n = std::max<size_t>(1, size_t(std::floor(w->r.w / d.spaltenBreite)));
        const bool rechtsFest = std::abs(w->r.x - d.x) > 0.5 && std::abs(w->r.rechts() - (d.x + d.fensterBreite)) < 0.5;
        if (n != alt && rechtsFest) {
            // Größe an der linken Kante geändert: rechte Kante bleibt, links Spalten weg oder dazu
            if (n < alt) {
                zerfallen(d, 0, alt - n);
                d.hoehe.erase(d.hoehe.begin(), d.hoehe.begin() + long(alt - n));
                d.sichtbar.erase(d.sichtbar.begin(), d.sichtbar.begin() + long(alt - n));
            } else {
                d.hoehe.insert(d.hoehe.begin(), n - alt, 0.f);
                d.sichtbar.insert(d.sichtbar.begin(), n - alt, 1);
            }
        } else {
            if (n < alt) zerfallen(d, n, alt);
            d.hoehe.resize(n, 0.f);
            d.sichtbar.resize(n, 1);
        }
        // Bewegung messen (fürs Schütteln): geglättete Geschwindigkeit und Beschleunigung
        const double dx = w->r.x - d.x, dy = w->r.y - d.y;
        d.x = w->r.x;
        d.y = w->r.y;
        d.fensterBreite = w->r.w;
        bewegungMessen(d, dx, dy, zeitpunkt);
    }
    m_decken.erase(std::remove_if(m_decken.begin(), m_decken.end(), [](const Decke &d) { return !d.boden && d.hoehe.empty(); }), m_decken.end());
    // neue Decken
    for (const Fenster &w : m_fenster) {
        if (!traegt(&w) || decke(w.id)) continue;
        Decke d;
        d.id = w.id;
        d.schluessel = schluesselVon(d.id);
        d.x = w.r.x;
        d.y = w.r.y;
        d.fensterBreite = w.r.w;
        d.dock = w.dock;
        d.geboren = m_zeit;
        d.tLetzt = zeitpunkt;
        d.spaltenBreite = m_e.spaltenBreite;
        size_t n = std::max<size_t>(1, size_t(std::floor(w.r.w / m_e.spaltenBreite)));
        d.hoehe.assign(n, 0.f);
        d.sichtbar.assign(n, 1);
        m_decken.push_back(std::move(d));
    }
    nachSichtbarkeitZerfallen();
}

// Geschwindigkeit und Beschleunigung einer Decke aus den Fensterpositionen (geglättet, Zeitkonstante 30 ms).
// Ein Sprung ohne Zwischenschritte (Kacheln, Tastenkürzel) zählt nicht als Ruck.
void Simulation::bewegungMessen(Decke &d, double dx, double dy, double t) {
    const double dt = t - d.tLetzt;
    if (std::abs(dx) + std::abs(dy) > 300) {
        d.vx = d.vy = d.ax = d.ay = 0;
        d.tLetzt = t;
        return;
    }
    if (dt <= 0.002) {
        if (dt < 0) d.tLetzt = t;
        d.offenX += dx; d.offenY += dy; // zu dicht hintereinander: mit dem nächsten Messpunkt verrechnen
        return;
    }
    dx += d.offenX; dy += d.offenY;
    d.offenX = d.offenY = 0;
    const double k = 1 - std::exp(-dt / 0.03);
    const double vx = d.vx + (dx / dt - d.vx) * k, vy = d.vy + (dy / dt - d.vy) * k;
    d.ax += ((vx - d.vx) / dt - d.ax) * k;
    d.ay += ((vy - d.vy) / dt - d.ay) * k;
    d.vx = vx; d.vy = vy;
    d.tLetzt = t;
}

void Simulation::zerfallen(Decke &d, size_t von, size_t bis, double anteil, bool seitwaerts) {
    ++m_deckenStand;
    std::uniform_real_distribution<double> u(0, 1);
    const double portion = ablageFuer(m_e.groesse);
    const size_t grenze = 6000; // Schutz gegen Flockenlawinen
    for (size_t i = von; i < bis && i < d.hoehe.size(); ++i) {
        double h = d.hoehe[i] * anteil;
        d.hoehe[i] -= float(h);
        if (anteil >= 1) d.hoehe[i] = 0;
        if (h <= 0) continue;
        double anzahlD = h / portion;
        int anzahl = int(anzahlD);
        if (u(m_rng) < anzahlD - anzahl) ++anzahl;
        for (int k = 0; k < anzahl && m_flocken.size() < grenze; ++k) {
            double x = d.x + (i + u(m_rng)) * d.spaltenBreite;
            double y = d.y - u(m_rng) * (d.hoehe[i] + h) - 0.5;
            Flocke f = neueFlocke(x, y);
            f.vy = 28 + 30 * u(m_rng);
            f.fang = 1.0f;
            f.vx = (u(m_rng) - 0.5) * 4 + m_e.wind * 0.3; // fast senkrecht
            if (seitwaerts && !d.boden && !d.dock) {
                // fest: der Schnee rutscht oben über die nähere Fensterkante und fällt dann
                const double links = x - d.x, rechts = d.x + d.breite() - x;
                const double weg = std::min(links, rechts);
                f.vx = (links < rechts ? -1 : 1) * (weg / 0.45 + 20 + 15 * u(m_rng)) + d.vx * 0.5;
            }
            f.herkunft = d.schluessel;
            f.art = Flockenart::Pulver;
            m_flocken.push_back(f);
        }
    }
}

// Fenster minimiert, geschlossen oder Vollbild: alter oder viel Schnee rutscht als Schneebrett ab,
// junger und wenig rieselt als Wolke herunter.
void Simulation::verschwinden(Decke &d) {
    double menge = 0;
    size_t spalten = 0;
    for (size_t i = 0; i < d.hoehe.size(); ++i)
        if (d.sichtbar[i]) { menge += d.hoehe[i]; ++spalten; }
    if (menge <= 0) return;
    const double alter = m_zeit - d.geboren;
    const bool viel = spalten > 0 && menge / double(spalten) >= 0.6 * maxFuer(d);
    if (alter >= m_e.rutschAbMinuten * 60 || viel) rutsch(d);
    else wolke(d);
}

void Simulation::rutsch(Decke &d) {
    ++m_deckenStand;
    const size_t n = d.hoehe.size();
    for (size_t i = 0; i < n;) {
        if (!d.sichtbar[i] || d.hoehe[i] <= 0) { ++i; continue; }
        size_t j = i;
        while (j < n && d.sichtbar[j]) ++j;
        while (j > i && d.hoehe[j - 1] <= 0) --j;
        Brocken b;
        b.x = d.x + i * d.spaltenBreite;
        b.y = d.y;
        b.spaltenBreite = d.spaltenBreite;
        b.hoehe.assign(d.hoehe.begin() + long(i), d.hoehe.begin() + long(j));
        b.vy = 30 * m_e.rutschTempo;
        b.herkunft = d.schluessel;
        m_brocken.push_back(std::move(b));
        std::fill(d.hoehe.begin() + long(i), d.hoehe.begin() + long(j), 0.f);
        i = j;
    }
}

void Simulation::wolke(Decke &d) {
    ++m_deckenStand;
    std::uniform_real_distribution<double> u(0, 1);
    const double portion = ablageFuer(m_e.groesse);
    const size_t grenze = 6000;
    double rest = 0;
    for (size_t i = 0; i < d.hoehe.size(); ++i) {
        const double h = d.sichtbar[i] ? d.hoehe[i] : 0;
        d.hoehe[i] = 0;
        if (h <= 0) continue;
        // was liegen bleibt: echte Flocken (Rest über die Spalten mitnehmen, damit die Menge stimmt)
        rest += h * m_e.wolkeLiegen;
        const int liegen = int(rest / portion);
        rest -= liegen * portion;
        // feiner Staub, der sich auflöst (rein fürs Aussehen)
        double staubD = h * (1 - m_e.wolkeLiegen) / portion * 1.5;
        int staub = int(staubD);
        if (u(m_rng) < staubD - staub) ++staub;
        for (int k = 0; k < liegen + staub && m_flocken.size() < grenze; ++k) {
            Flocke f = neueFlocke(d.x + (i + u(m_rng)) * d.spaltenBreite, d.y - u(m_rng) * h - 0.5);
            f.art = Flockenart::Pulver;
            f.fang = 1.0f;
            f.herkunft = d.schluessel;
            f.vx = (u(m_rng) - 0.5) * 24 + m_e.wind * 0.4; // treibt
            if (k < liegen) {
                f.vy = 12 + 13 * u(m_rng);
            } else {
                f.vy = 4 + 10 * u(m_rng);
                f.groesse *= 0.7;
                f.leben = float(m_e.wolkeDauer * (0.35 + 0.65 * u(m_rng)));
            }
            m_flocken.push_back(f);
        }
    }
}

void Simulation::schuetteln(Decke &d, double dt) {
    const double heftig = std::hypot(d.ax, d.ay);
    const double s = std::max(1.0, m_e.schuettelnSchwelle);
    double menge = 0;
    for (size_t i = 0; i < d.hoehe.size(); ++i) if (d.sichtbar[i]) menge += d.hoehe[i];
    if (heftig <= s) {
        // Ruhe: nach einer halben Sekunde ist der nächste Ruck ein neuer Schüttelvorgang
        if (d.schuettelnSeit >= 0 && m_zeit - d.schuettelnSeit > 0.5) d.schuettelnSeit = -1;
        return;
    }
    if (d.schuettelnSeit < 0) { d.schuettelnMenge = menge; d.schuettelnMax = 0; }
    d.schuettelnSeit = m_zeit;
    d.schuettelnMax = std::max(d.schuettelnMax, heftig);
    // es bleibt so viel haften, wie zur Heftigkeit passt: bei doppelter Schwelle die Hälfte
    const double halten = d.schuettelnMenge * s / d.schuettelnMax;
    if (menge <= halten || menge <= 0) return;
    const double anteil = std::min(1.0, (menge - halten) / menge * std::min(1.0, dt / 0.15));
    zerfallen(d, 0, d.hoehe.size(), anteil, m_e.modus == Fenstermodus::Fest);
}

void Simulation::sichtbarkeitBerechnen() {
    for (Decke &d : m_decken) {
        const Fenster *eigen = nullptr;
        for (const Fenster &w : m_fenster)
            if (w.id == d.id) eigen = &w;
        int stapel = eigen ? eigen->stapel : -1000000; // Boden liegt unter allem
        for (size_t s = 0; s < d.hoehe.size(); ++s) {
            double cx = d.x + (s + 0.5) * d.spaltenBreite;
            bool sicht;
            if (d.boden) // Boden: Monitor endet hier und geht darunter nicht weiter
                sicht = m_bildschirme.empty() || (aufBildschirm(cx, d.y - 1) && !aufBildschirm(cx, d.y + 1));
            else         // Fensterkante selbst muss auf einem Monitor liegen (auch ganz oben am Rand)
                sicht = m_bildschirme.empty() || aufBildschirm(cx, d.y + 0.5);
            for (const Fenster &w : m_fenster) {
                if (!sicht) break;
                if (&w == eigen || w.minimiert || w.stapel <= stapel) continue;
                // die Schnee-Ebene liegt über allen Fenstern: den Boden verdecken nur Docks (die tragen selbst)
                if (d.boden && !w.dock) continue;
                // höheres Fenster überdeckt die Kante oder den Raum direkt darüber
                if (cx >= w.r.x && cx < w.r.rechts() && d.y + 1 > w.r.y && d.y - 2 < w.r.unten())
                    sicht = false;
            }
            d.sichtbar[s] = sicht ? 1 : 0;
        }
    }
}

double Simulation::ablageFuer(double groesse) const {
    if (m_e.ablageJeFlocke > 0) return m_e.ablageJeFlocke;
    return groesse * 0.3;
}

void Simulation::ablegen(Decke &d, size_t spalte, double menge) {
    d.hoehe[spalte] += float(menge);
}

namespace {
// fest je (Flocke, Kante): dieselbe Flocke entscheidet an derselben Kante immer gleich
bool faengt(const Flocke &f, const Decke &d) {
    if (f.fang >= 1.0f) return true;
    if (f.fang <= 0.0f) return false;
    uint32_t h = f.los ^ (uint32_t(int64_t(d.x)) * 2654435761u) ^ (uint32_t(int64_t(d.y)) * 40503u);
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
    return (h & 0xffff) < uint32_t(f.fang * 65536.0f);
}
}

Flocke Simulation::neueFlocke(double x, double y) {
    std::uniform_real_distribution<double> u(0, 1);
    double g = m_e.groesse * (0.5 + u(m_rng));
    // große Flocken wirken näher und fallen deutlich schneller (Tiefenwirkung); im Mittel fallGeschw
    double vy = m_e.fallGeschw * (0.3 + 0.7 * g / m_e.groesse) * (0.8 + 0.4 * u(m_rng));
    Flocke f{x, y, m_e.wind * (0.7 + 0.6 * u(m_rng)), vy, g, u(m_rng) * 6.2831853};
    f.fang = float(m_e.fangAnteil);
    f.los = uint32_t(m_rng());
    return f;
}

void Simulation::herunterfallen(Decke &d, size_t spalte, double menge, int richtung) {
    float &acc = richtung < 0 ? d.abfallLinks : d.abfallRechts;
    acc += float(menge);
    double portion = ablageFuer(m_e.groesse);
    while (acc >= portion) {
        acc -= float(portion);
        double kante = richtung < 0 ? d.x + spalte * d.spaltenBreite : d.x + (spalte + 1) * d.spaltenBreite;
        Flocke f = neueFlocke(kante + richtung * (m_e.groesse * 0.6 + 1.0), d.y - d.hoehe[spalte] * 0.5);
        // Klümpchen fallen fast senkrecht und zügig, nur der Wind nimmt sie etwas mit
        f.vx = richtung * 2.0 + m_e.wind * 0.5;
        f.vy = std::max(30.0, f.vy * 1.3);
        f.fang = 1.0f;
        f.herkunft = d.schluessel; // nie zurück auf dieselbe Decke
        f.art = Flockenart::Pulver;
        m_flocken.push_back(f);
    }
}

void Simulation::rutschen(Decke &d, double) {
    const size_t n = d.hoehe.size();
    if (n == 0) return;
    const float maxDiff = float(d.spaltenBreite * 1.2);
    // Am Fensterende hält sich der Schnee fast in voller Höhe (runde Kappe). Früher lag die Grenze
    // bei 1,5 Spaltenbreiten – zusammen mit maxDiff ergab das eine gerade 45°-Rampe zur Kante.
    const double mh = maxFuer(d); // Fenster: max_deckenhoehe, Boden und Taskleiste: boden_max
    const float randGrenze = float(std::max(std::min(mh, d.spaltenBreite * 1.5), mh * 0.85));
    const float maxH = float(mh);
    auto sicht = [&](long i) { return i >= 0 && size_t(i) < n && d.sichtbar[size_t(i)]; };
    for (size_t i = 0; i < n; ++i) {
        if (!d.sichtbar[i]) continue;
        // Höhenbegrenzung: Überschuss zum niedrigeren Nachbarn oder herunter
        for (int richtung : {-1, 1}) {
            long j = long(i) + richtung;
            float &h = d.hoehe[i];
            if (!sicht(j)) {
                // Kante (Fensterende oder verdeckter Teil): Überhang fällt herunter
                if (h > randGrenze && !d.boden) {
                    float ueber = h - randGrenze;
                    h -= ueber;
                    herunterfallen(d, i, ueber, richtung);
                }
                continue;
            }
            float &hn = d.hoehe[size_t(j)];
            if (h - hn > maxDiff) {
                float m = (h - hn - maxDiff) * 0.5f;
                h -= m; hn += m;
            }
        }
        float &h = d.hoehe[i];
        if (h > maxH) {
            float ueber = h - maxH;
            long l = long(i) - 1, r = long(i) + 1;
            long ziel = !sicht(l) ? (sicht(r) ? r : -2) : (!sicht(r) ? l : (d.hoehe[size_t(l)] < d.hoehe[size_t(r)] ? l : r));
            h = maxH;
            if (ziel == -2 || (ziel >= 0 && d.hoehe[size_t(ziel)] >= maxH)) {
                // kein Platz: an der näheren Kante herunterfallen
                if (!d.boden) herunterfallen(d, i < n / 2 ? 0 : n - 1, ueber, i < n / 2 ? -1 : 1);
            } else {
                d.hoehe[size_t(ziel)] += ueber;
            }
        }
    }
}

bool Simulation::aufBildschirm(double x, double y) const {
    for (const Rechteck &b : m_bildschirme)
        if (b.enthaelt(x, y)) return true;
    return false;
}

// oberstes nicht minimiertes Fenster, das den Punkt enthält (Docks zählen mit)
const Fenster *Simulation::fensterBei(double x, double y) const {
    const Fenster *oben = nullptr;
    for (const Fenster &w : m_fenster)
        if (!w.minimiert && w.r.w >= 1 && w.r.enthaelt(x, y) && (!oben || w.stapel > oben->stapel)) oben = &w;
    return oben;
}

// fest: Fenster sind Hindernisse. Wer seitlich anprallt, rutscht an der Seite ab; Schnee, über den ein
// Fenster geschoben wurde, liegt dahinter; Schnee, der von der eigenen Decke abrutscht, gleitet oben weiter.
void Simulation::festKollision(Flocke &f, double x0, double y0, bool &weg) {
    const Fenster *t = fensterBei(f.x, f.y);
    if (!t) { f.hinten = false; return; }
    if (f.hinten) return;
    if (f.herkunft && f.y - t->r.y < 40 && m_fensterSchluessel[size_t(t - m_fenster.data())] == f.herkunft) {
        f.y = t->r.y - 0.5; // gleitet auf der Oberkante bis zum Fensterende
        return;
    }
    if (t->r.enthaelt(x0, y0)) { f.hinten = true; return; } // Fenster wurde über die Flocke geschoben
    if (y0 < t->r.y) { weg = true; return; }                // von oben auf eine verdeckte Kante: geschluckt
    // seitlich: an die Seitenkante zurück, dort fällt sie weiter
    // (um etwa den Flockenradius abgesetzt, damit das Bild der Flocke nicht ins Fenster ragt)
    const double abstand = 0.01 + f.groesse * 0.8;
    f.x = (std::abs(x0 - t->r.x) < std::abs(x0 - t->r.rechts())) ? t->r.x - abstand : t->r.rechts() + abstand;
    if (fensterBei(f.x, f.y)) f.hinten = true;
}

void Simulation::schritt(double dt, bool erzeugenAn) {
    m_zeit += dt;
    if (erzeugenAn) erzeugen(dt);
    const bool fest = m_e.modus == Fenstermodus::Fest;
    // gezogene Fenster: Stillstand nach der letzten Bewegung erkennen, dann Schütteln prüfen
    for (Decke &d : m_decken) {
        if (d.boden) continue;
        if (m_zeit - d.tLetzt >= 0.05 && (d.vx != 0 || d.vy != 0 || d.ax != 0 || d.ay != 0)) {
            bewegungMessen(d, 0, 0, m_zeit);
            if (std::abs(d.vx) + std::abs(d.vy) < 0.5 && std::abs(d.ax) + std::abs(d.ay) < 50) d.vx = d.vy = d.ax = d.ay = 0;
        }
        schuetteln(d, dt);
    }
    std::vector<Flocke> bleiben;
    bleiben.reserve(m_flocken.size());
    for (Flocke f : m_flocken) {
        const double x0 = f.x, y0 = f.y;
        f.phase += dt * 1.7;
        // Kristalle schaukeln im Fall, Pulverklümpchen kaum
        const double schaukeln = f.art == Flockenart::Pulver ? 0.75 : 2.5;
        f.x += (f.vx + std::cos(f.phase) * f.groesse * schaukeln) * dt;
        f.y += f.vy * dt;
        if (f.leben > 0) {
            // Wolkenstaub: löst sich auf, landet nie
            f.leben -= float(dt);
            if (f.leben <= 0) continue;
            if (aufBildschirm(f.x, f.y)) bleiben.push_back(f);
            continue;
        }
        // erste getroffene Decke suchen
        Decke *treffer = nullptr;
        size_t trefferSpalte = 0;
        double trefferOben = 1e300;
        for (Decke &d : m_decken) {
            double rel = (f.x - d.x) / d.spaltenBreite;
            if (rel < 0) continue;
            size_t s = size_t(rel);
            if (s >= d.hoehe.size() || !d.sichtbar[s]) continue;
            if (f.herkunft == d.schluessel) continue;          // gerade von hier heruntergefallen
            if (!fest && !d.boden && !d.dock && !faengt(f, d)) continue; // fällt vor diesem Fenster vorbei
            double oben = d.y - d.hoehe[s];
            // y0 <= d.y: auch Flocken, in die die Decke hineingewachsen ist, bleiben liegen
            if (y0 <= d.y && f.y >= oben && oben < trefferOben) {
                treffer = &d; trefferSpalte = s; trefferOben = oben;
            }
        }
        if (treffer) {
            ablegen(*treffer, trefferSpalte, ablageFuer(f.groesse));
            continue;
        }
        if (fest) {
            bool weg = false;
            festKollision(f, x0, y0, weg);
            if (weg) continue;
        }
        // nur auf Monitorflächen (oder knapp über/neben dem Rand beim Hereinkommen)
        double rand = f.groesse + 2;
        if (!aufBildschirm(f.x, f.y) && !aufBildschirm(f.x, f.y + rand) && !aufBildschirm(f.x + rand, f.y) && !aufBildschirm(f.x - rand, f.y))
            continue;
        bleiben.push_back(f);
    }
    m_flocken.swap(bleiben);
    brockenSchritt(dt);
    for (Decke &d : m_decken) {
        rutschen(d, dt);
        if (d.boden && m_e.schmelzen > 0) {
            for (float &h : d.hoehe)
                if (h > 0) h = std::max(0.f, h - float(m_e.schmelzen * dt * (0.2 + h / m_e.bodenMax)));
        }
    }
}

// Schneebretter fallen beschleunigt. Beim ersten Aufprall bleibt liegen, was auf der getroffenen
// Fläche aufsetzt (ein Teil stiebt als Pulver auf); der Rest bricht ab und fällt als eigener Brocken weiter.
void Simulation::brockenSchritt(double dt) {
    if (m_brocken.empty()) return;
    std::uniform_real_distribution<double> u(0, 1);
    const double g = 1200 * m_e.rutschTempo;
    double unterkante = -1e300;
    for (const Rechteck &b : m_bildschirme) unterkante = std::max(unterkante, b.unten());
    std::vector<Brocken> neu;
    for (Brocken &b : m_brocken) {
        const double y0 = b.y;
        b.vy = std::min(b.vy + g * dt, 1500.0);
        b.y += b.vy * dt;
        b.x += b.vx * dt;
        const size_t n = b.hoehe.size();
        // je Spalte die erste getroffene Deckenoberfläche
        struct Ziel { Decke *d = nullptr; size_t s = 0; double oben = 1e300; };
        std::vector<Ziel> ziel(n);
        double erster = 1e300;
        for (size_t i = 0; i < n; ++i) {
            const double cx = b.x + (i + 0.5) * b.spaltenBreite;
            for (Decke &d : m_decken) {
                const double rel = (cx - d.x) / d.spaltenBreite;
                if (rel < 0) continue;
                const size_t s = size_t(rel);
                if (s >= d.hoehe.size() || !d.sichtbar[s] || d.schluessel == b.herkunft) continue;
                const double oben = d.y - d.hoehe[s];
                if (y0 <= d.y + 0.5 && oben < ziel[i].oben && b.y >= oben) ziel[i] = {&d, s, oben};
            }
            erster = std::min(erster, ziel[i].oben);
        }
        if (erster >= 1e299) {
            if (b.y < unterkante + 50) neu.push_back(std::move(b));
            continue;
        }
        // Aufprall: aufsetzende Spalten liegen bleiben lassen, überhängende brechen ab
        ++m_deckenStand;
        const double toleranz = 6;
        double staub = 0;
        std::vector<char> traegt(n, 0);
        for (size_t i = 0; i < n; ++i)
            if (ziel[i].d && ziel[i].oben <= erster + toleranz) {
                traegt[i] = 1;
                const double h = b.hoehe[i];
                ablegen(*ziel[i].d, ziel[i].s, h * 0.85);
                staub += h * 0.15;
            }
        // aufstiebendes Pulver (Menge bleibt erhalten: Rest einer Portion geht an die Decke)
        const double portion = ablageFuer(m_e.groesse);
        const int anzahl = int(staub / portion);
        double restStaub = staub - anzahl * portion;
        for (size_t i = 0; i < n && restStaub > 0; ++i)
            if (traegt[i]) { ablegen(*ziel[i].d, ziel[i].s, restStaub); restStaub = 0; }
        std::vector<size_t> traeger;
        for (size_t i = 0; i < n; ++i) if (traegt[i]) traeger.push_back(i);
        for (int k = 0; k < anzahl && m_flocken.size() < 6000 && !traeger.empty(); ++k) {
            const size_t i = traeger[size_t(u(m_rng) * traeger.size()) % traeger.size()];
            Flocke f = neueFlocke(b.x + (i + u(m_rng)) * b.spaltenBreite, ziel[i].oben - 1 - u(m_rng) * 6);
            f.art = Flockenart::Pulver;
            f.fang = 1.0f;
            f.vx = (u(m_rng) - 0.5) * 80;
            f.vy = 10 + 20 * u(m_rng);
            m_flocken.push_back(f);
        }
        // überhängende Stücke fallen als eigene Brocken weiter (etwas gebremst)
        for (size_t i = 0; i < n;) {
            if (traegt[i] || b.hoehe[i] <= 0) { ++i; continue; }
            size_t j = i;
            while (j < n && !traegt[j]) ++j;
            Brocken r;
            r.x = b.x + i * b.spaltenBreite;
            r.y = b.y;
            r.spaltenBreite = b.spaltenBreite;
            r.hoehe.assign(b.hoehe.begin() + long(i), b.hoehe.begin() + long(j));
            r.vx = b.vx;
            r.vy = b.vy * 0.6;
            r.herkunft = b.herkunft;
            neu.push_back(std::move(r));
            i = j;
        }
    }
    m_brocken.swap(neu);
}

const Decke *Simulation::decke(const std::string &id) const {
    for (const Decke &d : m_decken)
        if (d.id == id) return &d;
    return nullptr;
}

double Simulation::hoeheBei(const std::string &id, double x) const {
    const Decke *d = decke(id);
    if (!d) return 0;
    double rel = (x - d->x) / d->spaltenBreite;
    if (rel < 0 || size_t(rel) >= d->hoehe.size()) return 0;
    return d->hoehe[size_t(rel)];
}

double Simulation::gesamtSchnee(const std::string &id) const {
    const Decke *d = decke(id);
    if (!d) return 0;
    double s = 0;
    for (float h : d->hoehe) s += h;
    return s;
}


bool Simulation::flockeZeichnen(const Flocke &f) const {
    switch (m_e.modus) {
    case Fenstermodus::Davor: return true;
    case Fenstermodus::Fest: // nie in einem sichtbaren Fenster (auch nicht vor der Taskleiste)
        if (f.hinten) return false;
        for (const Fenster &w : m_fenster)
            if (!w.minimiert && w.r.enthaelt(f.x, f.y)) return false;
        return true;
    case Fenstermodus::Dahinter: break;
    }
    for (const Fenster &w : m_fenster) {
        if (w.minimiert || w.dock) continue; // Taskleiste/Panels zählen nicht
        if (w.r.enthaelt(f.x, f.y)) return false;
    }
    return true;
}

void Simulation::vorfuellen() {
    std::uniform_real_distribution<double> u(0, 1);
    const bool fest = m_e.modus == Fenstermodus::Fest;
    for (const Rechteck &b : m_bildschirme) {
        int n = int(b.w * b.h * m_e.menge / 10000.0);
        for (int i = 0; i < n; ++i) {
            const double x = b.x + u(m_rng) * b.w, y = b.y + u(m_rng) * b.h;
            if (fest && fensterBei(x, y)) continue; // Schneeschatten unter Fenstern gleich von Anfang an
            m_flocken.push_back(neueFlocke(x, y));
        }
    }
}

void Simulation::abschuetteln() {
    for (Decke &d : m_decken) zerfallen(d, 0, d.hoehe.size(), 1.0, m_e.modus == Fenstermodus::Fest);
}

// ---- Schnittstellen für Räumdienst und Schneebälle ----

double Simulation::oberkanteBei(const std::string &id, double x) const {
    const Decke *d = decke(id);
    if (!d) return std::numeric_limits<double>::quiet_NaN();
    const double rel = (x - d->x) / d->spaltenBreite;
    if (rel < 0 || size_t(rel) >= d->hoehe.size()) return std::numeric_limits<double>::quiet_NaN();
    return d->y - d->hoehe[size_t(rel)];
}

namespace {
Decke *deckeVon(std::vector<Decke> &decken, const std::string &id) {
    for (Decke &d : decken)
        if (d.id == id) return &d;
    return nullptr;
}
}

double Simulation::schneeHinzufuegen(const std::string &id, double x, double menge) {
    Decke *d = deckeVon(m_decken, id);
    if (!d || menge <= 0) return 0;
    const double rel = (x - d->x) / d->spaltenBreite;
    if (rel < 0 || size_t(rel) >= d->hoehe.size() || !d->sichtbar[size_t(rel)]) return 0;
    ablegen(*d, size_t(rel), menge);
    return menge;
}

double Simulation::schneeEntnehmen(const std::string &id, double x0, double x1, double menge) {
    Decke *d = deckeVon(m_decken, id);
    if (!d || menge <= 0 || x1 <= x0) return 0;
    const long a = std::max(0L, long(std::floor((x0 - d->x) / d->spaltenBreite)));
    const long b = std::min(long(d->hoehe.size()), long(std::ceil((x1 - d->x) / d->spaltenBreite)));
    double vorrat = 0;
    for (long i = a; i < b; ++i) if (d->sichtbar[size_t(i)]) vorrat += d->hoehe[size_t(i)];
    if (vorrat <= 0) return 0;
    const double weg = std::min(menge, vorrat);
    // gleichmäßig von oben: jede Spalte gibt denselben Anteil ab
    const double anteil = weg / vorrat;
    for (long i = a; i < b; ++i)
        if (d->sichtbar[size_t(i)]) d->hoehe[size_t(i)] -= float(d->hoehe[size_t(i)] * anteil);
    ++m_deckenStand;
    return weg;
}

double Simulation::abtragenUndFallenLassen(const std::string &id, double x, double menge, int richtung) {
    Decke *d = deckeVon(m_decken, id);
    if (!d || d->boden) return 0;
    const double sb = d->spaltenBreite;
    // erst die Spalte bei x, reicht es nicht, auch die Nachbarn (bis 3 Spalten weit)
    double weg = 0;
    for (int r = 0; r <= 3 && weg < menge; ++r)
        weg += schneeEntnehmen(id, x - (r + 0.5) * sb, x + (r + 0.5) * sb, menge - weg);
    if (weg <= 0) return 0;
    const double breite = std::clamp(weg / std::max(1.0, m_e.maxHoehe * 0.5) * sb, sb, 8 * sb);
    const double kante = richtung < 0 ? d->x : d->x + d->breite();
    const double mitte = kante + (richtung < 0 ? -1 : 1) * (breite / 2 + 0.5);
    brockenFallenLassen(mitte, d->y, breite, weg, (richtung < 0 ? -1 : 1) * 15.0, 20, d->schluessel);
    return weg;
}

void Simulation::brockenFallenLassen(double x, double y, double breite, double menge, double vx, double vy, uint32_t herkunft) {
    if (menge <= 0) return;
    const double sb = m_e.spaltenBreite;
    const size_t n = std::max<size_t>(1, size_t(std::lround(breite / sb)));
    Brocken b;
    b.spaltenBreite = sb;
    b.x = x - n * sb / 2;
    b.y = y;
    b.vx = vx;
    b.vy = vy;
    b.herkunft = herkunft;
    // flacher Hügel: in der Mitte höher als am Rand, Summe = menge
    b.hoehe.resize(n);
    double summe = 0;
    for (size_t i = 0; i < n; ++i) { b.hoehe[i] = float(1.0 + std::sin(3.14159265 * (i + 0.5) / n)); summe += b.hoehe[i]; }
    for (float &h : b.hoehe) h = float(h * menge / summe);
    m_brocken.push_back(std::move(b));
}

double Simulation::fallenderSchnee() const {
    double m = 0;
    for (const Flocke &f : m_flocken) if (f.leben < 0) m += ablageFuer(f.groesse);
    for (const Brocken &b : m_brocken) m += b.menge();
    return m;
}

bool vollbildAktiv(const std::vector<Fenster> &fenster) {
    for (const Fenster &f : fenster)
        if (f.aktiv && f.vollbild && !f.minimiert) return true;
    return false;
}

} // namespace schnee
