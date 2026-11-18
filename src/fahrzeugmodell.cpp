#include "fahrzeugmodell.h"
#include <cmath>

namespace schnee {
namespace {
const double PI = 3.14159265358979;
constexpr uint32_t GELB = 0xfff3ac1c, GELB_HELL = 0xffffd64a, GELB_DUNKEL = 0xffb0700a, DUNKEL = 0xff2c3038, GRAU = 0xff767e8a,
                   WEISS = 0xffeceef2, ORANGE = 0xfff0681a, ORANGE_HELL = 0xffff963e, HAUT = 0xfff0c4a0, JACKE = 0xffff8c28,
                   HELM = 0xfff6f6f6, ROT = 0xffc42c26, LEUCHTE = 0xffffa63c, STAHL_DUNKEL = 0xff3a4048, SITZ = 0xff1c1e24;

void textur(Modell &m, size_t flaeche, int bild, double tu, double tv) {
    m.flaechen[flaeche].textur = bild;
    m.flaechen[flaeche].tu = tu;
    m.flaechen[flaeche].tv = tv;
}
// beide Seitenflächen eines Quaders mit dem Seitenbild belegen
size_t quaderMitSeiten(Modell &m, double x0, double x1, double y0, double y1, double z0, double z1, uint32_t farbe, int bild, double tu, double tv) {
    const size_t q = m.quader(x0, x1, y0, y1, z0, z1, farbe);
    textur(m, q + 4, bild, tu, tv);
    textur(m, q + 5, bild, tu, tv);
    return q;
}
P2 gedreht(P2 p, P2 um, double winkel) {
    const double c = std::cos(winkel), s = std::sin(winkel), dx = p.x - um.x, dy = p.y - um.y;
    return {um.x + c * dx - s * dy, um.y + s * dx + c * dy};
}
// Punkte einer quadratischen Kurve a → b mit Stützpunkt k (ohne Endpunkt)
void kurve(std::vector<P2> &aus, P2 a, P2 k, P2 b, int schritte) {
    for (int i = 0; i < schritte; ++i) {
        const double t = double(i) / schritte, u = 1 - t;
        aus.push_back({u * u * a.x + 2 * u * t * k.x + t * t * b.x, u * u * a.y + 2 * u * t * k.y + t * t * b.y});
    }
}
// gewölbtes Schild: Profil aus dem Seitenbild (dort Drehpunkt (px, py)), am Fahrzeug bei (ax, ay), um „hub“ angehoben
void schild(Modell &m, const std::vector<P2> &ruecken, const std::vector<P2> &vorn, P2 bildDreh, P2 anbau, double hub, double halbeBreite,
            uint32_t rueckenFarbe) {
    auto ins = [&](P2 b) { return gedreht({anbau.x + b.x - bildDreh.x, anbau.y + bildDreh.y - b.y}, anbau, hub); };
    std::vector<P2> profil;
    kurve(profil, ins(ruecken[0]), ins(ruecken[1]), ins(ruecken[2]), 6);
    const size_t hinten = profil.size();
    profil.push_back(ins(ruecken[2]));
    kurve(profil, ins(vorn[0]), ins(vorn[1]), ins(vorn[2]), 6);
    profil.push_back(ins(vorn[2]));
    const size_t erste = m.prisma(profil, -halbeBreite, halbeBreite, rueckenFarbe, rueckenFarbe);
    // die vordere, hohle Seite ist blanker Stahl
    for (size_t i = hinten + 1; i + 1 < profil.size(); ++i) m.flaechen[erste + i].farbe = FARBE_SCHILD_STAHL;
}
} // namespace

Modell raupenModell(int kettenPhase) {
    Modell m;
    // Ketten: Umlauf wie im Seitenbild, als Band aus Gliedern (abwechselnd hell und dunkel)
    const double r1 = 8, r2 = 7.6, gerade = 45, b1 = PI * r1, b2 = PI * r2, umfang = 2 * gerade + b1 + b2;
    const int glieder = 52;
    std::vector<P2> umlauf;
    for (int i = 0; i < glieder; ++i) {
        const double s = i * umfang / glieder;
        if (s < gerade) umlauf.push_back({22 - s, 1.2});
        else if (s < gerade + b1) { const double a = (s - gerade) / r1; umlauf.push_back({-23 - std::sin(a) * r1, 9.2 - std::cos(a) * r1}); }
        else if (s < 2 * gerade + b1) { const double t = (s - gerade - b1) / gerade; umlauf.push_back({-23 + t * gerade, 17.2 - t}); }
        else { const double a = (s - 2 * gerade - b1) / r2; umlauf.push_back({22 + std::sin(a) * r2, 8.6 + std::cos(a) * r2}); }
    }
    for (int seite : {-1, 1}) {
        const double innen = seite * 9.0, aussen = seite * 15.5;
        const size_t k = m.prisma(umlauf, std::min(innen, aussen), std::max(innen, aussen), FARBE_KETTE_DUNKEL, DUNKEL);
        for (int i = 0; i < glieder; ++i) m.flaechen[k + size_t(i)].farbe = (i + kettenPhase) % 2 ? FARBE_KETTE_HELL : FARBE_KETTE_DUNKEL;
        // Deckel: +z zuerst, dann -z; der äußere zeigt Laufwerk und Rollen
        textur(m, k + size_t(glieder) + (seite > 0 ? 0 : 1), BildRaupeKette, 35, 18);
    }
    m.quader(-22, 20, 3, 14, -9, 9, DUNKEL);                                   // Wanne zwischen den Ketten
    // Heckgewicht mit Rücklichtern
    quaderMitSeiten(m, -27.5, -19.5, 14, 27, -10, 10, GELB, BildRaupeAufbau, 30, 48);
    m.aufkleberX(-27.5, 20, 23.5, 5.5, 9, false, FARBE_RUECKLICHT);
    m.aufkleberX(-27.5, 20, 23.5, -9, -5.5, false, FARBE_RUECKLICHT);
    m.aufkleberX(-27.5, 15, 18.5, -8, 8, false, DUNKEL);
    // Motorhaube mit Kühlergrill und Scheinwerfern
    const size_t haube = quaderMitSeiten(m, 2, 28, 14, 31, -9, 9, GELB, BildRaupeAufbau, 30, 48);
    m.flaechen[haube + 2].farbe = GELB_HELL;
    m.aufkleberX(28, 16, 27, -6.5, 6.5, true, FARBE_KUEHLER);
    for (double z = -5; z < 6; z += 2.4) m.aufkleberX(28, 17, 26, z, z + 0.8, true, GRAU, 0.4);
    m.aufkleberX(28, 27.5, 30, 5.6, 8.4, true, FARBE_SCHEINWERFER);
    m.aufkleberX(28, 27.5, 30, -8.4, -5.6, true, FARBE_SCHEINWERFER);
    // Kabine: Seiten aus dem Seitenbild, vorn und hinten Scheiben, dahinter der Fahrer
    quaderMitSeiten(m, -22, 2, 14, 43, -10, 10, GELB, BildRaupeAufbau, 30, 48);
    for (bool vorn : {true, false}) {
        const double x = vorn ? 2 : -22;
        m.aufkleberX(x, vorn ? 32 : 28.5, 41.5, -8.3, 8.3, vorn, FARBE_GLAS);
        m.aufkleberX(x, vorn ? 32 : 28.5, 35.5, -3.6, 3.6, vorn, JACKE, 0.4);       // Schultern
        m.aufkleberX(x, 35.5, 39.6, -2.5, 2.5, vorn, vorn ? HAUT : HELM, 0.4);      // Gesicht bzw. Hinterkopf
        m.aufkleberX(x, 39, 41.2, -3.1, 3.1, vorn, HELM, 0.6);                      // Helm
        if (vorn) {
            m.aufkleberX(x, 37.4, 38.2, -1.7, -0.7, vorn, DUNKEL, 0.8);             // Augen
            m.aufkleberX(x, 37.4, 38.2, 0.7, 1.7, vorn, DUNKEL, 0.8);
        }
    }
    m.quader(-24.5, 5, 43, 46.2, -11.5, 11.5, WEISS);                          // Dach mit Überstand
    m.quader(-18, -14.5, 46.2, 49, -1.7, 1.7, LEUCHTE);                        // Rundumleuchte
    m.quader(9.6, 12.4, 31, 44, 3, 5.8, GRAU);                                 // Auspuff
    m.quader(16, 20.5, 31, 35.5, -5.5, -1.5, STAHL_DUNKEL);                    // Luftfilter
    // Schubarme und Schild (angehoben, wie beim Wenden)
    const double hub = 0.42;
    for (int seite : {-1, 1}) {
        std::vector<P2> arm = {{19, 6.3}, {35, 4}, {35, 7.2}, {19, 9.7}};
        for (P2 &p : arm) p = gedreht(p, {19, 8}, hub);
        m.prisma(arm, std::min(seite * 15.8, seite * 17.6), std::max(seite * 15.8, seite * 17.6), GELB_DUNKEL, GELB_DUNKEL);
    }
    schild(m, {{14.4, 1.2}, {10.6, 14}, {15.6, 27}}, {{21.2, 27}, {16.8, 14}, {19.6, 1.2}}, {2, 20}, {19, 8}, hub, 19, GELB);
    return m;
}

Modell pflugModell(int radPhase) {
    Modell m;
    // vier Räder (Zwölfecke), außen das gemalte Rad
    for (double x : {-11.0, 11.0})
        for (int seite : {-1, 1}) {
            std::vector<P2> kreis;
            for (int i = 0; i < 12; ++i) {
                const double w = (i + radPhase * 0.5) * 2 * PI / 12;
                kreis.push_back({x + std::cos(w) * 5.8, 5.8 + std::sin(w) * 5.8});
            }
            const double z0 = seite * 7.4, z1 = seite * 11.2;
            const size_t k = m.prisma(kreis, std::min(z0, z1), std::max(z0, z1), FARBE_KETTE_DUNKEL, DUNKEL);
            for (int i = 0; i < 12; ++i) m.flaechen[k + size_t(i)].farbe = (i + radPhase) % 2 ? 0xff3a3d45 : FARBE_KETTE_DUNKEL;
            textur(m, k + 12 + (seite > 0 ? 0 : 1), BildRad, 6 - x, 11.8);
        }
    quaderMitSeiten(m, -17, 18, 6.5, 12, -7, 7, ORANGE, BildPflug, 22, 31);       // Rahmen
    const size_t haube = quaderMitSeiten(m, 1.5, 18, 12, 18.8, -6, 6, ORANGE, BildPflug, 22, 31);
    m.flaechen[haube + 2].farbe = ORANGE_HELL;
    m.aufkleberX(18, 8, 13, -4, 4, true, FARBE_KUEHLER);
    m.aufkleberX(18, 13.5, 16, 3.4, 5.6, true, FARBE_SCHEINWERFER);
    m.aufkleberX(18, 13.5, 16, -5.6, -3.4, true, FARBE_SCHEINWERFER);
    m.aufkleberX(-17, 8, 10.5, 3.6, 6.4, false, FARBE_RUECKLICHT);
    m.aufkleberX(-17, 8, 10.5, -6.4, -3.6, false, FARBE_RUECKLICHT);
    m.quader(-12.5, -9, 12, 21, -4.2, 4.2, SITZ);                                 // Sitzlehne
    // Fahrer
    m.quader(-9, -3.5, 12, 20.5, -3.3, 3.3, JACKE);
    m.quader(-8.4, -4, 20.5, 25.2, -2.3, 2.3, HAUT);
    m.quader(-8.9, -3.2, 24.4, 26.8, -2.8, 2.8, HELM);
    // Überrollbügel mit Leuchte
    for (int seite : {-1, 1}) m.quader(-14, -12.6, 10, 28.6, std::min(seite * 5.2, seite * 6.4), std::max(seite * 5.2, seite * 6.4), STAHL_DUNKEL);
    m.quader(-14, -12.6, 27.4, 28.6, -6.4, 6.4, STAHL_DUNKEL);
    m.quader(-11.8, -9.2, 28.6, 31, -1.3, 1.3, LEUCHTE);
    // Schild
    const double hub = 0.4;
    std::vector<P2> arm = {{13, 4}, {20, 3}, {20, 5.4}, {13, 6}};
    for (P2 &p : arm) p = gedreht(p, {13, 5}, hub);
    m.prisma(arm, -1.6, 1.6, STAHL_DUNKEL, STAHL_DUNKEL);
    schild(m, {{7.6, 1}, {5, 9}, {8.2, 17}}, {{11.2, 17}, {8, 9}, {10.2, 1}}, {1, 12}, {13, 5}, hub, 13.5, 0xff9e3c0a);
    for (Flaeche &f : m.flaechen)
        if (f.farbe == FARBE_SCHILD_STAHL) f.farbe = ORANGE;   // der kleine Pflug hat ein lackiertes Schild
    return m;
}

Modell heliModell() {
    Modell m;
    const double tu = 92, tv = 47;
    auto mitBild = [&](size_t von) {
        for (size_t i = von; i < m.flaechen.size(); ++i) textur(m, i, BildHeli, tu, tv);
    };
    // Heckausleger, Rumpf und Triebwerksverkleidung als runde Schalen; das Seitenbild liegt wie ein Abziehbild darauf
    mitBild(m.schale({{-84, 20.2, 2.3, 2.0}, {-52, 19, 5.2, 4.2}, {-20, 18, 8, 6.5}}, 10, ROT));
    mitBild(m.schale({{-32, 19, 3.5, 3}, {-28, 18, 11, 9}, {-15, 17.5, 16.6, 13.5}, {5, 17.5, 17.8, 15}, {26, 17.5, 17, 14.2}, {40, 16.4, 13.4, 11.5},
                      {49, 14.5, 8, 7}, {53.2, 13.2, 2.4, 2.2}}, 14, ROT));
    mitBild(m.schale({{-26, 29, 3, 4}, {-21, 30, 7, 8}, {10, 30, 7, 8}, {17.5, 28.5, 3.4, 4.5}}, 10, 0xff4a505a));
    m.quader(-7.4, -2.6, 33, 44.5, -2.4, 2.4, GRAU);                                   // Rotormast
    // Seitenleitwerk (dünn) und Höhenflosse
    mitBild(m.prisma({{-89, 44}, {-81, 44}, {-73, 22}, {-86, 18}}, -1.1, 1.1, ROT, ROT));
    m.quader(-76, -58, 17.4, 19.6, -10, 10, GRAU);
    // Kufen mit Streben
    for (int seite : {-1, 1}) {
        const double z0 = std::min(seite * 11.0, seite * 12.8), z1 = std::max(seite * 11.0, seite * 12.8);
        m.quader(-26, 40, -7.8, -6.2, z0, z1, WEISS);
        m.quader(-10.6, -8.4, -7, 3, z0, z1, GRAU);
        m.quader(21.4, 23.6, -7, 3, z0, z1, GRAU);
    }
    // Heckrotor: durchscheinende Scheibe neben dem Leitwerk
    Flaeche scheibe;
    for (int i = 0; i < 16; ++i) scheibe.ecken.push_back({-82 + std::cos(i * PI / 8) * 12, 31 + std::sin(i * PI / 8) * 12, 3.6});
    scheibe.farbe = 0x50586070;
    scheibe.beidseitig = true;
    m.flaechen.push_back(std::move(scheibe));
    return m;
}

} // namespace schnee
