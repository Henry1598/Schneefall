// Linux-Fensterquelle: Fensterliste vom KWin-Skript (JSON über D-Bus) → schnee::Fenster.
#include <QtTest>
#include "plattform/linux/kwinfensterquelle.h"

class TestKwinquelle : public QObject {
    Q_OBJECT
private slots:
    void listeWirdUebersetzt() {
        const auto f = plattform::fensterAusKwinJson(QStringLiteral(
            R"([{"id":"{a1}","x":10,"y":20.5,"w":300,"h":200,"s":3,"min":false,"fs":true,"akt":true,"dock":false},)"
            R"({"id":"{b2}","x":0,"y":1384,"w":1920,"h":56,"s":7,"min":true,"fs":false,"akt":false,"dock":true}])"));
        QCOMPARE(f.size(), size_t(2));
        QCOMPARE(f[0].id, std::string("{a1}"));
        QCOMPARE(f[0].r.x, 10.0);
        QCOMPARE(f[0].r.y, 20.5);
        QCOMPARE(f[0].r.w, 300.0);
        QCOMPARE(f[0].r.h, 200.0);
        QCOMPARE(f[0].stapel, 3);
        QVERIFY(!f[0].minimiert && f[0].vollbild && f[0].aktiv && !f[0].dock);
        QCOMPARE(f[1].stapel, 7);
        QVERIFY(f[1].minimiert && !f[1].vollbild && !f[1].aktiv && f[1].dock);
    }
    void fensterOhneIdEntfallen() {
        const auto f = plattform::fensterAusKwinJson(QStringLiteral(R"([{"x":1,"y":2,"w":3,"h":4},{"id":"x","w":5}])"));
        QCOMPARE(f.size(), size_t(1));
        QCOMPARE(f[0].id, std::string("x"));
    }
    void kaputteDatenGebenLeereListe() {
        QVERIFY(plattform::fensterAusKwinJson(QStringLiteral("kein json")).empty());
    }
    void unveraenderteListeWirdNichtErneutGemeldet() {
        plattform::KwinFensterquelle q;
        int meldungen = 0;
        size_t zuletzt = 99;
        q.starten([&](std::vector<schnee::Fenster> l) { ++meldungen; zuletzt = l.size(); });
        const QString a = QStringLiteral(R"([{"id":"a","x":0,"y":0,"w":10,"h":10,"s":0}])");
        q.listeEmpfangen(a);
        q.listeEmpfangen(a); // Puls alle 2 s ohne Änderung
        QCOMPARE(meldungen, 1);
        QCOMPARE(zuletzt, size_t(1));
        QCOMPARE(q.diagnose(), a);
        q.listeEmpfangen(QStringLiteral("[]"));
        QCOMPARE(meldungen, 2);
        QCOMPARE(zuletzt, size_t(0));
    }
    void vorDemStartGehtNichtsVerloren() {
        plattform::KwinFensterquelle q;
        const QString a = QStringLiteral(R"([{"id":"a","x":0,"y":0,"w":10,"h":10,"s":0}])");
        q.listeEmpfangen(a); // noch kein Empfänger
        int meldungen = 0;
        q.starten([&](std::vector<schnee::Fenster>) { ++meldungen; });
        q.listeEmpfangen(a); // derselbe Puls danach muss ankommen
        QCOMPARE(meldungen, 1);
    }
};

QTEST_GUILESS_MAIN(TestKwinquelle)
#include "test_kwinquelle.moc"
