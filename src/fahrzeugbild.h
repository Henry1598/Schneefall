// Fahrzeug-Atlas: Hubschrauber, Raupe und Pflug werden einmal je Monitor-Skalierung und Fahrzeuggröße aus
// ihren 3D-Modellen gerastert (alle Schichten in allen Winkelstufen, siehe fahrzeugmodell.h), Rotor,
// Lichter, Schneeball, Sprechblasen usw. mit QPainter gemalt. Danach wird nur noch aus dem Atlas gezeichnet.
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

// dpr: Gerätepixel je logischem Pixel; groesse: fahrzeug_groesse. Malt alles sofort (Probe, Tests).
FahrzeugAtlas fahrzeugAtlasErzeugen(double dpr, double groesse);
// Geteilt zwischen allen Ebenen und im Hintergrund gemalt: nullptr, solange der Atlas noch nicht fertig ist.
const FahrzeugAtlas *fahrzeugAtlasFuer(double dpr, double groesse);
