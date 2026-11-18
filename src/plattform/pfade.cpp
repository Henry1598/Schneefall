#include "plattform/pfade.h"
#include <QDir>
#include <QStandardPaths>

namespace plattform {

QString konfigPfad() {
#ifdef Q_OS_WIN
    // %APPDATA% (Roaming), unabhängig vom Programmnamen
    const QString roaming = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString appdata = qEnvironmentVariable("APPDATA", roaming);
    return QDir::cleanPath(appdata + QStringLiteral("/Schneefall/schneefall.conf"));
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/schneefall.conf");
#endif
}

} // namespace plattform
