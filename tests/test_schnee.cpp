#include <QtTest>
#include "schnee.h"
#include "einstellungen.h"

using namespace schnee;

namespace {
Einstellungen ruhig() {
    Einstellungen e;
    e.wind = 0;
    e.ablageJeFlocke = 1.0;
    e.maxHoehe = 20;
    e.spaltenBreite = 4;
    return e;
}
Fenster fenster(const char *id, double x, double y, double w, double h, int stapel) {
    Fenster f; f.id = id; f.r = {x, y, w, h}; f.stapel = stapel; return f;
}
Flocke flocke(double x, double y) { return Flocke{x, y, 0, 60, 3, 0}; }
// höchste Deckenhöhe in der Umgebung von x (Flocken schaukeln ein paar px seitlich)
double hoeheUm(const Simulation &s, const char *id, double x, double radius = 8) {
    double m = 0;
    for (double t = x - radius; t <= x + radius; t += 1) m = std::max(m, s.hoeheBei(id, t));
    return m;
}
// lässt die Simulation ohne neue Flocken laufen
void laufen(Simulation &s, double sekunden) {
    for (double t = 0; t < sekunden; t += 1.0 / 60) s.schritt(1.0 / 60, false);
}
}

class TestSchnee : public QObject {
    Q_OBJECT
private slots:
    void flockeBleibtAufFensterkanteLiegen();
    void verdeckteKanteNimmtKeinenSchneeAuf();
    void deckeIstBegrenztUndRutschtAmRandAb();
    void deckeZerfaelltBeiVeraenderung_data();
    void deckeZerfaelltBeiVeraenderung();
    void schmalerWerdenWirftUeberstandAb();
    void bodenSammeltUndSchmilzt();
    void flockenNurAufMonitorflaechen();
    void taskleisteTraegtSchneeStattBoden();
};

void TestSchnee::flockeBleibtAufFensterkanteLiegen() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    s.setFenster({fenster("a", 100, 300, 400, 300, 1)});
    s.flockeHinzufuegen(flocke(250, 250));
    laufen(s, 2.0);
    QCOMPARE(int(s.flocken().size()), 0);
    QVERIFY(hoeheUm(s, "a", 250) > 0.5);
    QCOMPARE(s.gesamtSchnee("a"), 1.0);
}

void TestSchnee::verdeckteKanteNimmtKeinenSchneeAuf() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    // "b" liegt oben und überdeckt die Kante von "a" zwischen x=200 und x=400
    s.setFenster({fenster("a", 100, 300, 400, 300, 1), fenster("b", 200, 250, 200, 200, 2)});
    s.flockeHinzufuegen(flocke(300, 270));   // unterhalb von b's Kante, über a's verdeckter Kante
    s.flockeHinzufuegen(flocke(450, 250));   // über a's sichtbarem Teil
    laufen(s, 6.0);
    QCOMPARE(hoeheUm(s, "a", 300, 80), 0.0);
    QVERIFY(hoeheUm(s, "a", 450) > 0.5);
    // die andere Flocke fiel durch, statt auf dem verdeckten Teil liegen zu bleiben
    QCOMPARE(s.gesamtSchnee("a"), 1.0);
    // wird b danach nach unten gestapelt, ist die Kante wieder frei
    s.setFenster({fenster("a", 100, 300, 400, 300, 2), fenster("b", 200, 250, 200, 200, 1)});
    s.flockeHinzufuegen(flocke(300, 200));
    laufen(s, 2.0);
    QVERIFY(hoeheUm(s, "b", 300) > 0.5);   // b's Kante bleibt sichtbar (liegt höher)
    s.flockeHinzufuegen(flocke(450, 280));
    laufen(s, 2.0);
    QVERIFY(hoeheUm(s, "a", 450) > 1.5);
}

void TestSchnee::deckeIstBegrenztUndRutschtAmRandAb() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    s.setFenster({fenster("a", 100, 300, 40, 300, 1)});   // 10 Spalten à 4 px
    bool fielHerunter = false;
    for (int i = 0; i < 400; ++i) {
        s.flockeHinzufuegen(flocke(118, 200));         // immer dieselbe Spalte
        s.schritt(0.05, false);
        for (const Flocke &f : s.flocken())
            if ((f.x < 100 || f.x >= 140) && f.y > 290) fielHerunter = true;
    }
    laufen(s, 10.0);
    const Decke *d = s.decke("a");
    QVERIFY(d);
    float maxH = *std::max_element(d->hoehe.begin(), d->hoehe.end());
    QVERIFY2(maxH <= 20.0f + 1.0f, qPrintable(QString::number(maxH)));
    // Schnee ist zu den Nachbarn gerutscht, auch bis an die Ränder
    QVERIFY(d->hoehe.front() > 0.5f);
    QVERIFY(d->hoehe.back() > 0.5f);
    // ein Teil ist über die Fensterkanten heruntergefallen
    QVERIFY(fielHerunter);
    QVERIFY(s.gesamtSchnee("a") < 400.0);
}

void TestSchnee::deckeZerfaelltBeiVeraenderung_data() {
    QTest::addColumn<QString>("art");
    QTest::newRow("verschoben") << "verschoben";
    QTest::newRow("geschlossen") << "geschlossen";
    QTest::newRow("minimiert") << "minimiert";
    QTest::newRow("verdeckt") << "verdeckt";
}

void TestSchnee::deckeZerfaelltBeiVeraenderung() {
    QFETCH(QString, art);
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    s.setFenster({fenster("a", 100, 300, 400, 300, 1)});
    for (int i = 0; i < 20; ++i) s.flockeHinzufuegen(flocke(120 + i * 15, 250));
    laufen(s, 2.0);
    QCOMPARE(s.gesamtSchnee("a"), 20.0);
    QCOMPARE(int(s.flocken().size()), 0);

    if (art == "verschoben") s.setFenster({fenster("a", 150, 400, 400, 300, 1)});
    if (art == "geschlossen") s.setFenster({});
    if (art == "minimiert") { Fenster f = fenster("a", 100, 300, 400, 300, 1); f.minimiert = true; s.setFenster({f}); }
    if (art == "verdeckt") s.setFenster({fenster("a", 100, 300, 400, 300, 1), fenster("b", 50, 200, 600, 400, 2)});

    QCOMPARE(s.gesamtSchnee("a"), 0.0);
    // der Schnee ist zu fallenden Flocken an der alten Stelle geworden
    QVERIFY(s.flocken().size() >= 10);
    for (const Flocke &f : s.flocken()) {
        QVERIFY(f.x >= 95 && f.x <= 505);
        QVERIFY(f.y <= 301 && f.y > 270);
        QVERIFY(f.vy > 0);
    }
    // und fällt nach unten
    double yVorher = s.flocken().front().y;
    s.schritt(0.1, false);
    QVERIFY(s.flocken().front().y > yVorher);
}

void TestSchnee::schmalerWerdenWirftUeberstandAb() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    s.setFenster({fenster("a", 100, 300, 400, 300, 1)});
    for (int i = 0; i < 20; ++i) s.flockeHinzufuegen(flocke(120 + i * 15, 250));
    laufen(s, 2.0);
    // gleiche Oberkante, nur schmaler: linker Teil bleibt liegen
    s.setFenster({fenster("a", 100, 300, 200, 300, 1)});
    QCOMPARE(s.gesamtSchnee("a"), 12.0);   // x = 120 … 285
    QVERIFY(s.flocken().size() >= 7);
    for (const Flocke &f : s.flocken()) QVERIFY(f.x >= 295);
}

void TestSchnee::bodenSammeltUndSchmilzt() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    s.flockeHinzufuegen(flocke(500, 700));
    s.flockeHinzufuegen(flocke(504, 700));
    laufen(s, 3.0);
    QCOMPARE(int(s.flocken().size()), 0);
    QVERIFY(hoeheUm(s, "boden:0", 502) > 0.5);
    laufen(s, 60.0);
    QCOMPARE(s.gesamtSchnee("boden:0"), 0.0);
}

void TestSchnee::flockenNurAufMonitorflaechen() {
    Einstellungen ohneSchmelzen = ruhig();
    ohneSchmelzen.schmelzen = 0;
    Simulation s(ohneSchmelzen);
    // zwei nebeneinander mit Lücke, und ein dritter direkt unter dem ersten
    s.setBildschirme({{0, 0, 500, 500}, {600, 0, 500, 800}, {0, 500, 500, 500}});
    s.flockeHinzufuegen(flocke(550, 100));   // in der Lücke
    s.schritt(1.0 / 60, false);
    QCOMPARE(int(s.flocken().size()), 0);
    // fällt vom oberen in den unteren Monitor und landet erst ganz unten
    s.flockeHinzufuegen(flocke(250, 450));
    laufen(s, 15.0);
    QCOMPARE(s.gesamtSchnee("boden:0"), 0.0);
    QVERIFY(hoeheUm(s, "boden:2", 250) > 0.5);
    // keine Flocken entstehen über einem Monitor, der unter einem anderen liegt
    Einstellungen e = ruhig();
    e.menge = 50;
    Simulation t(e);
    t.setBildschirme({{0, 0, 500, 500}, {0, 500, 500, 500}});
    t.schritt(0.2, true);
    QVERIFY(!t.flocken().empty());
    for (const Flocke &f : t.flocken()) QVERIFY(f.y < 30);
}

void TestSchnee::taskleisteTraegtSchneeStattBoden() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    Fenster leiste = fenster("leiste", 0, 760, 1000, 40, 5);
    leiste.dock = true;
    s.setFenster({leiste});
    s.flockeHinzufuegen(flocke(300, 700));
    laufen(s, 3.0);
    QVERIFY(hoeheUm(s, "leiste", 300) > 0.5);
    QCOMPARE(s.gesamtSchnee("boden:0"), 0.0);
}

QTEST_GUILESS_MAIN(TestSchnee)
#include "test_schnee.moc"
