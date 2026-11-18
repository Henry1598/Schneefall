#include "overlay.h"
#include <LayerShellQt/Window>
#include <QScreen>
#include <algorithm>
#include <cmath>

using namespace schnee;

namespace {
const char *VERTEX = R"(
attribute vec2 pos;
attribute vec3 uva;
uniform vec4 bild;
varying vec3 v;
void main() {
    vec2 n = (pos - bild.xy) / bild.zw;
    gl_Position = vec4(n.x * 2.0 - 1.0, 1.0 - n.y * 2.0, 0.0, 1.0);
    v = uva;
}
)";
const char *FRAGMENT = R"(
#ifdef GL_ES
precision mediump float;
#endif
varying vec3 v;
void main() {
    float a;
    if (v.y < -2.0) {
        // Schneedecke: oben leicht bläulicher Schatten nach unten
        a = v.z;
        vec3 c = mix(vec3(1.0, 1.0, 1.0), vec3(0.86, 0.91, 0.98), clamp(v.x, 0.0, 1.0));
        gl_FragColor = vec4(c * a, a);
    } else {
        a = v.z * (1.0 - smoothstep(0.45, 1.0, length(v.xy)));
        gl_FragColor = vec4(vec3(0.97, 0.985, 1.0) * a, a);
    }
}
)";
} // namespace

Overlay::Overlay(QScreen *bildschirm, const Simulation *sim)
    : QOpenGLWindow(QOpenGLWindow::NoPartialUpdate), m_bildschirm(bildschirm), m_sim(sim) {
    QSurfaceFormat f = format();
    f.setAlphaBufferSize(8);
    f.setSwapInterval(0); // Takt kommt vom eigenen Timer, nicht im Swap blockieren
    f.setDepthBufferSize(0);
    f.setStencilBufferSize(0);
    setFormat(f);
    setFlags(Qt::FramelessWindowHint | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
    setScreen(bildschirm);
    setTitle(QStringLiteral("Schneefall-Ebene"));

    auto *ls = LayerShellQt::Window::get(this);
    ls->setLayer(LayerShellQt::Window::LayerOverlay);
    ls->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                                                  | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
    ls->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    ls->setExclusiveZone(-1);
    ls->setScope(QStringLiteral("schneefall"));
    ls->setWantsToBeOnActiveScreen(false);
    ls->setScreen(bildschirm);
    ls->setActivateOnShow(false);
    anBildschirmAnpassen();
}

Overlay::~Overlay() {
    makeCurrent();
    m_vbo.destroy();
    delete m_prog;
    doneCurrent();
}

void Overlay::anBildschirmAnpassen() { resize(m_bildschirm->geometry().size()); }

void Overlay::initializeGL() {
    initializeOpenGLFunctions();
    m_prog = new QOpenGLShaderProgram;
    m_prog->addShaderFromSourceCode(QOpenGLShader::Vertex, VERTEX);
    m_prog->addShaderFromSourceCode(QOpenGLShader::Fragment, FRAGMENT);
    m_prog->bindAttributeLocation("pos", 0);
    m_prog->bindAttributeLocation("uva", 1);
    if (!m_prog->link()) qWarning("Schneefall: Shader-Fehler: %s", qPrintable(m_prog->log()));
    m_vbo.create();
    m_vbo.setUsagePattern(QOpenGLBuffer::StreamDraw);
}

void Overlay::ecke(float x, float y, float u, float v, float a) {
    m_daten.insert(m_daten.end(), {x, y, u, v, a});
}

void Overlay::paintGL() {
    const qreal dpr = devicePixelRatio();
    glViewport(0, 0, int(std::lround(width() * dpr)), int(std::lround(height() * dpr)));
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    const QRect g = m_bildschirm->geometry();
    const float bx = g.x(), by = g.y(), bw = g.width(), bh = g.height();
    // Pixelraster des Monitors: Kanten auf ganze Gerätepixel runden (scharf bei 1,5×)
    auto px = [dpr](float v) { return float(std::round(v * dpr) / dpr); };

    m_daten.clear();
    // Flocken als weiche Kreise
    for (const Flocke &f : m_sim->flocken()) {
        float r = float(f.groesse * 0.5 + 0.8);
        if (f.x + r < bx || f.x - r > bx + bw || f.y + r < by || f.y - r > by + bh) continue;
        float x0 = f.x - r, x1 = f.x + r, y0 = f.y - r, y1 = f.y + r;
        float a = 0.92f;
        ecke(x0, y0, -1, -1, a); ecke(x1, y0, 1, -1, a); ecke(x0, y1, -1, 1, a);
        ecke(x1, y0, 1, -1, a); ecke(x1, y1, 1, 1, a); ecke(x0, y1, -1, 1, a);
    }
    // Schneedecken: Höhe an Spaltengrenzen gemittelt -> weiche Oberfläche
    for (const Decke &d : m_sim->decken()) {
        if (d.y < by || d.y > by + bh + 1 || d.x > bx + bw || d.x + d.breite() < bx) continue;
        const size_t n = d.hoehe.size();
        auto h = [&](long i) -> float {
            if (i < 0 || size_t(i) >= n || !d.sichtbar[size_t(i)]) return -1.f;
            return d.hoehe[size_t(i)];
        };
        for (size_t i = 0; i < n; ++i) {
            float hi = h(long(i));
            if (hi < 0.25f) continue;
            float x0 = float(d.x + i * d.spaltenBreite), x1 = float(x0 + d.spaltenBreite);
            if (x1 < bx || x0 > bx + bw) continue;
            float hl = h(long(i) - 1), hr = h(long(i) + 1);
            float l = hl < 0 ? hi * 0.55f : (hl + hi) * 0.5f;
            float r = hr < 0 ? hi * 0.55f : (hr + hi) * 0.5f;
            float y = px(float(d.y));
            float a = 0.96f;
            // v.x trägt die relative Tiefe (0 oben, 1 unten) für die Schattierung, v.y=-3 = Decke
            ecke(x0, y - l, 0, -3, a); ecke(x1, y - r, 0, -3, a); ecke(x0, y, 1, -3, a);
            ecke(x1, y - r, 0, -3, a); ecke(x1, y, 1, -3, a); ecke(x0, y, 1, -3, a);
        }
    }
    if (m_daten.empty()) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // vormultipliziertes Alpha
    m_prog->bind();
    m_prog->setUniformValue("bild", bx, by, bw, bh);
    m_vbo.bind();
    m_vbo.allocate(m_daten.data(), int(m_daten.size() * sizeof(float)));
    m_prog->enableAttributeArray(0);
    m_prog->enableAttributeArray(1);
    m_prog->setAttributeBuffer(0, GL_FLOAT, 0, 2, 5 * sizeof(float));
    m_prog->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 3, 5 * sizeof(float));
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(m_daten.size() / 5));
    m_prog->disableAttributeArray(0);
    m_prog->disableAttributeArray(1);
    m_vbo.release();
    m_prog->release();
}
