// Windows-Ebene – STUB, wird in Schritt 3 (Windows-Backend) gefüllt.
// Plan: je Monitor ein randloses Fenster mit WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE |
// WS_EX_TOOLWINDOW, Alpha über DWM, Per-Monitor-DPI v2. Eingabeflächen: Mausposition mit 60 Hz prüfen und
// WS_EX_TRANSPARENT nur über Schnee/Fahrzeug entfernen. Takt: DwmFlush bzw. Swap-Intervall 1.
// Die Qt-Grundfunktionen unten sind nur ein Platzhalter, damit das Programm baut.
#include "plattform/ebene.h"
#include <QScreen>
#include <QWindow>

namespace plattform {

namespace {
class WindowsEbene : public Ebene {
public:
    void einrichten(QWindow &fenster, QScreen *bildschirm) override {
        fenster.setFlags(Qt::FramelessWindowHint | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus
                         | Qt::WindowStaysOnTopHint | Qt::Tool);
        fenster.setScreen(bildschirm);
        fenster.setTitle(QStringLiteral("Schneefall-Ebene"));
        // TODO Schritt 3: erweiterte Fensterstile über winId() setzen
    }
    void anBildschirmAnpassen(QWindow &fenster, QScreen *bildschirm) override { fenster.setGeometry(bildschirm->geometry()); }
    void eingabeSetzen(QWindow &fenster, const QRegion &lokal) override {
        // TODO Schritt 3: WS_EX_TRANSPARENT je nach Mausposition (Region) umschalten
        fenster.setFlag(Qt::WindowTransparentForInput, lokal.isEmpty());
    }
    void zeigen(QWindow &fenster, bool sichtbar) override {
        if (sichtbar) fenster.show();
        else fenster.hide();
    }
    void bildAnfordern(QWindow &fenster) override { fenster.requestUpdate(); } // TODO Schritt 3: Monitortakt (DWM)
};
} // namespace

std::unique_ptr<Ebene> ebeneErzeugen() { return std::make_unique<WindowsEbene>(); }

} // namespace plattform
