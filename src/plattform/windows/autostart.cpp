// Windows-Autostart: Wert „Schneefall“ unter HKCU\Software\Microsoft\Windows\CurrentVersion\Run.
// In der VM geprüft; der Installer (Schritt 4) kann denselben Wert setzen.
#include "plattform/autostart.h"
#ifdef Q_OS_WIN
#include <QDir>
#include <QSettings>
#include <algorithm>
#endif

namespace plattform {

#ifdef Q_OS_WIN
namespace {
const QString WERT = QStringLiteral("Schneefall");
// Tests setzen SCHNEEFALL_TEST_RUN (Schlüssel unter HKCU\Software\…), damit sie den echten Autostart nie berühren
QString runSchluessel() {
    const QString test = qEnvironmentVariable("SCHNEEFALL_TEST_RUN");
    if (!test.isEmpty()) return QStringLiteral("HKEY_CURRENT_USER\\Software\\") + test;
    return QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}
} // namespace

bool autostartAktiv() { return QSettings(runSchluessel(), QSettings::NativeFormat).contains(WERT); }

bool autostartSetzen(bool an, const QString &programm) {
    QSettings run(runSchluessel(), QSettings::NativeFormat);
    if (an) {
        // Windows-Pfade enthalten kein ", aber Steuerzeichen wären möglich: dann keinen Eintrag
        if (programm.isEmpty() || std::any_of(programm.begin(), programm.end(), [](QChar c) {
                return c.category() == QChar::Other_Control || c == QLatin1Char('"');
            }))
            return false;
        run.setValue(WERT, QLatin1Char('"') + QDir::toNativeSeparators(programm) + QLatin1Char('"'));
    } else {
        run.remove(WERT); // nur genau diesen einen, von Schneefall angelegten Eintrag
    }
    run.sync();
    return run.status() == QSettings::NoError;
}
#else
bool autostartAktiv() { return false; }
bool autostartSetzen(bool, const QString &) { return false; }
#endif

} // namespace plattform
