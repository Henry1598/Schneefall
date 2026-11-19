// Windows-Autostart: Wert „Schneefall“ unter HKCU\…\Run. Der Test schreibt nur in einen eigenen Testschlüssel
// (SCHNEEFALL_TEST_RUN) und räumt ihn am Ende weg – der echte Autostart bleibt unberührt.
#include <QtTest>
#include <QSettings>
#include "plattform/autostart.h"

namespace {
const QString TEST = QStringLiteral("SchneefallTest\\Run");
const QString PFAD = QStringLiteral("HKEY_CURRENT_USER\\Software\\SchneefallTest");
} // namespace

class TestAutostartWin : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { qputenv("SCHNEEFALL_TEST_RUN", TEST.toUtf8()); }
    void cleanupTestCase() { QSettings(PFAD, QSettings::NativeFormat).remove(QString()); }

    void anUndAusGenauEinEintrag() {
        QVERIFY(plattform::autostartSetzen(false, QString()));
        QVERIFY(!plattform::autostartAktiv());
        QVERIFY(plattform::autostartSetzen(true, QStringLiteral("C:/Program Files/Schneefall/schneefall.exe")));
        QVERIFY(plattform::autostartAktiv());
        // Pfad mit Leerzeichen: in Anführungszeichen, Windows-Trenner
        QCOMPARE(wert(), QStringLiteral("\"C:\\Program Files\\Schneefall\\schneefall.exe\""));
        QVERIFY(plattform::autostartSetzen(false, QString()));
        QVERIFY(!plattform::autostartAktiv());
    }
    void andereEintraegeBleiben() {
        QSettings run(PFAD + QStringLiteral("\\Run"), QSettings::NativeFormat);
        run.setValue(QStringLiteral("Anderes"), QStringLiteral("C:\\x.exe"));
        run.sync();
        QVERIFY(plattform::autostartSetzen(true, QStringLiteral("C:/a/schneefall.exe")));
        QVERIFY(plattform::autostartSetzen(false, QString()));
        run.sync();
        QCOMPARE(QSettings(PFAD + QStringLiteral("\\Run"), QSettings::NativeFormat).value(QStringLiteral("Anderes")).toString(),
                 QStringLiteral("C:\\x.exe"));
    }
    void steuerzeichenUndAnfuehrungszeichenWerdenAbgelehnt() {
        plattform::autostartSetzen(false, QString());
        QVERIFY(!plattform::autostartSetzen(true, QStringLiteral("C:/a\"b/schneefall.exe")));
        QVERIFY(!plattform::autostartSetzen(true, QStringLiteral("C:/a\nb/schneefall.exe")));
        QVERIFY(!plattform::autostartSetzen(true, QString()));
        QVERIFY(!plattform::autostartAktiv());
    }

private:
    QString wert() const { return QSettings(PFAD + QStringLiteral("\\Run"), QSettings::NativeFormat).value(QStringLiteral("Schneefall")).toString(); }
};

QTEST_GUILESS_MAIN(TestAutostartWin)
#include "test_autostart_win.moc"
