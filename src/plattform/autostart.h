// Autostart: „Mit dem System starten“.
// Linux: ~/.config/autostart/schneefall.desktop; Windows: HKCU\Software\Microsoft\Windows\CurrentVersion\Run, Wert „Schneefall“.
// Ausschalten entfernt genau diesen einen, von Schneefall angelegten Eintrag.
#pragma once
#include <QString>

namespace plattform {
bool autostartAktiv();
// programm: Pfad des zu startenden Programms (normalerweise QCoreApplication::applicationFilePath()).
// false, wenn sich der Eintrag nicht anlegen ließ (z. B. Pfad mit Steuerzeichen, Linux) – dann gibt es keinen.
bool autostartSetzen(bool an, const QString &programm);
} // namespace plattform
