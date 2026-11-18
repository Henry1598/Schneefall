// Wie eine Flocke aussieht: aus der Größe wird die Tiefe (klein = fern, groß = nah),
// daraus Sprite, gezeichneter Durchmesser, Deckkraft, Drehung und Taumeln.
#pragma once
#include "schnee.h"

namespace schnee {

// Sinus/Kosinus aus einer Tabelle (1024 Schritte): genau genug fürs Drehen von Sprites,
// spart bei tausenden Flocken je Bild spürbar Rechenzeit.
float sinusSchnell(float winkel);
inline float kosinusSchnell(float winkel) { return sinusSchnell(winkel + 1.5707963f); }

enum class Sprite { Punkt, Kristall, Weich, Pulver };

struct Flockenbild {
    Sprite sprite = Sprite::Punkt;
    float durchmesser = 1; // gezeichnet, logische px
    float deckkraft = 1;
    int variante = 0;      // welche Form (fest je Flocke)
    float winkel = 0;      // Drehung in rad, ändert sich langsam im Fall
    float stauchung = 1;   // Taumeln: Flocke kippt, eine Achse wird kürzer (0,64 … 1)
};

// grundgroesse = eingestellte Flockengröße; realistisch = false: weiche Punkte wie früher
Flockenbild flockenbild(const Flocke &f, double grundgroesse, bool realistisch);
Flockenbild flockenbildOhneStaub(const Flocke &f, double grundgroesse, bool realistisch);

} // namespace schnee
