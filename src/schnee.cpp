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
        m_flocken.push_back(f);
    }
}

int Simulation::zielAnzahl() const {
    double flaeche = 0;
    for (const Rechteck &b : m_bildschirme) flaeche += b.w * b.h;
    return int(flaeche * m_e.menge / 10000.0);
}

void Simulation::setFenster(const std::vector<Fenster> &f) {
    m_fenster = f;
    auto finde = [&](const std::string &id) -> const Fenster * {
        for (const Fenster &w : m_fenster)
            if (w.id == id) return &w;
        return nullptr;
    };
    auto traegt = [](const Fenster *w) { return w && !w->minimiert && !w->vollbild && w->r.w >= 1; };
    // bestehende Decken anpassen oder zerfallen lassen
    for (Decke &d : m_decken) {
        if (d.boden) continue;
        const Fenster *w = finde(d.id);
        bool weg = !traegt(w) || std::abs(w->r.x - d.x) > 0.5 || std::abs(w->r.y - d.y) > 0.5;
        if (weg) {
            zerfallen(d, 0, d.hoehe.size());
            d.hoehe.clear();
            continue;
        }
        size_t n = std::max<size_t>(1, size_t(std::floor(w->r.w / d.spaltenBreite)));
        if (n < d.hoehe.size()) zerfallen(d, n, d.hoehe.size());
        d.hoehe.resize(n, 0.f);
        d.sichtbar.resize(n, 1);
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
        d.dock = w.dock;
        d.spaltenBreite = m_e.spaltenBreite;
        size_t n = std::max<size_t>(1, size_t(std::floor(w.r.w / m_e.spaltenBreite)));
        d.hoehe.assign(n, 0.f);
        d.sichtbar.assign(n, 1);
        m_decken.push_back(std::move(d));
    }
    nachSichtbarkeitZerfallen();
}

void Simulation::zerfallen(Decke &d, size_t von, size_t bis) {
    std::uniform_real_distribution<double> u(0, 1);
    const double portion = ablageFuer(m_e.groesse);
    const size_t grenze = 6000; // Schutz gegen Flockenlawinen
    for (size_t i = von; i < bis && i < d.hoehe.size(); ++i) {
        double h = d.hoehe[i];
        d.hoehe[i] = 0;
        if (h <= 0) continue;
        double anzahlD = h / portion;
        int anzahl = int(anzahlD);
        if (u(m_rng) < anzahlD - anzahl) ++anzahl;
        for (int k = 0; k < anzahl && m_flocken.size() < grenze; ++k) {
            double x = d.x + (i + u(m_rng)) * d.spaltenBreite;
            double y = d.y - u(m_rng) * h - 0.5;
            Flocke f = neueFlocke(x, y);
            f.vy = 10 + 30 * u(m_rng);
            f.fang = 1.0f;
            f.vx = (u(m_rng) - 0.5) * 20 + m_e.wind * 0.3;
            f.herkunft = d.schluessel;
            m_flocken.push_back(f);
        }
    }
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
    // große Flocken fallen etwas schneller
    double vy = m_e.fallGeschw * (0.6 + 0.4 * g / m_e.groesse) * (0.8 + 0.4 * u(m_rng));
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
        f.vx = richtung * 8.0 + m_e.wind * 0.5;
        f.vy = std::max(15.0, f.vy * 0.5);
        f.fang = 1.0f;
        m_flocken.push_back(f);
    }
}

void Simulation::rutschen(Decke &d, double) {
    const size_t n = d.hoehe.size();
    if (n == 0) return;
    const float maxDiff = float(d.spaltenBreite * 1.2);
    const float randGrenze = float(std::min(m_e.maxHoehe, d.spaltenBreite * 1.5));
    const float maxH = float(m_e.maxHoehe);
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

void Simulation::schritt(double dt, bool erzeugenAn) {
    if (erzeugenAn) erzeugen(dt);
    std::vector<Flocke> bleiben;
    bleiben.reserve(m_flocken.size());
    for (Flocke f : m_flocken) {
        double y0 = f.y;
        f.phase += dt * 1.7;
        f.x += (f.vx + std::cos(f.phase) * f.groesse * 2.5) * dt;
        f.y += f.vy * dt;
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
            if (!d.boden && !d.dock && !faengt(f, d)) continue; // fällt vor diesem Fenster vorbei
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
        // nur auf Monitorflächen (oder knapp über/neben dem Rand beim Hereinkommen)
        double rand = f.groesse + 2;
        if (!aufBildschirm(f.x, f.y) && !aufBildschirm(f.x, f.y + rand) && !aufBildschirm(f.x + rand, f.y) && !aufBildschirm(f.x - rand, f.y))
            continue;
        bleiben.push_back(f);
    }
    m_flocken.swap(bleiben);
    for (Decke &d : m_decken) {
        rutschen(d, dt);
        if (d.boden && m_e.schmelzen > 0) {
            for (float &h : d.hoehe)
                if (h > 0) h = std::max(0.f, h - float(m_e.schmelzen * dt * (0.2 + h / m_e.maxHoehe)));
        }
    }
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
    if (m_e.vorFenstern) return true;
    for (const Fenster &w : m_fenster) {
        if (w.minimiert || w.dock) continue; // Taskleiste/Panels zählen nicht
        if (w.r.enthaelt(f.x, f.y)) return false;
    }
    return true;
}

void Simulation::vorfuellen() {
    std::uniform_real_distribution<double> u(0, 1);
    for (const Rechteck &b : m_bildschirme) {
        int n = int(b.w * b.h * m_e.menge / 10000.0);
        for (int i = 0; i < n; ++i) m_flocken.push_back(neueFlocke(b.x + u(m_rng) * b.w, b.y + u(m_rng) * b.h));
    }
}

void Simulation::abschuetteln() {
    for (Decke &d : m_decken) zerfallen(d, 0, d.hoehe.size());
}

bool vollbildAktiv(const std::vector<Fenster> &fenster) {
    for (const Fenster &f : fenster)
        if (f.aktiv && f.vollbild && !f.minimiert) return true;
    return false;
}

} // namespace schnee
