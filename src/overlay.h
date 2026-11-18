// Durchsichtige Layer-Shell-Ebene über einem Monitor, zeichnet Flocken und Decken.
// Flocken sind Sprites aus einem einmal gerasterten Atlas (kristall.h), passend zur
// Skalierung des Monitors; die Decken bekommen ihre Schattierung im Shader.
#pragma once
#include <QElapsedTimer>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWindow>
#include <vector>
#include "deckenform.h"
#include "kristall.h"
#include "schnee.h"

class Overlay : public QOpenGLWindow, protected QOpenGLFunctions {
public:
    // realistisch zeigt auf die Einstellung „Flockendetail“ (wird in jedem Bild gelesen)
    Overlay(QScreen *bildschirm, const schnee::Simulation *sim, const bool *realistisch);
    ~Overlay() override;
    QScreen *bildschirm() const { return m_bildschirm; }
    void anBildschirmAnpassen();
    // Lastmessung seit dem letzten Aufruf (für --status): Zeit fürs Zusammenstellen und fürs Zeichnen, Ecken je Bild
    struct Last { double sammelnMs = 0, zeichnenMs = 0; int ecken = 0; };
    Last lastAbholen();

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    void viereck(const float e[4][5]);
    void atlasLaden(double dpr);
    void flockenSammeln(float bx, float by, float bw, float bh);
    void deckenSammeln(float bx, float by, float bw, float bh, double dpr, float weich);
    QOpenGLShaderProgram *programm(const char *fragment);

    QScreen *m_bildschirm;
    const schnee::Simulation *m_sim;
    const bool *m_realistisch;
    QOpenGLShaderProgram *m_flockenProg = nullptr;
    QOpenGLShaderProgram *m_deckenProg = nullptr;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
    GLuint m_atlasTextur = 0;
    double m_atlasDpr = 0;
    schnee::AtlasPlan m_plan;
    QElapsedTimer m_uhr;
    std::vector<float> m_daten;
    std::vector<schnee::ProfilPunkt> m_punkte;
    std::vector<size_t> m_enden;
    // Decken ändern sich langsam: ihre Ecken bleiben einige Bilder gültig (vorn in m_daten)
    size_t m_deckenEnde = 0;
    uint64_t m_deckenStand = ~0ull;
    int m_deckenAlter = 0;
    float m_deckenFuer[5] = {0, 0, 0, 0, 0}; // Monitorfläche und Skalierung der gespeicherten Ecken
    qint64 m_sammelNs = 0, m_zeichenNs = 0, m_eckenSumme = 0;
    int m_bilder = 0;
};
