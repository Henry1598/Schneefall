// Fahrzeug-Atlas: Hubschrauber, Pistenraupe, kleiner Pflug, Fahrer, Sprechblasen, Schneeball …
// werden einmal je Monitor-Skalierung und Fahrzeuggröße mit QPainter als Vektorgrafik gemalt
// (Licht von links oben, Glanz, Schatten) und danach nur noch als Sprites gezeichnet.
#pragma once
#include <QImage>
#include <vector>
#include "raeumszene.h"

struct FahrzeugFeld {
    int x = 0, y = 0, w = 0, h = 0;   // Lage im Atlas in Pixeln
    float bw = 0, bh = 0;             // Größe in Einheiten
    float px = 0, py = 0;             // Drehpunkt in Einheiten (von links oben)
};

struct FahrzeugAtlas {
    QImage bild;                       // RGBA8888, vormultipliziert
    std::vector<FahrzeugFeld> felder;  // Index = schnee::Teil
};

// dpr: Gerätepixel je logischem Pixel; groesse: fahrzeug_groesse
FahrzeugAtlas fahrzeugAtlasErzeugen(double dpr, double groesse);
// geteilt zwischen allen Ebenen: je (dpr, groesse) einmal gemalt
const FahrzeugAtlas &fahrzeugAtlasFuer(double dpr, double groesse);
