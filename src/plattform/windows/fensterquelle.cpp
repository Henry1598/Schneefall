// Windows-Fensterquelle – STUB, wird in Schritt 3 (Windows-Backend) gefüllt.
// Plan: EnumWindows + SetWinEventHook (Bewegen/Größe, Min/Max, Vordergrund, Anzeigen/Verbergen, Z-Reihenfolge),
// Rahmen über DwmGetWindowAttribute(DWMWA_EXTENDED_FRAME_BOUNDS), ausgefiltert: DWMWA_CLOAKED, Werkzeug- und eigene
// Fenster; Vollbild: deckt Monitor ab bzw. SHQueryUserNotificationState; Taskleiste über SHAppBarMessage als dock.
// Vertrag: siehe plattform/fensterquelle.h (nur bei Änderung melden, Koordinaten global logisch, DPI beachten).
#include "plattform/fensterquelle.h"

namespace plattform {

namespace {
class WindowsFensterquelle : public Fensterquelle {
public:
    void starten(Empfaenger empfaenger) override { m_empfaenger = std::move(empfaenger); /* TODO Schritt 3 */ }
    QString diagnose() const override { return QString(); }

private:
    Empfaenger m_empfaenger;
};
} // namespace

std::unique_ptr<Fensterquelle> fensterquelleErzeugen() { return std::make_unique<WindowsFensterquelle>(); }

} // namespace plattform
