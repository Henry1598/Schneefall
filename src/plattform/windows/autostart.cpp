// Windows-Autostart: Wert „Schneefall“ unter HKCU\Software\Microsoft\Windows\CurrentVersion\Run.
// Vorbereitet für Schritt 3 (dort in der VM zu prüfen); der Installer (Schritt 4) kann denselben Wert setzen.
#include "plattform/autostart.h"
#ifdef Q_OS_WIN
#include <QDir>
#include <QSettings>
#include <algorithm>
#endif

namespace plattform {

#ifdef Q_OS_WIN
namespace {
const QString RUN = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString WERT = QStringLiteral("Schneefall");
} // namespace

bool autostartAktiv() { return QSettings(RUN, QSettings::NativeFormat).contains(WERT); }

bool autostartSetzen(bool an, const QString &programm) {
    QSettings run(RUN, QSettings::NativeFormat);
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
