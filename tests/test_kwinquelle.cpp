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
        QVERIFY(q.diagnose().contains(QStringLiteral("\"id\":\"a\""))); // Diagnose: die geprüfte Liste, neu serialisiert
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
    // Sicherheitsprüfung M1: jeder Sitzungsprozess kann Fenster(json) aufrufen – Zahlen und Größen begrenzen
    void unendlicheUndRiesigeZahlenWerdenBegrenzt() {
        const auto f = plattform::fensterAusKwinJson(QStringLiteral(
            R"([{"id":"riesig","x":-1e300,"y":1e18,"w":1e18,"h":1e308},)"
            R"({"id":"text","x":"abc","y":0,"w":10,"h":10},)"
            R"({"id":"negativ","x":5,"y":6,"w":-50,"h":-1}])"));
        // nicht endliche oder nicht-numerische Werte: Eintrag verworfen; endliche: geklemmt
        QCOMPARE(f.size(), size_t(2));
        QCOMPARE(f[0].id, std::string("riesig"));
        QCOMPARE(f[0].r.x, -100000.0);
        QCOMPARE(f[0].r.y, 100000.0);
        QCOMPARE(f[0].r.w, 100000.0);
        QCOMPARE(f[0].r.h, 100000.0);
        QCOMPARE(f[1].id, std::string("negativ"));
        QCOMPARE(f[1].r.w, 0.0);
        QCOMPARE(f[1].r.h, 0.0);
        // über den double-Bereich hinaus (1e400 = unendlich): Qt lehnt das ganze Dokument ab
        QVERIFY(plattform::fensterAusKwinJson(QStringLiteral(R"([{"id":"u","w":1e400},{"id":"v","w":-1e400}])")).empty());
    }
    void hoechstens1000Fenster() {
        QString json = QStringLiteral("[");
        for (int i = 0; i < 5000; ++i) json += QStringLiteral(R"({"id":"f%1","x":0,"y":0,"w":10,"h":10},)").arg(i);
        json.chop(1);
        json += QLatin1Char(']');
        const auto f = plattform::fensterAusKwinJson(json);
        QCOMPARE(f.size(), size_t(1000));
        QCOMPARE(f.back().id, std::string("f999"));
    }
    void zuLangeIdEntfaellt() {
        const QString lang128(128, QLatin1Char('a')), lang129(129, QLatin1Char('b'));
        const auto f = plattform::fensterAusKwinJson(QStringLiteral(R"([{"id":"%1","w":5},{"id":"%2","w":5}])").arg(lang128, lang129));
        QCOMPARE(f.size(), size_t(1));
        QCOMPARE(f[0].id, lang128.toStdString());
    }
    void riesigeOderKaputteNachrichtWirdIgnoriert() {
        plattform::KwinFensterquelle q;
        int meldungen = 0;
        q.starten([&](std::vector<schnee::Fenster>) { ++meldungen; });
        const QString a = QStringLiteral(R"([{"id":"a","x":0,"y":0,"w":10,"h":10,"s":0}])");
        q.listeEmpfangen(a);
        QCOMPARE(meldungen, 1);
        const QString vorher = q.diagnose();
        // über 1 MiB (z. B. eine Million Einträge): verworfen, bevor irgendetwas gespeichert wird
        QString riesig = QStringLiteral("[");
        riesig.reserve(12 * 1000000);
        for (int i = 0; i < 1000000; ++i) riesig += QStringLiteral(R"({"id":"x"},)");
        riesig.chop(1);
        riesig += QLatin1Char(']');
        q.listeEmpfangen(riesig);
        QCOMPARE(meldungen, 1);
        QCOMPARE(q.diagnose(), vorher);
        // kein gültiges JSON (mit Steuerzeichen fürs Terminal): weder gemeldet noch für --fensterliste gespeichert
        q.listeEmpfangen(QStringLiteral("\x1b]0;boese\x07\x1b[2J"));
        QCOMPARE(meldungen, 1);
        QCOMPARE(q.diagnose(), vorher);
        q.listeEmpfangen(QStringLiteral(R"({"id":"kein Array"})"));
        QCOMPARE(meldungen, 1);
    }
    void diagnoseEnthaeltKeineSteuerzeichen() {
        plattform::KwinFensterquelle q;
        q.starten([](std::vector<schnee::Fenster>) {});
        q.listeEmpfangen(QStringLiteral("[{\"id\":\"a\\u001b[31m\\u0007\",\"x\":1,\"y\":2,\"w\":10,\"h\":10,\"s\":0}]"));
        // dazu Steuerzeichen roh im Text (ESC, BEL, DEL)
        q.listeEmpfangen(QStringLiteral("[{\"id\":\"b\x1b]0;x\x07\x7f\",\"x\":1,\"y\":2,\"w\":10,\"h\":10,\"s\":0}]"));
        const QString d = q.diagnose();
        QVERIFY(!d.isEmpty());
        for (const QChar c : d) QVERIFY2(c.unicode() >= 0x20 && c.unicode() != 0x7f, qPrintable(d));
    }
};

QTEST_GUILESS_MAIN(TestKwinquelle)
#include "test_kwinquelle.moc"
