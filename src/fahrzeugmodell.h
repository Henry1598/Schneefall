// 3D-Modelle der Fahrzeuge fürs Wenden (Pistenraupe, kleiner Pflug, Hubschrauber) aus einfachen Körpern.
// Die Seitenflächen tragen die gemalten Seitenbilder als Textur, damit die Seitenansicht dem
// gewohnten Bild gleicht; Front, Heck und Dach sind eigene Flächen. Ohne Qt.
#pragma once
#include "ansicht3d.h"

namespace schnee {

// Seitenbilder, die fahrzeugbild.cpp als Texturen bereitstellt
enum Seitenbild { BildRaupeAufbau, BildRaupeKette, BildHeli, BildPflug, BildRad, SeitenbildAnzahl };

// Farben (ARGB), an denen Tests die Teile erkennen
constexpr uint32_t FARBE_SCHILD_STAHL = 0xffc6ced8, FARBE_RUECKLICHT = 0xffe02820, FARBE_SCHEINWERFER = 0xfffff4c0,
                   FARBE_KUEHLER = 0xff22252b, FARBE_GLAS = 0xff35587e, FARBE_KETTE_DUNKEL = 0xff1e2127, FARBE_KETTE_HELL = 0xff6c7480;

// kettenPhase 0/1: die Kettenglieder sind um ein halbes Glied versetzt (Ketten laufen beim Drehen)
Modell raupenModell(int kettenPhase);
Modell pflugModell(int radPhase);
Modell heliModell();

// Bildrahmen der vorgemalten Ansichten in Einheiten: Breite, Höhe, Drehpunkt (Modellursprung)
struct Rahmen { float w, h, px, py; };
constexpr Rahmen RAHMEN_RAUPE{100, 70, 50, 60}, RAHMEN_PFLUG{68, 48, 34, 40}, RAHMEN_HELI{196, 80, 98, 62};

// Anbaupunkte im Modell (für Leuchten, Rauch, Schnee auf dem Dach, Rotor)
constexpr P3 RAUPE_LEUCHTE{-16, 48, 0}, RAUPE_AUSPUFF{11, 45, 4.4}, RAUPE_DACH{-10, 46.2, 0};
constexpr P3 PFLUG_LEUCHTE{-10.5, 30, 0};
constexpr P3 HELI_ROTOR{-4, 42.5, 0}, HELI_BLINKLICHT{-22, -0.5, 0};

} // namespace schnee
