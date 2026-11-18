// Windows-Autostart: Wert „Schneefall“ unter HKCU\Software\Microsoft\Windows\CurrentVersion\Run.
// Vorbereitet für Schritt 3 (dort in der VM zu prüfen); der Installer (Schritt 4) kann denselben Wert setzen.
#include "plattform/autostart.h"
#ifdef Q_OS_WIN
#include <QDir>
#include <QSettings>
#endif

namespace plattform {

#ifdef Q_OS_WIN
namespace {
const QString RUN = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString WERT = QStringLiteral("Schneefall");
} // namespace

bool autostartAktiv() { return QSettings(RUN, QSettings::NativeFormat).contains(WERT); }

void autostartSetzen(bool an, const QString &programm) {
    QSettings run(RUN, QSettings::NativeFormat);
    if (an) run.setValue(WERT, QLatin1Char('"') + QDir::toNativeSeparators(programm) + QLatin1Char('"'));
    else run.remove(WERT); // nur genau diesen einen, von Schneefall angelegten Eintrag
}
#else
bool autostartAktiv() { return false; }
void autostartSetzen(bool, const QString &) {}
#endif

} // namespace plattform
