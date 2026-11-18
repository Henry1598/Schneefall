// Geräusche offline prüfen: Synthese ohne Audiogerät (Klang(false)).
#include <QtTest>
#include <cmath>
#include <vector>
#include "klang.h"
#include "einstellungen.h"

namespace {
using Puffer = std::vector<float>;
Puffer rendern(Klang &k, int frames) {
    Puffer p(size_t(frames) * 2);
    // in Blöcken wie das Audiogerät
    for (int i = 0; i < frames; i += 480) k.render(p.data() + size_t(i) * 2, std::min(480, frames - i));
    return p;
}
float spitze(const Puffer &p, int ab = 0) {
    float m = 0;
    for (size_t i = size_t(ab) * 2; i < p.size(); ++i) m = std::max(m, std::fabs(p[i]));
    return m;
}
double energie(const Puffer &p, int kanal) {
    double e = 0;
    for (size_t i = size_t(kanal); i < p.size(); i += 2) e += double(p[i]) * p[i];
    return e;
}
float maxSprung(const Puffer &p) {
    float m = 0;
    for (size_t i = 2; i < p.size(); ++i) m = std::max(m, std::fabs(p[i] - p[i - 2]));
    return m;
}
const KlangArt alleArten[] = {KlangArt::Rotor, KlangArt::Motor, KlangArt::Rueckfahrwarner, KlangArt::Rutsch,
                              KlangArt::Plumps, KlangArt::Wurf, KlangArt::Treffer, KlangArt::Murren};
}

class TestKlang : public QObject {
    Q_OBJECT
private slots:
    void jederKlangIstHoerbarUndBegrenzt() {
        for (KlangArt a : alleArten) {
            Klang k(false);
            k.setzeAn(true);
            k.setzeLautstaerke(1.0);
            k.setzeWeltbreite(0, 1920);
            k.ereignis(a, 960, 500, 2.0);
            Puffer p = rendern(k, 48000);
            QVERIFY2(spitze(p) > 0.02f, qPrintable(QStringLiteral("zu leise: Art %1").arg(int(a))));
            QVERIFY2(spitze(p) <= 1.0f, "Clipping");
        }
    }
    void vieleStimmenUeberschreitenNie1() {
        Klang k(false);
        k.setzeAn(true);
        k.setzeLautstaerke(1.0);
        k.setzeWeltbreite(0, 1920);
        for (int i = 0; i < 6; ++i) k.dauerklang(i, i % 3 == 0 ? KlangArt::Rotor : i % 3 == 1 ? KlangArt::Motor : KlangArt::Rueckfahrwarner, 100 + 300 * i, 300, 1.0);
        for (KlangArt a : alleArten) k.ereignis(a, 500, 400, 2.0);
        Puffer p = rendern(k, 48000);
        QVERIFY(spitze(p) <= 1.0f);
        QVERIFY(spitze(p) > 0.1f);
    }
    void panningLinksUndRechts() {
        for (KlangArt a : {KlangArt::Plumps, KlangArt::Rutsch, KlangArt::Wurf}) {
            Klang l(false), r(false), m(false);
            for (Klang *k : {&l, &r, &m}) { k->setzeAn(true); k->setzeLautstaerke(1.0); k->setzeWeltbreite(0, 1920); }
            l.ereignis(a, 0, 300);
            r.ereignis(a, 1920, 300);
            m.ereignis(a, 960, 300);
            Puffer pl = rendern(l, 48000), pr = rendern(r, 48000), pm = rendern(m, 48000);
            QVERIFY(energie(pl, 0) > 20 * energie(pl, 1));
            QVERIFY(energie(pr, 1) > 20 * energie(pr, 0));
            QVERIFY(std::fabs(energie(pm, 0) / energie(pm, 1) - 1.0) < 0.05); // Mitte: gleich laut
        }
        // Dauerklang folgt der Position
        Klang k(false);
        k.setzeAn(true); k.setzeLautstaerke(1.0); k.setzeWeltbreite(0, 1920);
        k.dauerklang(1, KlangArt::Rotor, 0, 100, 1.0);
        Puffer p = rendern(k, 24000);
        QVERIFY(energie(p, 0) > 20 * energie(p, 1));
    }
    void einUndAusblendenOhneSprung() {
        for (KlangArt a : {KlangArt::Rotor, KlangArt::Motor, KlangArt::Rueckfahrwarner}) {
            Klang k(false);
            k.setzeAn(true); k.setzeLautstaerke(1.0); k.setzeWeltbreite(0, 1920);
            k.dauerklang(1, a, 960, 100, 0.5);
            Puffer ein = rendern(k, 4800);
            QVERIFY(std::fabs(ein[0]) < 0.01f && std::fabs(ein[1]) < 0.01f);
            QVERIFY2(maxSprung(ein) < 0.25f, qPrintable(QString::number(int(a)) + " " + QString::number(maxSprung(ein))));
            rendern(k, 48000);
            k.dauerklangStopp(1);
            Puffer aus = rendern(k, 12000);
            QVERIFY(maxSprung(aus) < 0.25f);
            QCOMPARE(spitze(aus, 6000), 0.0f);         // nach 125 ms still
            QCOMPARE(k.aktiveStimmen(), 0);
        }
        // Einmalklang endet ebenfalls ohne Sprung
        Klang k(false);
        k.setzeAn(true); k.setzeLautstaerke(1.0); k.setzeWeltbreite(0, 1920);
        k.ereignis(KlangArt::Treffer, 960, 0, 2.0);
        Puffer p = rendern(k, 48000);
        QVERIFY(maxSprung(p) < 0.5f);
        QVERIFY(std::fabs(p[2 * 47999]) < 1e-6f);
    }
    void anAusSchalterBlendetWeichAus() {
        Klang k(false);
        k.setzeAn(true); k.setzeLautstaerke(1.0); k.setzeWeltbreite(0, 1920);
        k.dauerklang(1, KlangArt::Rueckfahrwarner, 960, 0, 0);
        rendern(k, 24000);
        k.setzeAn(false);
        Puffer p = rendern(k, 4800);
        QVERIFY(maxSprung(p) < 0.15f);
        QCOMPARE(spitze(rendern(k, 4800)), 0.0f);
    }
    void dauerklangUpdateAendertTonhoehe() {
        Klang k(false);
        k.setzeAn(true); k.setzeWeltbreite(0, 1920);
        k.dauerklang(7, KlangArt::Motor, 960, 0, 0.0);
        const double leerlauf = k.dauerFrequenz(7);
        k.dauerklang(7, KlangArt::Motor, 960, 0, 1.0);
        const double last = k.dauerFrequenz(7);
        QVERIFY(leerlauf > 15 && leerlauf < 40);
        QVERIFY(last > 1.8 * leerlauf);
        QCOMPARE(k.aktiveStimmen(), 1); // Update, kein zweiter Klang
        // hörbar: Zündfolge im Signal – Nulldurchgänge der Zündimpulse über Autokorrelation
        auto periode = [](Klang &kk) {
            Puffer p = rendern(kk, 48000 * 2);
            std::vector<float> x(p.size() / 2);
            for (size_t i = 0; i < x.size(); ++i) x[i] = p[2 * i];
            int bestLag = 0; double best = -1e30;
            for (int lag = 300; lag < 3000; ++lag) {   // 16 … 160 Hz
                double s = 0;
                for (size_t i = 48000; i + size_t(lag) < x.size(); i += 3) s += double(x[i]) * x[i + size_t(lag)];
                if (s > best) { best = s; bestLag = lag; }
            }
            return 48000.0 / bestLag;
        };
        Klang a(false), b(false);
        for (Klang *q : {&a, &b}) { q->setzeAn(true); q->setzeWeltbreite(0, 1920); }
        a.dauerklang(1, KlangArt::Motor, 960, 0, 0.0);
        b.dauerklang(1, KlangArt::Motor, 960, 0, 1.0);
        double fa = periode(a), fb = periode(b);
        QVERIFY2(fb > 1.5 * fa, qPrintable(QStringLiteral("%1 Hz vs %2 Hz").arg(fa).arg(fb)));
    }
    void dopplerRotorAnnaeherungHoeherAlsEntfernung() {
        Klang k(false);
        k.setzeAn(true); k.setzeWeltbreite(0, 1920);
        // von links zur Mitte (nähert sich), 600 px/s
        double x = 100;
        k.dauerklang(1, KlangArt::Rotor, x, 0, 1.0);
        for (int i = 0; i < 30; ++i) { rendern(k, 1600); x += 20; k.dauerklang(1, KlangArt::Rotor, x, 0, 1.0); }
        double nah = k.dauerFrequenz(1);
        // jenseits der Mitte entfernt er sich
        Klang k2(false);
        k2.setzeAn(true); k2.setzeWeltbreite(0, 1920);
        x = 1000;
        k2.dauerklang(1, KlangArt::Rotor, x, 0, 1.0);
        for (int i = 0; i < 30; ++i) { rendern(k2, 1600); x += 20; k2.dauerklang(1, KlangArt::Rotor, x, 0, 1.0); }
        QVERIFY(nah > 1.03 * k2.dauerFrequenz(1));
    }
    void stimmenBegrenzung() {
        Klang k(false);
        k.setzeAn(true); k.setzeLautstaerke(1.0); k.setzeWeltbreite(0, 1920);
        for (int i = 0; i < 200; ++i) k.ereignis(KlangArt::Rutsch, i * 9, 0, 2.0);
        rendern(k, 480);
        QVERIFY(k.aktiveStimmen() <= Klang::MaxEinmal);
        for (int i = 0; i < 40; ++i) k.dauerklang(i, KlangArt::Motor, 500, 0, 0.5);
        rendern(k, 480);
        QVERIFY(k.aktiveStimmen() <= Klang::MaxEinmal + Klang::MaxDauer);
        QVERIFY(spitze(rendern(k, 4800)) <= 1.0f);
    }
    void ausErzeugtStille() {
        Klang k(false);   // Standard: aus
        QVERIFY(!k.istAn());
        k.ereignis(KlangArt::Plumps, 100, 0);
        k.dauerklang(1, KlangArt::Rotor, 100, 0, 1.0);
        QCOMPARE(k.aktiveStimmen(), 0);
        QCOMPARE(spitze(rendern(k, 9600)), 0.0f);
    }
    void lautstaerkeSkaliert() {
        auto pegel = [](double l) {
            Klang k(false);
            k.setzeAn(true); k.setzeLautstaerke(l); k.setzeWeltbreite(0, 1920);
            k.ereignis(KlangArt::Plumps, 960, 0, 1.0);
            return spitze(rendern(k, 48000));
        };
        QVERIFY(pegel(0.2) < pegel(0.5));
        QVERIFY(pegel(0.5) < pegel(1.0));
        QCOMPARE(pegel(0.0), 0.0f);
    }
    void einstellungenTonUndLautstaerke() {
        using namespace schnee;
        Programmeinstellungen alt = einstellungenAusText("menge=2\nwind=10\n"); // alte Datei ohne neue Schlüssel
        QVERIFY(!alt.ton);
        QCOMPARE(alt.lautstaerke, 50);
        Programmeinstellungen p = einstellungenAusText("ton=an\nlautstaerke=80\n");
        QVERIFY(p.ton);
        QCOMPARE(p.lautstaerke, 80);
        QCOMPARE(einstellungenAusText("lautstaerke=500\n").lautstaerke, 100);
        QCOMPARE(einstellungenAusText("lautstaerke=-5\n").lautstaerke, 0);
        QCOMPARE(einstellungenAusText("ton=quatsch\n").ton, false);
        Programmeinstellungen r = einstellungenAusText(einstellungenAlsText(p));
        QVERIFY(r.ton);
        QCOMPARE(r.lautstaerke, 80);
    }
};

QTEST_MAIN(TestKlang)
#include "test_klang.moc"
