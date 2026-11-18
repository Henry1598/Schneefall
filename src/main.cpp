// Schneefall – Schnee über allen Fenstern (KDE Plasma 6 / Wayland)
#include <QApplication>
#include <QDBusInterface>
#include <QDBusReply>
#include <cstdio>
#include "app.h"

int main(int argc, char **argv) {
    // ohne Wayland keine Layer-Shell
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication a(argc, argv);
    a.setApplicationName(QStringLiteral("schneefall"));
    a.setApplicationDisplayName(QStringLiteral("Schneefall"));
    a.setDesktopFileName(QStringLiteral("schneefall"));
    a.setQuitOnLastWindowClosed(false);

    const QStringList args = a.arguments();
    auto aufruf = [](const char *methode) -> int {
        QDBusInterface i(QStringLiteral("org.schneefall"), QStringLiteral("/"), QStringLiteral("org.schneefall"));
        if (!i.isValid()) {
            std::fprintf(stderr, "schneefall läuft nicht\n");
            return 1;
        }
        QDBusMessage r = i.call(QString::fromLatin1(methode));
        if (r.type() == QDBusMessage::ReplyMessage && !r.arguments().isEmpty())
            std::printf("%s\n", qPrintable(r.arguments().first().toString()));
        return r.type() == QDBusMessage::ErrorMessage ? 1 : 0;
    };
    if (args.contains(QStringLiteral("--beenden"))) return aufruf("Beenden");
    if (args.contains(QStringLiteral("--status"))) return aufruf("Status");
    if (args.contains(QStringLiteral("--umschalten"))) return aufruf("Umschalten");
    if (args.contains(QStringLiteral("--fensterliste"))) return aufruf("Fensterliste");
    if (args.contains(QStringLiteral("--raeumen"))) return aufruf("Raeumen");

    App app;
    if (!app.dienstAnmelden()) {
        // läuft schon: nur das Steuerfenster der laufenden Instanz öffnen
        return aufruf("Steuerfenster");
    }
    app.starten();
    if (args.contains(QStringLiteral("--steuerfenster"))) app.Steuerfenster();
    return a.exec();
}
