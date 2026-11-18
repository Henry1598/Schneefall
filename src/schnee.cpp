#include "schnee.h"
#include <algorithm>
#include <cmath>

namespace schnee {

Simulation::Simulation(Einstellungen e, uint32_t saat) : m_e(e), m_rng(saat) {}

void Simulation::setEinstellungen(const Einstellungen &e) { m_e = e; }

void Simulation::setBildschirme(const std::vector<Rechteck> &b) {
    m_bildschirme = b;
    sichtbarkeitBerechnen();
}

void Simulation::setFenster(const std::vector<Fenster> &f) {
    m_fenster = f;
    for (const Fenster &w : m_fenster) {
        if (w.minimiert || w.vollbild || w.r.w <= 0) continue;
        auto it = std::find_if(m_decken.begin(), m_decken.end(), [&](const Decke &d) { return d.id == w.id; });
        if (it != m_decken.end()) continue;
        Decke d;
        d.id = w.id;
        d.x = w.r.x;
        d.y = w.r.y;
        d.spaltenBreite = m_e.spaltenBreite;
        size_t n = std::max<size_t>(1, size_t(std::floor(w.r.w / m_e.spaltenBreite)));
        d.hoehe.assign(n, 0.f);
        d.sichtbar.assign(n, 1);
        m_decken.push_back(std::move(d));
    }
    sichtbarkeitBerechnen();
}

void Simulation::sichtbarkeitBerechnen() {
    for (Decke &d : m_decken) {
        const Fenster *eigen = nullptr;
        for (const Fenster &w : m_fenster)
            if (w.id == d.id) eigen = &w;
        int stapel = eigen ? eigen->stapel : -1000000; // Boden liegt unter allem
        for (size_t s = 0; s < d.hoehe.size(); ++s) {
            double cx = d.x + (s + 0.5) * d.spaltenBreite;
            bool sicht = m_bildschirme.empty() || aufBildschirm(cx, d.y - 1);
            if (d.boden) sicht = m_bildschirme.empty() || aufBildschirm(cx, d.y - 1);
            for (const Fenster &w : m_fenster) {
                if (!sicht) break;
                if (&w == eigen || w.minimiert || w.stapel <= stapel) continue;
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

bool Simulation::aufBildschirm(double x, double y) const {
    for (const Rechteck &b : m_bildschirme)
        if (b.enthaelt(x, y)) return true;
    return false;
}

void Simulation::schritt(double dt, bool) {
    std::vector<Flocke> bleiben;
    bleiben.reserve(m_flocken.size());
    for (Flocke f : m_flocken) {
        double y0 = f.y;
        f.x += f.vx * dt;
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
            double oben = d.y - d.hoehe[s];
            if (y0 <= oben && f.y >= oben && oben < trefferOben) {
                treffer = &d; trefferSpalte = s; trefferOben = oben;
            }
        }
        if (treffer) {
            ablegen(*treffer, trefferSpalte, ablageFuer(f.groesse));
            continue;
        }
        bleiben.push_back(f);
    }
    m_flocken.swap(bleiben);
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

int Simulation::zielAnzahl() const { return 0; }

} // namespace schnee
