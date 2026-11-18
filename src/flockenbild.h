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
    float drehtempo = 0;   // rad/s, wie schnell sich winkel gerade ändert
    float stauchTempo = 0; // 1/s, relative Änderung der Stauchung (d stauchung/dt ÷ stauchung)
};

// Lage einer Flocke als Viereck zur Simulationszeit, mit allem, was der Vertex-Shader braucht,
// um sie bis zum nächsten Simulationsschritt stufenlos weiterzuschieben und zu drehen.
struct FlockenViereck {
    float mx = 0, my = 0;       // Mitte
    float ax = 0, ay = 0;       // halbe Achse quer (gedreht)
    float qx = 0, qy = 0;       // halbe Achse längs (gedreht und gestaucht)
    float vx = 0, vy = 0;       // Tempo der Mitte, px/s
    float drehtempo = 0;        // rad/s
    float stauchTempo = 0;      // 1/s
};
FlockenViereck flockenViereck(const Flocke &f, const Flockenbild &b);
// Ecken nach h Sekunden, genau wie im Vertex-Shader: links oben, rechts oben, links unten, rechts unten
void viereckEcken(const FlockenViereck &v, float h, float ecken[4][2]);

// grundgroesse = eingestellte Flockengröße; realistisch = false: weiche Punkte wie früher
Flockenbild flockenbild(const Flocke &f, double grundgroesse, bool realistisch);
Flockenbild flockenbildOhneStaub(const Flocke &f, double grundgroesse, bool realistisch);

} // namespace schnee
