// Durchsichtige Layer-Shell-Ebene über einem Monitor, zeichnet Flocken und Decken.
// Flocken sind Sprites aus einem einmal gerasterten Atlas (kristall.h), passend zur
// Skalierung des Monitors; die Decken bekommen ihre Schattierung im Shader.
#pragma once
#include <QElapsedTimer>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWindow>
#include <QRegion>
#include <QTimer>
#include <functional>
#include <vector>
#include "bildtakt.h"
#include "deckenform.h"
#include "kristall.h"
#include "fahrzeugbild.h"
#include "raeumszene.h"
#include "schnee.h"

class Overlay : public QOpenGLWindow, protected QOpenGLFunctions {
public:
    // realistisch zeigt auf die Einstellung „Flockendetail“ (wird in jedem Bild gelesen)
    // szene: Räumdienst-Bild (wird von der App je Takt gefüllt); fahrzeugGroesse zeigt auf die Einstellung
    Overlay(QScreen *bildschirm, const schnee::Simulation *sim, const bool *realistisch, const schnee::Szene *szene = nullptr,
            const double *fahrzeugGroesse = nullptr, const schnee::Taktstand *takt = nullptr);
    ~Overlay() override;
    QScreen *bildschirm() const { return m_bildschirm; }
    void anBildschirmAnpassen();
    // Lastmessung seit dem letzten Aufruf (für --status): Zeit fürs Zusammenstellen und fürs Zeichnen, Ecken je Bild
    struct Last { double sammelnMs = 0, zeichnenMs = 0, tauschMs = 0; int ecken = 0; }; // tausch: Ende paintGL bis Bild abgegeben (Swap)
    Last lastAbholen();
    // Abstände der gezeichneten Bilder seit dem letzten Abholen (für --status)
    schnee::Bildzeiten::Bericht bildzeitenAbholen() { return m_bildzeiten.abholen(); }
    double sollAbstand() const { return m_bildzeiten.soll(); }
    // Obergrenze Bilder/s (0 = so oft wie der Monitor); gezeichnet wird jedes teiler()-te Monitorbild
    void setzeObergrenze(int bilderJeSekunde);
    int teiler() const { return m_teiler; }
    double monitorHz() const;
    // wird zu Beginn jedes Bildes mit der Uhrzeit (uhrSekunden) gerufen: dort rechnet die App die Simulation weiter
    std::function<void(double jetzt)> vorDemBild;
    // Flächen (global logisch), in denen die Ebene Mausklicks annimmt; leer = alles klickt durch
    void eingabeSetzen(const std::vector<schnee::Rechteck> &flaechen);
    // Maus über Schnee oder Fahrzeug: art 0 = gedrückt, 1 = bewegt, 2 = losgelassen; globale logische Koordinaten
    std::function<void(int art, double x, double y)> mausziel;

protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    void naechstesBild();
    void rasterNachfuehren(double jetzt);
    void teilerBerechnen();
    void flockenBauen(float bx, float by, float bw, float bh);
    void flockenZeichnen(float bx, float by, float bw, float bh, float h);
    void viereck(const float e[4][5]);
    void atlasLaden(double dpr);
    void fahrzeugAtlasLaden(double dpr);
    void fahrzeugeSammeln(float bx, float by, float bw, float bh);
    void haufenSammeln(float bx, float by, float bw, float bh, double dpr, float weich);
    void deckenSammeln(float bx, float by, float bw, float bh, double dpr, float weich);
    void brockenSammeln(float bx, float by, float bw, float bh, double dpr, float weich);
    void deckeSammeln(const schnee::Decke &d, float bx, float by, float bw, float bh, double dpr, float weich);
    QOpenGLShaderProgram *programm(const char *fragment, const char *vertex = nullptr);

    QScreen *m_bildschirm;
    const schnee::Simulation *m_sim;
    const bool *m_realistisch;
    QOpenGLShaderProgram *m_flockenProg = nullptr;
    QOpenGLShaderProgram *m_deckenProg = nullptr;
    QOpenGLShaderProgram *m_fortProg = nullptr;    // Flocken: schiebt und dreht im Vertex-Shader weiter
    QOpenGLBuffer m_flockenVbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_flockenIndex{QOpenGLBuffer::IndexBuffer};
    std::vector<float> m_flocken;  // je Flocke 4 Ecken zu FORT_WERTE Zahlen
    size_t m_flockenZahl = 0;      // Flocken im Puffer
    uint64_t m_flockenGebaut = ~0ull; // Taktstand::flockenSchritt der Flocken im Puffer
    double m_flockenUhr = 0;       // Uhrzeit des Simulationsstands, aus dem sie gebaut wurden
    uint64_t m_teileGebaut = ~0ull;   // Taktstand::schritt der Decken/Fahrzeuge im Puffer
    GLint m_deckenEcken = 0, m_fahrzeugEcken = 0;
    const schnee::Taktstand *m_takt;
    schnee::Taktstand m_eigenerTakt; // ohne App (Tests): jedes Bild ist ein Schritt
    int m_obergrenze = 0, m_teiler = 1;
    double m_bildBeginn = 0;
    double m_raster = 0;          // Teiler > 1: Beginn des nächsten Bildes (Raster aus Monitorbildern)
    bool m_rasterGueltig = false;
    bool m_perCallback = true;    // das nächste Bild kommt per Frame-Callback (nicht per Timer)
    QTimer m_naechstes;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
    GLuint m_atlasTextur = 0;
    double m_atlasDpr = 0;
    schnee::AtlasPlan m_plan;
    const schnee::Szene *m_szene;
    const double *m_fahrzeugGroesse;
    GLuint m_fahrzeugTextur = 0;
    double m_fahrzeugDpr = 0, m_fahrzeugMass = 0;
    std::vector<FahrzeugFeld> m_felder;
    int m_fahrzeugBreite = 1, m_fahrzeugHoehe = 1;
    QRegion m_eingabe;          // zuletzt gesetzte Eingaberegion (lokal)
    bool m_gedrueckt = false;
    QElapsedTimer m_uhr;
    std::vector<float> m_daten;
    std::vector<schnee::ProfilPunkt> m_punkte;
    std::vector<size_t> m_enden;
    schnee::Decke m_brockenDecke; // Arbeitsspeicher fürs Zeichnen der Brocken
    // Decken ändern sich langsam: ihre Ecken bleiben einige Bilder gültig (vorn in m_daten)
    size_t m_deckenEnde = 0;
    uint64_t m_deckenStand = ~0ull;
    int m_deckenAlter = 0;
    float m_deckenFuer[5] = {0, 0, 0, 0, 0}; // Monitorfläche und Skalierung der gespeicherten Ecken
    qint64 m_sammelNs = 0, m_zeichenNs = 0, m_eckenSumme = 0, m_tauschNs = 0;
    double m_zeichnenEnde = 0;
    int m_bilder = 0;
    schnee::Bildzeiten m_bildzeiten;
};
