// Linux-Autostart: ~/.config/autostart/schneefall.desktop (XDG), Plasma startet es in Phase 2.
#include "plattform/autostart.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace plattform {

namespace {
QString autostartPfad() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart/schneefall.desktop");
}
} // namespace

bool autostartAktiv() { return QFile::exists(autostartPfad()); }

void autostartSetzen(bool an, const QString &programm) {
    const QString pfad = autostartPfad();
    if (an) {
        QDir().mkpath(QFileInfo(pfad).absolutePath());
        QSaveFile d(pfad);
        if (d.open(QIODevice::WriteOnly)) {
            d.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=Schneefall\n"
                                   "Comment=Schnee über allen Fenstern\nExec=%1\nIcon=weather-snow\n"
                                   "X-KDE-autostart-phase=2\n")
                        .arg(programm).toUtf8());
            d.commit();
        }
    } else {
        // nur genau diese eine, von Schneefall angelegte Datei entfernen
        QFile::remove(pfad);
    }
}

} // namespace plattform
