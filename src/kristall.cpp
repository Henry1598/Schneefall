#include "kristall.h"
#include <algorithm>
#include <cmath>

namespace schnee {
namespace {

const float PI = 3.14159265f;

// kleiner, fester Zufallsgenerator (gleiche Saat = gleiche Folge auf jedem Rechner)
struct Zufall {
    uint32_t z;
    explicit Zufall(uint32_t saat) : z(saat * 2654435761u + 0x9e3779b9u) {}
    uint32_t roh() {
        z += 0x6d2b79f5u;
        uint32_t t = z;
        t = (t ^ (t >> 15)) * (t | 1u);
        t ^= t + (t ^ (t >> 7)) * (t | 61u);
        return t ^ (t >> 14);
    }
    float operator()() { return float(roh() >> 8) / 16777216.f; }
    float bereich(float a, float b) { return a + (b - a) * (*this)(); }
    int ganz(int a, int b) { return a + int(roh() % uint32_t(b - a + 1)); } // einschließlich b
};

// Baut einen Kristall aus einem halben Arm (Arm zeigt nach +x, Seitenäste nach +y):
// jeder Teil wird gespiegelt und sechsmal um 60° gedreht.
struct Bauer {
    Kristall k;
    void strich(float x0, float y0, float x1, float y1, float b0, float b1, uint8_t rang) {
        const bool aufAchse = y0 == 0.f && y1 == 0.f;
        for (int i = 0; i < 6; ++i) {
            const float c = std::cos(i * PI / 3), s = std::sin(i * PI / 3);
            for (float m : {1.f, -1.f}) {
                if (m < 0 && aufAchse) continue;
                k.striche.push_back({x0 * c - m * y0 * s, x0 * s + m * y0 * c, x1 * c - m * y1 * s, x1 * s + m * y1 * c, b0, b1, rang});
            }
        }
    }
    // Plättchen auf der Armachse im Abstand x von der Mitte (x = 0: genau eines in der Mitte)
    void sechseck(float x, float radius, float fuellung, float rand, uint8_t rang) {
        if (x == 0.f) {
            k.sechsecke.push_back({0, 0, radius, fuellung, rand, rang});
            return;
        }
        for (int i = 0; i < 6; ++i)
            k.sechsecke.push_back({x * std::cos(i * PI / 3), x * std::sin(i * PI / 3), radius, fuellung, rand, rang});
    }
};

// Sternförmiger Dendrit: sechs Arme mit Seitenästen im 60°-Winkel, bei „farn“ dicht besetzt
void dendrit(Bauer &b, Zufall &z, bool farn) {
    const float R = z.bereich(0.90f, 0.97f);
    const float bw = farn ? z.bereich(0.045f, 0.06f) : z.bereich(0.07f, 0.11f);
    b.strich(0, 0, R, 0, bw, bw * 0.55f, 0);
    const int n = farn ? z.ganz(4, 5) : z.ganz(2, 4);
    const int form = z.ganz(0, 2); // 0 = Tanne (zur Spitze kürzer), 1 = Raute, 2 = gleichmäßig
    const float lmax = z.bereich(0.30f, 0.46f);
    for (int j = 0; j < n; ++j) {
        const float p = 0.2f + (j + z.bereich(0.3f, 0.7f)) / n * 0.7f;
        const float form_ = form == 0 ? 1.f - 0.75f * p : form == 1 ? 0.35f + 0.65f * std::sin(PI * std::min(1.f, p * 1.1f)) : 0.7f;
        float len = std::min(lmax * form_, 0.85f * p); // länger als p würde in den Nachbararm ragen
        const float px = p * R;
        while (len > 0.05f && std::hypot(px + len * 0.5f, len * 0.866f) > 0.97f) len -= 0.02f;
        const float qz = z.bereich(0.45f, 0.7f); // immer ziehen, damit die Folge gleich bleibt
        if (len < 0.07f) continue;
        // beim Farn fällt in kleinen Größen jeder zweite Ast weg, sonst verklumpt die Form
        const uint8_t astRang = farn && (j & 1) ? 2 : 1, zweigRang = farn ? 3 : 2;
        b.strich(px, 0, px + len * 0.5f, len * 0.866f, bw * 0.8f, bw * 0.45f, astRang);
        if (len > 0.2f) {
            // feine Zweige, wieder im 60°-Raster: einer parallel zum Arm, einer zum Nachbararm hin
            const float sx = px + qz * len * 0.5f, sy = qz * len * 0.866f;
            const float tl = len * 0.38f * (1.f - qz * 0.4f);
            if (std::hypot(sx + tl, sy) < 0.97f) b.strich(sx, sy, sx + tl, sy, bw * 0.5f, bw * 0.35f, zweigRang);
            b.strich(sx, sy, sx - tl * 0.4f, sy + tl * 0.693f, bw * 0.5f, bw * 0.35f, zweigRang);
        }
    }
    b.sechseck(0, z.bereich(0.10f, 0.17f), 0.75f, 0.04f, 0);
    if (z() < 0.35f) b.sechseck(R - 0.08f, 0.065f, 0.6f, 0.03f, 1); // kleine Plättchen an den Spitzen
}

// Sternplättchen: Mittelplatte, breite Arme oder Arme mit Endplättchen
void sternplaettchen(Bauer &b, Zufall &z) {
    const float R = z.bereich(0.90f, 0.97f);
    const float r0 = z.bereich(0.28f, 0.42f);
    b.sechseck(0, r0, 0.5f, 0.05f, 0);
    b.sechseck(0, r0 * z.bereich(0.4f, 0.6f), 0.25f, 0.03f, 1);
    b.sechseck(0, 0.07f, 0.9f, 0.03f, 0); // dichter Kern
    if (z() < 0.5f) {
        // Arme mit sechseckigen Endplättchen
        const float rp = z.bereich(0.15f, 0.22f);
        b.strich(r0 * 0.5f, 0, R - rp, 0, 0.06f, 0.05f, 0);
        b.sechseck(R - rp, rp, 0.55f, 0.045f, 0);
        b.strich(R - rp * 1.9f, 0, R - rp * 0.1f, 0, 0.03f, 0.03f, 1); // Grat im Endplättchen
    } else {
        // breite, spitz zulaufende Arme mit einem Paar kurzer Seitenäste
        const float bb = z.bereich(0.16f, 0.24f);
        b.strich(r0 * 0.6f, 0, R * 0.55f, 0, bb * 0.7f, bb, 0);
        b.strich(R * 0.55f, 0, R, 0, bb, 0.04f, 0);
        const float p = z.bereich(0.5f, 0.68f), len = z.bereich(0.12f, 0.2f);
        b.strich(p * R, 0, p * R + len * 0.5f, len * 0.866f, 0.07f, 0.04f, 1);
    }
}

// Sechseckiges Plättchen mit Graten zu den Ecken und kurzen Armen
void plaettchen(Bauer &b, Zufall &z) {
    const float R = z.bereich(0.90f, 0.97f);
    const float r = z.bereich(0.60f, 0.76f);
    b.sechseck(0, r, 0.42f, 0.06f, 0);
    b.sechseck(0, r * z.bereich(0.35f, 0.6f), 0.22f, 0.035f, 1);
    b.strich(0, 0, r, 0, 0.03f, 0.03f, 1);
    b.strich(r * 0.95f, 0, R, 0, 0.10f, 0.05f, 0);
    if (z() < 0.5f) b.strich(r * 0.5f, 0, r * 0.5f + 0.09f, 0.156f, 0.03f, 0.03f, 2); // feine Rippen
}

float begrenzt(float v) { return std::clamp(v, 0.f, 1.f); }

// Zwischenbild beim Rastern: getrennt nach deckendem Eis und durchscheinender Fläche
struct Ebenen {
    int g;
    std::vector<float> fest, kern, durch, hell;
    explicit Ebenen(int g_) : g(g_), fest(size_t(g_) * g_, 0.f), kern(fest), durch(fest.size(), 1.f), hell(fest) {}
};

void aufBild(const Ebenen &e, Bild &b) {
    const int g = e.g;
    const float m = g / 2.f;
    b.breite = b.hoehe = g;
    b.rgba.assign(size_t(g) * g * 4, 0);
    const float kante[3] = {0.78f, 0.89f, 1.0f}, flaeche[3] = {0.80f, 0.90f, 1.0f};
    for (int y = 0; y < g; ++y)
        for (int x = 0; x < g; ++x) {
            const size_t i = size_t(y) * g + x;
            // runde Maske: nichts erreicht den Feldrand (kein Durchscheinen ins Nachbarfeld)
            const float maske = begrenzt(m - 1.f - std::hypot(x + 0.5f - m, y + 0.5f - m));
            const float f = e.fest[i];
            const float a = (1.f - (1.f - f) * e.durch[i]) * maske;
            if (a <= 0.002f) continue;
            const float anteilFest = f / std::max(1e-6f, 1.f - (1.f - f) * e.durch[i]);
            const float w = f > 0 ? begrenzt(e.kern[i] / f) : 0.f;
            uint8_t *p = &b.rgba[i * 4];
            for (int c = 0; c < 3; ++c) {
                const float eis = kante[c] + (1.f - kante[c]) * w;
                const float fl = flaeche[c] + (1.f - flaeche[c]) * 0.85f * e.hell[i];
                const float farbe = anteilFest * eis + (1.f - anteilFest) * fl;
                p[c] = uint8_t(std::lround(farbe * a * 255.f));
            }
            p[3] = uint8_t(std::lround(a * 255.f));
            for (int c = 0; c < 3; ++c) p[c] = std::min(p[c], p[3]);
        }
}

} // namespace

Kristall kristallErzeugen(uint32_t saat) {
    Zufall z(saat);
    Bauer b;
    // Mischung wie draußen: meist Dendriten, dazu Sternplättchen und einfache Plättchen
    switch (saat % 8) {
    case 2: case 5: b.k.art = Kristallart::Sternplaettchen; sternplaettchen(b, z); break;
    case 6: b.k.art = Kristallart::Plaettchen; plaettchen(b, z); break;
    case 3: case 7: b.k.art = Kristallart::Dendrit; dendrit(b, z, true); break;
    default: b.k.art = Kristallart::Dendrit; dendrit(b, z, false); break;
    }
    return b.k;
}

Bild kristallRastern(const Kristall &k, int g, float weich) {
    Ebenen e(g);
    const float m = g / 2.f;
    const float R = std::max(1.f, (m - 1.5f - 1.5f * weich) / 1.04f);
    const float aa = 1.f + 2.f * weich;              // Breite des Kantenübergangs in px
    const int maxRang = g >= 56 ? 3 : g >= 36 ? 2 : g >= 20 ? 1 : 0; // kleine Flocken: nur die großen Züge
    auto bereich = [g](float lo, float hi, int &a, int &b) {
        a = std::max(0, int(std::floor(lo)));
        b = std::min(g - 1, int(std::ceil(hi)));
    };
    for (const Strich &s : k.striche) {
        if (s.rang > maxRang) continue;
        const float ax = s.x0 * R, ay = s.y0 * R, bx = s.x1 * R - ax, by = s.y1 * R - ay;
        const float l2 = bx * bx + by * by;
        if (l2 < 1.0f) continue; // kürzer als ein Pixel
        const float mind = (s.rang == 0 ? 1.5f : 1.15f) + weich;
        const float h0 = std::max(s.b0 * R, mind) * 0.5f, h1 = std::max(s.b1 * R, mind) * 0.5f;
        const float rand = std::max(h0, h1) + aa;
        int xa, xb, ya, yb;
        bereich(m + std::min(ax, ax + bx) - rand, m + std::max(ax, ax + bx) + rand, xa, xb);
        bereich(m + std::min(ay, ay + by) - rand, m + std::max(ay, ay + by) + rand, ya, yb);
        for (int y = ya; y <= yb; ++y)
            for (int x = xa; x <= xb; ++x) {
                const float px = x + 0.5f - m - ax, py = y + 0.5f - m - ay;
                const float t = begrenzt((px * bx + py * by) / l2);
                const float h = h0 + (h1 - h0) * t;
                const float d = std::hypot(px - t * bx, py - t * by) - h;
                const float c = begrenzt(0.5f - d / aa);
                if (c <= 0) continue;
                const size_t i = size_t(y) * g + x;
                e.fest[i] = std::max(e.fest[i], c);
                e.kern[i] = std::max(e.kern[i], c * begrenzt(-d / (0.45f * h)));
            }
    }
    for (const Sechseck &h : k.sechsecke) {
        if (h.rang > maxRang) continue;
        const float cx = h.x * R, cy = h.y * R, inkreis = h.radius * R * 0.8660254f;
        if (inkreis < 0.6f) continue;
        const float rb = std::max(h.rand * R, 1.0f + weich);
        int xa, xb, ya, yb;
        bereich(m + cx - h.radius * R - aa, m + cx + h.radius * R + aa, xa, xb);
        bereich(m + cy - h.radius * R - aa, m + cy + h.radius * R + aa, ya, yb);
        for (int y = ya; y <= yb; ++y)
            for (int x = xa; x <= xb; ++x) {
                const float qx = x + 0.5f - m, qy = y + 0.5f - m;
                const float px = std::abs(qx - cx), py = std::abs(qy - cy);
                const float d = std::max(py, px * 0.8660254f + py * 0.5f) - inkreis; // Ecken auf den Achsen
                const float cf = begrenzt(0.5f - d / aa);
                if (cf <= 0) continue;
                const size_t i = size_t(y) * g + x;
                // Glanz: zur einen Seite hin heller, wie eine spiegelnde Eisfläche
                const float glanz = begrenzt(0.5f - 0.35f * (qx + qy) / R);
                e.durch[i] *= 1.f - h.fuellung * cf; // Deckkraft bleibt symmetrisch, der Glanz steckt in der Farbe
                e.hell[i] = std::max(e.hell[i], glanz * cf);
                const float dr = std::abs(d + rb * 0.5f) - rb * 0.5f; // heller Rand
                const float cr = begrenzt(0.5f - dr / aa);
                e.fest[i] = std::max(e.fest[i], cr);
                e.kern[i] = std::max(e.kern[i], cr * begrenzt(-dr / (0.45f * rb * 0.5f)));
            }
    }
    Bild b;
    aufBild(e, b);
    return b;
}

Bild pulverRastern(uint32_t saat, int g) {
    Zufall z(saat ^ 0x51ed270bu);
    Ebenen e(g);
    const float m = g / 2.f, R = std::max(1.f, m - 2.f);
    // ein paar verschieden große Brocken entlang einer schiefen Achse, dazu Krümel
    const float achse = z.bereich(0, 2 * PI);
    const int n = z.ganz(3, 5);
    for (int j = 0; j < n + 3; ++j) {
        const bool kruemel = j >= n;
        const float lage = kruemel ? z.bereich(-0.8f, 0.8f) : z.bereich(-0.5f, 0.5f);
        const float quer = kruemel ? z.bereich(-0.6f, 0.6f) : z.bereich(-0.18f, 0.18f);
        const float cx = (std::cos(achse) * lage - std::sin(achse) * quer) * R;
        const float cy = (std::sin(achse) * lage + std::cos(achse) * quer) * R;
        const float r = (kruemel ? z.bereich(0.08f, 0.16f) : z.bereich(0.24f, 0.42f)) * R;
        const float kraft = kruemel ? 0.7f : 0.92f;
        for (int y = 0; y < g; ++y)
            for (int x = 0; x < g; ++x) {
                const float d = std::hypot(x + 0.5f - m - cx, y + 0.5f - m - cy) / std::max(0.5f, r);
                if (d >= 1.25f) continue;
                const float t = begrenzt((1.25f - d) / 0.75f);
                const float c = kraft * t * t * (3.f - 2.f * t);
                const size_t i = size_t(y) * g + x;
                e.fest[i] = std::max(e.fest[i], c);
                e.kern[i] = std::max(e.kern[i], c * begrenzt(1.5f - 1.3f * d));
            }
    }
    Bild b;
    aufBild(e, b);
    return b;
}

Bild punktRastern(int g) {
    Ebenen e(g);
    const float m = g / 2.f, R = std::max(1.f, m - 1.5f);
    for (int y = 0; y < g; ++y)
        for (int x = 0; x < g; ++x) {
            const float d = std::hypot(x + 0.5f - m, y + 0.5f - m) / R;
            const float t = begrenzt((1.f - d) / 0.55f);
            const size_t i = size_t(y) * g + x;
            e.fest[i] = t * t * (3.f - 2.f * t);
            e.kern[i] = e.fest[i] * 0.85f;
        }
    Bild b;
    aufBild(e, b);
    return b;
}

namespace {
int feldGroesse(double logisch, double dpr) {
    int g = int(std::ceil(logisch * dpr - 1e-9));
    return g + (g & 1); // gerade: Mitte liegt auf einer Pixelgrenze, Spiegelsymmetrie bleibt exakt
}
const int WEICH_LOGISCH = 32, PULVER_LOGISCH = 16, PULVER_ANZAHL = 8;
}

AtlasPlan atlasPlanen(double dpr) {
    AtlasPlan p;
    p.dpr = std::clamp(dpr, 1.0, 4.0);
    p.varianten = 16;
    p.stufen = {8, 12, 16, 24, 32, 48, 64};
    int y = 0;
    auto zeile = [&](int anzahl, int g, std::vector<AtlasFeld> &ziel) {
        for (int i = 0; i < anzahl; ++i) ziel.push_back({i * g, y, g});
        p.breite = std::max(p.breite, anzahl * g);
        y += g;
    };
    for (double s : p.stufen) zeile(p.varianten, feldGroesse(s, p.dpr), p.kristalle);
    zeile(p.varianten, feldGroesse(WEICH_LOGISCH, p.dpr), p.weich);
    std::vector<AtlasFeld> klein;
    zeile(PULVER_ANZAHL + 1, feldGroesse(PULVER_LOGISCH, p.dpr), klein);
    p.punkt = klein.back();
    klein.pop_back();
    p.pulver = klein;
    p.hoehe = y;
    return p;
}

int AtlasPlan::stufeFuer(double durchmesser) const {
    for (size_t s = 0; s < stufen.size(); ++s)
        if (stufen[s] >= durchmesser - 1e-9) return int(s);
    return int(stufen.size()) - 1;
}

Bild atlasErzeugen(const AtlasPlan &p) {
    Bild atlas;
    atlas.breite = p.breite;
    atlas.hoehe = p.hoehe;
    atlas.rgba.assign(size_t(p.breite) * p.hoehe * 4, 0);
    auto setzen = [&](const AtlasFeld &f, const Bild &b) {
        for (int y = 0; y < f.groesse; ++y)
            std::copy_n(b.pixel(0, y), size_t(f.groesse) * 4, &atlas.rgba[(size_t(f.y + y) * p.breite + f.x) * 4]);
    };
    for (int v = 0; v < p.varianten; ++v) {
        const Kristall k = kristallErzeugen(uint32_t(v));
        for (size_t s = 0; s < p.stufen.size(); ++s) {
            const AtlasFeld &f = p.kristall(int(s), v);
            setzen(f, kristallRastern(k, f.groesse));
        }
        const AtlasFeld &w = p.weich[size_t(v)];
        setzen(w, kristallRastern(k, w.groesse, w.groesse * 0.03f));
    }
    for (size_t i = 0; i < p.pulver.size(); ++i) setzen(p.pulver[i], pulverRastern(uint32_t(i), p.pulver[i].groesse));
    setzen(p.punkt, punktRastern(p.punkt.groesse));
    return atlas;
}

} // namespace schnee
