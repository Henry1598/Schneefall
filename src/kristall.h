// Schneekristalle: prozedural erzeugte sechszählige Formen, einmal gerastert und in
// einem Textur-Atlas abgelegt (mehrere Größen, passend zur Monitor-Skalierung).
// Reine Logik ohne Qt – die Ebene (overlay) lädt das Atlasbild nur noch als Textur.
#pragma once
#include <cstdint>
#include <vector>

namespace schnee {

enum class Kristallart { Dendrit, Sternplaettchen, Plaettchen };

// Linie mit Breite (verjüngt von b0 nach b1). Koordinaten im Einheitskreis, Mitte = (0,0).
struct Strich {
    float x0, y0, x1, y1;
    float b0, b1;
    uint8_t rang; // 0 = Hauptarm, 1 = Seitenast, 2/3 = feine Zweige (fallen bei kleinen Größen weg)
};

// Sechseckiges Eisplättchen, Ecken auf den Kristallachsen (0°, 60°, …)
struct Sechseck {
    float x, y, radius; // Umkreisradius
    float fuellung;     // Deckkraft der durchscheinenden Fläche (0 … 1)
    float rand;         // Breite des hellen Randes
    uint8_t rang;
};

// Ein Kristall: alle Teile bereits sechsfach gedreht und gespiegelt.
struct Kristall {
    Kristallart art = Kristallart::Dendrit;
    std::vector<Strich> striche;
    std::vector<Sechseck> sechsecke;
};

// Gleiche Saat = gleicher Kristall, andere Saat = andere Form.
Kristall kristallErzeugen(uint32_t saat);

// RGBA, 8 Bit je Kanal, vormultipliziertes Alpha
struct Bild {
    int breite = 0, hoehe = 0;
    std::vector<uint8_t> rgba;
    const uint8_t *pixel(int x, int y) const { return &rgba[(size_t(y) * size_t(breite) + size_t(x)) * 4]; }
};

// Rastert den Kristall in ein Quadrat von groessePx Pixeln: weißer Kern, bläulich
// durchscheinende Kante, Glanz auf den Plättchen. weich > 0 = unscharf (sehr nahe Flocke).
Bild kristallRastern(const Kristall &k, int groessePx, float weich = 0.f);
// Pulverklümpchen (abfallender Schnee): unregelmäßig, keine Kristallform
Bild pulverRastern(uint32_t saat, int groessePx);
// weicher runder Punkt (ferne Flocken, Flockendetail „einfach“)
Bild punktRastern(int groessePx);

struct AtlasFeld {
    int x = 0, y = 0, groesse = 0; // Gerätepixel
};

// Aufteilung des Atlas für eine Monitor-Skalierung
struct AtlasPlan {
    double dpr = 1;
    int breite = 0, hoehe = 0;
    int varianten = 0;                // verschiedene Kristalle je Größenstufe
    std::vector<double> stufen;       // logische Durchmesser der Größenstufen, aufsteigend
    std::vector<AtlasFeld> kristalle; // stufe * varianten + variante
    std::vector<AtlasFeld> weich;     // unscharfe Kristalle, je Variante eines
    std::vector<AtlasFeld> pulver;
    AtlasFeld punkt;

    const AtlasFeld &kristall(int stufe, int variante) const { return kristalle[size_t(stufe * varianten + variante)]; }
    // kleinste Stufe, die den logischen Durchmesser ohne Vergrößern abdeckt (sonst die größte)
    int stufeFuer(double durchmesser) const;
};

AtlasPlan atlasPlanen(double dpr);
Bild atlasErzeugen(const AtlasPlan &plan);

} // namespace schnee
