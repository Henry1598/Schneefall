// App mit nachgebildeter Fensterquelle: Fensterliste, Vollbild-Pause und Befehle laufen unabhängig vom
// Plattform-Backend (kein KWin, kein D-Bus nötig). Offscreen, mit eigenem XDG_CONFIG_HOME.
#include <QtTest>
#include <QTemporaryDir>
#include "app.h"
#include "fakefensterquelle.h"

using plattform::Antwort;

namespace {
// ein Wert aus der ersten Zeile von --status, z. B. wert(status, "fenster") → "2"
QString wert(const QString &status, const QString &schluessel) {
    for (const QString &teil : status.section(QLatin1Char('\n'), 0, 0).split(QLatin1Char(' ')))
        if (teil.startsWith(schluessel + QLatin1Char('='))) return teil.mid(schluessel.size() + 1);
    return QString();
}
} // namespace

class TestApp : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        qputenv("XDG_CONFIG_HOME", m_dir.path().toUtf8());
    }
    void fensterlisteKommtAnDerAppAn() {
        auto quelle = std::make_unique<FakeFensterquelle>();
        FakeFensterquelle *q = quelle.get();
        App app(std::move(quelle));
        QVERIFY(q->gestartet()); // die App hört ab dem Anlegen zu
        app.starten();
        QCOMPARE(wert(app.Status(), QStringLiteral("fenster")), QStringLiteral("0"));
        q->melden({fakeFenster("a", 100, 300, 400, 300, 1), fakeFenster("b", 600, 400, 300, 200, 2)});
        QCOMPARE(wert(app.Status(), QStringLiteral("fenster")), QStringLiteral("2"));
        QCOMPARE(app.Fensterliste(), QStringLiteral("fake:1")); // Diagnose kommt von der Quelle
    }
    void vollbildFensterPausiertDenSchnee() {
        auto quelle = std::make_unique<FakeFensterquelle>();
        FakeFensterquelle *q = quelle.get();
        App app(std::move(quelle));
        app.starten();
        QCOMPARE(wert(app.Status(), QStringLiteral("laeuft")), QStringLiteral("1"));
        q->melden({fakeFenster("spiel", 0, 0, 1920, 1080, 5, /*vollbild*/ true, /*aktiv*/ true)});
        QCOMPARE(wert(app.Status(), QStringLiteral("vollbild")), QStringLiteral("1"));
        QCOMPARE(wert(app.Status(), QStringLiteral("laeuft")), QStringLiteral("0"));
        q->melden({fakeFenster("spiel", 0, 0, 1920, 1080, 5, true, /*aktiv*/ false)}); // anderes Fenster aktiv
        QCOMPARE(wert(app.Status(), QStringLiteral("laeuft")), QStringLiteral("1"));
    }
    void befehleWieUeberSteuerkanal() {
        App app(std::make_unique<FakeFensterquelle>());
        app.starten();
        const Antwort status = app.befehl(QStringLiteral("Status"), {});
        QCOMPARE(status.ergebnis, Antwort::Ergebnis::Ok);
        QVERIFY(status.text && status.text->startsWith(QStringLiteral("laeuft=")));
        // Umschalten: Schnee aus, ohne Text
        const Antwort um = app.befehl(QStringLiteral("Umschalten"), {});
        QCOMPARE(um.ergebnis, Antwort::Ergebnis::Ok);
        QVERIFY(!um.text);
        QCOMPARE(app.einstellungen().an, false);
        app.befehl(QStringLiteral("Umschalten"), {});
        QCOMPARE(app.einstellungen().an, true);
        // Werfen mit fünf Zahlen: ein Schneeball fliegt
        QCOMPARE(app.befehl(QStringLiteral("Werfen"), {"500", "300", "200", "-100", "120"}).ergebnis, Antwort::Ergebnis::Ok);
        QCOMPARE(wert(app.Status(), QStringLiteral("baelle")), QStringLiteral("1"));
        QCOMPARE(app.befehl(QStringLiteral("Zeitlupe"), {"0.5"}).ergebnis, Antwort::Ergebnis::Ok);
        // Räumen ohne Schnee: Text leer, aber vorhanden (wie bisher über D-Bus)
        const Antwort r = app.befehl(QStringLiteral("Raeumen"), {});
        QCOMPARE(r.ergebnis, Antwort::Ergebnis::Ok);
        QVERIFY(r.text && r.text->isEmpty());
    }
    void falscheBefehleWerdenAbgelehnt() {
        App app(std::make_unique<FakeFensterquelle>());
        QCOMPARE(app.befehl(QStringLiteral("Gibtsnicht"), {}).ergebnis, Antwort::Ergebnis::Unbekannt);
        QCOMPARE(app.befehl(QStringLiteral("Werfen"), {"1", "2"}).ergebnis, Antwort::Ergebnis::Unbekannt);       // zu wenige
        QCOMPARE(app.befehl(QStringLiteral("Zeitlupe"), {"schnell"}).ergebnis, Antwort::Ergebnis::Unbekannt);    // keine Zahl
        QCOMPARE(app.befehl(QStringLiteral("Status"), {"1"}).ergebnis, Antwort::Ergebnis::Unbekannt);            // zu viele
        QCOMPARE(app.befehl(QStringLiteral("Fenster"), {"[]"}).ergebnis, Antwort::Ergebnis::Unbekannt);          // nur über die Fensterquelle
    }

private:
    QTemporaryDir m_dir;
};

QTEST_MAIN(TestApp)
#include "test_app.moc"
