#include "deckenform.h"
#include <algorithm>
#include <cmath>

namespace schnee {
namespace {

const float SCHWELLE = 0.25f; // dünner liegt noch kein sichtbarer Schnee

// Viertelkreis: 0 am Ende, 1 ab Abstand „breite“ – gibt die gerundete Schulter
float rund(float abstand, float breite) {
    if (abstand >= breite) return 1.f;
    if (abstand <= 0.f) return 0.f;
    const float t = 1.f - abstand / breite;
    return std::sqrt(std::max(0.f, 1.f - t * t));
}

} // namespace

void deckenProfilAnhaengen(const Decke &d, std::vector<ProfilPunkt> &punkte, std::vector<size_t> &enden) {
    const size_t n = d.hoehe.size();
    const float sb = float(d.spaltenBreite);
    auto liegt = [&](size_t i) { return i < n && i < d.sichtbar.size() && d.sichtbar[i] && d.hoehe[i] >= SCHWELLE; };
    // Arbeitsspeicher wiederverwenden: läuft für jede Decke in jedem Bild
    static thread_local std::vector<float> s, t, xs;
    for (size_t i = 0; i < n;) {
        if (!liegt(i)) { ++i; continue; }
        // Ein Stück reicht von der ersten bis zur letzten Schneespalte. Bis zu zwei leere Spalten
        // dazwischen werden überbrückt (die Glättung füllt sie), sonst läge der Schnee beim
        // Aufbau als lauter einzelne Brocken da. Verdeckte Spalten trennen immer.
        const size_t a = i;
        size_t ende = i + 1;
        for (size_t j = i + 1, leer = 0; j < n && d.sichtbar[j]; ++j) {
            if (liegt(j)) { ende = j + 1; leer = 0; }
            else if (++leer > 2) break;
        }
        i = ende;
        const size_t m = ende - a;
        // dreimal 1-2-1 glätten: aus Spaltenstufen wird eine weiche Linie
        s.assign(d.hoehe.begin() + long(a), d.hoehe.begin() + long(a + m));
        for (int lauf = 0; lauf < 3; ++lauf) {
            t = s;
            for (size_t j = 0; j < m; ++j) {
                const float l = j > 0 ? t[j - 1] : t[j], r = j + 1 < m ? t[j + 1] : t[j];
                s[j] = (l + 2.f * t[j] + r) * 0.25f;
            }
        }
        const float links = float(d.x) + a * sb, rechts = float(d.x) + (a + m) * sb;
        // freie Fensterenden: Schnee quillt etwas über. Boden und Taskleiste enden am Monitorrand.
        const bool endeL = a == 0, endeR = a + m == n;
        const bool ueberhang = !d.boden && !d.dock;
        const bool rundL = !(d.boden && endeL), rundR = !(d.boden && endeR);
        const float ueberL = ueberhang && endeL ? std::clamp(0.25f * s.front() + 0.8f, 1.f, 3.5f) : 0.f;
        const float ueberR = ueberhang && endeR ? std::clamp(0.25f * s.back() + 0.8f, 1.f, 3.5f) : 0.f;
        const float x0 = links - ueberL, x1 = rechts + ueberR;
        const float schulterL = std::clamp(0.6f * s.front(), sb * 0.5f, 8.f) + ueberL * 0.5f;
        const float schulterR = std::clamp(0.6f * s.back(), sb * 0.5f, 8.f) + ueberR * 0.5f;
        const float haengtL = std::min(2.2f, 0.18f * s.front() + 0.4f), haengtR = std::min(2.2f, 0.18f * s.back() + 0.4f);

        // Stützstellen: Spaltenmitten, Enden, Fensterkanten und dichter in den Schultern
        // (sparsam: jede Stützstelle kostet in jedem Bild sechs Ecken)
        xs.assign({x0, x1});
        if (ueberL > 0) xs.push_back(links);
        if (ueberR > 0) xs.push_back(rechts);
        float zuletzt = -1e9f;
        for (size_t j = 0; j < m; ++j) {
            // ebene Abschnitte brauchen keine Zwischenpunkte
            const bool eben = j > 0 && j + 1 < m && std::abs(s[j] - s[j - 1]) < 0.03f && std::abs(s[j] - s[j + 1]) < 0.03f
                              && std::abs(s[j] - zuletzt) < 0.06f;
            if (eben) continue;
            zuletzt = s[j];
            xs.push_back(links + (j + 0.5f) * sb);
        }
        auto schulter = [&](float anfang, float richtung, float breite) {
            // je breiter die Rundung, desto mehr Punkte; kleine Hügel kommen mit zweien aus
            static const float fein[] = {0.05f, 0.18f, 0.4f, 0.7f, 1.f}, mittel[] = {0.12f, 0.45f, 1.f}, grob[] = {0.3f, 1.f};
            const float *f = breite >= 4.f ? fein : breite >= 2.f ? mittel : grob;
            const int anzahl = breite >= 4.f ? 5 : breite >= 2.f ? 3 : 2;
            for (int k = 0; k < anzahl; ++k) xs.push_back(anfang + richtung * f[k] * breite);
        };
        if (rundL) schulter(x0, 1.f, schulterL);
        if (rundR) schulter(x1, -1.f, schulterR);
        std::sort(xs.begin(), xs.end());
        const size_t beginn = punkte.size();
        for (float x : xs) {
            if (x < x0 || x > x1) continue;
            if (punkte.size() > beginn && x - punkte.back().x < 0.02f) continue;
            // geglättete Höhe, zwischen den Spaltenmitten geradlinig
            const float rel = std::clamp((x - links) / sb - 0.5f, 0.f, float(m - 1));
            const size_t j = std::min(m - 1, size_t(rel));
            const float h = j + 1 < m ? s[j] + (s[j + 1] - s[j]) * (rel - j) : s[j];
            ProfilPunkt q{x, h, 0.f};
            if (rundL) q.oben *= rund(x - x0, schulterL);
            if (rundR) q.oben *= rund(x1 - x, schulterR);
            if (x < links && ueberL > 0) q.unten = haengtL * std::sin(3.14159265f * (links - x) / ueberL);
            if (x > rechts && ueberR > 0) q.unten = haengtR * std::sin(3.14159265f * (x - rechts) / ueberR);
            q.unten = std::max(0.f, q.unten);
            if (x == x0 && rundL) q.unten = 0.f;
            if (x == x1 && rundR) q.unten = 0.f;
            punkte.push_back(q);
        }
        if (punkte.size() - beginn >= 2) enden.push_back(punkte.size());
        else punkte.resize(beginn);
    }
}

std::vector<std::vector<ProfilPunkt>> deckenProfil(const Decke &d) {
    std::vector<ProfilPunkt> punkte;
    std::vector<size_t> enden;
    deckenProfilAnhaengen(d, punkte, enden);
    std::vector<std::vector<ProfilPunkt>> stuecke;
    size_t a = 0;
    for (size_t e : enden) {
        stuecke.emplace_back(punkte.begin() + long(a), punkte.begin() + long(e));
        a = e;
    }
    return stuecke;
}

} // namespace schnee
