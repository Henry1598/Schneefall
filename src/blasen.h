// Sprechblasen mit freiem Text: ein kleiner Vorrat von Plätzen in einem Bild (Textur). Jeder neue Satz wird
// einmal mit QPainter in einen freien oder den am längsten ungenutzten Platz gemalt (Zeilenumbruch, Schwanz
// zum Sprecher links oder rechts) und danach nur noch als Ausschnitt gezeichnet.
#pragma once
#include <QImage>
#include <string>
#include <vector>

struct Blasenfeld {
    int x = 0, y = 0, w = 0, h = 0;   // Ausschnitt im Bild (px)
    float bw = 0, bh = 0;             // Größe der Blase in Einheiten
    float px = 0, py = 0;             // Schwanzspitze in Einheiten (zeigt auf den Sprecher)
};

class Blasenpool {
public:
    static constexpr int PLATZ_BREITE = 226, PLATZ_HOEHE = 88, TEXT_BREITE = 196;   // Einheiten
    explicit Blasenpool(int plaetze = 6);
    // Beginn eines neuen Bildes: Plätze, die in diesem Bild schon vergeben wurden, werden nicht überschrieben
    void bildBeginn() { ++m_bildNr; }
    // Feld für diesen Satz; gemalt wird nur, wenn er noch in keinem Platz steht. gespiegelt = Schwanz rechts.
    const Blasenfeld &feld(const std::string &text, bool gespiegelt, double pxJeEinheit);
    const QImage &bild() const { return m_bild; }   // RGBA8888, vormultipliziert
    // true (einmal), wenn seit dem letzten Aufruf neu gemalt wurde: dann muss die Textur neu geladen werden
    bool geaendert() { const bool g = m_geaendert; m_geaendert = false; return g; }
    int gemalt() const { return m_gemalt; }          // wie oft bisher ein Satz gemalt wurde
    int plaetze() const { return int(m_plaetze.size()); }

private:
    struct Platz { std::string text; bool gespiegelt = false; bool belegt = false; quint64 zuletzt = 0; Blasenfeld feld; };
    void malen(Platz &p, int nr);
    std::vector<Platz> m_plaetze;
    QImage m_bild;
    double m_skala = 0;
    quint64 m_bildNr = 1;
    int m_gemalt = 0;
    bool m_geaendert = false;
};
