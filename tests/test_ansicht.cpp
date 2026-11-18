// 3D-Ansicht fürs Wenden: Projektion, Winkelstufen, Sichtbarkeit, weltfestes Licht, Modelle.
#include <QtTest>
#include <cmath>
#include "ansicht3d.h"
#include "fahrzeugmodell.h"

using namespace schnee;

namespace {
const double PI = 3.14159265358979;
// Breite und Höhe der Ansicht in Einheiten
struct Ausmass { double links = 1e9, rechts = -1e9, oben = 1e9, unten = -1e9; double breite() const { return rechts - links; } };
Ausmass ausmass(const Modell &m, double gier) {
    Ausmass a;
    for (const Sichtflaeche &s : ansicht(m, gier))
        for (const P2 &p : s.ecken) { a.links = std::min(a.links, p.x); a.rechts = std::max(a.rechts, p.x); a.oben = std::min(a.oben, p.y); a.unten = std::max(a.unten, p.y); }
    return a;
}
// sichtbare (ungefähre) Bildfläche aller Flächen dieser Farbe
double sichtbar(const Modell &m, double gier, uint32_t farbe) {
    double summe = 0;
    for (const Sichtflaeche &s : ansicht(m, gier)) {
        if (s.quelle->farbe != farbe) continue;
        double a = 0;
        for (size_t i = 0; i < s.ecken.size(); ++i) {
            const P2 &p = s.ecken[i], &q = s.ecken[(i + 1) % s.ecken.size()];
            a += p.x * q.y - q.x * p.y;
        }
        summe += std::abs(a) / 2;
    }
    return summe;
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
        // Höhe bleibt Höhe (oben = kleineres y im Bild), unabhängig vom Gierwinkel
        for (double g : {0.0, 1.0, 2.5, 4.0}) QVERIFY(projizieren({0, 20, 0}, g).y < projizieren({0, 0, 0}, g).y - 19);
        // Blick leicht von oben: was näher ist, liegt im Bild tiefer
        QVERIFY(projizieren({0, 0, 10}, 0).y > projizieren({0, 0, -10}, 0).y);
    }
    void winkelstufenDeckenDenVollkreis() {
        QCOMPARE(winkelstufe(0), 0);
        QCOMPARE(winkelstufe(PI), WINKELSTUFEN / 2);
        QCOMPARE(winkelstufe(PI / 2), WINKELSTUFEN / 4);
        QCOMPARE(winkelstufe(2 * PI - 0.01), 0);            // rundet über den Nullpunkt
        QCOMPARE(winkelstufe(-PI / 2), 3 * WINKELSTUFEN / 4);
        QCOMPARE(winkelstufe(2 * PI + PI / 2), WINKELSTUFEN / 4);
        for (int i = 0; i < WINKELSTUFEN; ++i) QCOMPARE(winkelstufe(stufenWinkel(i)), i);
        // höchstens eine halbe Stufe daneben
        for (double g = 0; g < 2 * PI; g += 0.01) QVERIFY(std::abs(stufenWinkel(winkelstufe(g)) - g) <= PI / WINKELSTUFEN + 1e-9 || g > 2 * PI - PI / WINKELSTUFEN);
    }
    void quaderZeigtNurZugewandteSeitenVonHintenNachVorn() {
        Modell m;
        m.quader(-10, 10, 0, 5, -3, 3, 0xff112233);
        QCOMPARE(m.flaechen.size(), size_t(6));
        // genau von der Seite (und leicht von oben): Seite und Deckel
        QCOMPARE(ansicht(m, 0).size(), size_t(2));
        // schräg: Seite, Front und Deckel
        const auto schraeg = ansicht(m, PI / 4);
        QCOMPARE(schraeg.size(), size_t(3));
        for (size_t i = 1; i < schraeg.size(); ++i) QVERIFY(schraeg[i - 1].tiefe <= schraeg[i].tiefe);
        // der Boden ist nie zu sehen
        for (double g = 0; g < 2 * PI; g += 0.3)
            for (const Sichtflaeche &s : ansicht(m, g)) QVERIFY(normale(*s.quelle).y > -0.5);
        // Länge verkürzt sich beim Drehen: von vorn ist nur noch die Breite zu sehen
        QVERIFY(std::abs(ausmass(m, 0).breite() - 20) < 1e-6);
        QVERIFY(std::abs(ausmass(m, PI / 2).breite() - 6) < 1e-6);
        QVERIFY(ausmass(m, PI / 4).breite() < 20 && ausmass(m, PI / 4).breite() > 6);
    }
    void lichtIstWeltfestUndDrehtNichtMit() {
        Modell m;
        const size_t q = m.quader(-10, 10, 0, 5, -3, 3, 0xff112233);
        auto hell = [&](double gier, size_t flaeche) {
            for (const Sichtflaeche &s : ansicht(m, gier)) if (s.quelle == &m.flaechen[flaeche]) return s.hell;
            return -1.0;
        };
        // Deckel: bei jedem Gierwinkel gleich hell, und heller als jede Seite
        const double deckel = hell(0, q + 2);
        for (double g = 0; g < 2 * PI; g += 0.4) QVERIFY(std::abs(hell(g, q + 2) - deckel) < 1e-9);
        QVERIFY(deckel > hell(0, q + 4));
        // die dem Betrachter zugewandte Seite ist immer gleich hell – egal, welche Fahrzeugseite es ist
        QVERIFY(std::abs(hell(0, q + 4) - 1.0) < 1e-9);
        QVERIFY(std::abs(hell(PI, q + 5) - 1.0) < 1e-9);
        // Licht kommt von links: zeigt die Front im Bild nach links (Gier knapp über π/2 … π), ist sie heller als nach rechts
        QVERIFY(hell(3 * PI / 4, q + 0) > hell(PI / 4, q + 0) + 0.1);
        // und dieselbe Bildrichtung ist gleich hell, ob Front oder Heck dorthin zeigt
        QVERIFY(std::abs(hell(3 * PI / 4, q + 0) - hell(7 * PI / 4, q + 1)) < 1e-9);
    }
    void raupeZeigtVonVornDasSchildUndVonHintenDieRuecklichter() {
        const Modell m = raupenModell(0);
        // Front: Schild breit und blank, keine Rücklichter
        QVERIFY(sichtbar(m, PI / 2, FARBE_SCHILD_STAHL) > 300);
        QCOMPARE(sichtbar(m, PI / 2, FARBE_RUECKLICHT), 0.0);
        // Heck: Rücklichter, Schildfläche abgewandt, kein Kühlergrill
        QVERIFY(sichtbar(m, 3 * PI / 2, FARBE_RUECKLICHT) > 10);
        QCOMPARE(sichtbar(m, 3 * PI / 2, FARBE_SCHILD_STAHL), 0.0);
        QCOMPARE(sichtbar(m, 3 * PI / 2, FARBE_KUEHLER), 0.0);
        // von der Seite: lang und schmal im Rahmen; von vorn viel kürzer, aber so breit wie das Schild
        const Ausmass seite = ausmass(m, 0), front = ausmass(m, PI / 2);
        QVERIFY(seite.breite() > 68);
        QVERIFY(front.breite() < 42 && front.breite() > 34);
        // Seitenansicht gleicht dem Seitenbild: Aufbau und Laufwerk tragen die gemalten Bilder
        bool aufbau = false, kette = false;
        for (const Sichtflaeche &s : ansicht(m, 0)) { aufbau |= s.quelle->textur == BildRaupeAufbau; kette |= s.quelle->textur == BildRaupeKette; }
        QVERIFY(aufbau && kette);
        // alles passt bei jedem Winkel in den Bildrahmen
        for (int i = 0; i < WINKELSTUFEN; ++i) {
            const Ausmass a = ausmass(m, stufenWinkel(i));
            QVERIFY2(a.links >= -RAHMEN_RAUPE.px && a.rechts <= RAHMEN_RAUPE.w - RAHMEN_RAUPE.px && a.oben >= -RAHMEN_RAUPE.py
                     && a.unten <= RAHMEN_RAUPE.h - RAHMEN_RAUPE.py, qPrintable(QString("Stufe %1: %2 … %3, %4 … %5").arg(i).arg(a.links).arg(a.rechts).arg(a.oben).arg(a.unten)));
        }
    }
    void kettengliederWechselnMitDerPhase() {
        const Modell a = raupenModell(0), b = raupenModell(1);
        QCOMPARE(a.flaechen.size(), b.flaechen.size());
        int anders = 0, glieder = 0;
        for (size_t i = 0; i < a.flaechen.size(); ++i) {
            const bool glied = a.flaechen[i].farbe == FARBE_KETTE_HELL || a.flaechen[i].farbe == FARBE_KETTE_DUNKEL;
            glieder += glied;
            if (a.flaechen[i].farbe != b.flaechen[i].farbe) { ++anders; QVERIFY(glied); }
        }
        QVERIFY(glieder >= 100);     // zwei Ketten mit je 52 Gliedern
        QCOMPARE(anders, 104);       // jedes Glied wechselt hell/dunkel: die Kette „läuft“
    }
    void pflugUndHubschrauberPassenInIhreRahmen() {
        const Modell p = pflugModell(0), h = heliModell();
        for (int i = 0; i < WINKELSTUFEN; ++i) {
            const Ausmass a = ausmass(p, stufenWinkel(i)), b = ausmass(h, stufenWinkel(i));
            QVERIFY2(a.links >= -RAHMEN_PFLUG.px && a.rechts <= RAHMEN_PFLUG.w - RAHMEN_PFLUG.px && a.oben >= -RAHMEN_PFLUG.py
                     && a.unten <= RAHMEN_PFLUG.h - RAHMEN_PFLUG.py, qPrintable(QString("Pflug Stufe %1: %2 … %3, %4 … %5").arg(i).arg(a.links).arg(a.rechts).arg(a.oben).arg(a.unten)));
            QVERIFY2(b.links >= -RAHMEN_HELI.px && b.rechts <= RAHMEN_HELI.w - RAHMEN_HELI.px && b.oben >= -RAHMEN_HELI.py
                     && b.unten <= RAHMEN_HELI.h - RAHMEN_HELI.py, qPrintable(QString("Heli Stufe %1: %2 … %3, %4 … %5").arg(i).arg(b.links).arg(b.rechts).arg(b.oben).arg(b.unten)));
        }
        // Hubschrauber von vorn: schmal (Rumpfbreite plus Kufen), von der Seite lang
        QVERIFY(ausmass(h, PI / 2).breite() < 40);
        QVERIFY(ausmass(h, 0).breite() > 130);
        // Pflug von vorn: Scheinwerfer zu sehen, von hinten Rücklichter
        QVERIFY(sichtbar(p, PI / 2, FARBE_SCHEINWERFER) > 2);
        QVERIFY(sichtbar(p, 3 * PI / 2, FARBE_RUECKLICHT) > 2);
        QCOMPARE(sichtbar(p, 3 * PI / 2, FARBE_SCHEINWERFER), 0.0);
    }
};

QTEST_MAIN(TestAnsicht)
#include "test_ansicht.moc"
