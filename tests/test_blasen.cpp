// Blasenvorrat: jeder Satz wird einmal gemalt und danach wiederverwendet; wenige Plätze reichen für beliebig viele Sätze.
#include <QtTest>
#include "blasen.h"

class TestBlasen : public QObject {
    Q_OBJECT
private slots:
    void gleicherSatzWirdNurEinmalGemalt() {
        Blasenpool pool(4);
        pool.bildBeginn();
        const Blasenfeld a = pool.feld("Hey! Ich arbeite hier!", false, 1.5);
        QCOMPARE(pool.gemalt(), 1);
        QVERIFY(pool.geaendert());
        QVERIFY(!pool.geaendert());                    // Textur muss nur einmal neu geladen werden
        for (int bild = 0; bild < 100; ++bild) {
            pool.bildBeginn();
            const Blasenfeld &b = pool.feld("Hey! Ich arbeite hier!", false, 1.5);
            QCOMPARE(b.y, a.y);
            QCOMPARE(b.w, a.w);
        }
        QCOMPARE(pool.gemalt(), 1);
        QVERIFY(!pool.geaendert());
        // gespiegelt (Schwanz rechts) ist ein eigenes Bild, der Schwanz zeigt dann rechts unten
        pool.bildBeginn();
        const Blasenfeld g = pool.feld("Hey! Ich arbeite hier!", true, 1.5);
        QCOMPARE(pool.gemalt(), 2);
        QVERIFY(g.y != a.y);
        QVERIFY(a.px < 10 && g.px > g.bw - 10);
        QVERIFY(std::abs(a.py - a.bh) < 2);
    }
    void plaetzeWerdenFuerNeueSaetzeWiederverwendet() {
        Blasenpool pool(3);
        QSet<int> zeilen;
        // zehn verschiedene Sätze nacheinander: nie mehr als drei Plätze, der älteste wird überschrieben
        for (int i = 0; i < 10; ++i) {
            pool.bildBeginn();
            zeilen.insert(pool.feld("Satz Nummer " + std::to_string(i), false, 1.0).y);
        }
        QCOMPARE(zeilen.size(), 3);
        QCOMPARE(pool.gemalt(), 10);
        QCOMPARE(pool.bild().height(), 3 * Blasenpool::PLATZ_HOEHE);
        // die drei zuletzt gezeigten stehen noch da (kein Neumalen), ein älterer muss neu gemalt werden
        pool.bildBeginn();
        pool.feld("Satz Nummer 9", false, 1.0);
        pool.feld("Satz Nummer 8", false, 1.0);
        pool.feld("Satz Nummer 7", false, 1.0);
        QCOMPARE(pool.gemalt(), 10);
        pool.bildBeginn();
        pool.feld("Satz Nummer 2", false, 1.0);
        QCOMPARE(pool.gemalt(), 11);
    }
    void zweiBlasenImSelbenBildVerdraengenSichNicht() {
        Blasenpool pool(2);
        pool.bildBeginn();
        pool.feld("alt eins", false, 1.0);
        pool.feld("alt zwei", false, 1.0);
        // nächstes Bild: Fahrer und Pilot sprechen gleichzeitig etwas Neues – beide brauchen einen eigenen Platz
        pool.bildBeginn();
        const int fahrer = pool.feld("Fahrer sagt etwas", false, 1.0).y;
        const int pilot = pool.feld("Pilot sagt etwas", false, 1.0).y;
        QVERIFY(fahrer != pilot);
        // und im selben Bild noch einmal gefragt, stehen beide unverändert da
        QCOMPARE(pool.feld("Fahrer sagt etwas", false, 1.0).y, fahrer);
        QCOMPARE(pool.feld("Pilot sagt etwas", false, 1.0).y, pilot);
        QCOMPARE(pool.gemalt(), 4);
    }
    void langeSaetzeBrechenUmUndBleibenImPlatz() {
        Blasenpool pool;
        pool.bildBeginn();
        const Blasenfeld kurz = pool.feld("Moin!", false, 2.0);
        const Blasenfeld lang = pool.feld("Wenn du noch einmal auf meine frisch gewaschene Raupe wirfst, fahre ich heim!", false, 2.0);
        QVERIFY(lang.bh > kurz.bh + 10);                       // mehrere Zeilen
        QVERIFY(lang.bw > kurz.bw);
        QVERIFY(lang.bw <= Blasenpool::PLATZ_BREITE && lang.bh <= Blasenpool::PLATZ_HOEHE);
        QVERIFY(lang.w <= pool.bild().width());
        // Text ist wirklich gemalt: im Feld gibt es dunkle Schrift auf heller Blase
        const QImage &b = pool.bild();
        int dunkel = 0, hell = 0;
        for (int y = lang.y; y < lang.y + lang.h; ++y)
            for (int x = lang.x; x < lang.x + lang.w; ++x) {
                const QColor c = b.pixelColor(x, y);
                if (c.alpha() < 250) continue;
                dunkel += c.lightness() < 80;
                hell += c.lightness() > 220;
            }
        QVERIFY2(dunkel > 300 && hell > 3000, qPrintable(QString("%1 dunkel, %2 hell").arg(dunkel).arg(hell)));
        // schärfer bei höherer Skalierung: doppelt so viele Bildpunkte je Einheit
        Blasenpool fein;
        fein.bildBeginn();
        QCOMPARE(fein.feld("Moin!", false, 4.0).w, kurz.w * 2);
        // andere Skalierung im selben Vorrat: alles wird neu angelegt
        pool.bildBeginn();
        pool.feld("Moin!", false, 1.0);
        QCOMPARE(pool.bild().width(), Blasenpool::PLATZ_BREITE);
    }
};

QTEST_MAIN(TestBlasen)
#include "test_blasen.moc"
