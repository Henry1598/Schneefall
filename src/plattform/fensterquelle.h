// Fensterquelle: liefert der App die Liste der Fenster, auf denen Schnee liegen kann.
//
// Schnittstelle für jedes Plattform-Backend (Linux: KWin-Skript über D-Bus, Windows: EnumWindows/WinEventHook,
// Auswahl und DPI-Umrechnung in windows/fensterauswahl.cpp).
// Vertrag für jede gemeldete Liste (schnee::Fenster):
//   - id: dauerhaft je Fenster (gleiches Fenster = gleiche id, solange es lebt), nie leer
//   - r: äußerer sichtbarer Rahmen in globalen logischen Koordinaten (ohne Schattenrand)
//   - stapel: höher = weiter oben; nur die Reihenfolge zählt
//   - minimiert: auch versteckt oder auf einer anderen Arbeitsfläche
//   - vollbild, aktiv, dock (Taskleiste/Panel: trägt Schnee wie der Boden)
//   - maximiert/„oben“ wird NICHT gemeldet: der Kern erkennt es selbst an der Oberkante am Monitorrand
//   - die eigenen Schnee-Ebenen werden nie gemeldet (das Steuerfenster zählt wie ein normales Fenster)
// Gemeldet wird nur bei Änderung; eine unveränderte Liste löst keinen Aufruf aus.
#pragma once
#include <QString>
#include <functional>
#include <memory>
#include <vector>
#include "schnee.h"

namespace plattform {

class Fensterquelle {
public:
    using Empfaenger = std::function<void(std::vector<schnee::Fenster> liste)>;
    virtual ~Fensterquelle() = default;
    // ab jetzt Änderungen an den Empfänger melden (im Thread der App)
    virtual void starten(Empfaenger empfaenger) = 0;
    // Rohdaten der zuletzt gemeldeten Liste, für --fensterliste (Format je Backend)
    virtual QString diagnose() const = 0;
};

// die Fensterquelle der Plattform, für die gebaut wurde
std::unique_ptr<Fensterquelle> fensterquelleErzeugen();

} // namespace plattform
