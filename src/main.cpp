// Schneefall – Schnee über allen Fenstern (KDE Plasma 6 / Wayland, Windows 10/11)
#include <QApplication>
#include <QDeadlineTimer>
#include <QMessageBox>
#include <QThread>
#include <cstdio>
#include <cstring>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#endif
#include "app.h"
#include "version.h"
#include "plattform/steuerkanal.h"

namespace {
#ifdef Q_OS_WIN
// Windows-Programm (ohne eigenes Konsolenfenster): Ausgaben in die Konsole, aus der es gestartet wurde. Ist die
// Ausgabe umgeleitet (Pipe/Datei), bleibt sie, wie sie ist. true, wenn es eine Konsole oder Umleitung gibt.
bool konsoleAnbinden() {
    auto umgeleitet = [](FILE *f) { return _fileno(f) >= 0 && _get_osfhandle(_fileno(f)) >= 0; };
    const bool aus = umgeleitet(stdout), fehler = umgeleitet(stderr);
    if (aus && fehler) return true;
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return aus || fehler;
    FILE *f = nullptr; // nur die nicht umgeleiteten Kanäle in die Konsole
    if (!aus) freopen_s(&f, "CONOUT$", "w", stdout);
    if (!fehler) freopen_s(&f, "CONOUT$", "w", stderr);
    return true;
}
bool g_konsole = false;
#endif

// Zeile ausgeben: in eine Windows-Konsole als UTF-16 (richtige Umlaute unabhängig von deren Codepage, ohne sie zu
// verstellen), in Datei/Pipe und unter Linux als UTF-8
void zeile(FILE *ziel, const QString &text) {
#ifdef Q_OS_WIN
    std::fflush(ziel);
    HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(ziel)));
    DWORD modus = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &modus)) {
        const QString t = text + QLatin1Char('\n');
        DWORD n = 0;
        WriteConsoleW(h, reinterpret_cast<const wchar_t *>(t.utf16()), DWORD(t.size()), &n, nullptr);
        return;
    }
#endif
    std::fprintf(ziel, "%s\n", text.toUtf8().constData());
    std::fflush(ziel);
}

// Meldung an den Menschen: in die Konsole; unter Windows ohne Konsole (Start aus dem Explorer) als Fenster
void melden(const QString &text, bool fehler) {
    zeile(fehler ? stderr : stdout, text);
#ifdef Q_OS_WIN
    if (!g_konsole && qApp) {
        QMessageBox box(fehler ? QMessageBox::Warning : QMessageBox::Information, QStringLiteral("Schneefall"), text);
        box.setTextFormat(Qt::PlainText); // kein Rich-Text aus Pfaden/Namen
        box.exec();
    }
#endif
}

// Befehl an die laufende Instanz: zuerst über den Steuerkanal, sonst über den Systemkanal (Linux: D-Bus, auch
// für eine laufende ältere Version). Gibt den Rückgabewert fürs Programm zurück.
int aufruf(const QString &befehl, const QStringList &argumente = {}) {
    plattform::Antwort a = plattform::Steuerkanal::senden(befehl, argumente);
    if (a.ergebnis == plattform::Antwort::Ergebnis::NichtErreicht)
        if (auto s = plattform::systemkanalSenden(befehl, argumente)) a = *s;
    if (a.ergebnis == plattform::Antwort::Ergebnis::NichtErreicht) {
        // Windows: Zugriff verweigert (erhöht gestartet, fremder Besitzer) – Grund aus dem Steuerkanal
        if (a.text) melden(QStringLiteral("schneefall: ") + plattform::terminalText(*a.text), true);
        else zeile(stderr, QStringLiteral("schneefall läuft nicht"));
        return 1;
    }
    if (a.ergebnis == plattform::Antwort::Ergebnis::Unbekannt) {
        zeile(stderr, QStringLiteral("schneefall: unbekannter Befehl oder falsche Argumente: %1 %2")
                          .arg(plattform::terminalText(befehl), plattform::terminalText(argumente.join(QLatin1Char(' ')))));
        return 1;
    }
    // die Antwort kommt von außen (Socket/D-Bus) und enthält z. B. Fenster-ids: ohne Steuerzeichen ausgeben
    if (a.text) zeile(stdout, plattform::terminalText(*a.text));
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
    // --version ohne Qt-Start (läuft auch ohne Bildschirm/Wayland); Windows: in die Konsole bzw. Umleitung
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") != 0) continue;
#ifdef Q_OS_WIN
        konsoleAnbinden();
#endif
        zeile(stdout, QStringLiteral("schneefall ") + QStringLiteral(SCHNEEFALL_VERSION));
        return 0;
    }
#ifdef Q_OS_LINUX
    // ohne Wayland keine Layer-Shell
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "wayland");
#endif
#ifdef Q_OS_WIN
    g_konsole = konsoleAnbinden();
    // Ohne OpenGL-Treiber (VM, Fernsitzung) nimmt Qt opengl32sw.dll (Mesa). Dessen D3D12-Weg zeigt in
    // Layered-Fenstern nichts an; llvmpipe zeichnet zuverlässig durchsichtig. Betrifft nur Mesa.
    if (qEnvironmentVariableIsEmpty("GALLIUM_DRIVER")) qputenv("GALLIUM_DRIVER", "llvmpipe");
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
    {
        // läuft schon: nur das Steuerfenster der laufenden Instanz öffnen. Bei gleichzeitigem Start hält die andere
        // Instanz zwar schon die Sperre, lauscht aber evtl. noch nicht – kurz nachfassen statt „läuft nicht“ zu melden
        const QDeadlineTimer frist(5000);
        while (!frist.hasExpired()) {
            const plattform::Antwort a = plattform::Steuerkanal::senden(QStringLiteral("Status"), {});
            if (a.ergebnis != plattform::Antwort::Ergebnis::NichtErreicht || a.text) break;
            QThread::msleep(100);
        }
        return aufruf(QStringLiteral("Steuerfenster"));
    }
    case plattform::Steuerkanal::Anmeldung::Fehler:
        melden(QStringLiteral("schneefall: Start nicht möglich: ") + plattform::terminalText(app.anmeldeFehler()), true);
        return 1;
    }
    if (!app.anmeldeHinweis().isEmpty()) melden(QStringLiteral("schneefall: ") + app.anmeldeHinweis(), false);
    app.starten();
    if (args.contains(QStringLiteral("--steuerfenster"))) app.Steuerfenster();
    return a.exec();
}
