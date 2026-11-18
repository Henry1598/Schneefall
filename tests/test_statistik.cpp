// Statistik: Zählen je Ereignis, Speichern/Laden, kaputte Datei, Zurücksetzen, Trefferquote
// und die Anbindung an den Räumdienst (Einsatz abgeschlossen/abgebrochen, Treffer, Streik).
#include <QtTest>
#include <QTemporaryDir>
#include "raeumdienst.h"
#include "schnee.h"
#include "statistik.h"

using namespace schnee;
using E = Statistikereignis;

namespace {
Einstellungen ruhig() {
    Einstellungen e;
    e.wind = 0;
    e.ablageJeFlocke = 1.0;
    e.maxHoehe = 20;
    e.bodenMax = 20;
    e.spaltenBreite = 4;
    e.schmelzen = 0;
    return e;
}
struct Welt {
    Simulation sim{ruhig()};
    Raeumdienst rd{sim};
    Statistik stat{QString()};
    std::vector<Fenster> fenster_;
    explicit Welt(const QString &pfad) : stat(pfad) {
        sim.setBildschirme({{0, 0, 2000, 1200}});
        Fenster f;
        f.id = "a"; f.r = {700, 400, 600, 300}; f.stapel = 2;
        fenster_.push_back(f);
        sim.setFenster(fenster_);
        RaeumEinstellungen e;
        e.an = false;
        rd.setEinstellungen(e);
    }
    void fuellen(const char *id, double hoehe) {
        const Decke *d = sim.decke(id);
        for (size_t i = 0; i < d->hoehe.size(); ++i) sim.schneeHinzufuegen(id, d->x + (i + 0.5) * d->spaltenBreite, hoehe);
    }
    void laufen(double sekunden, double dt = 1.0 / 30) {
        for (double t = 0; t < sekunden; t += dt) { sim.schritt(dt, false); rd.schritt(dt); stat.verbuchen(rd.statistikAbholen()); }
    }
    template <class B> bool bis(B bedingung, double maxSekunden = 300, double dt = 1.0 / 30) {
        for (double t = 0; t < maxSekunden; t += dt) {
            if (bedingung()) return true;
            laufen(dt, dt);
        }
        return bedingung();
    }
    const Einsatz *einsatz() const { return rd.einsaetze().empty() ? nullptr : &rd.einsaetze().front(); }
    bool raeumt() const { return einsatz() && einsatz()->phase == Phase::Raeumen && einsatz()->fz.phase == FahrPhase::Schieben; }
    void triff() { const Fahrzeug &f = einsatz()->fz; rd.werfen(f.x, f.y - 20, 0, 0, 60); laufen(0.05); }
};
} // namespace

class TestStatistik : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;
    QString pfad(const char *name = "stat.conf") const { return m_dir.filePath(QString::fromLatin1(name)); }
private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void jedesEreignisZaehltSeinenZaehler() {
        Statistik s(pfad("a.conf"));
        QCOMPARE(s.stand().trefferGesamt(), qint64(0));
        s.verbuchen({E::EinsatzGestartet, false, 0});
        s.verbuchen({E::EinsatzGestartet, true, 0});
        s.verbuchen({E::EinsatzAbgeschlossen, true, 0});
        s.verbuchen({E::EinsatzAbgebrochen, false, 0});
        s.verbuchen({E::SchneeGeraeumt, false, 120.5});
        s.verbuchen({E::SchneeGeraeumt, false, -5});      // Unsinn zählt nicht
        s.verbuchen({E::BallGeworfen, false, 0});
        s.verbuchen({E::TrefferFahrzeug, false, 0});
        s.verbuchen({E::TrefferFahrzeug, false, 0});
        s.verbuchen({E::TrefferHeli, false, 0});
        s.verbuchen({E::BallDaneben, false, 0});
        s.verbuchen({E::Streik, false, 0});
        s.verbuchen({E::BeleidigterAbflug, false, 0});
        s.verbuchen({E::Spruch, false, 0});
        const Zaehlerstand &z = s.stand();
        QCOMPARE(z.einsaetzeGestartet, qint64(2));
        QCOMPARE(z.pflugGestartet, qint64(1));
        QCOMPARE(z.einsaetzeAbgeschlossen, qint64(1));
        QCOMPARE(z.pflugAbgeschlossen, qint64(1));
        QCOMPARE(z.einsaetzeAbgebrochen, qint64(1));
        QCOMPARE(z.schneeGeraeumt, 120.5);
        QCOMPARE(z.baelleGeworfen, qint64(1));
        QCOMPARE(z.trefferFahrzeug, qint64(2));
        QCOMPARE(z.trefferHeli, qint64(1));
        QCOMPARE(z.trefferGesamt(), qint64(3));
        QCOMPARE(z.baelleDaneben, qint64(1));
        QCOMPARE(z.streiks, qint64(1));
        QCOMPARE(z.beleidigteAbfluege, qint64(1));
        QCOMPARE(z.sprueche, qint64(1));
        QVERIFY(s.geaendert());
        QVERIFY(z.seit.isValid());
    }
    void trefferquoteIstTrefferJeEntschiedenemWurf() {
        Zaehlerstand z;
        QCOMPARE(z.trefferquote(), -1.0);                 // noch nichts entschieden
        z.trefferFahrzeug = 2; z.trefferHeli = 1; z.baelleDaneben = 1;
        QCOMPARE(z.trefferquote(), 75.0);
        z.trefferFahrzeug = z.trefferHeli = 0;
        QCOMPARE(z.trefferquote(), 0.0);
    }
    void speichernUndLadenGebenDenStandZurueck() {
        const QString p = pfad("b.conf");
        QDateTime seit;
        {
            Statistik s(p);
            for (int i = 0; i < 7; ++i) s.verbuchen({E::TrefferFahrzeug, false, 0});
            s.verbuchen({E::SchneeGeraeumt, false, 1234.5});
            s.verbuchen({E::EinsatzAbgeschlossen, true, 0});
            s.verbuchen({E::Streik, false, 0});
            QVERIFY(s.speichern());
            QVERIFY(!s.geaendert());
            seit = s.stand().seit;
        }
        Statistik n(p);
        QCOMPARE(n.stand().trefferFahrzeug, qint64(7));
        QCOMPARE(n.stand().schneeGeraeumt, 1234.5);
        QCOMPARE(n.stand().einsaetzeAbgeschlossen, qint64(1));
        QCOMPARE(n.stand().pflugAbgeschlossen, qint64(1));
        QCOMPARE(n.stand().streiks, qint64(1));
        QCOMPARE(n.stand().seit.toSecsSinceEpoch(), seit.toSecsSinceEpoch());
        QVERIFY(!n.geaendert());
    }
    void unveraenderteStatistikSchreibtNichtNeu() {
        const QString p = pfad("c.conf");
        Statistik s(p);
        QVERIFY(s.speichern());                           // legt die Datei an
        const QDateTime m1 = QFileInfo(p).lastModified();
        QTest::qWait(1100);
        QVERIFY(s.speichern());
        QCOMPARE(QFileInfo(p).lastModified(), m1);
    }
    void fehlendeDateiBeginntBeiNull() {
        Statistik s(pfad("gibts-nicht.conf"));
        QCOMPARE(s.stand().einsaetzeGestartet, qint64(0));
        QVERIFY(s.stand().seit.isValid());
        QVERIFY(!s.geaendert());
    }
    void kaputteDateiBeginntBeiNullUndWirdErsetzt() {
        for (const QByteArray &muell : {QByteArray("\x00\xff\xfe garbage \x01", 16), QByteArray("hallo welt"), QByteArray(), QByteArray("streiks=abc\ntreffer_fahrzeug=-4\n=\n")}) {
            const QString p = pfad("d.conf");
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(muell);
            f.close();
            Statistik s(p);
            QCOMPARE(s.stand().streiks, qint64(0));
            QCOMPARE(s.stand().trefferFahrzeug, qint64(0));
            QVERIFY(s.stand().seit.isValid());
            s.verbuchen({E::Streik, false, 0});
            QVERIFY(s.speichern());
            Statistik n(p);
            QCOMPARE(n.stand().streiks, qint64(1));
        }
    }
    void teilweiseKaputteDateiBehaeltGuteWerte() {
        const QString p = pfad("e.conf");
        QFile f(p);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("streiks=4\ntreffer_fahrzeug=kaputt\nseit=gestern\nunbekannt=1\nsprueche=9\n");
        f.close();
        Statistik s(p);
        QCOMPARE(s.stand().streiks, qint64(4));
        QCOMPARE(s.stand().sprueche, qint64(9));
        QCOMPARE(s.stand().trefferFahrzeug, qint64(0));
        QVERIFY(s.stand().seit.isValid());
    }
    void zuruecksetzenLoeschtAllesUndSetztDenZeitpunktNeu() {
        const QString p = pfad("f.conf");
        Statistik s(p);
        s.verbuchen({E::Streik, false, 0});
        s.verbuchen({E::SchneeGeraeumt, false, 50});
        QVERIFY(s.speichern());
        const QDateTime alt = QDateTime::currentDateTimeUtc().addDays(-3);
        s.zuruecksetzen();
        QCOMPARE(s.stand().streiks, qint64(0));
        QCOMPARE(s.stand().schneeGeraeumt, 0.0);
        QVERIFY(s.stand().seit > alt);
        QVERIFY(s.geaendert());
        QVERIFY(s.speichern());
        QCOMPARE(Statistik(p).stand().streiks, qint64(0));
    }
    void statusUndTooltipText() {
        Statistik s(pfad("g.conf"));
        for (int i = 0; i < 12; ++i) s.verbuchen({E::EinsatzAbgeschlossen, false, 0});
        for (int i = 0; i < 37; ++i) s.verbuchen({E::TrefferFahrzeug, false, 0});
        for (int i = 0; i < 3; ++i) s.verbuchen({E::Streik, false, 0});
        QCOMPARE(s.kurzText(), QString::fromUtf8("Schneefall – 12× geräumt, 37 Treffer, 3 Streiks"));
        QVERIFY(s.statusZeile().startsWith(QLatin1String("statistik=geraeumt:12,")));
        QVERIFY(!s.statusZeile().contains(QLatin1Char('\n')));
        QVERIFY(s.statusZeile().contains(QLatin1String("quote:100")));
    }

    // ---- Anbindung an den Räumdienst ----
    void abgeschlossenerEinsatzWirdMitSchneeGezaehlt() {
        Welt w(pfad("h.conf"));
        w.fuellen("a", 16);
        QVERIFY(w.rd.buchen("a"));
        QVERIFY(w.bis([&] { return w.rd.einsaetze().empty() && w.stat.stand().einsaetzeGestartet > 0; }, 600));
        const Zaehlerstand &z = w.stat.stand();
        QCOMPARE(z.einsaetzeGestartet, qint64(1));
        QCOMPARE(z.einsaetzeAbgeschlossen, qint64(1));
        QCOMPARE(z.einsaetzeAbgebrochen, qint64(0));
        QCOMPARE(z.pflugGestartet, qint64(0));
        QVERIFY2(z.schneeGeraeumt > 500, qPrintable(QString::number(z.schneeGeraeumt)));
        QVERIFY(z.sprueche >= 0);
    }
    void abgebrochenerEinsatzWirdAlsAbbruchGezaehlt() {
        Welt w(pfad("i.conf"));
        w.fuellen("a", 16);
        QVERIFY(w.rd.buchen("a"));
        QVERIFY(w.bis([&] { return w.raeumt(); }, 120));
        w.fenster_[0].minimiert = true;
        w.sim.setFenster(w.fenster_);
        QVERIFY(w.bis([&] { return w.rd.einsaetze().empty(); }, 120));
        QCOMPARE(w.stat.stand().einsaetzeGestartet, qint64(1));
        QCOMPARE(w.stat.stand().einsaetzeAbgebrochen, qint64(1));
        QCOMPARE(w.stat.stand().einsaetzeAbgeschlossen, qint64(0));
    }
    void bodenEinsatzZaehltAlsPflug() {
        Welt w(pfad("j.conf"));
        RaeumEinstellungen e;
        w.rd.setEinstellungen(e);
        w.fuellen("boden:0", 18);
        QVERIFY(w.bis([&] { return w.stat.stand().einsaetzeGestartet > 0; }, 30));
        QCOMPARE(w.stat.stand().pflugGestartet, qint64(1));
        QVERIFY(w.bis([&] { return w.rd.einsaetze().empty(); }, 600));
        QCOMPARE(w.stat.stand().pflugAbgeschlossen, qint64(1));
    }
    void wuerfeTrefferStreikUndSpruecheWerdenGezaehlt() {
        Welt w(pfad("k.conf"));
        w.fuellen("a", 16);
        QVERIFY(w.rd.buchen("a"));
        QVERIFY(w.bis([&] { return w.raeumt(); }, 120));
        const qint64 sprueche0 = w.stat.stand().sprueche;
        w.triff(); w.triff();
        QCOMPARE(w.stat.stand().baelleGeworfen, qint64(2));
        QCOMPARE(w.stat.stand().trefferFahrzeug, qint64(2));
        QCOMPARE(w.stat.stand().streiks, qint64(0));
        w.triff();                                        // der dritte Treffer löst den Streik aus
        QCOMPARE(w.stat.stand().trefferFahrzeug, qint64(3));
        QCOMPARE(w.stat.stand().streiks, qint64(1));
        QVERIFY(w.stat.stand().sprueche > sprueche0);
        // Hubschrauber
        const Einsatz *e = w.einsatz();
        w.rd.werfen(e->heli.x, e->heli.y - 22, 0, 0, 60);
        w.laufen(0.05);
        QCOMPARE(w.stat.stand().trefferHeli, qint64(1));
        QCOMPARE(w.stat.stand().baelleGeworfen, qint64(4));
        // ein Wurf ins Leere: nach oben aus dem Bild
        w.rd.werfen(100, 100, 0, -2000, 40);
        QVERIFY(w.bis([&] { return w.stat.stand().baelleDaneben > 0; }, 10));
        QCOMPARE(w.stat.stand().baelleDaneben, qint64(1));
    }
    void beleidigterAbflugWirdGezaehlt() {
        Welt w(pfad("l.conf"));
        RaeumEinstellungen e;
        e.an = false;
        e.trefferBisStreik = 1;
        w.rd.setEinstellungen(e);
        w.fuellen("a", 16);
        QVERIFY(w.rd.buchen("a"));
        QVERIFY(w.bis([&] { return w.raeumt(); }, 120));
        w.triff();                                        // Streik
        QCOMPARE(w.stat.stand().streiks, qint64(1));
        w.triff(); w.triff();                             // zwei Treffer im Streik: beleidigt
        QCOMPARE(w.stat.stand().beleidigteAbfluege, qint64(1));
        QVERIFY(w.bis([&] { return w.rd.einsaetze().empty(); }, 120));
        QCOMPARE(w.stat.stand().einsaetzeAbgebrochen, qint64(1));
    }
};

QTEST_MAIN(TestStatistik)
#include "test_statistik.moc"
