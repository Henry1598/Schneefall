#include "fahrzeugmodell.h"
#include <cmath>

namespace schnee {
namespace {
const double PI = 3.14159265358979;
constexpr uint32_t GELB = 0xfff2a818, GELB_DUNKEL = 0xffb9780c, WANNE = 0xff2b2e35, DUNKEL = 0xff24262c, STAHL = 0xffc4ccd6, STAHL_DUNKEL = 0xff4a505a,
                   WEISS = 0xffe6e9ee, GLAS = 0x669cc6ea, ORANGE = 0xfff26a1c, ORANGE_DUNKEL = 0xffc8520e, ROT = 0xffc82a24, ROT_DUNKEL = 0xff8e1c1a,
                   HAUT = 0xffecbf9c, JACKE = 0xffff8424, HELM = 0xfff4f4f2, LEUCHTE = 0xffffa028, RUECKLICHT = 0xffe8281c, LAMPE = 0xfffff0b8;

// Punkte einer quadratischen Kurve a → b mit Stützpunkt k (mit Endpunkt)
std::vector<P2> kurve(P2 a, P2 k, P2 b, int schritte) {
    std::vector<P2> p;
    for (int i = 0; i <= schritte; ++i) {
        const double t = double(i) / schritte, u = 1 - t;
        p.push_back({u * u * a.x + 2 * u * t * k.x + t * t * b.x, u * u * a.y + 2 * u * t * k.y + t * t * b.y});
    }
    return p;
}

// sitzender Mensch, schaut nach vorn (+x): gerundeter Rumpf in Warnjacke, Kopf mit Gesicht, Helm, Arme
void mensch(Modell &m, P3 sitz, double k, bool pilot) {
    const double merkSchnee = m.schnee, merkSchmutz = m.schmutz;
    m.schnee = 0;
    m.schmutz = 0;
    auto bei = [&](double x, double y, double z) { return P3{sitz.x + x * k, sitz.y + y * k, sitz.z + z * k}; };
    const uint16_t rumpf = m.teil("Fahrer", Stoff::Tuch, pilot ? 0xff2e4a6e : JACKE, pilot ? Muster::Keins : Muster::Jacke);
    m.ellipsoid(rumpf, bei(0, 5.6, 0), {3.3 * k, 5.8 * k, 4.5 * k}, 14);
    const uint16_t arm = m.teil("Fahrer", Stoff::Tuch, pilot ? 0xff2e4a6e : JACKE);
    for (int seite : {-1, 1}) {
        m.rohr(arm, bei(0.2, 8.6, seite * 4.2), bei(2.6, 5.4, seite * 4.4), 1.25 * k, 1.1 * k, 8);
        m.rohr(arm, bei(2.6, 5.4, seite * 4.4), bei(6.0, 6.6, seite * 2.6), 1.1 * k, 0.95 * k, 8);
    }
    const uint16_t hand = m.teil("Fahrer", Stoff::Haut, HAUT);
    for (int seite : {-1, 1}) m.ellipsoid(hand, bei(6.4, 6.8, seite * 2.5), {1.1 * k, 1.0 * k, 1.0 * k}, 8);
    m.rohr(m.teil("Fahrer", Stoff::Haut, HAUT), bei(0.2, 10.4, 0), bei(0.4, 12, 0), 1.3 * k, 1.3 * k, 8, false);
    const uint16_t kopf = m.teil("Fahrerkopf", Stoff::Haut, HAUT, Muster::Gesicht);
    m.ellipsoid(kopf, bei(0.6, 13.6, 0), {2.9 * k, 3.1 * k, 2.8 * k}, 16);
    // Helm: Halbschale mit Schirm (Pilot: dunkler Helm mit Visier)
    const uint16_t helm = m.teil("Helm", Stoff::Lack, pilot ? 0xff30343c : HELM);
    m.ellipsoid(helm, bei(0.4, 14.3, 0), {3.3 * k, 3.2 * k, 3.25 * k}, 16, 0.02, 1);
    if (pilot) {
        const uint16_t visier = m.teil("Helm", Stoff::Glas, 0xff1a2230);
        m.ellipsoid(visier, bei(1.6, 13.9, 0), {2.5 * k, 1.5 * k, 2.6 * k}, 12);
    } else {
        m.rohr(helm, bei(1.6, 14.4, 0), bei(1.6, 14.75, 0), 3.9 * k, 3.7 * k, 16);
    }
    m.schnee = merkSchnee;
    m.schmutz = merkSchmutz;
}

// Kabine aus Holmen, Scheiben und Dach über einem Sockel: x0 … x1, Scheiben von y0 bis y1, halbe Breite b
void kabine(Modell &m, double x0, double x1, double y0, double y1, double b, uint32_t lack, uint32_t dach, double ueberstand) {
    const double h = 1.7;
    for (double x : {x0, x1 - h})
        for (int seite : {-1, 1}) {
            const uint16_t holm = m.teil("Holm", Stoff::Lack, lack);
            m.quader(holm, x, x + h, y0, y1, seite > 0 ? b - h : -b, seite > 0 ? b : -b + h, 0.4);
        }
    // Fensterrahmen oben und unten
    for (int seite : {-1, 1}) {
        const uint16_t rahmen = m.teil("Holm", Stoff::Lack, lack);
        m.quader(rahmen, x0, x1, y1 - 1.1, y1, seite > 0 ? b - 1.2 : -b, seite > 0 ? b : -b + 1.2, 0.3);
    }
    const uint16_t glas = m.teil("Scheibe", Stoff::Glas, GLAS);
    m.teile[glas].schnee = 0.25;
    m.quader(glas, x1 - 0.9, x1 - 0.5, y0, y1 - 1, -b + h, b - h);          // vorn
    m.quader(glas, x0 + 0.5, x0 + 0.9, y0, y1 - 1, -b + h, b - h);          // hinten
    for (int seite : {-1, 1}) m.quader(glas, x0 + h, x1 - h, y0, y1 - 1, seite * (b - 0.9), seite * (b - 0.5));
    const uint16_t d = m.teil("Dach", Stoff::Lack, dach);
    m.quader(d, x0 - ueberstand, x1 + ueberstand, y1, y1 + 3, -b - 1.5, b + 1.5, 1.2);
}

// gewölbtes Schild: vorn die Arbeitsfläche (Kurve unten → oben), dahinter der Rücken
void schildblatt(Modell &m, P2 unten, P2 bauch, P2 oben, double dicke, double ruecken, double halbeBreite, Stoff vornStoff, uint32_t vorn, uint32_t hinten) {
    const std::vector<P2> front = kurve(unten, bauch, oben, 10);
    std::vector<P2> blatt = front, koerper;
    for (auto it = front.rbegin(); it != front.rend(); ++it) blatt.push_back({it->x - dicke, it->y});
    for (const P2 &p : front) koerper.push_back({p.x - dicke, p.y});
    for (auto it = front.rbegin(); it != front.rend(); ++it) koerper.push_back({it->x - dicke - ruecken, it->y});
    m.prisma(m.teil("Schild", vornStoff, vorn, vornStoff == Stoff::Metall ? Muster::Schneide : Muster::Keins), blatt, -halbeBreite, halbeBreite, true);
    m.prisma(m.teil("Schildruecken", Stoff::Lack, hinten), koerper, -halbeBreite, halbeBreite, true);
}
} // namespace

// ---- Kettendozer ----
Modell raupeKoerper(bool mitFahrer) {
    Modell m;
    m.quader(m.teil("Wanne", Stoff::Matt, WANNE), -24, 22, 4, 14.5, -9, 9, 0.6);
    // Gegengewicht am Heck mit Warnschraffur und Rücklichtern
    m.quader(m.teil("Gegengewicht", Stoff::Lack, GELB, Muster::Warnstreifen), -27.5, -21, 14, 26, -10, 10, 1.5);
    for (int seite : {-1, 1}) m.quader(m.teil("Ruecklicht", Stoff::Licht, RUECKLICHT), -28, -27.5, 21.6, 24.6, seite * 6.0, seite * 9.0);
    m.quader(m.teil("Zughaken", Stoff::Metall, STAHL_DUNKEL), -29, -27.5, 15.2, 17, -1.6, 1.6, 0.4);
    // Motorhaube, vorn abgeschrägt, mit Lüftungsschlitzen
    m.prisma(m.teil("Haube", Stoff::Lack, GELB, Muster::Raupenhaube), {{2, 14}, {30, 14}, {30, 23}, {26.5, 29.5}, {2, 29.5}}, -8.5, 8.5);
    m.quader(m.teil("Kuehler", Stoff::Matt, 0xff22252b, Muster::Kuehler), 30, 30.5, 15.4, 22.4, -5.8, 5.8);
    for (int seite : {-1, 1}) m.rohr(m.teil("Scheinwerfer", Stoff::Licht, LAMPE), {29.8, 20.6, seite * 7.3}, {30.9, 20.6, seite * 7.3}, 1.15, 1.15, 12);
    // Auspuff mit Regenklappe, Luftfilter
    const uint16_t auspuff = m.teil("Auspuff", Stoff::Metall, 0xff6c727c);
    m.zylinder(auspuff, {11, 29.5, 4.4}, {11, 43, 4.4}, 1.35, 12);
    m.quader(m.teil("Regenklappe", Stoff::Metall, STAHL_DUNKEL), 9.3, 13.3, 43.2, 43.7, 3.0, 5.8);
    m.zylinder(m.teil("Luftfilter", Stoff::Matt, 0xff3c4048), {18, 29.5, -4}, {18, 34, -4}, 2.1, 12);
    m.zylinder(m.teil("Luftfilter", Stoff::Metall, 0xff8a919c), {18, 34, -4}, {18, 34.8, -4}, 2.5, 12);
    // Kabine: Sockel mit Tür, Holme, Scheiben, Dach
    m.quader(m.teil("Kabine", Stoff::Lack, GELB), -21, 2, 14, 27, -10, 10, 1.2);
    for (int seite : {-1, 1}) {
        m.quader(m.teil("Tuergriff", Stoff::Metall, STAHL), -6, -3.4, 22.4, 23.2, seite * 10.0, seite * 10.5);
        m.zylinder(m.teil("Handlauf", Stoff::Metall, STAHL), {-20.4, 28, seite * 10.5}, {-20.4, 40, seite * 10.5}, 0.4, 6);
        // Spiegel am vorderen Holm
        m.zylinder(m.teil("Spiegel", Stoff::Matt, DUNKEL), {1.6, 36, seite * 10.0}, {3, 36.4, seite * 12.4}, 0.3, 6);
        m.quader(m.teil("Spiegel", Stoff::Matt, DUNKEL), 2.5, 3.3, 34, 38.6, seite * 11.6, seite * 13.4, 0.3);
    }
    kabine(m, -21, 2, 27, 42.2, 10, GELB, WEISS, 2.5);
    // Innenraum
    m.quader(m.teil("Sitz", Stoff::Matt, 0xff191b20), -16.5, -13, 19.5, 33, -4, 4, 0.8);
    m.quader(m.teil("Armaturen", Stoff::Matt, 0xff16181c), -2.6, 0.4, 24, 30.5, -7, 7, 0.6);
    if (mitFahrer) mensch(m, {-11.4, 20.4, 0}, 1.0, false);
    // Rundumleuchte und Öse für den Lasthaken auf dem Dach
    m.zylinder(m.teil("Leuchtensockel", Stoff::Matt, DUNKEL), {-16, 45.2, 0}, {-16, 45.9, 0}, 2.0, 12);
    const uint16_t leuchte = m.teil("Leuchte", Stoff::Licht, LEUCHTE);
    m.zylinder(leuchte, {-16, 45.9, 0}, {-16, 48.2, 0}, 1.55, 12);
    m.quader(m.teil("Oese", Stoff::Metall, STAHL), -1.2, 1.2, 45.2, 46.8, -0.6, 0.6, 0.3);
    return m;
}

Modell raupeKette(int seite, int phase) {
    Modell m;
    m.schnee = 0.35;
    const double innen = seite * 9.0, aussen = seite * 15.5;
    // Umlauf: unten gerade, hinten um das Triebrad, oben leicht ansteigend, vorn um das Leitrad
    const double r1 = 8, r2 = 7.6, gerade = 45, b1 = PI * r1, b2 = PI * r2, umfang = 2 * gerade + b1 + b2;
    const int punkte = 104;
    std::vector<P2> umlauf;
    for (int i = 0; i < punkte; ++i) {
        const double s = i * umfang / punkte;
        if (s < gerade) umlauf.push_back({22 - s, 1.2});
        else if (s < gerade + b1) { const double a = (s - gerade) / r1; umlauf.push_back({-23 - std::sin(a) * r1, 9.2 - std::cos(a) * r1}); }
        else if (s < 2 * gerade + b1) { const double t = (s - gerade - b1) / gerade; umlauf.push_back({-23 + t * gerade, 17.2 - t}); }
        else { const double a = (s - 2 * gerade - b1) / r2; umlauf.push_back({22 + std::sin(a) * r2, 8.6 + std::cos(a) * r2}); }
    }
    const uint16_t band = m.teil("Kette", Stoff::Metall, 0xff5a616c, Muster::Kette);
    m.teile[band].phase = double(phase) / KETTENPHASEN;
    m.prisma(band, umlauf, std::min(innen, aussen), std::max(innen, aussen), true);
    // Triebrad (acht Löcher), Leitrad (sieben), Laufrollen und Stützrollen: stehen etwas über die Kette vor
    const double z0 = seite * 9.6, z1 = seite * 16.2;
    auto rolle = [&](const char *name, double x, double y, double r, int teilung, double dreh) {
        const uint16_t t = m.teil(name, Stoff::Metall, 0xff8f97a3, Muster::Rolle);
        m.teile[t].teilung = teilung;
        m.teile[t].phase = dreh;
        m.teile[t].schnee = 0;
        m.rohr(t, {x, y, z0}, {x, y, z1}, r, r, r > 4 ? 24 : 14);
    };
    const double dreh = double(phase) / KETTENPHASEN;
    rolle("Triebrad", -23, 9.2, 6.79, 8, dreh * 2 * PI / 8);
    rolle("Leitrad", 22, 8.6, 5.94, 7, dreh * 2 * PI / 7);
    // Radien so, dass ein Kettenglied Weg genau einer Teilung der Rolle entspricht: die Rollen drehen sichtbar mit
    for (double x : {-13.0, -5.5, 2.0, 9.5}) rolle("Laufrolle", x, 4.6, 3.39, 4, -dreh * 2 * PI / 4);
    for (double x : {-8.0, 8.0}) rolle("Stuetzrolle", x, 14.3, 1.697, 2, dreh * 2 * PI / 2);
    // Schubrahmen des Schilds: läuft außen an der Kette vorbei und gehört deshalb zu dieser Schicht
    m.quader(m.teil("Schubrahmen", Stoff::Lack, GELB_DUNKEL), 4, 33.5, 5.4, 8.8, std::min(seite * 16.8, seite * 18.6), std::max(seite * 16.8, seite * 18.6), 0.5);
    m.zylinder(m.teil("Gelenk", Stoff::Metall, STAHL_DUNKEL), {5.6, 7.1, seite * 16.4}, {5.6, 7.1, seite * 19.0}, 1.9, 10);
    // Laufwerksträger in Wagenfarbe
    m.quader(m.teil("Laufwerk", Stoff::Lack, GELB_DUNKEL), -17, 16, 6.6, 11.4, std::min(seite * 15.7, seite * 16.6), std::max(seite * 15.7, seite * 16.6), 0.5);
    return m;
}

Modell raupeSchild() {
    Modell m;
    m.schnee = 0.6;
    for (int seite : {-1, 1}) {
        // Hydraulikzylinder: Rohr in Wagenfarbe, blanke Kolbenstange
        m.zylinder(m.teil("Hydraulik", Stoff::Lack, GELB_DUNKEL), {17, 19.6, seite * 9.6}, {26, 16.4, seite * 9.6}, 1.3, 10);
        m.zylinder(m.teil("Kolbenstange", Stoff::Metall, 0xffe2e7ee), {26, 16.4, seite * 9.6}, {32.4, 14.1, seite * 9.6}, 0.6, 8);
        m.quader(m.teil("Seitenblech", Stoff::Lack, GELB_DUNKEL), 31.6, 38.4, 1.2, 26.6, std::min(seite * 19.0, seite * 19.7), std::max(seite * 19.0, seite * 19.7), 0.3);
    }
    schildblatt(m, {38.2, 1}, {33.8, 14}, {36.6, 26.8}, 1.3, 2.6, 19, Stoff::Metall, 0xffbcc5d0, GELB);
    m.quader(m.teil("Schneidkante", Stoff::Metall, 0xff3a3e46), 36.4, 39.3, 0.2, 2.4, -19.4, 19.4, 0.3);
    return m;
}

// ---- kleiner Radpflug (Kommunalfahrzeug mit Schneeschild) ----
Modell pflugKoerper(bool mitFahrer) {
    Modell m;
    m.quader(m.teil("Rahmen", Stoff::Matt, WANNE), -19.5, 19.5, 5, 8.2, -6.6, 6.6, 0.5);
    for (double x : {-11.0, 11.0}) m.zylinder(m.teil("Achse", Stoff::Metall, STAHL_DUNKEL), {x, 5.8, -8}, {x, 5.8, 8}, 0.9, 8);
    // kurze Haube mit schrägem Bug, Kühlergrill, Scheinwerfern und Stoßstange
    m.prisma(m.teil("Haube", Stoff::Lack, ORANGE), {{11.5, 8.2}, {20.5, 8.2}, {20.5, 13}, {18.6, 15.8}, {11.5, 16.4}}, -7.6, 7.6);
    m.quader(m.teil("Kuehler", Stoff::Matt, 0xff22252b, Muster::Kuehler), 20.5, 20.9, 9.2, 12.6, -4.6, 4.6);
    for (int seite : {-1, 1}) m.rohr(m.teil("Scheinwerfer", Stoff::Licht, LAMPE), {20.3, 11, seite * 6.1}, {21.1, 11, seite * 6.1}, 1.05, 1.05, 12);
    m.quader(m.teil("Stossstange", Stoff::Matt, DUNKEL), 20.4, 21.7, 5.4, 8, -8.4, 8.4, 0.5);
    // Fahrerhaus
    m.quader(m.teil("Kabine", Stoff::Lack, ORANGE), 0, 12, 8.2, 16.6, -8, 8, 1.2);
    kabine(m, 0, 12, 16.6, 25, 8, ORANGE, WEISS, 0.7);
    for (int seite : {-1, 1}) {
        m.quader(m.teil("Tuergriff", Stoff::Metall, STAHL), 2.2, 4.2, 13.6, 14.2, seite * 8.0, seite * 8.4);
        m.zylinder(m.teil("Spiegel", Stoff::Matt, DUNKEL), {11.6, 20.4, seite * 8.0}, {12.6, 20.8, seite * 10.2}, 0.28, 6);
        m.quader(m.teil("Spiegel", Stoff::Matt, DUNKEL), 12.2, 12.9, 18.6, 22.6, seite * 9.6, seite * 11.0, 0.3);
        // Trittstufe unter der Tür
        m.quader(m.teil("Trittstufe", Stoff::Metall, STAHL_DUNKEL), 2.6, 8.4, 6.2, 6.9, std::min(seite * 6.6, seite * 9.0), std::max(seite * 6.6, seite * 9.0), 0.2);
    }
    m.quader(m.teil("Sitz", Stoff::Matt, 0xff191b20), 1.2, 3.4, 12.4, 21.4, -3.4, 3.4, 0.6);
    m.quader(m.teil("Armaturen", Stoff::Matt, 0xff16181c), 9, 11, 14.6, 18.2, -6, 6, 0.5);
    if (mitFahrer) mensch(m, {5.0, 12.9, 0}, 0.74, false);
    // Rundumleuchte
    m.zylinder(m.teil("Leuchtensockel", Stoff::Matt, DUNKEL), {6, 28, 0}, {6, 28.5, 0}, 1.7, 12);
    m.zylinder(m.teil("Leuchte", Stoff::Licht, LEUCHTE), {6, 28.5, 0}, {6, 30.4, 0}, 1.3, 12);
    // Auspuff hinter dem Fahrerhaus
    m.zylinder(m.teil("Auspuff", Stoff::Metall, 0xff6c727c), {-0.4, 9, 7}, {-0.4, 27, 7}, 0.7, 8);
    // Pritsche mit Streugutbehälter
    m.quader(m.teil("Pritsche", Stoff::Lack, ORANGE_DUNKEL, Muster::Pritsche), -19.5, -0.9, 8.2, 13.6, -8.4, 8.4, 0.5);
    m.prisma(m.teil("Streuer", Stoff::Matt, 0xff727985), {{-17.6, 13.6}, {-3, 13.6}, {-4.4, 19.6}, {-16.2, 19.6}}, -6.6, 6.6);
    m.quader(m.teil("Streuerdeckel", Stoff::Lack, 0xff8a919c), -16.6, -4, 19.6, 20.5, -7, 7, 0.4);
    for (int seite : {-1, 1}) m.quader(m.teil("Ruecklicht", Stoff::Licht, RUECKLICHT), -19.9, -19.5, 9.4, 11.6, seite * 5.2, seite * 7.8);
    // Kotflügel über den Rädern
    for (double x : {-11.0, 11.0})
        for (int seite : {-1, 1})
            m.quader(m.teil("Kotfluegel", Stoff::Matt, DUNKEL), x - 6.8, x + 6.8, 12, 13, std::min(seite * 6.6, seite * 11.4), std::max(seite * 6.6, seite * 11.4), 0.4);
    // Anbauplatte für das Schild
    m.quader(m.teil("Anbauplatte", Stoff::Metall, STAHL_DUNKEL), 21.6, 22.4, 4.6, 9.4, -4.4, 4.4, 0.3);
    return m;
}

Modell pflugRad(int phase) {
    Modell m;
    m.schnee = 0;
    m.schmutz = 0.6;
    const uint16_t rad = m.teil("Rad", Stoff::Gummi, 0xff17181c, Muster::Reifen);
    m.teile[rad].phase = phase * (PI / 3) / RADPHASEN;
    m.teile[rad].schnee = 0;
    // Reifen mit runden Schultern, Felge etwas eingezogen
    m.drehkoerper(rad, {0, 0, 0}, {{0.01, -1.3}, {3.6, -1.3}, {3.8, -1.75}, {5.0, -1.9}, {5.6, -1.5}, {5.8, -0.8}, {5.8, 0.8}, {5.6, 1.5}, {5.0, 1.9}, {3.8, 1.75}, {3.6, 1.3},
                                {0.01, 1.3}}, 36);
    return m;
}

Modell pflugSchild() {
    Modell m;
    m.schnee = 0.6;
    for (int seite : {-1, 1}) {
        m.zylinder(m.teil("Schubrahmen", Stoff::Matt, DUNKEL), {22.2, 7, seite * 3.6}, {26.2, 5.4, seite * 6.5}, 0.8, 8);
        m.zylinder(m.teil("Hydraulik", Stoff::Metall, 0xffe2e7ee), {22.2, 8.6, seite * 1.2}, {26.2, 9.6, seite * 1.2}, 0.5, 8);
        // Warnfahnen an den Schildecken
        m.zylinder(m.teil("Fahnenstange", Stoff::Metall, STAHL), {27, 13, seite * 13.2}, {27, 18, seite * 13.2}, 0.25, 6);
        m.quader(m.teil("Fahne", Stoff::Tuch, 0xffe8281c), 26.9, 27.1, 15.6, 18, std::min(seite * 13.2, seite * 15.4), std::max(seite * 13.2, seite * 15.4));
    }
    schildblatt(m, {28.6, 0.9}, {25.8, 7}, {28.2, 13.2}, 0.9, 1.3, 13.6, Stoff::Lack, ORANGE, ORANGE_DUNKEL);
    m.quader(m.teil("Schneidkante", Stoff::Metall, 0xff3a3e46), 27.6, 29.4, 0.2, 1.8, -13.8, 13.8, 0.25);
    return m;
}

Modell fahrerStehend(int haltung) {
    Modell m;
    m.schnee = 0;
    m.schmutz = 0;
    const uint16_t stiefel = m.teil("Stiefel", Stoff::Gummi, 0xff1c1c20);
    const uint16_t hose = m.teil("Hose", Stoff::Tuch, 0xff2c3852);
    for (int seite : {-1, 1}) {
        m.ellipsoid(stiefel, {0.7, 0.9, seite * 1.7}, {2.3, 0.95, 1.25}, 8);
        m.rohr(hose, {0, 1.4, seite * 1.7}, {0, 9.8, seite * 1.5}, 1.3, 1.5, 8);
    }
    m.ellipsoid(m.teil("Fahrer", Stoff::Tuch, JACKE, Muster::Jacke), {0, 14.2, 0}, {2.9, 5.3, 4.0}, 14);
    const uint16_t arm = m.teil("Fahrer", Stoff::Tuch, JACKE);
    const uint16_t hand = m.teil("Faust", Stoff::Haut, HAUT);
    // linker Arm in die Hüfte gestemmt
    m.rohr(arm, {0, 17.6, -3.9}, {-0.6, 14.4, -5.6}, 1.2, 1.1, 8);
    m.rohr(arm, {-0.6, 14.4, -5.6}, {0.8, 12.6, -3.6}, 1.1, 0.95, 8);
    // rechter Arm mit geballter Faust: hoch erhoben oder nach vorn gestoßen
    const P3 ellbogen = haltung == 0 ? P3{2.6, 20.6, 4.9} : P3{3.4, 18.6, 5.0};
    const P3 faust = haltung == 0 ? P3{4.2, 25.6, 4.6} : P3{7.4, 22.4, 4.6};
    m.rohr(arm, {0, 17.8, 3.9}, ellbogen, 1.25, 1.1, 8);
    m.rohr(arm, ellbogen, faust, 1.1, 0.95, 8);
    m.ellipsoid(hand, faust, {1.45, 1.35, 1.35}, 8);
    m.rohr(m.teil("Fahrer", Stoff::Haut, HAUT), {0.2, 18.8, 0}, {0.4, 20.2, 0}, 1.3, 1.3, 8, false);
    m.ellipsoid(m.teil("Fahrerkopf", Stoff::Haut, HAUT, Muster::Gesicht), {0.6, 21.8, 0}, {2.8, 3.0, 2.7}, 16);
    const uint16_t helm = m.teil("Helm", Stoff::Lack, HELM);
    m.ellipsoid(helm, {0.4, 22.5, 0}, {3.2, 3.1, 3.15}, 16, 0.02, 1);
    m.rohr(helm, {1.6, 22.6, 0}, {1.6, 22.95, 0}, 3.8, 3.6, 16);
    return m;
}

// ---- Hubschrauber ----
Modell heliKoerper() {
    Modell m;
    m.schnee = 0.22;
    m.schmutz = 0;
    using S = Modell::Spant;
    const double teilung = 14;   // ab hier vorn ist die obere Rumpfhälfte Glas
    const std::vector<S> hinten = {{-34, 19, 2.5, 2.5}, {-30, 18.5, 9, 7.6}, {-22, 18, 14, 11.6}, {-10, 17.5, 17.2, 14.2}, {5, 17.5, 18, 15}, {teilung, 17.5, 17.85, 14.92}};
    const std::vector<S> vorn = {{teilung, 17.5, 17.85, 14.92}, {20, 17.5, 17.6, 14.8}, {32, 17, 15.6, 13.2}, {42, 16, 12.4, 10.6}, {49, 14.6, 8, 7}, {52.5, 13.6, 3.6, 3.2}, {53.7, 13.2, 0.7, 0.7}};
    const double rand = 0.26;    // Glas reicht etwas unter die Rumpfmitte
    // Innenraum zuerst: Rückwand, Boden, Sitze, Instrumente, Pilot
    m.quader(m.teil("Rueckwand", Stoff::Matt, 0xff1b1d22), 12.6, 13.6, 2, 33, -13.4, 13.4);
    m.quader(m.teil("Kabinenboden", Stoff::Matt, 0xff24272d), 13.6, 47, 3.4, 5.2, -9.6, 9.6);
    for (int seite : {-1, 1}) m.quader(m.teil("Sitz", Stoff::Matt, 0xff2e3138), 16.4, 19, 5.2, 21, seite * 4.8 - 3.2, seite * 4.8 + 3.2, 0.8);
    m.quader(m.teil("Instrumente", Stoff::Matt, 0xff101216), 38, 42, 5.2, 14.6, -6.4, 6.4, 0.8);
    mensch(m, {21.6, 6.2, 4.8}, 0.86, true);
    // Rumpf: hinten rundum Blech, vorn nur die untere Hälfte
    const uint16_t rumpf = m.teil("Rumpf", Stoff::Lack, ROT, Muster::HeliRumpf);
    m.schale(rumpf, hinten, 28);
    m.schale(m.teil("Rumpf", Stoff::Lack, ROT, Muster::HeliRumpf), vorn, 14, PI + rand, 2 * PI - rand);
    // Kanzel aus Glas mit Mittelsteg und Rahmen
    const uint16_t kanzel = m.teil("Kanzel", Stoff::Glas, 0x5c9cc6ea);
    m.teile[kanzel].schnee = 0;
    m.schale(kanzel, vorn, 18, -rand, PI + rand);
    auto etwasGroesser = [](std::vector<S> s, double mehr) { for (S &q : s) { q.ry += mehr; q.rz += mehr; } return s; };
    m.schale(m.teil("Kanzelrahmen", Stoff::Lack, ROT_DUNKEL), etwasGroesser(vorn, 0.25), 2, PI / 2 - 0.07, PI / 2 + 0.07);
    m.schale(m.teil("Kanzelrahmen", Stoff::Lack, ROT), etwasGroesser({{teilung - 1.2, 17.5, 17.85, 14.92}, {teilung + 1.0, 17.5, 17.8, 14.9}}, 0.3), 18, -rand, PI + rand);
    for (double w : {-rand, PI + rand})
        m.schale(m.teil("Kanzelrahmen", Stoff::Lack, ROT), etwasGroesser(vorn, 0.25), 1, w - 0.05, w + 0.05);
    // Triebwerksverkleidung mit Lufteinlass und Abgasrohr, Rotormast
    m.schale(m.teil("Triebwerk", Stoff::Lack, 0xff5a606a), {{-25, 30, 2, 3.4}, {-19, 31, 5.6, 7.2}, {-6, 32, 7, 8.6}, {8, 32, 7, 8.6}, {15, 30.6, 4.6, 6.6}, {17.6, 29.6, 1.4, 3}}, 16);
    m.ellipsoid(m.teil("Lufteinlass", Stoff::Matt, 0xff0e0f12), {16.4, 30.6, 0}, {1.3, 2.7, 4.2}, 10);
    m.rohr(m.teil("Abgasrohr", Stoff::Metall, 0xff3e4249), {-19, 33, 0}, {-27.5, 35.4, 0}, 2.0, 1.7, 12);
    m.zylinder(m.teil("Rotormast", Stoff::Metall, 0xff8a919c), {-4, 37, 0}, {-4, 44, 0}, 1.3, 10);
    m.zylinder(m.teil("Rotorkopf", Stoff::Matt, 0xff2a2d33), {-4, 43.4, 0}, {-4, 45.6, 0}, 3.1, 14);
    // Heckausleger mit Kennung, Leitwerk, Höhenflosse mit Endscheiben
    m.schale(m.teil("Ausleger", Stoff::Lack, ROT, Muster::HeliAusleger), {{-86, 20.4, 2.1, 1.9}, {-60, 19.6, 4.2, 3.6}, {-38, 18.6, 7, 5.8}, {-28, 18.2, 9.4, 7.8}}, 16);
    m.prisma(m.teil("Leitwerk", Stoff::Lack, ROT), {{-90, 43}, {-82, 43}, {-73.5, 23}, {-78, 17.4}, {-86.5, 17.4}}, -0.9, 0.9, true);
    m.quader(m.teil("Leitwerksspitze", Stoff::Lack, 0xfff4f4f4), -90.2, -81.6, 39.6, 43.2, -1.05, 1.05, 0.3);
    m.quader(m.teil("Hoehenflosse", Stoff::Lack, ROT), -75, -62.5, 18.4, 19.8, -11, 11, 0.5);
    for (int seite : {-1, 1}) m.quader(m.teil("Endscheibe", Stoff::Lack, ROT_DUNKEL), -76, -61.5, 15.6, 22.8, std::min(seite * 10.6, seite * 11.5), std::max(seite * 10.6, seite * 11.5), 0.4);
    // Heckrotor: Getriebe und durchscheinende Scheibe mit verwischten Blättern
    m.zylinder(m.teil("Heckgetriebe", Stoff::Metall, 0xff5a606a), {-83, 31, 0.6}, {-83, 31, 4.4}, 1.5, 10);
    const uint16_t scheibe = m.teil("Heckrotor", Stoff::Scheibe, 0xff30343c, Muster::Rotorscheibe);
    m.teile[scheibe].beidseitig = true;
    m.teile[scheibe].mitte = {-83, 31, 3.6};
    m.teile[scheibe].halb = {11.6, 11.6, 0.1};
    for (int i = 0; i < 24; ++i) {
        const double a = i * PI / 12, b = (i + 1) * PI / 12;
        const P3 n{0, 0, 1};
        m.dreieck(scheibe, {{-83, 31, 3.6}, n, 0}, {{-83 + std::cos(a) * 12, 31 + std::sin(a) * 12, 3.6}, n, 0}, {{-83 + std::cos(b) * 12, 31 + std::sin(b) * 12, 3.6}, n, 0});
    }
    // Kufen mit Querrohren, Lasthaken, Positionslichter
    for (int seite : {-1, 1}) {
        const uint16_t kufe = m.teil("Kufe", Stoff::Metall, 0xffd2d7de);
        m.zylinder(kufe, {-24, -7, seite * 12.6}, {36, -7, seite * 12.6}, 0.95, 8);
        m.zylinder(kufe, {36, -7, seite * 12.6}, {41.4, -4, seite * 12.6}, 0.95, 8);
        const uint16_t strebe = m.teil("Kufenstrebe", Stoff::Metall, 0xff9aa1ab);
        m.zylinder(strebe, {-8, 2.4, seite * 8.0}, {-9, -7, seite * 12.6}, 0.8, 8);
        m.zylinder(strebe, {22, 2.4, seite * 8.0}, {23, -7, seite * 12.6}, 0.8, 8);
        m.ellipsoid(m.teil("Positionslicht", Stoff::Licht, seite > 0 ? 0xff30f060 : 0xffff3828), {30, 9, seite * 13.6}, {0.9, 0.9, 0.9}, 8);
    }
    m.quader(m.teil("Lasthaken", Stoff::Matt, 0xff2a2d33), -2.2, 2.2, -2.8, -0.2, -1.7, 1.7, 0.5);
    return m;
}

} // namespace schnee
