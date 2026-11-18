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
};

void TestSchnee::flockeBleibtAufFensterkanteLiegen() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    s.setFenster({fenster("a", 100, 300, 400, 300, 1)});
    s.flockeHinzufuegen(flocke(250, 250));
    laufen(s, 2.0);
    QCOMPARE(int(s.flocken().size()), 0);
    QVERIFY(s.hoeheBei("a", 250) > 0.5);
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
    QCOMPARE(s.hoeheBei("a", 300), 0.0);
    QVERIFY(s.hoeheBei("a", 450) > 0.5);
    // die andere Flocke fiel durch, statt auf dem verdeckten Teil liegen zu bleiben
    QCOMPARE(s.gesamtSchnee("a"), 1.0);
    // wird b danach nach unten gestapelt, ist die Kante wieder frei
    s.setFenster({fenster("a", 100, 300, 400, 300, 2), fenster("b", 200, 250, 200, 200, 1)});
    s.flockeHinzufuegen(flocke(300, 200));
    laufen(s, 2.0);
    QVERIFY(s.hoeheBei("b", 300) > 0.5);   // b's Kante bleibt sichtbar (liegt höher)
    s.flockeHinzufuegen(flocke(450, 280));
    laufen(s, 2.0);
    QVERIFY(s.hoeheBei("a", 450) > 1.5);
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

QTEST_GUILESS_MAIN(TestSchnee)
#include "test_schnee.moc"
