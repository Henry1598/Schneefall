// Schneefall – Schnee über allen Fenstern (KDE Plasma 6 / Wayland; Windows-Backend in Arbeit)
#include <QApplication>
#include <cstdio>
#include "app.h"
#include "plattform/steuerkanal.h"

namespace {
// Befehl an die laufende Instanz: zuerst über den Steuerkanal, sonst über den Systemkanal (Linux: D-Bus, auch
// für eine laufende ältere Version). Gibt den Rückgabewert fürs Programm zurück.
int aufruf(const QString &befehl, const QStringList &argumente = {}) {
    plattform::Antwort a = plattform::Steuerkanal::senden(befehl, argumente);
    if (a.ergebnis == plattform::Antwort::Ergebnis::NichtErreicht)
        if (auto s = plattform::systemkanalSenden(befehl, argumente)) a = *s;
    if (a.ergebnis == plattform::Antwort::Ergebnis::NichtErreicht) {
        std::fprintf(stderr, "schneefall läuft nicht\n");
        return 1;
    }
    if (a.ergebnis == plattform::Antwort::Ergebnis::Unbekannt) {
        std::fprintf(stderr, "schneefall: unbekannter Befehl oder falsche Argumente: %s %s\n", qPrintable(plattform::terminalText(befehl)),
                     qPrintable(plattform::terminalText(argumente.join(QLatin1Char(' ')))));
        return 1;
    }
    // die Antwort kommt von außen (Socket/D-Bus) und enthält z. B. Fenster-ids: ohne Steuerzeichen ausgeben
    if (a.text) std::printf("%s\n", qPrintable(plattform::terminalText(*a.text)));
    return 0;
}

// die n Werte nach der Option (z. B. --zeitlupe 0.2); leer, wenn sie fehlen
QStringList werteNach(const QStringList &args, const QString &option, int n) {
    const qsizetype i = args.indexOf(option);
    if (i < 0 || i + n >= args.size()) return {};
    return args.mid(i + 1, n);
}
} // namespace

int main(int argc, char **argv) {
#ifdef Q_OS_LINUX
    // ohne Wayland keine Layer-Shell
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "wayland");
#endif
    QApplication a(argc, argv);
    a.setApplicationName(QStringLiteral("schneefall"));
    a.setApplicationDisplayName(QStringLiteral("Schneefall"));
    a.setDesktopFileName(QStringLiteral("schneefall"));
    a.setQuitOnLastWindowClosed(false);

    const QStringList args = a.arguments();
    if (args.contains(QStringLiteral("--beenden"))) return aufruf(QStringLiteral("Beenden"));
    if (args.contains(QStringLiteral("--status"))) return aufruf(QStringLiteral("Status"));
    if (args.contains(QStringLiteral("--umschalten"))) return aufruf(QStringLiteral("Umschalten"));
    if (args.contains(QStringLiteral("--fensterliste"))) return aufruf(QStringLiteral("Fensterliste"));
    if (args.contains(QStringLiteral("--raeumen"))) return aufruf(QStringLiteral("Raeumen"));
    if (args.contains(QStringLiteral("--abschuetteln"))) return aufruf(QStringLiteral("Abschuetteln"));
    // --zeitlupe 0.2   |   --werfen x y vx vy menge
    if (args.contains(QStringLiteral("--zeitlupe"))) return aufruf(QStringLiteral("Zeitlupe"), werteNach(args, QStringLiteral("--zeitlupe"), 1));
    if (args.contains(QStringLiteral("--werfen"))) return aufruf(QStringLiteral("Werfen"), werteNach(args, QStringLiteral("--werfen"), 5));

    App app;
    switch (app.dienstAnmelden()) {
    case plattform::Steuerkanal::Anmeldung::Ok:
        break;
    case plattform::Steuerkanal::Anmeldung::LaeuftSchon:
        // läuft schon: nur das Steuerfenster der laufenden Instanz öffnen
        return aufruf(QStringLiteral("Steuerfenster"));
    case plattform::Steuerkanal::Anmeldung::Fehler:
        std::fprintf(stderr, "schneefall: Start nicht möglich: %s\n", qPrintable(plattform::terminalText(app.anmeldeFehler())));
        return 1;
    }
    app.starten();
    if (args.contains(QStringLiteral("--steuerfenster"))) app.Steuerfenster();
    return a.exec();
}
