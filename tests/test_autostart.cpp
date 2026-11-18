// Autostart (Linux: ~/.config/autostart/schneefall.desktop): der Programmpfad landet in Exec= und muss nach der
// Desktop-Entry-Spezifikation gequotet sein (Sicherheitsprüfung N4) – sonst zerlegt der Starter ihn an
// Leerzeichen oder wertet $, `, % aus. Pfade mit Steuerzeichen (z. B. Zeilenumbruch → neue Schlüssel) werden abgelehnt.
#include <QtTest>
#include <QTemporaryDir>
#include "plattform/autostart.h"

class TestAutostart : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        qputenv("XDG_CONFIG_HOME", m_dir.path().toUtf8());
    }
    void einfacherPfadInAnfuehrungszeichen() {
        QVERIFY(plattform::autostartSetzen(true, QStringLiteral("/home/henry/Schneefall/build/schneefall")));
        QCOMPARE(execZeile(), QStringLiteral("Exec=\"/home/henry/Schneefall/build/schneefall\""));
        QVERIFY(plattform::autostartSetzen(false, QString()));
        QVERIFY(!plattform::autostartAktiv());
    }
    void sonderzeichenWerdenMaskiert() {
        // Leerzeichen, ", $, ` – erst Quoting-Ebene (\" \$ \`), dann String-Ebene (\ → \\).
        // Praxistest: so starten gio launch und systemd-xdg-autostart-generator (Plasma) den richtigen Pfad.
        QVERIFY(plattform::autostartSetzen(true, QStringLiteral("/home/a b/x\"y$z`q")));
        QCOMPARE(execZeile(), QStringLiteral("Exec=\"/home/a b/x\\\\\"y\\\\$z\\\\`q\""));
    }
    void steuerzeichenWerdenAbgelehnt() {
        plattform::autostartSetzen(false, QString());
        QVERIFY(!plattform::autostartSetzen(true, QStringLiteral("/tmp/x\nExec=/bin/boese")));
        QVERIFY(!plattform::autostartAktiv()); // keine Datei angelegt
        QVERIFY(!plattform::autostartSetzen(true, QStringLiteral("/tmp/x\ty")));
        QVERIFY(!plattform::autostartSetzen(true, QString()));
        QVERIFY(!plattform::autostartAktiv());
        // „=“, „%“ und Backslash im Pfad (Nachprüfung R6): abgelehnt – „%“ starten weder gio noch der
        // systemd-Generator richtig, einen Backslash der systemd-Generator nicht
        QVERIFY(!plattform::autostartSetzen(true, QStringLiteral("/opt/a=b/schneefall")));
        QVERIFY(!plattform::autostartSetzen(true, QStringLiteral("/opt/a%1/schneefall")));
        QVERIFY(!plattform::autostartSetzen(true, QStringLiteral("/opt/a\\b/schneefall")));
        QVERIFY(!plattform::autostartAktiv());
    }

private:
    QString execZeile() const {
        QFile f(m_dir.filePath(QStringLiteral("autostart/schneefall.desktop")));
        if (!f.open(QIODevice::ReadOnly)) return QStringLiteral("(keine Datei)");
        for (const QString &z : QString::fromUtf8(f.readAll()).split(QLatin1Char('\n')))
            if (z.startsWith(QStringLiteral("Exec="))) return z;
        return QStringLiteral("(kein Exec)");
    }
    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(TestAutostart)
#include "test_autostart.moc"
