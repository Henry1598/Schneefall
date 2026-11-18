#include "ansicht3d.h"
#include <algorithm>
#include <cmath>

namespace schnee {
namespace {
const double PI = 3.14159265358979;
P3 plus(P3 a, P3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
P3 minus(P3 a, P3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
P3 mal(P3 a, double f) { return {a.x * f, a.y * f, a.z * f}; }
P3 kreuz(P3 a, P3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double punkt(P3 a, P3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
P3 einheit(P3 a) {
    const double l = std::sqrt(punkt(a, a));
    return l > 1e-12 ? P3{a.x / l, a.y / l, a.z / l} : P3{0, 1, 0};
}
double weich(double a, double b, double x) {
    const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3 - 2 * t);
}
double bruch(double x) { return x - std::floor(x); }

// Werterauschen 0 … 1 (fest, ohne Zufallszahlen: jedes Bild sieht gleich aus)
double streu(int x, int y, int z) {
    uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + uint32_t(z) * 2147483647u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xffff) / 65535.0;
}
double rauschen(P3 p) {
    const int x = int(std::floor(p.x)), y = int(std::floor(p.y)), z = int(std::floor(p.z));
    const double fx = p.x - x, fy = p.y - y, fz = p.z - z;
    const double ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy), uz = fz * fz * (3 - 2 * fz);
    auto l = [](double a, double b, double t) { return a + (b - a) * t; };
    return l(l(l(streu(x, y, z), streu(x + 1, y, z), ux), l(streu(x, y + 1, z), streu(x + 1, y + 1, z), ux), uy),
             l(l(streu(x, y, z + 1), streu(x + 1, y, z + 1), ux), l(streu(x, y + 1, z + 1), streu(x + 1, y + 1, z + 1), ux), uy), uz);
}

struct Farbe { double r = 0, g = 0, b = 0; };
Farbe aus(uint32_t argb) { return {((argb >> 16) & 255) / 255.0, ((argb >> 8) & 255) / 255.0, (argb & 255) / 255.0}; }
Farbe misch(Farbe a, Farbe b, double t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
Farbe mal(Farbe a, double f) { return {a.r * f, a.g * f, a.b * f}; }

// Licht im Bildraum: von oben, links und etwas von vorn (weltfest, dreht nicht mit dem Fahrzeug)
const P3 LICHT = einheit({-0.5, 0.78, 0.42});
const P3 BLICK{0, std::sin(KAMERA_NEIGUNG), std::cos(KAMERA_NEIGUNG)};

// winzige Punktschrift für die Kennung am Hubschrauber (5 × 7)
bool schriftpunkt(char c, int x, int y) {
    static const char *const D[7] = {"1111.", "1...1", "1...1", "1...1", "1...1", "1...1", "1111."};
    static const char *const H[7] = {"1...1", "1...1", "1...1", "11111", "1...1", "1...1", "1...1"};
    static const char *const S[7] = {".1111", "1....", "1....", ".111.", "....1", "....1", "1111."};
    static const char *const N[7] = {"1...1", "11..1", "1.1.1", "1.1.1", "1..11", "1...1", "1...1"};
    static const char *const E[7] = {"11111", "1....", "1....", "1111.", "1....", "1....", "11111"};
    static const char *const M[7] = {".....", ".....", ".....", "11111", ".....", ".....", "....."};
    const char *const *g = c == 'D' ? D : c == 'H' ? H : c == 'S' ? S : c == 'N' ? N : c == 'E' ? E : M;
    return x >= 0 && x < 5 && y >= 0 && y < 7 && g[y][x] == '1';
}
bool kennung(double x, double y) { // Text „D-HSNE“, x/y in Schriftpunkten ab links oben
    static const char text[] = "D-HSNE";
    const int spalte = int(std::floor(x / 6));
    if (x < 0 || spalte < 0 || spalte >= 6) return false;
    return schriftpunkt(text[spalte], int(std::floor(x - spalte * 6)), int(std::floor(y)));
}

struct Stelle {
    Farbe farbe;
    double alpha = 1;
    Stoff stoff = Stoff::Lack;
    double glanz = 0;      // zusätzlicher Glanz (z. B. blanke Kanten)
};

// Farbe und Werkstoff an einer Stelle des Teils (p, n im Modell)
Stelle stelleVon(const Teilinfo &t, P3 p, P3 &n, double u) {
    Stelle s;
    s.farbe = aus(t.farbe);
    s.stoff = t.stoff;
    s.alpha = ((t.farbe >> 24) & 255) / 255.0;
    const P3 r{p.x - t.mitte.x, p.y - t.mitte.y, p.z - t.mitte.z};
    switch (t.muster) {
    case Muster::Keins: break;
    case Muster::Kette:
        if (std::abs(n.z) > 0.7) { s.farbe = aus(0xff1c1e23); s.stoff = Stoff::Matt; break; }
        s.farbe = aus(kettenMuster(u, t.phase));
        s.stoff = Stoff::Metall;
        break;
    case Muster::Rolle: {
        // Stirnseite: Felgenring, Löcher, Nabe; dreht sich mit der Phase
        if (std::abs(n.z) < 0.7) { s.farbe = aus(0xff3a3f48); break; }
        const double rad = std::hypot(r.x, r.y) / std::max(1e-6, t.halb.x), w = std::atan2(r.y, r.x) + t.phase;
        s.farbe = aus(0xff8f97a3);
        if (rad > 0.84) s.farbe = aus(0xff4a505a);
        else if (rad < 0.26) s.farbe = aus(0xfff3ac1c);
        else if (rad > 0.42 && rad < 0.72 && t.halb.x > 4 && bruch(w * t.teilung / (2 * PI)) < 0.42) { s.farbe = aus(0xff17191d); s.stoff = Stoff::Matt; }
        else if (t.halb.x <= 4 && rad > 0.5 && rad < 0.66) s.farbe = aus(0xff6c737e);
        break;
    }
    case Muster::Reifen: {
        const double rad = std::hypot(r.x, r.y), w = std::atan2(r.y, r.x) + t.phase;
        if (rad > 3.75) {
            s.stoff = Stoff::Gummi;
            if (std::abs(n.z) < 0.75) {
                // Stollen: versetzt links und rechts der Mitte
                const double versatz = r.z > 0 ? 0.5 : 0.0;
                s.farbe = bruch(w * 12 / (2 * PI) + versatz + std::abs(r.z) * 0.12) < 0.56 ? aus(0xff30333a) : aus(0xff0b0c0e);
            } else {
                s.farbe = rad > 5.0 ? aus(0xff17181c) : aus(0xff202227);
                if (std::abs(rad - 4.55) < 0.12) s.farbe = aus(0xff34373e); // Schriftwulst
            }
        } else {
            s.stoff = Stoff::Metall;
            s.farbe = aus(0xffc9cfd8);
            const double teil = bruch(w * 6 / (2 * PI));
            if (rad > 1.75 && rad < 3.0 && std::hypot((teil - 0.5) * 2.4, rad - 2.4) < 0.55) { s.farbe = aus(0xff15161a); s.stoff = Stoff::Matt; }
            if (rad < 1.15) s.farbe = aus(0xfff0681a);
            else if (rad < 1.6 && bruch(w * 6 / (2 * PI) + 0.5) < 0.3) s.farbe = aus(0xff50555e); // Radmuttern
            if (rad > 3.45) s.farbe = aus(0xff8a919c);
        }
        break;
    }
    case Muster::Kuehler:
        s.farbe = bruch(p.y * 0.9) < 0.45 ? aus(0xff0e0f12) : aus(0xff3b4048);
        s.stoff = Stoff::Matt;
        break;
    case Muster::Warnstreifen:
        if (std::abs(n.x) > 0.7) s.farbe = bruch((p.y + p.z) * 0.2) < 0.5 ? aus(0xfff6c21e) : aus(0xff1b1c20);
        break;
    case Muster::Jacke:
        if (std::abs(r.y + t.halb.y * 0.15) < t.halb.y * 0.11 || std::abs(r.y + t.halb.y * 0.5) < t.halb.y * 0.09) { s.farbe = aus(0xffe9ecc6); s.glanz = 0.5; }
        break;
    case Muster::Gesicht: {
        // schaut nach vorn (+x): Augen, Brauen, Bart
        const double rx = t.halb.x;
        if (r.x > rx * 0.45) {
            if (std::hypot(r.y - rx * 0.12, std::abs(r.z) - rx * 0.36) < rx * 0.13) s.farbe = aus(0xff1c1a1a);
            else if (std::abs(r.y - rx * 0.36) < rx * 0.06 && std::abs(std::abs(r.z) - rx * 0.36) < rx * 0.24) s.farbe = aus(0xff5a3a26);
        }
        if (r.y < -rx * 0.28 && r.x > -rx * 0.2) s.farbe = misch(s.farbe, aus(0xff6a4830), 0.85);
        break;
    }
    case Muster::Schneide: {
        // blankes Schild: senkrechte Schleifspuren, unten abgewetzt
        const double spur = rauschen({p.z * 1.7, p.y * 0.15, 3.0});
        s.farbe = mal(s.farbe, 0.86 + 0.24 * spur);
        s.glanz = 0.25 * spur;
        break;
    }
    case Muster::Raupenhaube:
        // Lüftungsschlitze an den Haubenseiten
        if (std::abs(n.z) > 0.7 && p.x > 7 && p.x < 22.5 && p.y > 18.6 && p.y < 26.2 && bruch(p.x * 0.42) < 0.34) { s.farbe = aus(0xff3a2a08); s.stoff = Stoff::Matt; }
        break;
    case Muster::HeliRumpf: {
        // rot, weißer Zierstreifen, dunkler Bauch, Türfuge und Seitenfenster
        const bool seite = std::abs(n.z) > 0.35;
        if (p.y > 9.2 && p.y < 13.4) s.farbe = aus(0xfff4f4f4);
        else if (p.y < 4.5) s.farbe = mal(s.farbe, 0.72);
        if (seite && p.x > -11 && p.x < 9.5 && p.y > 4 && p.y < 32) {
            const double dx = std::max(std::abs(p.x + 0.75) - 9.4, 0.0), dy = std::max(std::abs(p.y - 18) - 13.2, 0.0);
            const double rand = std::hypot(dx, dy);
            const double innen = std::min(9.4 + 0.6 - std::abs(p.x + 0.75), 13.2 + 0.6 - std::abs(p.y - 18));
            if (rand < 0.6 && innen < 0.42) s.farbe = mal(s.farbe, 0.55);   // Türfuge
            // Seitenfenster mit runden Ecken
            const double fx = std::max(std::abs(p.x + 1) - 5.2, 0.0), fy = std::max(std::abs(p.y - 24.6) - 3.6, 0.0);
            if (std::hypot(fx, fy) < 1.6) { s.farbe = misch(aus(0xff8fb6dc), aus(0xff1a2c44), weich(29, 20, p.y)); s.stoff = Stoff::Glas; }
            else if (std::hypot(fx, fy) < 2.1) s.farbe = aus(0xff1a1c22);
            if (std::abs(p.x - 8.3) < 0.45 && std::abs(p.y - 17) < 1.3) { s.farbe = aus(0xffc8ced6); s.stoff = Stoff::Metall; } // Türgriff
        }
        // Lufteinlass der Turbine oben
        if (p.y > 30.5 && p.x > 9 && p.x < 16 && std::abs(p.z) < 4.2) { s.farbe = aus(0xff101216); s.stoff = Stoff::Matt; }
        break;
    }
    case Muster::HeliAusleger: {
        if (std::abs(p.y - 19.2) < 0.95) s.farbe = aus(0xfff4f4f4);
        // Kennung auf beiden Seiten
        if (std::abs(n.z) > 0.5 && p.x > -62 && p.x < -30) {
            const double sx = n.z > 0 ? (p.x + 60) / 0.72 : (-32 - p.x) / 0.72 + 3;
            if (kennung(sx, (24.6 - p.y) / 0.62 + 0.2)) s.farbe = aus(0xfffafafa);
        }
        break;
    }
    case Muster::Rotorscheibe: {
        // Heckrotor: durchscheinende Scheibe mit verwischten Blättern
        const double rad = std::hypot(r.x, r.y) / std::max(1e-6, t.halb.x), w = std::atan2(r.y, r.x);
        double blatt = 0;
        for (int i = 0; i < 2; ++i) {
            double d = std::remainder(w - 0.5 - i * PI, 2 * PI);
            blatt = std::max(blatt, std::exp(-d * d * 9.0));
        }
        s.farbe = aus(0xff30343c);
        s.alpha = rad > 1 ? 0 : (0.13 + 0.42 * blatt) * weich(1.0, 0.86, rad);
        if (rad > 0.86 && rad < 0.97 && blatt > 0.4) s.farbe = aus(0xffe8c030);
        break;
    }
    case Muster::Pritsche:
        // Bordwände: senkrechte Sicken
        if (std::abs(n.y) < 0.5 && bruch((std::abs(n.z) > 0.5 ? p.x : p.z) * 0.36) < 0.12) s.farbe = mal(s.farbe, 0.7);
        break;
    }
    // gerundete Kanten: die Normale biegt sich zur Kante hin, dort ist der Lack leicht abgewetzt
    if (t.rundung > 0) {
        const P3 q{std::max(std::abs(r.x) - (t.halb.x - t.rundung), 0.0), std::max(std::abs(r.y) - (t.halb.y - t.rundung), 0.0),
                   std::max(std::abs(r.z) - (t.halb.z - t.rundung), 0.0)};
        const int kanten = (q.x > 0) + (q.y > 0) + (q.z > 0);
        if (kanten >= 2) {
            n = einheit({q.x * (r.x < 0 ? -1 : 1), q.y * (r.y < 0 ? -1 : 1), q.z * (r.z < 0 ? -1 : 1)});
            if (s.stoff == Stoff::Lack) { s.farbe = misch(s.farbe, {1, 1, 1}, 0.10); s.glanz += 0.15; }
        }
    }
    return s;
}

// Beleuchtung einer Stelle; nb = Normale im Bildraum
Farbe beleuchten(const Teilinfo &t, const Stelle &st, P3 p, P3 n, P3 nb, double verdeckung) {
    Farbe f = st.farbe;
    Stoff stoff = st.stoff;
    const bool fest = stoff == Stoff::Lack || stoff == Stoff::Metall || stoff == Stoff::Matt || stoff == Stoff::Tuch;
    // Schmutz und Spritzer unten am Fahrzeug
    if (t.schmutz > 0 && fest) {
        const double dreck = t.schmutz * weich(11, 1.5, p.y) * (0.35 + 0.65 * rauschen({p.x * 0.5, p.y * 0.8, p.z * 0.5}));
        f = misch(f, aus(0xff4a4238), 0.42 * dreck);
    }
    // Schnee auf waagerechten Flächen
    double schnee = 0;
    if (t.schnee > 0 && stoff != Stoff::Licht && stoff != Stoff::Scheibe) {
        // fleckig: der Wind hat nicht überall gleich viel liegen lassen
        const double flocken = 0.65 * rauschen({p.x * 0.3, 7.0, p.z * 0.3}) + 0.35 * rauschen({p.x * 0.9, 3.0, p.z * 0.9});
        const double decke = weich(0.38, 0.62, flocken + t.schnee - 0.5);
        schnee = weich(0.78, 0.96, n.y) * decke * (stoff == Stoff::Glas ? 0.6 : 1.0);
        f = misch(f, aus(0xfff4f7fc), schnee);
    }
    if (stoff == Stoff::Licht) return mal(f, 1.15);
    const double streu = std::max(0.0, punkt(nb, LICHT));
    const double himmel = 0.34 + 0.2 * nb.y;            // Umgebungslicht: von oben heller
    double hell = (himmel + 0.78 * streu) * verdeckung;
    // Glanzlicht je Werkstoff
    const P3 halb = einheit(plus(LICHT, BLICK));
    const double nh = std::max(0.0, punkt(nb, halb));
    double staerke = 0, haerte = 20;
    switch (stoff) {
    case Stoff::Lack: staerke = 0.42; haerte = 46; break;
    case Stoff::Metall: staerke = 0.6; haerte = 22; break;
    case Stoff::Gummi: staerke = 0.07; haerte = 8; break;
    case Stoff::Glas: staerke = 0.95; haerte = 90; break;
    case Stoff::Haut: staerke = 0.1; haerte = 12; break;
    default: break;
    }
    staerke = (staerke + st.glanz) * (1 - schnee) * verdeckung;
    const double glanz = staerke * std::pow(nh, haerte);
    // Kantenlicht: streifende Flächen fangen etwas Himmelslicht
    const double streif = std::pow(1 - std::max(0.0, punkt(nb, BLICK)), 3) * (stoff == Stoff::Glas ? 0.5 : 0.16) * (0.5 + 0.5 * nb.y + 0.5);
    if (stoff == Stoff::Glas) {
        // Glas spiegelt oben den hellen Himmel, unten ist es dunkel
        f = misch(f, aus(0xffcfe6fa), 0.35 * weich(-0.2, 0.9, nb.y));
    }
    Farbe e = mal(f, hell);
    e.r += glanz + streif * 0.8;
    e.g += glanz + streif * 0.9;
    e.b += glanz + streif;
    return {std::min(1.0, e.r), std::min(1.0, e.g), std::min(1.0, e.b)};
}
} // namespace

uint32_t kettenMuster(double u, double phase) {
    // je Glied: breite Platte mit hellem Steg (greift in den Schnee), dann die dunkle Fuge
    const double s = bruch(u / KETTEN_TEILUNG - phase);
    if (s > 0.66) return 0xff101114;
    if (s > 0.24 && s < 0.42) return 0xffaab2be;
    return 0xff5a616c;
}

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

int winkelstufe(double gier) {
    const double schritt = 2 * PI / WINKELSTUFEN;
    int i = int(std::lround(gier / schritt)) % WINKELSTUFEN;
    return i < 0 ? i + WINKELSTUFEN : i;
}

double stufenWinkel(int stufe) { return stufe * 2 * PI / WINKELSTUFEN; }

// ---- Körper ----
uint16_t Modell::teil(const std::string &name, Stoff stoff, uint32_t farbe, Muster muster) {
    Teilinfo t;
    t.name = name;
    t.stoff = stoff;
    t.farbe = farbe;
    t.muster = muster;
    t.schnee = stoff == Stoff::Gummi ? 0 : schnee;
    t.schmutz = schmutz;
    teile.push_back(std::move(t));
    return uint16_t(teile.size() - 1);
}

void Modell::dreieck(uint16_t t, Ecke a, Ecke b, Ecke c) {
    // Umlaufsinn so, dass die Flächennormale zu den Eckennormalen passt (außen)
    const P3 g = kreuz(minus(b.p, a.p), minus(c.p, a.p));
    if (punkt(g, g) < 1e-14) return;
    if (punkt(g, plus(plus(a.n, b.n), c.n)) < 0) std::swap(b, c);
    dreiecke.push_back({{a, b, c}, t});
}

void Modell::quader(uint16_t t, double x0, double x1, double y0, double y1, double z0, double z1, double rundung) {
    Teilinfo &info = teile[t];
    info.mitte = {(x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2};
    info.halb = {std::abs(x1 - x0) / 2, std::abs(y1 - y0) / 2, std::abs(z1 - z0) / 2};
    info.rundung = std::min({rundung, info.halb.x, info.halb.y, info.halb.z});
    auto seite = [&](P3 n, P3 a, P3 b, P3 c, P3 d) { viereck(t, {a, n}, {b, n}, {c, n}, {d, n}); };
    seite({1, 0, 0}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {x1, y0, z1});
    seite({-1, 0, 0}, {x0, y0, z0}, {x0, y1, z0}, {x0, y1, z1}, {x0, y0, z1});
    seite({0, 1, 0}, {x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1});
    seite({0, -1, 0}, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1});
    seite({0, 0, 1}, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1});
    seite({0, 0, -1}, {x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0});
}

void Modell::rohr(uint16_t t, P3 a, P3 b, double ra, double rb, int ecken, bool deckel) {
    const P3 achse = einheit(minus(b, a));
    const P3 quer = einheit(kreuz(achse, std::abs(achse.y) < 0.9 ? P3{0, 1, 0} : P3{1, 0, 0}));
    const P3 hoch = kreuz(achse, quer);
    const double laenge = std::sqrt(punkt(minus(b, a), minus(b, a)));
    if (teile[t].halb.x == 0) { teile[t].mitte = mal(plus(a, b), 0.5); teile[t].halb = {std::max(ra, rb), std::max(ra, rb), laenge / 2}; }
    auto rand = [&](P3 m, double r, int k) {
        const double w = 2 * PI * k / ecken;
        const P3 dir = plus(mal(quer, std::cos(w)), mal(hoch, std::sin(w)));
        // Mantelnormale: bei Kegeln leicht zur Spitze geneigt
        const P3 n = einheit(plus(dir, mal(achse, (ra - rb) / std::max(1e-9, laenge))));
        return Ecke{plus(m, mal(dir, r)), n, w};
    };
    for (int k = 0; k < ecken; ++k) {
        Ecke a0 = rand(a, ra, k), a1 = rand(a, ra, k + 1), b0 = rand(b, rb, k), b1 = rand(b, rb, k + 1);
        a1.u = b1.u = 2 * PI * (k + 1) / ecken;
        viereck(t, a0, a1, b1, b0);
        if (deckel) {
            if (rb > 1e-6) dreieck(t, {b, achse, 0}, {b0.p, achse, 0}, {b1.p, achse, 0});
            if (ra > 1e-6) dreieck(t, {a, mal(achse, -1), 0}, {a0.p, mal(achse, -1), 0}, {a1.p, mal(achse, -1), 0});
        }
    }
}

void Modell::ellipsoid(uint16_t t, P3 mitte, P3 r, int ecken, double von, double bis) {
    if (teile[t].halb.x == 0) { teile[t].mitte = mitte; teile[t].halb = r; }
    const int ringe = std::max(3, ecken / 2);
    const double b0 = std::asin(std::clamp(von, -1.0, 1.0)), b1 = std::asin(std::clamp(bis, -1.0, 1.0));
    auto ecke = [&](int i, int k) {
        const double br = b0 + (b1 - b0) * i / ringe, w = 2 * PI * k / ecken;
        const P3 e{std::cos(br) * std::cos(w), std::sin(br), std::cos(br) * std::sin(w)};
        return Ecke{{mitte.x + e.x * r.x, mitte.y + e.y * r.y, mitte.z + e.z * r.z}, einheit({e.x / r.x, e.y / r.y, e.z / r.z}), w};
    };
    for (int i = 0; i < ringe; ++i)
        for (int k = 0; k < ecken; ++k) viereck(t, ecke(i, k), ecke(i, k + 1), ecke(i + 1, k + 1), ecke(i + 1, k));
    // offenes Band unten schließen (z. B. Helmrand)
    if (von > -0.999) {
        const P3 n{0, -1, 0};
        const P3 m{mitte.x, mitte.y + std::sin(b0) * r.y, mitte.z};
        for (int k = 0; k < ecken; ++k) dreieck(t, {m, n, 0}, {ecke(0, k).p, n, 0}, {ecke(0, k + 1).p, n, 0});
    }
}

void Modell::schale(uint16_t t, const std::vector<Spant> &sp, int ecken, double w0, double w1) {
    const bool voll = w1 - w0 > 2 * PI - 1e-6;
    auto ecke = [&](size_t i, int k) {
        const Spant &s = sp[i];
        const double w = w0 + (w1 - w0) * k / ecken;
        // Normale: aus der Ellipse und der Verjüngung zu den Nachbarspanten
        const Spant &v = sp[i > 0 ? i - 1 : i], &n = sp[i + 1 < sp.size() ? i + 1 : i];
        const double dx = std::max(1e-6, n.x - v.x);
        const double steigung = ((n.ry - v.ry) * std::abs(std::sin(w)) + (n.rz - v.rz) * std::abs(std::cos(w))) / dx + (n.y - v.y) * std::sin(w) / dx;
        const P3 quer = einheit({0, std::sin(w) / std::max(1e-6, s.ry), std::cos(w) / std::max(1e-6, s.rz)});
        return Ecke{{s.x, s.y + std::sin(w) * s.ry, std::cos(w) * s.rz}, einheit({-steigung, quer.y, quer.z}), w};
    };
    for (size_t i = 0; i + 1 < sp.size(); ++i)
        for (int k = 0; k < ecken; ++k) viereck(t, ecke(i, k), ecke(i + 1, k), ecke(i + 1, k + 1), ecke(i, k + 1));
    if (!voll) return;
    for (size_t i : {size_t(0), sp.size() - 1}) {
        const P3 n{i == 0 ? -1.0 : 1.0, 0, 0};
        const P3 m{sp[i].x, sp[i].y, 0};
        for (int k = 0; k < ecken; ++k) dreieck(t, {m, n, 0}, {ecke(i, k).p, n, 0}, {ecke(i, k + 1).p, n, 0});
    }
}

void Modell::prisma(uint16_t t, const std::vector<P2> &profil, double z0, double z1, bool glatt, bool deckel) {
    const size_t n = profil.size();
    if (n < 3) return;
    // Umlaufsinn des Profils (für die Außenrichtung der Kanten)
    double flaeche = 0;
    for (size_t i = 0; i < n; ++i) flaeche += profil[i].x * profil[(i + 1) % n].y - profil[(i + 1) % n].x * profil[i].y;
    const double sinn = flaeche >= 0 ? 1 : -1;
    auto kante = [&](size_t i) { // Außennormale der Kante i → i+1
        const P2 &a = profil[i % n], &b = profil[(i + 1) % n];
        const double l = std::max(1e-9, std::hypot(b.x - a.x, b.y - a.y));
        return P3{sinn * (b.y - a.y) / l, -sinn * (b.x - a.x) / l, 0};
    };
    double u = 0;
    for (size_t i = 0; i < n; ++i) {
        const P2 &a = profil[i], &b = profil[(i + 1) % n];
        const double l = std::hypot(b.x - a.x, b.y - a.y);
        const P3 na = glatt ? einheit(plus(kante(i + n - 1), kante(i))) : kante(i), nb = glatt ? einheit(plus(kante(i), kante(i + 1))) : kante(i);
        viereck(t, {{a.x, a.y, z1}, na, u}, {{a.x, a.y, z0}, na, u}, {{b.x, b.y, z0}, nb, u + l}, {{b.x, b.y, z1}, nb, u + l});
        u += l;
    }
    if (!deckel) return;
    P2 m;
    for (const P2 &p : profil) { m.x += p.x / n; m.y += p.y / n; }
    for (double z : {z0, z1}) {
        const P3 nn{0, 0, z == std::max(z0, z1) ? 1.0 : -1.0};
        for (size_t i = 0; i < n; ++i) {
            const P2 &a = profil[i], &b = profil[(i + 1) % n];
            dreieck(t, {{m.x, m.y, z}, nn, 0}, {{a.x, a.y, z}, nn, 0}, {{b.x, b.y, z}, nn, 0});
        }
    }
}

void Modell::drehkoerper(uint16_t t, P3 mitte, const std::vector<P2> &profil, int ecken) {
    if (teile[t].halb.x == 0) {
        double r = 0, z = 0;
        for (const P2 &p : profil) { r = std::max(r, p.x); z = std::max(z, std::abs(p.y)); }
        teile[t].mitte = mitte;
        teile[t].halb = {r, r, z};
    }
    const size_t n = profil.size();
    auto normale = [&](size_t i) { // im Profil (Radius, z): senkrecht zur Linie, gemittelt
        const P2 &a = profil[i > 0 ? i - 1 : i], &b = profil[i + 1 < n ? i + 1 : i];
        const double dr = b.x - a.x, dz = b.y - a.y, l = std::max(1e-9, std::hypot(dr, dz));
        return P2{dz / l, -dr / l};
    };
    // Außenrichtung: weg von der Achse bzw. von der Mitte
    auto ecke = [&](size_t i, int k) {
        const double w = 2 * PI * k / ecken;
        P2 nn = normale(i);
        const P2 &p = profil[i];
        if (nn.x * p.x + nn.y * p.y < 0) { nn.x = -nn.x; nn.y = -nn.y; }
        return Ecke{{mitte.x + std::cos(w) * p.x, mitte.y + std::sin(w) * p.x, mitte.z + p.y}, einheit({std::cos(w) * nn.x, std::sin(w) * nn.x, nn.y}), w};
    };
    for (size_t i = 0; i + 1 < n; ++i)
        for (int k = 0; k < ecken; ++k) viereck(t, ecke(i, k), ecke(i, k + 1), ecke(i + 1, k + 1), ecke(i + 1, k));
}

// ---- Rastern ----
Ansichtsbild rendern(const Modell &m, double gier, double s, int w, int h, double ox, double oy, int ab) {
    Ansichtsbild bild;
    bild.w = w;
    bild.h = h;
    bild.rgba.assign(size_t(w) * h * 4, 0);
    bild.teil.assign(size_t(w) * h, 0);
    const int W = w * ab, H = h * ab;
    struct Probe { float tiefe = -1e30f; uint16_t teil = 0xffff; float px, py, pz, nx, ny, nz, u; };
    std::vector<Probe> puffer(size_t(W) * H);
    struct Glas { float r = 0, g = 0, b = 0, a = 0; };
    const double c = std::cos(gier), sn = std::sin(gier);
    const double ce = std::cos(KAMERA_NEIGUNG), se = std::sin(KAMERA_NEIGUNG);
    struct Bildecke { double x, y, t; };
    auto ins = [&](P3 p) {
        const double bx = p.x * c - p.z * sn, bz = p.x * sn + p.z * c;
        return Bildecke{(ox + bx * s) * ab, (oy + (-p.y * ce + bz * se) * s) * ab, bz * ce + p.y * se};
    };
    auto durchsichtig = [&](const Teilinfo &t) { return t.stoff == Stoff::Scheibe || ((t.farbe >> 24) & 255) < 255; };

    // rastert ein Dreieck und ruft je getroffener Probe f(Index, Tiefe, Gewichte) auf
    auto rastern = [&](const Dreieck &d, bool &gewendet, auto &&f) {
        const Bildecke a = ins(d.e[0].p), b = ins(d.e[1].p), cc = ins(d.e[2].p);
        double fl = (b.x - a.x) * (cc.y - a.y) - (cc.x - a.x) * (b.y - a.y);
        // Bild-y zeigt nach unten: von außen gesehene Dreiecke haben hier negativen Umlaufsinn
        gewendet = fl > 0;
        if (std::abs(fl) < 1e-9 || (gewendet && !m.teile[d.teil].beidseitig)) return;
        const int x0 = std::max(0, int(std::floor(std::min({a.x, b.x, cc.x})))), x1 = std::min(W - 1, int(std::ceil(std::max({a.x, b.x, cc.x}))));
        const int y0 = std::max(0, int(std::floor(std::min({a.y, b.y, cc.y})))), y1 = std::min(H - 1, int(std::ceil(std::max({a.y, b.y, cc.y}))));
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const double px = x + 0.5, py = y + 0.5;
                const double w0 = ((b.x - px) * (cc.y - py) - (cc.x - px) * (b.y - py)) / fl;
                const double w1 = ((cc.x - px) * (a.y - py) - (a.x - px) * (cc.y - py)) / fl;
                const double w2 = 1 - w0 - w1;
                if (w0 < 0 || w1 < 0 || w2 < 0) continue;
                f(size_t(y) * W + x, w0 * a.t + w1 * b.t + w2 * cc.t, w0, w1, w2);
            }
    };
    auto stelleDer = [&](const Dreieck &d, double w0, double w1, double w2, bool gewendet, P3 &p, P3 &n, double &u) {
        p = plus(plus(mal(d.e[0].p, w0), mal(d.e[1].p, w1)), mal(d.e[2].p, w2));
        n = einheit(plus(plus(mal(d.e[0].n, w0), mal(d.e[1].n, w1)), mal(d.e[2].n, w2)));
        if (gewendet) n = mal(n, -1);
        u = w0 * d.e[0].u + w1 * d.e[1].u + w2 * d.e[2].u;
    };

    // 1. deckende Teile in den Tiefenpuffer
    for (const Dreieck &d : m.dreiecke) {
        if (durchsichtig(m.teile[d.teil])) continue;
        bool gewendet = false;
        rastern(d, gewendet, [&](size_t i, double t, double w0, double w1, double w2) {
            Probe &q = puffer[i];
            if (t <= q.tiefe) return;
            P3 p, n;
            double u;
            stelleDer(d, w0, w1, w2, gewendet, p, n, u);
            q = {float(t), d.teil, float(p.x), float(p.y), float(p.z), float(n.x), float(n.y), float(n.z), float(u)};
        });
    }
    // 2. beleuchten, mit Umgebungsverdeckung aus dem Tiefenpuffer (Ecken und Fugen werden dunkler)
    std::vector<Glas> farbe(size_t(W) * H);
    const int radius = std::max(1, int(std::lround(1.5 * s * ab)));
    static const int richtung[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const Probe &q = puffer[size_t(y) * W + x];
            if (q.teil == 0xffff) continue;
            double verdeckt = 0;
            for (const auto &r : richtung)
                for (int weit = 1; weit <= 2; ++weit) {
                    const int xx = x + r[0] * radius * weit, yy = y + r[1] * radius * weit;
                    if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
                    const Probe &o = puffer[size_t(yy) * W + xx];
                    if (o.teil == 0xffff) continue;
                    const double naeher = o.tiefe - q.tiefe;
                    if (naeher > 0.35 && naeher < 9) verdeckt += std::min(1.0, naeher / 2.5) / weit;
                }
            const double verdeckung = 1 - 0.5 * std::min(1.0, verdeckt / 6.5);
            const Teilinfo &t = m.teile[q.teil];
            P3 n{q.nx, q.ny, q.nz};
            const P3 p{q.px, q.py, q.pz};
            const Stelle st = stelleVon(t, p, n, q.u);
            const Farbe f = beleuchten(t, st, p, n, imBildraum(n, gier), verdeckung);
            farbe[size_t(y) * W + x] = {float(f.r), float(f.g), float(f.b), 1};
        }
    // 3. durchsichtige Teile (Glas, Rotorscheibe) von hinten nach vorn darüber
    std::vector<const Dreieck *> scheiben;
    for (const Dreieck &d : m.dreiecke)
        if (durchsichtig(m.teile[d.teil])) scheiben.push_back(&d);
    std::stable_sort(scheiben.begin(), scheiben.end(), [&](const Dreieck *a, const Dreieck *b) {
        auto t = [&](const Dreieck *d) { return tiefe(d->e[0].p, gier) + tiefe(d->e[1].p, gier) + tiefe(d->e[2].p, gier); };
        return t(a) < t(b);
    });
    std::vector<float> glasTiefe(size_t(W) * H, -1e30f);
    for (const Dreieck *d : scheiben) {
        bool gewendet = false;
        rastern(*d, gewendet, [&](size_t i, double t, double w0, double w1, double w2) {
            if (t <= puffer[i].tiefe) return;
            const Teilinfo &ti = m.teile[d->teil];
            P3 p, n;
            double u;
            stelleDer(*d, w0, w1, w2, gewendet, p, n, u);
            const Stelle st = stelleVon(ti, p, n, u);
            if (st.alpha <= 0) return;
            const Farbe f = beleuchten(ti, st, p, n, imBildraum(n, gier), 1);
            // Glanzlichter auf Glas decken stärker
            const double hellste = std::max({f.r, f.g, f.b});
            const double a = std::clamp(st.alpha + (st.stoff == Stoff::Glas ? std::max(0.0, hellste - 0.75) * 1.6 : 0.0), 0.0, 1.0);
            // Farben sind vormultipliziert: deckende Proben haben Alpha 1
            Glas &z = farbe[i];
            z.r = float(f.r * a + z.r * (1 - a));
            z.g = float(f.g * a + z.g * (1 - a));
            z.b = float(f.b * a + z.b * (1 - a));
            z.a = float(a + z.a * (1 - a));
            glasTiefe[i] = std::max(glasTiefe[i], float(t));
        });
    }
    // 4. dunkle Außenkante und Kanten an Tiefensprüngen, dann herunterrechnen
    auto belegt = [&](int x, int y) { return x >= 0 && y >= 0 && x < W && y < H && farbe[size_t(y) * W + x].a > 0.05f; };
    auto tiefeBei = [&](int x, int y) { return std::max(puffer[size_t(y) * W + x].tiefe, glasTiefe[size_t(y) * W + x]); };
    const int kante = std::max(1, ab / 2 + (s * ab > 4 ? 1 : 0));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            double r = 0, g = 0, b = 0, a = 0;
            for (int j = 0; j < ab; ++j)
                for (int i = 0; i < ab; ++i) {
                    const int X = x * ab + i, Y = y * ab + j;
                    const Glas &q = farbe[size_t(Y) * W + X];
                    if (q.a <= 0) continue;
                    double dunkel = 1;
                    const float t0 = tiefeBei(X, Y);
                    for (const auto &ri : richtung) {
                        const int xx = X + ri[0] * kante, yy = Y + ri[1] * kante;
                        if (!belegt(xx, yy)) { dunkel = std::min(dunkel, 0.45); break; }
                        if (tiefeBei(xx, yy) - t0 > 1.6) dunkel = std::min(dunkel, 0.72);
                    }
                    r += q.r * dunkel; g += q.g * dunkel; b += q.b * dunkel; a += q.a;
                }
            const double n = ab * ab;
            uint8_t *z = &bild.rgba[size_t(4 * (y * w + x))];
            z[0] = uint8_t(std::lround(std::min(1.0, r / n) * 255));
            z[1] = uint8_t(std::lround(std::min(1.0, g / n) * 255));
            z[2] = uint8_t(std::lround(std::min(1.0, b / n) * 255));
            z[3] = uint8_t(std::lround(std::min(1.0, a / n) * 255));
            const Probe &mitte = puffer[size_t(y * ab + ab / 2) * W + (x * ab + ab / 2)];
            bild.teil[size_t(y) * w + x] = mitte.teil == 0xffff ? 0 : uint16_t(mitte.teil + 1);
        }
    return bild;
}

std::map<std::string, int> abdeckung(const Modell &m, double gier) {
    // Rahmen großzügig um den Ursprung: reicht für alle Fahrzeuge
    const Ansichtsbild b = rendern(m, gier, 2.0, 480, 320, 240, 220, 1);
    std::map<std::string, int> a;
    for (uint16_t t : b.teil)
        if (t) ++a[m.teile[t - 1].name];
    return a;
}

} // namespace schnee
