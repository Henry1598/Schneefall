// Bildtakt ohne Qt: Messung der Bildzeiten (Abstände gezeichneter Bilder, Jitter, verpasste
// Bilder) und wie oft eine Ebene bei gegebener Bildwiederholrate und Obergrenze zeichnet.
#pragma once
#include <array>
#include <vector>

namespace schnee {

// gleichmäßige Uhr für alle Ebenen und die Simulation (Sekunden, monoton)
double uhrSekunden();

class Bildzeiten {
public:
    // erwarteter Abstand zweier Bilder in s (Monitorbild × Teiler)
    void setzeSoll(double sekunden) { m_soll = sekunden; }
    double soll() const { return m_soll; }
    // ein Bild wurde zum Zeitpunkt t (s) gezeichnet
    void bild(double t);
    struct Bericht {
        int bilder = 0;            // gemessene Abstände
        double fps = 0;
        double mittelMs = 0, jitterMs = 0; // Mittelwert und Standardabweichung der Abstände
        double p50Ms = 0, p95Ms = 0, p99Ms = 0, maxMs = 0;
        int verpasst = 0;          // Abstände über 1,5 × Soll (mindestens ein Monitorbild ausgelassen)
        // Abstände in Vielfachen des Solls: < 0,5 | 0,5 … 0,9 | 0,9 … 1,1 | 1,1 … 1,5 | 1,5 … 2,5 | > 2,5
        std::array<int, 6> stufen{};
    };
    // Bericht seit dem letzten Abholen; danach beginnt die Zählung neu
    Bericht abholen();

private:
    double m_soll = 1.0 / 60;
    double m_letzt = -1;
    std::vector<float> m_abstaende; // s
};

// Jedes wievielte Monitorbild gezeichnet wird, damit die Obergrenze (Bilder/s, 0 = keine) gleichmäßig
// eingehalten wird: 165 Hz mit Grenze 60 → jedes 3. (55 Bilder/s, alle gleich lang) statt 2-3-3-2 …
int bildTeiler(double monitorHz, int obergrenze);

} // namespace schnee
