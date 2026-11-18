#include "ansicht3d.h"
#include <algorithm>
#include <cmath>

namespace schnee {
namespace {
const double PI = 3.14159265358979;
P3 minus(P3 a, P3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
P3 kreuz(P3 a, P3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double punkt(P3 a, P3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
P3 einheit(P3 a) {
    const double l = std::sqrt(punkt(a, a));
    return l > 1e-12 ? P3{a.x / l, a.y / l, a.z / l} : P3{0, 0, 0};
}
P3 mitte(const std::vector<P3> &e) {
    P3 m;
    for (const P3 &p : e) { m.x += p.x; m.y += p.y; m.z += p.z; }
    const double n = std::max<size_t>(1, e.size());
    return {m.x / n, m.y / n, m.z / n};
}
// dreht die Fläche so, dass ihre Normale vom Innenpunkt weg zeigt
void nachAussen(Flaeche &f, P3 innen) {
    if (punkt(normale(f), minus(mitte(f.ecken), innen)) < 0) std::reverse(f.ecken.begin(), f.ecken.end());
}
// Licht im Bildraum: von oben, links und etwas von vorn
const P3 LICHT = einheit({-0.5, 0.78, 0.42});
double roh(P3 n) { return 0.5 + 0.62 * std::max(0.0, punkt(n, LICHT)) + 0.1 * n.y; }
} // namespace

P3 imBildraum(P3 r, double gier) {
    const double c = std::cos(gier), s = std::sin(gier);
    return {r.x * c - r.z * s, r.y, r.x * s + r.z * c};
}

P2 projizieren(P3 p, double gier) {
    const P3 b = imBildraum(p, gier);
    return {b.x, -b.y * std::cos(KAMERA_NEIGUNG) + b.z * std::sin(KAMERA_NEIGUNG)};
}

double tiefe(P3 p, double gier) {
    const P3 b = imBildraum(p, gier);
    return b.z * std::cos(KAMERA_NEIGUNG) + b.y * std::sin(KAMERA_NEIGUNG);
}

double helligkeit(P3 n) { return roh(einheit(n)) / roh({0, 0, 1}); }

int winkelstufe(double gier) {
    const double schritt = 2 * PI / WINKELSTUFEN;
    int i = int(std::lround(gier / schritt)) % WINKELSTUFEN;
    return i < 0 ? i + WINKELSTUFEN : i;
}

double stufenWinkel(int stufe) { return stufe * 2 * PI / WINKELSTUFEN; }

P3 normale(const Flaeche &f) {
    // Newell: auch für leicht unebene Vielecke brauchbar
    P3 n;
    for (size_t i = 0; i < f.ecken.size(); ++i) {
        const P3 &a = f.ecken[i], &b = f.ecken[(i + 1) % f.ecken.size()];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return einheit(n);
}

size_t Modell::quader(double x0, double x1, double y0, double y1, double z0, double z1, uint32_t farbe) {
    const size_t erste = flaechen.size();
    const P3 innen{(x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2};
    auto f = [&](std::vector<P3> e) {
        Flaeche fl;
        fl.ecken = std::move(e);
        fl.farbe = farbe;
        nachAussen(fl, innen);
        flaechen.push_back(std::move(fl));
    };
    f({{x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1}});
    f({{x0, y0, z0}, {x0, y1, z0}, {x0, y1, z1}, {x0, y0, z1}});
    f({{x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}});
    f({{x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}});
    f({{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}});
    f({{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0}});
    return erste;
}

size_t Modell::prisma(const std::vector<P2> &profil, double z0, double z1, uint32_t mantel, uint32_t deckel) {
    const size_t erste = flaechen.size(), n = profil.size();
    P3 innen{0, 0, (z0 + z1) / 2};
    for (const P2 &p : profil) { innen.x += p.x / n; innen.y += p.y / n; }
    for (size_t i = 0; i < n; ++i) {
        const P2 &a = profil[i], &b = profil[(i + 1) % n];
        Flaeche f;
        f.ecken = {{a.x, a.y, z1}, {a.x, a.y, z0}, {b.x, b.y, z0}, {b.x, b.y, z1}};
        f.farbe = mantel;
        nachAussen(f, innen);
        flaechen.push_back(std::move(f));
    }
    for (double z : {z1, z0}) {
        Flaeche f;
        for (const P2 &p : profil) f.ecken.push_back({p.x, p.y, z});
        f.farbe = deckel;
        nachAussen(f, innen);
        flaechen.push_back(std::move(f));
    }
    return erste;
}

size_t Modell::schale(const std::vector<Spant> &sp, int ecken, uint32_t farbe) {
    const size_t erste = flaechen.size();
    auto ecke = [&](const Spant &s, int k) {
        const double w = 2 * PI * k / ecken;
        return P3{s.x, s.y + std::sin(w) * s.ry, std::cos(w) * s.rz};
    };
    for (size_t i = 0; i + 1 < sp.size(); ++i) {
        const P3 innen{(sp[i].x + sp[i + 1].x) / 2, (sp[i].y + sp[i + 1].y) / 2, 0};
        for (int k = 0; k < ecken; ++k) {
            Flaeche f;
            f.ecken = {ecke(sp[i], k), ecke(sp[i + 1], k), ecke(sp[i + 1], k + 1), ecke(sp[i], k + 1)};
            f.farbe = farbe;
            nachAussen(f, innen);
            flaechen.push_back(std::move(f));
        }
    }
    // Abschlüsse vorn und hinten
    for (size_t i : {size_t(0), sp.size() - 1}) {
        Flaeche f;
        for (int k = 0; k < ecken; ++k) f.ecken.push_back(ecke(sp[i], k));
        f.farbe = farbe;
        nachAussen(f, {sp[sp.size() / 2].x, sp[sp.size() / 2].y, 0});
        flaechen.push_back(std::move(f));
    }
    return erste;
}

void Modell::aufkleberX(double x, double y0, double y1, double z0, double z1, bool nachVorn, uint32_t farbe, double vor) {
    Flaeche f;
    f.ecken = {{x, y0, z0}, {x, y1, z0}, {x, y1, z1}, {x, y0, z1}};
    f.farbe = farbe;
    f.vor = vor;
    nachAussen(f, {x + (nachVorn ? -1.0 : 1.0), (y0 + y1) / 2, (z0 + z1) / 2});
    flaechen.push_back(std::move(f));
}

std::vector<Sichtflaeche> ansicht(const Modell &m, double gier) {
    std::vector<Sichtflaeche> s;
    s.reserve(m.flaechen.size());
    const P3 blick{0, std::sin(KAMERA_NEIGUNG), std::cos(KAMERA_NEIGUNG)}; // zum Betrachter
    for (const Flaeche &f : m.flaechen) {
        P3 n = imBildraum(normale(f), gier);
        const double zu = punkt(n, blick);
        if (zu <= 1e-6) {
            if (!f.beidseitig) continue;
            n = {-n.x, -n.y, -n.z};
        }
        Sichtflaeche v;
        v.quelle = &f;
        v.hell = helligkeit(n);
        double t = 0;
        for (const P3 &p : f.ecken) {
            v.ecken.push_back(projizieren(p, gier));
            t += tiefe(p, gier);
        }
        v.tiefe = t / f.ecken.size() + f.vor;
        s.push_back(std::move(v));
    }
    std::stable_sort(s.begin(), s.end(), [](const Sichtflaeche &a, const Sichtflaeche &b) { return a.tiefe < b.tiefe; });
    return s;
}

} // namespace schnee
