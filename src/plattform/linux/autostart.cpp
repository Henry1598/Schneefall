// Linux-Autostart: ~/.config/autostart/schneefall.desktop (XDG), Plasma startet es in Phase 2.
#include "plattform/autostart.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <optional>

namespace plattform {

namespace {
QString autostartPfad() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart/schneefall.desktop");
}

// Wert für Exec= nach der Desktop-Entry-Spezifikation: der Pfad als ein Argument in "…"; darin " ` $ mit einem
// Backslash maskiert (Quoting-Ebene), danach jeder Backslash verdoppelt (String-Ebene, wird vor dem Quoting
// ausgewertet). Abgelehnt (kein Wert): Steuerzeichen (Zeilenumbruch würde neue Schlüssel einschleusen), „=“, und –
// laut Praxistest mit gio launch und systemd-xdg-autostart-generator (startet Plasmas Autostart) – „%“ und Backslash,
// die diese Starter nicht zuverlässig zurückübersetzen.
std::optional<QString> execWert(const QString &programm) {
    if (programm.isEmpty()) return std::nullopt;
    QString arg;
    for (const QChar c : programm) {
        if (c.category() == QChar::Other_Control || c == QLatin1Char('=') || c == QLatin1Char('%') || c == QLatin1Char('\\'))
            return std::nullopt;
        if (c == QLatin1Char('"') || c == QLatin1Char('`') || c == QLatin1Char('$')) arg += QStringLiteral("\\\\");
        arg += c;
    }
    return QLatin1Char('"') + arg + QLatin1Char('"');
}
} // namespace

bool autostartAktiv() { return QFile::exists(autostartPfad()); }

bool autostartSetzen(bool an, const QString &programm) {
    const QString pfad = autostartPfad();
    if (an) {
        const std::optional<QString> exec = execWert(programm);
        if (!exec) return false;
        QDir().mkpath(QFileInfo(pfad).absolutePath());
        QSaveFile d(pfad);
        if (!d.open(QIODevice::WriteOnly)) return false;
        d.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=Schneefall\n"
                               "Comment=Schnee über allen Fenstern\nExec=%1\nIcon=weather-snow\n"
                               "X-KDE-autostart-phase=2\n")
                    .arg(*exec).toUtf8());
        return d.commit();
    }
    // nur genau diese eine, von Schneefall angelegte Datei entfernen
    QFile::remove(pfad);
    return true;
}

} // namespace plattform
