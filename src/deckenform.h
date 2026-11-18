// Form einer Schneedecke fürs Zeichnen: weiche, gerundete Oberkante und kleine
// Überhänge an den Fensterenden. Reine Logik ohne Qt.
#pragma once
#include <vector>
#include "schnee.h"

namespace schnee {

struct ProfilPunkt {
    float x;     // globale x-Koordinate
    float oben;  // Schneehöhe über der Kante (px)
    float unten; // wie weit der Schnee unter die Kante hängt (nur im Überhang > 0)
};

// Zerlegt die Decke in zusammenhängende sichtbare Schneestücke. Jedes Stück ist eine
// Punktfolge mit aufsteigendem x; dazwischen wird geradlinig verbunden.
std::vector<std::vector<ProfilPunkt>> deckenProfil(const Decke &d);
// Dasselbe ohne neue Speicheranforderung je Stück (fürs Zeichnen in jedem Bild): hängt die
// Punkte aller Stücke an „punkte“ an und je Stück dessen Endindex an „enden“.
void deckenProfilAnhaengen(const Decke &d, std::vector<ProfilPunkt> &punkte, std::vector<size_t> &enden);

} // namespace schnee
