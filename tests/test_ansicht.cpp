// 3D-Ansicht der Fahrzeuge: Projektion, Winkelstufen, Rasterung (Sichtbarkeit, weltfestes Licht, Schnee),
// Kettenmuster mit Laufrichtung, Modelle (was aus welcher Richtung zu sehen ist, Bildrahmen) und die Szene.
#include <QtTest>
#include <cmath>
#include <set>
#include "ansicht3d.h"
#include "fahrzeugmodell.h"
#include "raeumszene.h"

using namespace schnee;

namespace {
const double PI = 3.14159265358979;
int sichtbar(const Modell &m, double gier, const char *name) {
    const auto a = abdeckung(m, gier);
    const auto it = a.find(name);
    return it == a.end() ? 0 : it->second;
}
// passt das Modell bei allen Winkelstufen in seinen Bildrahmen? (Rand bleibt durchsichtig)
QString passtInRahmen(const Modell &m, const Rahmen &r) {
    const double s = 2;
    const int w = int(r.w * s), h = int(r.h * s);
    for (int i = 0; i < WINKELSTUFEN; ++i) {
        const Ansichtsbild b = rendern(m, stufenWinkel(i), s, w, h, r.px * s, r.py * s, 1);
        int belegt = 0;
        for (int x = 0; x < w; ++x) { if (b.alpha(x, 0) || b.alpha(x, h - 1)) return QString("Stufe %1 stößt oben/unten an").arg(i); }
        for (int y = 0; y < h; ++y) { if (b.alpha(0, y) || b.alpha(w - 1, y)) return QString("Stufe %1 stößt seitlich an").arg(i); }
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) belegt += b.alpha(x, y) > 0;
        if (belegt < 40) return QString("Stufe %1 ist leer").arg(i);
    }
    return {};
}
// mittlere Helligkeit der deckenden Bildpunkte in einem Spaltenbereich
double hell(const Ansichtsbild &b, int x0, int x1) {
    double summe = 0;
    int n = 0;
    for (int y = 0; y < b.h; ++y)
        for (int x = x0; x < x1; ++x)
            if (b.alpha(x, y) == 255) { const uint8_t *p = &b.rgba[size_t(4 * (y * b.w + x))]; summe += p[0] + p[1] + p[2]; ++n; }
    return n ? summe / n / 3 : 0;
}
}

class TestAnsicht : public QObject {
    Q_OBJECT
private slots:
    void projektionDrehtUmDieHochachse() {
        // Gier 0: vorn ist rechts; π/2: vorn zeigt zum Betrachter; π: vorn ist links
        QVERIFY(std::abs(projizieren({10, 0, 0}, 0).x - 10) < 1e-9);
        QVERIFY(std::abs(projizieren({10, 0, 0}, PI / 2).x) < 1e-9);
        QVERIFY(tiefe({10, 0, 0}, PI / 2) > tiefe({-10, 0, 0}, PI / 2));
        QVERIFY(std::abs(projizieren({10, 0, 0}, PI).x + 10) < 1e-9);
        QVERIFY(tiefe({10, 0, 0}, 3 * PI / 2) < tiefe({-10, 0, 0}, 3 * PI / 2));   // Heck vorn
        for (double g : {0.0, 1.0, 2.5, 4.0}) QVERIFY(projizieren({0, 20, 0}, g).y < projizieren({0, 0, 0}, g).y - 19);
        // Blick leicht von oben: was näher ist, liegt im Bild tiefer
        QVERIFY(projizieren({0, 0, 10}, 0).y > projizieren({0, 0, -10}, 0).y);
    }
    void winkelstufenDeckenDenVollkreis() {
        QCOMPARE(winkelstufe(0), 0);
        QCOMPARE(winkelstufe(PI), WINKELSTUFEN / 2);
        QCOMPARE(winkelstufe(PI / 2), WINKELSTUFEN / 4);
        QCOMPARE(winkelstufe(2 * PI - 0.01), 0);
        QCOMPARE(winkelstufe(-PI / 2), 3 * WINKELSTUFEN / 4);
        for (int i = 0; i < WINKELSTUFEN; ++i) QCOMPARE(winkelstufe(stufenWinkel(i)), i);
    }
    void quaderWirdMitTiefeUndVerkuerzungGerastert() {
        Modell m;
        m.schnee = 0;
        m.schmutz = 0;
        m.quader(m.teil("hinten", Stoff::Lack, 0xff2040c0), -10, 10, 0, 6, -8, -2);
        m.quader(m.teil("vorn", Stoff::Lack, 0xffc04020), -10, 10, 0, 6, 2, 8);
        // von der Seite verdeckt der vordere Quader den hinteren fast ganz (nur dessen Deckel schaut oben heraus)
        QVERIFY(sichtbar(m, 0, "vorn") > 4 * sichtbar(m, 0, "hinten"));
        QVERIFY(sichtbar(m, PI, "hinten") > 4 * sichtbar(m, PI, "vorn"));
        // von vorn stehen beide nebeneinander, und die Länge von 20 schrumpft auf die Breite von 6
        const auto a = abdeckung(m, PI / 2);
        QVERIFY(std::abs(a.at("vorn") - a.at("hinten")) < a.at("vorn") * 0.1);
        const Ansichtsbild seite = rendern(m, 0, 2, 120, 60, 60, 40, 1), front = rendern(m, PI / 2, 2, 120, 60, 60, 40, 1);
        auto breite = [](const Ansichtsbild &b) {
            int l = b.w, r = 0;
            for (int y = 0; y < b.h; ++y) for (int x = 0; x < b.w; ++x) if (b.alpha(x, y)) { l = std::min(l, x); r = std::max(r, x); }
            return r - l + 1;
        };
        QVERIFY(std::abs(breite(seite) - 40) <= 2);
        QVERIFY(std::abs(breite(front) - 32) <= 2);   // zwei Quader von z = -8 … 8
    }
    void lichtIstWeltfestUndDrehtNichtMit() {
        // symmetrischer Quader mit gerundeten Kanten: links im Bild (zum Licht) heller als rechts –
        // egal, ob das Fahrzeug nach rechts oder nach links zeigt
        Modell m;
        m.schnee = 0;
        m.schmutz = 0;
        m.quader(m.teil("klotz", Stoff::Matt, 0xff909090), -10, 10, 0, 10, -10, 10, 3);
        for (double gier : {PI / 4, PI / 4 + PI, PI / 4 + PI / 2}) {
            const Ansichtsbild b = rendern(m, gier, 3, 120, 90, 60, 60, 1);
            // links der Mitte liegt die nach links zeigende Seitenfläche, rechts die nach rechts zeigende
            QVERIFY2(hell(b, 22, 50) > hell(b, 70, 98) + 12, qPrintable(QString("gier %1: %2 / %3").arg(gier).arg(hell(b, 22, 50)).arg(hell(b, 70, 98))));
        }
        // gedreht um 180° sieht der symmetrische Klotz genau gleich aus (nicht gespiegelt beleuchtet)
        const Ansichtsbild a = rendern(m, 0.4, 3, 120, 90, 60, 60, 1), c = rendern(m, 0.4 + PI, 3, 120, 90, 60, 60, 1);
        int anders = 0;
        for (size_t i = 0; i < a.rgba.size(); ++i) anders += std::abs(int(a.rgba[i]) - int(c.rgba[i])) > 6;
        QVERIFY2(anders < int(a.rgba.size() / 200), qPrintable(QString::number(anders)));
    }
    void schneeLiegtNurAufWaagerechtenFlaechen() {
        Modell m;
        m.schnee = 1;
        m.schmutz = 0;
        m.quader(m.teil("klotz", Stoff::Lack, 0xff2040c0), -10, 10, 0, 6, -10, 10);
        const Ansichtsbild b = rendern(m, 0.5, 4, 200, 120, 100, 80, 1);
        // Deckel (oben im Bild) weiß, Seitenfläche (unten) blau
        const P2 oben = projizieren({0, 6, 0}, 0.5), seite = projizieren({0, 3, 10}, 0.5);
        const uint8_t *po = &b.rgba[size_t(4 * (int(80 + oben.y * 4) * 200 + int(100 + oben.x * 4)))];
        // Seitenmitte: Punkt auf der dem Betrachter zugewandten Fläche
        const P2 vorn = projizieren({4, 3, 10}, 0.5);
        const uint8_t *ps = &b.rgba[size_t(4 * (int(80 + vorn.y * 4) * 200 + int(100 + vorn.x * 4)))];
        Q_UNUSED(seite);
        QVERIFY2(po[0] > 200 && po[1] > 200 && po[2] > 200, qPrintable(QString("%1 %2 %3").arg(po[0]).arg(po[1]).arg(po[2])));
        QVERIFY2(ps[2] > ps[0] + 40, qPrintable(QString("%1 %2 %3").arg(ps[0]).arg(ps[1]).arg(ps[2])));
    }
    void kettengliederHabenEineLaufrichtung() {
        // ein Glied weiter sieht die Kette wieder gleich aus …
        for (double u = 0; u < 20; u += 0.37) QCOMPARE(kettenMuster(u, 0.0), kettenMuster(u, 1.0));
        // … und jede Phase ist das um ein Sechstel Glied weitergerückte Muster (also keine bloße Vertauschung)
        for (int ph = 0; ph < KETTENPHASEN; ++ph)
            for (double u = 0; u < 20; u += 0.37)
                QCOMPARE(kettenMuster(u, double(ph) / KETTENPHASEN), kettenMuster(u - ph * KETTEN_TEILUNG / KETTENPHASEN, 0.0));
        // das Muster ist unsymmetrisch (Steg vorn, Fuge hinten): vorwärts und rückwärts unterscheiden sich
        int anders = 0;
        for (double u = 0.05; u < KETTEN_TEILUNG; u += 0.1) anders += kettenMuster(u, 0) != kettenMuster(KETTEN_TEILUNG - u, 0);
        QVERIFY(anders > 10);
        // gerastert: jede der sechs Phasen ergibt ein anderes Bild der Kette
        std::set<QByteArray> bilder;
        for (int ph = 0; ph < KETTENPHASEN; ++ph) {
            const Ansichtsbild b = rendern(raupeKette(1, ph), 0.6, 2, 168, 72, 84, 54, 1);
            bilder.insert(QByteArray(reinterpret_cast<const char *>(b.rgba.data()), qsizetype(b.rgba.size())));
        }
        QCOMPARE(bilder.size(), size_t(KETTENPHASEN));
        QVERIFY(sichtbar(raupeKette(1, 0), 0, "Triebrad") > 100);
        QVERIFY(sichtbar(raupeKette(1, 0), 0, "Laufrolle") > 60);
        QCOMPARE(sichtbar(raupeKette(1, 0), PI, "Triebrad"), 0);     // Räder sitzen außen: von innen sieht man sie nicht
    }
    void raupeZeigtVonVornDasSchildUndVonHintenDieRuecklichter() {
        const Modell k = raupeKoerper(true), s = raupeSchild();
        QVERIFY(sichtbar(s, PI / 2, "Schild") > 2500);              // Arbeitsfläche in voller Breite
        QVERIFY(sichtbar(s, PI / 2, "Schneidkante") > 100);
        QVERIFY(sichtbar(s, 3 * PI / 2, "Schild") < 200);           // von hinten nur der Rücken (und die Oberkante des Blatts)
        QVERIFY(sichtbar(s, 3 * PI / 2, "Schildruecken") > 2000);
        QVERIFY(sichtbar(s, 5.4, "Kolbenstange") > 5);             // Hydraulikzylinder
        QVERIFY(sichtbar(k, 3 * PI / 2, "Ruecklicht") > 20);
        QCOMPARE(sichtbar(k, PI / 2, "Ruecklicht"), 0);
        QVERIFY(sichtbar(k, PI / 2, "Kuehler") > 100);
        QCOMPARE(sichtbar(k, 3 * PI / 2, "Kuehler"), 0);
        QVERIFY(sichtbar(k, PI / 2, "Scheinwerfer") > 8);
        QVERIFY(sichtbar(k, 0, "Auspuff") > 20);
        QVERIFY(sichtbar(k, 0, "Spiegel") > 5);
        // der Fahrer sitzt hinter Glas und ist aus jeder Richtung zu sehen; ohne Fahrer ist die Kabine leer
        for (double g : {0.0, PI / 2, PI, 3 * PI / 2}) QVERIFY2(sichtbar(k, g, "Fahrerkopf") + sichtbar(k, g, "Helm") > 30, qPrintable(QString::number(g)));
        QCOMPARE(sichtbar(raupeKoerper(false), 0, "Helm"), 0);
        QCOMPARE(passtInRahmen(k, RAHMEN_RAUPE), QString());
        QCOMPARE(passtInRahmen(s, RAHMEN_RSCHILD), QString());
        QCOMPARE(passtInRahmen(raupeKette(1, 0), RAHMEN_KETTE), QString());
        QCOMPARE(passtInRahmen(raupeKette(-1, 3), RAHMEN_KETTE), QString());
    }
    void pflugIstEinRadfahrzeugMitFahrerhaus() {
        const Modell k = pflugKoerper(true);
        QVERIFY(sichtbar(k, PI / 2, "Scheinwerfer") > 6);
        QVERIFY(sichtbar(k, 3 * PI / 2, "Ruecklicht") > 10);
        QCOMPARE(sichtbar(k, 3 * PI / 2, "Scheinwerfer"), 0);
        QVERIFY(sichtbar(k, 0, "Pritsche") > 200);
        for (double g : {0.0, PI / 2, PI}) QVERIFY(sichtbar(k, g, "Fahrerkopf") + sichtbar(k, g, "Helm") > 12);
        QCOMPARE(passtInRahmen(k, RAHMEN_PFLUG), QString());
        QCOMPARE(passtInRahmen(pflugSchild(), RAHMEN_PSCHILD), QString());
        // Rad: rund, von der Seite Felge und Reifen, jede Phase ein anderes Bild
        QCOMPARE(passtInRahmen(pflugRad(0), RAHMEN_RAD), QString());
        std::set<QByteArray> bilder;
        for (int ph = 0; ph < RADPHASEN; ++ph) {
            const Ansichtsbild b = rendern(pflugRad(ph), 0, 4, 64, 64, 32, 32, 1);
            bilder.insert(QByteArray(reinterpret_cast<const char *>(b.rgba.data()), qsizetype(b.rgba.size())));
        }
        QCOMPARE(bilder.size(), size_t(RADPHASEN));
        // von vorn ist das Rad schmal (Lauffläche), von der Seite rund
        const Ansichtsbild seite = rendern(pflugRad(0), 0, 4, 64, 64, 32, 32, 1), front = rendern(pflugRad(0), PI / 2, 4, 64, 64, 32, 32, 1);
        int a = 0, b = 0;
        for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) { a += seite.alpha(x, y) > 0; b += front.alpha(x, y) > 0; }
        QVERIFY2(b < a * 0.6 && b > a * 0.2, qPrintable(QString("%1 / %2").arg(a).arg(b)));
    }
    void hubschrauberMitKanzelPilotUndKennung() {
        const Modell h = heliKoerper();
        QCOMPARE(passtInRahmen(h, RAHMEN_HELI), QString());
        for (double g : {0.0, PI / 2, PI}) QVERIFY2(sichtbar(h, g, "Fahrerkopf") + sichtbar(h, g, "Helm") + sichtbar(h, g, "Fahrer") > 30, qPrintable(QString::number(g)));
        QVERIFY(sichtbar(h, 0, "Ausleger") > 300);
        QVERIFY(sichtbar(h, 0, "Leitwerk") > 150);
        QVERIFY(sichtbar(h, 0, "Kufe") > 40);
        QVERIFY(sichtbar(h, 0, "Triebwerk") > 100);
        // von vorn schmal, von der Seite lang
        auto breite = [&](double g) {
            const Ansichtsbild b = rendern(h, g, 1, 200, 84, 100, 64, 1);
            int l = b.w, r = 0;
            for (int y = 0; y < b.h; ++y) for (int x = 0; x < b.w; ++x) if (b.alpha(x, y)) { l = std::min(l, x); r = std::max(r, x); }
            return r - l + 1;
        };
        QVERIFY(breite(0) > 135);
        QVERIFY(breite(PI / 2) < 40);
        // Kanzel ist durchscheinend: dort ist das Bild teilweise durchsichtig oder zeigt den Innenraum
        const Ansichtsbild b = rendern(h, 0, 2, 400, 168, 200, 128, 2);
        const P2 q = projizieren({40, 26, 11}, 0);
        const int alpha = b.alpha(int(200 + q.x * 2), int(128 + q.y * 2));
        QVERIFY2(alpha > 60, qPrintable(QString::number(alpha)));
    }
    void szeneNimmtInJederLageDieselbenModellschichten() {
        // kein Wechsel zwischen „Seitenbild“ und „Drehbild“: geradeaus und in der Wende kommen dieselben Schichten zum Einsatz
        for (int art : {0, 1}) {
            const Teil koerper = art == 0 ? Teil::RaupeKoerper : Teil::PflugKoerper;
            for (double gier : {0.0, 0.8, PI / 2, PI, 4.0}) {
                Szene s;
                szeneAnsicht(art, gier, 0, 0, 3, s);
                int koerperTeile = 0, laufwerk = 0, schild = 0;
                for (const Zeichenteil &z : s.teile) {
                    koerperTeile += z.teil == int(koerper) + winkelstufe(gier);
                    if (art == 0) laufwerk += z.teil >= int(Teil::RaupeKetteR) && z.teil < int(Teil::RaupeSchild);
                    else laufwerk += z.teil >= int(Teil::PflugRad) && z.teil < int(Teil::PflugSchild);
                    schild += z.teil == int(art == 0 ? Teil::RaupeSchild : Teil::PflugSchild) + winkelstufe(gier);
                }
                QCOMPARE(koerperTeile, 1);
                QCOMPARE(laufwerk, art == 0 ? 2 : 4);
                QCOMPARE(schild, 1);
            }
        }
        // Reihenfolge: von der Seite liegt die ferne Kette hinter dem Körper, die nahe davor
        Szene s;
        szeneAnsicht(0, 0, 0, 0, 0, s);
        int iKoerper = -1, iRechts = -1, iLinks = -1;
        for (size_t i = 0; i < s.teile.size(); ++i) {
            const int t = s.teile[i].teil;
            if (t == int(Teil::RaupeKoerper)) iKoerper = int(i);
            else if (t >= int(Teil::RaupeKetteR) && t < int(Teil::RaupeKetteL)) iRechts = int(i);
            else if (t >= int(Teil::RaupeKetteL) && t < int(Teil::RaupeSchild)) iLinks = int(i);
        }
        QVERIFY(iLinks < iKoerper && iKoerper < iRechts);
        // Hubschrauber: Rotor in jeder Gierlage über dem Rumpf gezeichnet
        for (double gier : {0.0, 1.0, PI / 2, 3.0, 5.0}) {
            Szene h;
            szeneAnsicht(2, gier, 0, 0, 0.1, h);
            int rumpf = -1, rotor = -1;
            for (size_t i = 0; i < h.teile.size(); ++i) {
                if (h.teile[i].teil == int(Teil::HeliKoerper) + winkelstufe(gier)) rumpf = int(i);
                if (h.teile[i].teil >= int(Teil::HeliRotor0) && h.teile[i].teil <= int(Teil::HeliRotor3)) rotor = int(i);
            }
            QVERIFY(rumpf >= 0 && rotor > rumpf);
        }
    }
};

QTEST_MAIN(TestAnsicht)
#include "test_ansicht.moc"
