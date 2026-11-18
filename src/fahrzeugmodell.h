// 3D-Modelle der Fahrzeuge des Räumdienstes: Kettendozer (Raupe), kleiner Radpflug, Hubschrauber.
// Jedes Fahrzeug besteht aus Schichten, die einzeln je Gierwinkel vorgemalt und beim Zeichnen
// zusammengesetzt werden – so können Ketten und Räder laufen und das Schild sich heben. Ohne Qt.
#pragma once
#include "ansicht3d.h"

namespace schnee {

constexpr int KETTENPHASEN = 6;   // Bilder je Kettenglied: genug, um die Laufrichtung zu erkennen
constexpr int RADPHASEN = 8;      // Bilder je 60° Raddrehung (Felge und Profil sind sechszählig)

Modell raupeKoerper(bool mitFahrer);
Modell raupeKette(int seite, int phase);   // seite: +1 rechte, -1 linke Kette; phase 0 … KETTENPHASEN-1
Modell raupeSchild();
Modell pflugKoerper(bool mitFahrer);
Modell pflugRad(int phase);                // einzelnes Rad um seinen Mittelpunkt; phase 0 … RADPHASEN-1
Modell pflugSchild();
Modell heliKoerper();
Modell fahrerStehend(int haltung);         // ausgestiegener Fahrer (Füße im Ursprung), Faust oben (0) oder vorn (1)

// Bildrahmen der vorgemalten Ansichten in Einheiten: Breite, Höhe, Lage des Modellursprungs
struct Rahmen { float w, h, px, py; };
constexpr Rahmen RAHMEN_RAUPE{76, 66, 38, 56}, RAHMEN_KETTE{84, 36, 42, 27}, RAHMEN_RSCHILD{96, 58, 48, 46},
                 RAHMEN_PFLUG{56, 46, 28, 38}, RAHMEN_RAD{16, 16, 8, 8}, RAHMEN_PSCHILD{66, 34, 33, 26}, RAHMEN_HELI{200, 84, 100, 64}, RAHMEN_FAHRER{26, 36, 13, 32};

// Maße und Anbaupunkte im Modell
constexpr double RAUPE_SCHILDHUB = 9;     // so weit hebt sich das Schild (Einheiten)
constexpr double PFLUG_SCHILDHUB = 5;
constexpr double PFLUG_RADSTAND = 22, PFLUG_RADHOEHE = 5.8, PFLUG_SPUR = 9.2;   // Achsen bei x = ±11, Räder bei z = ±9,2
constexpr P3 RAUPE_LEUCHTE{-16, 47.4, 0}, RAUPE_AUSPUFF{11, 44.5, 4.4}, RAUPE_DACH{-9.5, 45.2, 0}, RAUPE_STEHPLATZ{-9, 17.0, 12.4};   // auf der Kette neben der Kabinentür
constexpr P3 PFLUG_LEUCHTE{6, 28.6, 0}, PFLUG_AUSPUFF{-0.4, 27.4, 7}, PFLUG_STEHPLATZ{5.5, 6.9, 9.4};   // auf der Trittstufe
constexpr P3 HELI_ROTOR{-4, 45.4, 0}, HELI_BLINKLICHT{-10, -1.8, 0}, HELI_HECKBLITZ{-85, 43.5, 0}, HELI_GRUEN{30, 9, 13.8}, HELI_ROT{30, 9, -13.8};

} // namespace schnee
