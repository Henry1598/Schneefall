// Windows-Fensterquelle, plattformneutraler Teil: aus den Rohdaten der Fenster (EnumWindows, DWM, Stile) die Liste
// für den Kern machen – ausfiltern, Taskleiste als dock, Vollbild, Stapel, physische → logische Koordinaten.
// Ohne Win32, damit die Regeln auch unter Linux getestet werden (tests/test_fensterauswahl.cpp).
#pragma once
#include <QRect>
#include <QString>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "schnee.h"

namespace plattform::win {

// ein Monitor: physische Pixel im virtuellen Desktop und Skalierung (DPR, z. B. 1.5 bei 150 %).
// Logisch rechnet Qt (Per-Monitor-DPI v2) so: Ursprung des Monitors bleibt, alles darin wird durch die Skalierung
// geteilt – siehe physZuLogisch.
struct Monitor {
    QRect phys;
    double skala = 1.0;
};

// ein oberstes Fenster, wie es Win32 meldet (alle Rechtecke physisch)
struct RohFenster {
    std::uint64_t hwnd = 0;
    QString klasse;
    QRect rahmen;                // DWMWA_EXTENDED_FRAME_BOUNDS (ohne unsichtbaren Schattenrand)
    bool sichtbar = true;        // IsWindowVisible
    bool verhuellt = false;      // DWMWA_CLOAKED (andere Arbeitsfläche, UWP im Hintergrund, Startmenü zu …)
    bool minimiert = false;      // IsIconic
    bool werkzeug = false;       // WS_EX_TOOLWINDOW
    bool appfenster = false;     // WS_EX_APPWINDOW (erzwingt Taskleisteneintrag)
    bool keinAktivieren = false; // WS_EX_NOACTIVATE (Hinweise, Einblendungen)
    bool eigeneEbene = false;    // eine Schnee-Ebene dieses Programms
};

// Gedächtnis zwischen zwei Abfragen: letzte logische Lage je Fenster (minimierte Fenster liegen bei -32000)
using Lagen = std::unordered_map<std::uint64_t, schnee::Rechteck>;

// physisches Rechteck → global logisch, über den Monitor, mit dem es sich am meisten überdeckt (sonst den nächsten)
schnee::Rechteck physZuLogisch(const QRect &phys, const std::vector<Monitor> &monitore);
// Index des Monitors (größte Überdeckung, sonst nächster); -1 ohne Monitore
int monitorZu(const QRect &phys, const std::vector<Monitor> &monitore);
// Taskleiste (Haupt- oder Zweitmonitor)
bool istTaskleiste(const QString &klasse);
// Teil der Shell oder Einblendung, trägt nie Schnee (Desktop, Startmenü, Suche, Infobereich-Überlauf, Menüs …)
bool istShellFenster(const QString &klasse);

// roh: in Stapelreihenfolge, oberstes zuerst (wie EnumWindows/GetWindow(GW_HWNDNEXT)).
// vordergrund: GetForegroundWindow(). lagen wird fortgeschrieben (verschwundene Fenster entfallen).
// Ergebnis nach dem Vertrag in plattform/fensterquelle.h; id = "0x…" (HWND hexadezimal).
std::vector<schnee::Fenster> fensterAuswaehlen(const std::vector<RohFenster> &roh, const std::vector<Monitor> &monitore,
                                               std::uint64_t vordergrund, Lagen &lagen);

// zwei Listen gleich (für „nur bei Änderung melden“)
bool gleich(const std::vector<schnee::Fenster> &a, const std::vector<schnee::Fenster> &b);

} // namespace plattform::win
