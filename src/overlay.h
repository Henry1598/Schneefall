// Durchsichtige Layer-Shell-Ebene über einem Monitor, zeichnet Flocken und Decken.
#pragma once
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWindow>
#include <vector>
#include "schnee.h"

class Overlay : public QOpenGLWindow, protected QOpenGLFunctions {
public:
    Overlay(QScreen *bildschirm, const schnee::Simulation *sim);
    ~Overlay() override;
    QScreen *bildschirm() const { return m_bildschirm; }
    void anBildschirmAnpassen();

protected:
    void initializeGL() override;
    void paintGL() override;

private:
    void ecke(float x, float y, float u, float v, float a);
    QScreen *m_bildschirm;
    const schnee::Simulation *m_sim;
    QOpenGLShaderProgram *m_prog = nullptr;
    QOpenGLBuffer m_vbo{QOpenGLBuffer::VertexBuffer};
    std::vector<float> m_daten;
};
