// Bildtakt: Bildzeiten-Messung, Takt-Teiler je Monitor, Fortschreiben der Flocken zwischen zwei
// Simulationsschritten (wie im Vertex-Shader) und die Einstellung „Bildrate“.
#include <QtTest>
#include <cmath>
#include <random>
#include "bildtakt.h"
#include "einstellungen.h"
#include "flockenbild.h"
#include "schnee.h"

using namespace schnee;

class TestBildtakt : public QObject {
    Q_OBJECT
private Q_SLOTS:
    // ---- Bildzeiten ----
    void gleichmaessigeBilderHabenKeinenJitterUndKeineVerpassten() {
        Bildzeiten z;
        z.setzeSoll(1.0 / 60);
        for (int i = 0; i <= 120; ++i) z.bild(10.0 + i / 60.0);
        const Bildzeiten::Bericht b = z.abholen();
        QCOMPARE(b.bilder, 120);
        QVERIFY(std::abs(b.mittelMs - 16.667) < 0.01);
        QVERIFY(b.jitterMs < 0.01);
        QCOMPARE(b.verpasst, 0);
        QVERIFY(std::abs(b.fps - 60) < 0.1);
        QCOMPARE(b.stufen[2], 120); // alle im Band 0,9 … 1,1 × Soll
    }
    void verpassteBilderUndAusreisserWerdenGezaehlt() {
        Bildzeiten z;
        z.setzeSoll(0.010);
        double t = 0;
        z.bild(t);
        for (int i = 0; i < 97; ++i) z.bild(t += 0.010);
        z.bild(t += 0.020); // ein Bild verpasst
        z.bild(t += 0.031); // zwei verpasst
        z.bild(t += 0.004); // zu früh
        const Bildzeiten::Bericht b = z.abholen();
        QCOMPARE(b.bilder, 100);
        QCOMPARE(b.verpasst, 2);
        QCOMPARE(b.stufen[0], 1);  // < 0,5 × Soll
        QCOMPARE(b.stufen[4], 1);  // 1,5 … 2,5 ×
        QCOMPARE(b.stufen[5], 1);  // > 2,5 ×
        QVERIFY(std::abs(b.maxMs - 31) < 0.01);
        QVERIFY(std::abs(b.p50Ms - 10) < 0.01);
        QVERIFY(b.jitterMs > 1.0);
        // abholen setzt zurück, der Abstand zum letzten Bild zählt weiter
        z.bild(t += 0.010);
        QCOMPARE(z.abholen().bilder, 1);
    }
    void langePauseZaehltNichtAlsBild() {
        // nach Pause/Vollbild (Ebene verborgen) beginnt die Messung neu
        Bildzeiten z;
        z.setzeSoll(0.010);
        z.bild(0);
        z.bild(0.010);
        z.bild(5.0);
        z.bild(5.010);
        QCOMPARE(z.abholen().bilder, 2);
    }

    // ---- Teiler: jedes wievielte Monitorbild gezeichnet wird ----
    void teilerHaeltDieObergrenzeGleichmaessigEin_data() {
        QTest::addColumn<double>("hz");
        QTest::addColumn<int>("grenze");
        QTest::addColumn<int>("teiler");
        QTest::newRow("auto 165") << 165.06 << 0 << 1;
        QTest::newRow("auto 60") << 60.0 << 0 << 1;
        QTest::newRow("60 auf 60") << 60.0 << 60 << 1;
        QTest::newRow("60 auf 59,95") << 59.95 << 60 << 1;
        QTest::newRow("60 auf 144") << 143.98 << 60 << 3;
        QTest::newRow("60 auf 165") << 165.06 << 60 << 3;
        QTest::newRow("30 auf 60") << 60.0 << 30 << 2;
        QTest::newRow("120 auf 144") << 144.0 << 120 << 2;
        QTest::newRow("144 auf 144") << 144.0 << 144 << 1;
        QTest::newRow("unbekannte Rate") << 0.0 << 60 << 1;
    }
    void teilerHaeltDieObergrenzeGleichmaessigEin() {
        QFETCH(double, hz);
        QFETCH(int, grenze);
        QFETCH(int, teiler);
        QCOMPARE(bildTeiler(hz, grenze), teiler);
    }

    // ---- Fortschreiben der Flocken zwischen zwei Simulationsschritten ----
    // Die Ebene baut die Flocken nur 30-mal je Sekunde neu; dazwischen schiebt und dreht der
    // Vertex-Shader sie weiter (viereckEcken). Das muss dort landen, wo die Simulation sie hätte.
    void fortgeschriebeneFlockeLiegtWoDieSimulationSieHat() {
        std::mt19937 zufall(7);
        std::uniform_real_distribution<double> g(0, 1);
        const double h = 1.0 / 30;
        double groessterFehler = 0, ohneFortschreiben = 0;
        for (int i = 0; i < 400; ++i) {
            Flocke f{};
            f.x = 500 + 400 * g(zufall);
            f.y = 300 + 200 * g(zufall);
            f.vx = -40 + 80 * g(zufall);
            f.vy = 20 + 60 * g(zufall);
            f.groesse = 1.5 + 3 * g(zufall);
            f.phase = 100 * g(zufall);
            f.los = uint32_t(zufall());
            const Flockenbild b0 = flockenbild(f, 3.0, true);
            const FlockenViereck v0 = flockenViereck(f, b0);
            float jetzt[4][2], ohne[4][2];
            viereckEcken(v0, float(h), jetzt);
            viereckEcken(v0, 0.f, ohne);
            // Simulation in feinen Schritten (wie bei 165 Hz)
            Flocke s = f;
            for (int k = 0; k < 6; ++k) flockeBewegen(s, h / 6);
            const FlockenViereck v1 = flockenViereck(s, flockenbild(s, 3.0, true));
            float soll[4][2];
            viereckEcken(v1, 0.f, soll);
            for (int e = 0; e < 4; ++e) {
                groessterFehler = std::max(groessterFehler, double(std::hypot(jetzt[e][0] - soll[e][0], jetzt[e][1] - soll[e][1])));
                ohneFortschreiben = std::max(ohneFortschreiben, double(std::hypot(ohne[e][0] - soll[e][0], ohne[e][1] - soll[e][1])));
            }
        }
        // ohne Fortschreiben springt die Flocke um ein ganzes Stück, fortgeschrieben bleibt sie unter einem Viertelpixel
        QVERIFY2(ohneFortschreiben > 2.0, qPrintable(QString::number(ohneFortschreiben)));
        QVERIFY2(groessterFehler < 0.25, qPrintable(QString::number(groessterFehler)));
    }
    void drehungIstStufenlos() {
        // Winkel benachbarter Zeitpunkte unterscheiden sich stetig, nicht in Tabellenstufen
        Flocke f{};
        f.groesse = 4;
        f.los = 99;
        double vorher = flockenbild(f, 3.0, true).winkel;
        double kleinsterSchritt = 1e9, groessterSchritt = 0;
        for (int i = 1; i <= 200; ++i) {
            flockeBewegen(f, 1.0 / 165);
            const double w = flockenbild(f, 3.0, true).winkel;
            const double schritt = std::abs(std::remainder(w - vorher, 6.283185307179586));
            kleinsterSchritt = std::min(kleinsterSchritt, schritt);
            groessterSchritt = std::max(groessterSchritt, schritt);
            vorher = w;
        }
        QVERIFY(groessterSchritt < 0.01);
        QVERIFY(kleinsterSchritt > 0.3 * groessterSchritt); // gleichmäßig
        // Kosinus aus der Tabelle: stetig zwischen den Stützstellen
        QVERIFY(std::abs(kosinusSchnell(0.0031f) - float(std::cos(0.0031))) < 1e-4f);
        QVERIFY(std::abs(sinusSchnell(1.2345f) - float(std::sin(1.2345))) < 1e-4f);
    }

    // Henry 18.11.: „Der Schnee ist wieder unter den Fenstern.“ Im Modus fest darf keine Flocke – auch nicht
    // fortgeschrieben bis zum nächsten Aufbau – in einem Fenster gezeichnet werden, und unter einem Fenster
    // kommt nur an, was seitlich vorbeifällt. Gestartet wird wie im Programm: erst vorfüllen, dann kommt
    // die Fensterliste von KWin.
    void festKeineFlockeImOderUnterDemFenster() {
        Einstellungen e;
        e.wind = 0;
        e.menge = 3;
        e.fallGeschw = 60;
        Simulation s(e, 11);
        s.setBildschirme({{0, 0, 1600, 1000}});
        s.vorfuellen(); // Fensterliste noch unbekannt
        const Rechteck w{400, 200, 800, 500};
        s.setFenster({Fenster{"a", w, 1}});
        double tiefstes = 0; // wie weit die Mitte einer gezeichneten Flocke höchstens ins Fenster ragt
        int unter = 0;
        for (int i = 0; i < 30 * 6; ++i) { // 6 s mit 30 Schritten je s wie in Ruhe
            s.schritt(1.0 / 30, true);
            for (const Flocke &f : s.flocken()) {
                if (!s.flockeZeichnen(f)) continue;
                const FlockenViereck v = flockenViereck(f, flockenbild(f, e.groesse, true));
                for (float h : {0.f, 1.f / 60, 1.f / 30}) {
                    const float x = v.mx + v.vx * h, y = v.my + v.vy * h;
                    if (w.enthaelt(x, y)) // Abstand zur nächsten Fensterkante
                        tiefstes = std::max(tiefstes, double(std::min({y - w.y, x - w.x, w.rechts() - x, w.unten() - y})));
                }
                // direkt unter dem Fenster (mit Abstand zu den Seiten, wo Flocken vorbeifallen)
                if (f.x > w.x + 40 && f.x < w.rechts() - 40 && f.y > w.unten() + 5 && f.y < w.unten() + 150) ++unter;
            }
        }
        QVERIFY2(tiefstes < 4.0, qPrintable(QString::number(tiefstes)));
        QVERIFY2(unter == 0, qPrintable(QString::number(unter)));
        QVERIFY(!s.flocken().empty());
    }

    // ---- Einstellung „Bildrate“ (auto = Bildwiederholrate des Monitors) ----
    void bildrateWirdGelesen_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("bildrate");
        QTest::newRow("leer: Standard 60") << "" << 60;
        QTest::newRow("alter Standard fps=30: 60") << "fps=30\n" << 60;
        QTest::newRow("eigener fps-Wert bleibt") << "fps=45\n" << 45;
        QTest::newRow("bildrate auto") << "fps=45\nbildrate=auto\n" << 0;
        QTest::newRow("bildrate 144") << "fps=30\nbildrate=144\n" << 144;
        QTest::newRow("bildrate begrenzt") << "bildrate=5000\n" << 360;
        QTest::newRow("kaputt: aus fps") << "fps=60\nbildrate=schnell\n" << 60;
    }
    void bildrateWirdGelesen() {
        QFETCH(QString, text);
        QFETCH(int, bildrate);
        QCOMPARE(einstellungenAusText(text.toStdString()).bildrate, bildrate);
    }
    void bildrateWirdErgaenztOhneFpsZuVerstellen() {
        const std::string alt = "menge=5\nfps=30\nan=true\n";
        QVERIFY(einstellungenBrauchtErgaenzung(alt));
        const Programmeinstellungen p = einstellungenAusText(alt);
        const std::string neu = einstellungenAlsText(p);
        QVERIFY(neu.find("\nbildrate=60\n") != std::string::npos);
        QVERIFY(neu.find("\nfps=30\n") != std::string::npos); // ältere Versionen lesen weiter fps
        QVERIFY(!einstellungenBrauchtErgaenzung(neu));
        Programmeinstellungen q = p;
        q.bildrate = 120;
        QCOMPARE(einstellungenAusText(einstellungenAlsText(q)).bildrate, 120);
        QCOMPARE(einstellungenAusText(einstellungenAlsText(q)).fps, 30);
        q.bildrate = 0;
        QVERIFY(einstellungenAlsText(q).find("\nbildrate=auto\n") != std::string::npos);
        QCOMPARE(einstellungenAusText(einstellungenAlsText(q)).bildrate, 0);
    }
};

QTEST_GUILESS_MAIN(TestBildtakt)
#include "test_bildtakt.moc"
