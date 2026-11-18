// Bildtakt: Bildzeiten-Messung, Takt-Teiler je Monitor, Fortschreiben der Flocken zwischen zwei
// Simulationsschritten (wie im Vertex-Shader) und die Einstellung „Bildrate“.
#include <QtTest>
#include <cmath>
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
};

QTEST_GUILESS_MAIN(TestBildtakt)
#include "test_bildtakt.moc"
