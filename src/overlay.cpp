#include "overlay.h"
#include <LayerShellQt/Window>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QScreen>
#include <algorithm>
#include <cmath>
#include <map>
#include "flockenbild.h"

using namespace schnee;

namespace {
// Ecke: Position (logische Bildschirmkoordinaten) und drei Werte, je nach Programm:
// Flocken: Atlas-Koordinate u, v und Deckkraft
// Decken:  Tiefe unter der Oberfläche, Abstand zur Unterseite (beides px) und Deckkraft
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
// Flocken: Lage zur Simulationszeit, dazu Tempo, Drehtempo und Stauchtempo; der Shader schreibt sie
// um h Sekunden fort (wie viereckEcken in flockenbild.cpp). So bewegen und drehen sie sich in jedem
// Monitorbild stufenlos, obwohl die CPU sie nur 30-mal je Sekunde neu zusammenstellt.
const int FORT_WERTE = 13;
const char *FORT = R"(
attribute vec2 mitte;
attribute vec2 achseA;
attribute vec2 achseQ;
attribute vec2 tempo;
attribute vec2 dreh;
attribute vec3 uva;
uniform vec4 bild;
uniform float h;
varying vec3 v;
void main() {
    float w = dreh.x * h;
    float c = cos(w), s = sin(w);
    vec2 o = achseA + achseQ * (1.0 + dreh.y * h);
    vec2 p = mitte + tempo * h + vec2(c * o.x - s * o.y, s * o.x + c * o.y);
    vec2 n = (p - bild.xy) / bild.zw;
    gl_Position = vec4(n.x * 2.0 - 1.0, 1.0 - n.y * 2.0, 0.0, 1.0);
    v = uva;
}
)";
// dasselbe mit Instanzen (OpenGL 3.3 / ES 3): je Flocke ein Satz Werte statt vier Ecken, die Ecke kommt
// aus einem festen Puffer – ein Viertel der Daten, die die CPU je Aufbau schreibt und hochlädt
const int INST_WERTE = 15;
const char *INST = R"(
attribute vec2 ecke;
attribute vec2 mitte;
attribute vec2 achseA;
attribute vec2 achseQ;
attribute vec2 tempo;
attribute vec2 dreh;
attribute vec4 feld;
attribute float deckkraft;
uniform vec4 bild;
uniform float h;
varying vec3 v;
void main() {
    float w = dreh.x * h;
    float c = cos(w), s = sin(w);
    vec2 o = ecke.x * achseA + ecke.y * achseQ * (1.0 + dreh.y * h);
    vec2 p = mitte + tempo * h + vec2(c * o.x - s * o.y, s * o.x + c * o.y);
    vec2 n = (p - bild.xy) / bild.zw;
    gl_Position = vec4(n.x * 2.0 - 1.0, 1.0 - n.y * 2.0, 0.0, 1.0);
    v = vec3(mix(feld.xy, feld.zw, ecke * 0.5 + 0.5), deckkraft);
}
)";
// je Zeichenaufruf höchstens so viele Flocken (16-Bit-Indizes: 4 Ecken je Flocke)
const int FLOCKEN_JE_AUFRUF = 16000;
const char *FLOCKEN = R"(
#ifdef GL_ES
precision mediump float;
#endif
varying vec3 v;
uniform sampler2D atlas;
void main() {
    gl_FragColor = texture2D(atlas, v.xy) * v.z; // Atlas ist vormultipliziert
}
)";
const char *DECKEN = R"(
#ifdef GL_ES
precision highp float;
#endif
varying vec3 v;
uniform float weich; // halbe Breite des weichen Randes in px
uniform float korn;  // Größe eines Schneekorns in Gerätepixeln
uniform float zeit;
float zufall(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void main() {
    // weiche Ober- und Unterkante statt harter Pixelstufen
    float a = v.z * smoothstep(-weich, weich, v.x) * smoothstep(-weich, weich, v.y);
    // oben weiß, nach unten zart blaugrau (Schnee schluckt Licht in der Tiefe)
    float s = smoothstep(0.0, 11.0, v.x);
    vec3 c = mix(vec3(1.0), vec3(0.76, 0.84, 0.95), s);
    vec2 zelle = floor(gl_FragCoord.xy / korn);
    float n = zufall(zelle);
    c -= 0.05 * n * (0.3 + 0.7 * s); // feine Körnung
    // einzelne Kristalle glitzern und funkeln langsam
    float funke = step(0.972, zufall(zelle + 17.0));
    float blink = 0.5 + 0.5 * sin(zeit * (0.8 + 2.4 * n) + n * 60.0);
    c = mix(c, vec3(1.0), funke * blink);
    gl_FragColor = vec4(c * a, a);
}
)";

// Atlasbilder sind je Skalierung gleich: einmal rastern, alle Ebenen teilen sie sich
struct Atlas { AtlasPlan plan; Bild bild; };
const Atlas &atlasFuer(double dpr) {
    static std::map<int, Atlas> lager;
    const int schluessel = int(std::lround(dpr * 100));
    auto it = lager.find(schluessel);
    if (it == lager.end()) {
        Atlas a;
        a.plan = atlasPlanen(dpr);
        a.bild = atlasErzeugen(a.plan);
        it = lager.emplace(schluessel, std::move(a)).first;
    }
    return it->second;
}
} // namespace

Overlay::Overlay(QScreen *bildschirm, const Simulation *sim, const bool *realistisch, const Szene *szene, const double *fahrzeugGroesse,
                 const Taktstand *takt)
    : QOpenGLWindow(QOpenGLWindow::NoPartialUpdate), m_bildschirm(bildschirm), m_sim(sim), m_realistisch(realistisch), m_szene(szene),
      m_fahrzeugGroesse(fahrzeugGroesse) {
    m_takt = takt ? takt : &m_eigenerTakt;
    QSurfaceFormat f = format();
    f.setAlphaBufferSize(8);
    // nicht im Swap blockieren (alle Ebenen teilen sich einen Thread); den Takt geben die Frame-Callbacks
    // des Compositors: nach jedem Bild requestUpdate(), Qt liefert das nächste zum nächsten Monitorbild
    f.setSwapInterval(0);
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
    m_uhr.start();
    m_naechstes.setSingleShot(true);
    m_naechstes.setTimerType(Qt::PreciseTimer);
    connect(&m_naechstes, &QTimer::timeout, this, [this] { requestUpdate(); });
    connect(this, &QOpenGLWindow::frameSwapped, this, &Overlay::naechstesBild);
    connect(bildschirm, &QScreen::refreshRateChanged, this, [this] { teilerBerechnen(); });
    teilerBerechnen();
}

double Overlay::monitorHz() const { return m_bildschirm->refreshRate(); }

void Overlay::setzeObergrenze(int bilderJeSekunde) {
    if (bilderJeSekunde == m_obergrenze) return;
    m_obergrenze = bilderJeSekunde;
    teilerBerechnen();
}

void Overlay::teilerBerechnen() {
    const double hz = monitorHz() > 1 ? monitorHz() : 60.0;
    m_teiler = bildTeiler(hz, m_obergrenze);
    m_rasterGueltig = false;
    m_bildzeiten.setzeSoll(m_teiler / hz);
}

// Nach jedem Bild das nächste anfordern. Teiler 1: Qt liefert es mit dem nächsten Frame-Callback des
// Compositors, also genau einmal je Monitorbild. Teiler n: ein Bild, das per Frame-Callback kam, gibt die
// Lage der Monitorbilder vor (es beginnt kurz nach einem Monitorbild). Die weiteren Bilder beginnen auf
// einem festen Raster jeweils n Monitorbilder später, ein gutes Stück vor dem nächsten Zusammensetzen
// durch KWin. So zeigt KWin jedes genau n Monitorbilder nach dem
// vorigen, ohne Wechsel zwischen n-1 und n+1. Das Raster läuft fest weiter (Timer-Ungenauigkeit sammelt
// sich nicht an); wird ein Bild zu spät gezeichnet, richtet ein Frame-Callback-Bild es neu aus.
void Overlay::naechstesBild() {
    m_tauschNs += qint64((uhrSekunden() - m_zeichnenEnde) * 1e9);
    if (!isVisible()) return;
    if (m_teiler <= 1 || !m_rasterGueltig) {
        m_perCallback = true;
        requestUpdate();
        return;
    }
    m_naechstes.start(std::max(0, int(std::floor((m_raster - uhrSekunden()) * 1000))));
}

// im Bild: Raster nachführen (jetzt = Beginn dieses Bildes)
void Overlay::rasterNachfuehren(double jetzt) {
    const double periode = 1.0 / (monitorHz() > 1 ? monitorHz() : 60.0);
    if (m_teiler <= 1) {
        m_rasterGueltig = false;
    } else if (m_perCallback) {
        // Frame-Callback-Bild: KWin hat gerade zusammengesetzt (kurz vor oder nach einem Monitorbild), dieses Bild
        // erscheint mit dem nächsten. Das folgende soll n Monitorbilder später erscheinen: ein Drittel Periode
        // nach dem entsprechenden Monitorbild beginnen, mit Luft zu beiden Zusammensetz-Zeitpunkten.
        m_raster = jetzt + (m_teiler + 0.35) * periode;
        m_rasterGueltig = true;
    } else if (jetzt - m_raster > 0.35 * periode || jetzt - m_raster < -0.35 * periode) {
        m_rasterGueltig = false; // aus dem Tritt (Last, Pause): mit dem nächsten Frame-Callback neu ausrichten
    } else {
        m_raster += m_teiler * periode;
    }
    m_perCallback = false;
}

Overlay::~Overlay() {
    makeCurrent();
    m_vbo.destroy();
    if (m_atlasTextur) glDeleteTextures(1, &m_atlasTextur);
    if (m_fahrzeugTextur) glDeleteTextures(1, &m_fahrzeugTextur);
    if (m_blasenTextur) glDeleteTextures(1, &m_blasenTextur);
    m_flockenVbo.destroy();
    m_flockenIndex.destroy();
    m_eckenVbo.destroy();
    delete m_instProg;
    delete m_flockenProg;
    delete m_deckenProg;
    delete m_fortProg;
    doneCurrent();
}

void Overlay::anBildschirmAnpassen() { resize(m_bildschirm->geometry().size()); }

QOpenGLShaderProgram *Overlay::programm(const char *fragment, const char *vertex) {
    auto *p = new QOpenGLShaderProgram;
    p->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex ? vertex : VERTEX);
    p->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment);
    if (vertex == INST) {
        int i = 0;
        for (const char *name : {"ecke", "mitte", "achseA", "achseQ", "tempo", "dreh", "feld", "deckkraft"}) p->bindAttributeLocation(name, i++);
    } else if (vertex) {
        int i = 0;
        for (const char *name : {"mitte", "achseA", "achseQ", "tempo", "dreh", "uva"}) p->bindAttributeLocation(name, i++);
    } else {
        p->bindAttributeLocation("pos", 0);
        p->bindAttributeLocation("uva", 1);
    }
    if (!p->link()) qWarning("Schneefall: Shader-Fehler: %s", qPrintable(p->log()));
    return p;
}

void Overlay::initializeGL() {
    initializeOpenGLFunctions();
    m_flockenProg = programm(FLOCKEN);
    m_deckenProg = programm(DECKEN);
    m_fortProg = programm(FLOCKEN, FORT);
    // Instanzen, wenn der Kontext sie kann (sonst vier Ecken je Flocke)
    {
        QOpenGLContext *k = context();
        const auto v = k->format().version();
        m_instanzen = k->isOpenGLES() ? v.first >= 3 : (v >= qMakePair(3, 3) || k->hasExtension("GL_ARB_instanced_arrays"));
        if (qEnvironmentVariableIsSet("SCHNEEFALL_OHNE_INSTANZEN")) m_instanzen = false;
        if (m_instanzen) {
            m_instProg = programm(FLOCKEN, INST);
            if (!m_instProg->isLinked()) m_instanzen = false;
            static const float ecken[8] = {-1, -1, 1, -1, -1, 1, 1, 1};
            m_eckenVbo.create();
            m_eckenVbo.bind();
            m_eckenVbo.allocate(ecken, sizeof ecken);
            m_eckenVbo.release();
        }
    }
    m_vbo.create();
    m_vbo.setUsagePattern(QOpenGLBuffer::StreamDraw);
    m_flockenVbo.create();
    m_flockenVbo.setUsagePattern(QOpenGLBuffer::StreamDraw);
    // feste Indizes: Flocke k besteht aus den Ecken 4k … 4k+3 (links oben, rechts oben, links unten, rechts unten)
    std::vector<GLushort> index(size_t(FLOCKEN_JE_AUFRUF) * 6);
    for (int k = 0; k < FLOCKEN_JE_AUFRUF; ++k) {
        const GLushort b = GLushort(4 * k), folge[6] = {b, GLushort(b + 1), GLushort(b + 2), GLushort(b + 1), GLushort(b + 3), GLushort(b + 2)};
        std::copy_n(folge, 6, index.begin() + 6 * k);
    }
    m_flockenIndex.create();
    m_flockenIndex.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_flockenIndex.bind();
    m_flockenIndex.allocate(index.data(), int(index.size() * sizeof(GLushort)));
    m_flockenIndex.release();
    m_flockenGebaut = m_teileGebaut = ~0ull; // neuer Kontext: Puffer neu füllen
    m_atlasTextur = 0; // neuer Kontext: Textur neu anlegen
    m_atlasDpr = 0;
    m_fahrzeugTextur = 0;
    m_fahrzeugDpr = 0;
    m_blasenTextur = 0;
}

// Lädt den Atlas für diese Skalierung als Textur (einmal, und erneut, wenn sich die Skalierung ändert)
void Overlay::atlasLaden(double dpr) {
    if (m_atlasTextur && dpr == m_atlasDpr) return;
    const Atlas &a = atlasFuer(dpr);
    m_plan = a.plan;
    m_atlasDpr = dpr;
    if (!m_atlasTextur) glGenTextures(1, &m_atlasTextur);
    glBindTexture(GL_TEXTURE_2D, m_atlasTextur);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, a.bild.breite, a.bild.hoehe, 0, GL_RGBA, GL_UNSIGNED_BYTE, a.bild.rgba.data());
}

// Fahrzeug-Atlas für Skalierung und Fahrzeuggröße (erst beim ersten Einsatz, neu bei geänderter Größe)
bool Overlay::fahrzeugAtlasLaden(double dpr) {
    const double mass = m_fahrzeugGroesse ? *m_fahrzeugGroesse : 1.0;
    if (m_fahrzeugTextur && dpr == m_fahrzeugDpr && mass == m_fahrzeugMass) return true;
    const FahrzeugAtlas *fertig = fahrzeugAtlasFuer(dpr, mass);
    if (!fertig) return false;   // wird noch im Hintergrund gemalt
    const FahrzeugAtlas &a = *fertig;
    m_felder = a.felder;
    m_fahrzeugBreite = a.bild.width();
    m_fahrzeugHoehe = a.bild.height();
    m_fahrzeugDpr = dpr;
    m_fahrzeugMass = mass;
    if (!m_fahrzeugTextur) glGenTextures(1, &m_fahrzeugTextur);
    glBindTexture(GL_TEXTURE_2D, m_fahrzeugTextur);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // Zeilen von QImage sind auf 4 Byte ausgerichtet; RGBA hat ohnehin 4 Byte je Pixel
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, a.bild.width(), a.bild.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, a.bild.constBits());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    return true;
}

// Räumdienst: jedes Teil ist ein gedrehtes, evtl. gespiegeltes Viereck aus dem Fahrzeug-Atlas
void Overlay::fahrzeugeSammeln(float bx, float by, float bw, float bh) {
    const float aw = float(m_fahrzeugBreite), ah = float(m_fahrzeugHoehe);
    for (const Zeichenteil &z : m_szene->teile) {
        if (z.teil >= m_felder.size() || z.alpha <= 0.004f) continue;
        const FahrzeugFeld &f = m_felder[z.teil];
        const float reich = (f.bw + f.bh) * std::max(std::abs(z.sx), std::abs(z.sy));
        if (z.x + reich < bx || z.x - reich > bx + bw || z.y + reich < by || z.y - reich > by + bh) continue;
        const float c = std::cos(z.winkel), s = std::sin(z.winkel);
        const float x0 = -f.px * z.sx, x1 = (f.bw - f.px) * z.sx, y0 = -f.py * z.sy, y1 = (f.bh - f.py) * z.sy;
        const float u0 = (f.x + 0.5f) / aw, u1 = (f.x + f.w - 0.5f) / aw, v0 = (f.y + 0.5f) / ah, v1 = (f.y + f.h - 0.5f) / ah;
        const float e[4][5] = {{z.x + c * x0 - s * y0, z.y + s * x0 + c * y0, u0, v0, z.alpha}, {z.x + c * x1 - s * y0, z.y + s * x1 + c * y0, u1, v0, z.alpha},
                               {z.x + c * x0 - s * y1, z.y + s * x0 + c * y1, u0, v1, z.alpha}, {z.x + c * x1 - s * y1, z.y + s * x1 + c * y1, u1, v1, z.alpha}};
        viereck(e);
    }
}

// Sprechblasen: Text steht im Blasenvorrat (je Satz einmal gemalt), hier nur noch ein Viereck je Blase.
// Reicht der Platz rechts nicht, klappt die Blase nach links (Schwanz rechts).
void Overlay::blasenSammeln(float bx, float by, float bw, float bh, double dpr) {
    m_blasen.bildBeginn();
    for (const Blasenbild &b : m_szene->blasen) {
        if (b.alpha <= 0.004f || b.x < bx - 40 || b.x > bx + bw + 40 || b.y < by - 40 || b.y > by + bh + 200) continue;
        const bool gespiegelt = b.x + Blasenpool::PLATZ_BREITE * b.skala > bx + bw;
        const Blasenfeld &f = m_blasen.feld(b.text, gespiegelt, dpr * b.skala);
        const float aw = float(m_blasen.bild().width()), ah = float(m_blasen.bild().height());
        const float k = b.skala * b.mass;
        const float x0 = b.x - f.px * k, x1 = b.x + (f.bw - f.px) * k, y0 = b.y - f.py * k, y1 = b.y + (f.bh - f.py) * k;
        const float u0 = (f.x + 0.5f) / aw, u1 = (f.x + f.w - 0.5f) / aw, v0 = (f.y + 0.5f) / ah, v1 = (f.y + f.h - 0.5f) / ah;
        const float e[4][5] = {{x0, y0, u0, v0, b.alpha}, {x1, y0, u1, v0, b.alpha}, {x0, y1, u0, v1, b.alpha}, {x1, y1, u1, v1, b.alpha}};
        viereck(e);
    }
    if (m_blasen.geaendert() || !m_blasenTextur) {
        if (!m_blasenTextur) glGenTextures(1, &m_blasenTextur);
        glBindTexture(GL_TEXTURE_2D, m_blasenTextur);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_blasen.bild().width(), m_blasen.bild().height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, m_blasen.bild().constBits());
    }
}

// Schneewulst vor dem Schild: gezeichnet wie eine Decke
void Overlay::haufenSammeln(float bx, float by, float bw, float bh, double dpr, float weich) {
    if (!m_szene) return;
    for (const Decke &h : m_szene->haufen) deckeSammeln(h, bx, by, bw, bh, dpr, weich);
}

// Eingaberegion: nur dort nimmt die Ebene die Maus an. Leer = durchsichtig für Eingaben (wie bisher).
// Reihenfolge wichtig: erst die Maske setzen, dann die Durchlässigkeit aufheben – nie ist die ganze Fläche „dicht“.
void Overlay::eingabeSetzen(const std::vector<Rechteck> &flaechen) {
    const QRect g = m_bildschirm->geometry();
    QRegion region;
    for (const Rechteck &r : flaechen) {
        const QRect q = QRectF(r.x, r.y, r.w, r.h).toAlignedRect().intersected(g);
        if (!q.isEmpty()) region += q.translated(-g.topLeft());
    }
    if (region == m_eingabe) return;
    m_eingabe = region;
    if (region.isEmpty()) {
        setFlag(Qt::WindowTransparentForInput, true);
    } else {
        setMask(region);
        setFlag(Qt::WindowTransparentForInput, false);
    }
}

void Overlay::mousePressEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton || !mausziel) return;
    m_gedrueckt = true;
    const QPointF p = e->position() + m_bildschirm->geometry().topLeft();
    mausziel(0, p.x(), p.y());
}

void Overlay::mouseMoveEvent(QMouseEvent *e) {
    if (!m_gedrueckt || !mausziel) return;
    const QPointF p = e->position() + m_bildschirm->geometry().topLeft();
    mausziel(1, p.x(), p.y());
}

void Overlay::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton || !m_gedrueckt || !mausziel) return;
    m_gedrueckt = false;
    const QPointF p = e->position() + m_bildschirm->geometry().topLeft();
    mausziel(2, p.x(), p.y());
}

// Hängt ein Viereck als zwei Dreiecke an: Ecken in der Reihenfolge links oben, rechts oben,
// links unten, rechts unten, je Ecke x, y und drei Werte fürs Programm.
void Overlay::viereck(const float e[4][5]) {
    static const int folge[6] = {0, 1, 2, 1, 3, 2};
    const size_t alt = m_daten.size();
    m_daten.resize(alt + 30);
    float *z = m_daten.data() + alt;
    for (int k : folge) z = std::copy_n(e[k], 5, z);
}

// Jede Flocke ist ein gedrehtes, beim Taumeln gestauchtes Viereck mit einem Bild aus dem Atlas.
// Gebaut wird aus dem aktuellen Simulationsstand mit Tempo und Drehtempo; das Fortschreiben bis
// zum jeweiligen Monitorbild macht der Vertex-Shader (FORT).
void Overlay::flockenBauen(float bx, float by, float bw, float bh) {
    const bool realistisch = *m_realistisch;
    const double grund = m_sim->einstellungen().groesse;
    const float aw = float(m_plan.breite), ah = float(m_plan.hoehe);
    m_flocken.clear();
    m_flockenZahl = 0;
    for (const Flocke &f : m_sim->flocken()) {
        const float fx = float(f.x), fy = float(f.y);
        // grob aussortieren (größte Flocke: 64 px, dazu Weg bis zum nächsten Aufbau), bevor gerechnet wird
        if (fx + 80 < bx || fx - 80 > bx + bw || fy + 80 < by || fy - 80 > by + bh) continue;
        // bis zum nächsten Aufbau (1/30 s, bei Rucklern länger) wandert sie noch ein Stück: Rand dazugeben
        const float weg = 0.12f * float(std::abs(f.vx) + std::abs(f.vy) + 3 * f.groesse);
        const Flockenbild b = flockenbild(f, grund, realistisch);
        const float r = b.durchmesser * 0.5f + weg;
        if (fx + r < bx || fx - r > bx + bw || fy + r < by || fy - r > by + bh) continue;
        if (!m_sim->flockeZeichnen(f)) continue; // „hinter“ einem Fenster
        const AtlasFeld *feld = &m_plan.punkt;
        switch (b.sprite) {
        case Sprite::Punkt: break;
        case Sprite::Kristall: feld = &m_plan.kristall(m_plan.stufeFuer(b.durchmesser), b.variante % m_plan.varianten); break;
        case Sprite::Weich: feld = &m_plan.weich[size_t(b.variante) % m_plan.weich.size()]; break;
        case Sprite::Pulver: feld = &m_plan.pulver[size_t(b.variante) % m_plan.pulver.size()]; break;
        }
        // halbes Texel Abstand zum Feldrand: der Nachbar scheint nicht hinein
        const float u0 = (feld->x + 0.5f) / aw, u1 = (feld->x + feld->groesse - 0.5f) / aw;
        const float v0 = (feld->y + 0.5f) / ah, v1 = (feld->y + feld->groesse - 0.5f) / ah;
        const FlockenViereck v = flockenViereck(f, b);
        const float a = b.deckkraft;
        ++m_flockenZahl;
        if (m_instanzen) {
            const float w[INST_WERTE] = {v.mx, v.my, v.ax, v.ay, v.qx, v.qy, v.vx, v.vy, v.drehtempo, v.stauchTempo, u0, v0, u1, v1, a};
            m_flocken.insert(m_flocken.end(), w, w + INST_WERTE);
            continue;
        }
        // Ecken wie in viereckEcken: links oben, rechts oben, links unten, rechts unten
        const float ecke[4][5] = {{-1, -1, u0, v0, a}, {1, -1, u1, v0, a}, {-1, 1, u0, v1, a}, {1, 1, u1, v1, a}};
        const size_t alt = m_flocken.size();
        m_flocken.resize(alt + 4 * FORT_WERTE);
        float *z = m_flocken.data() + alt;
        for (const auto &e : ecke) {
            const float w[FORT_WERTE] = {v.mx, v.my, e[0] * v.ax, e[0] * v.ay, e[1] * v.qx, e[1] * v.qy, v.vx, v.vy,
                                         v.drehtempo, v.stauchTempo, e[2], e[3], e[4]};
            z = std::copy_n(w, FORT_WERTE, z);
        }
    }
}

void Overlay::flockenZeichnen(float bx, float by, float bw, float bh, float h) {
    if (m_flockenZahl == 0) return;
    if (m_instanzen) {
        QOpenGLExtraFunctions *f = context()->extraFunctions();
        m_instProg->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_atlasTextur);
        m_instProg->setUniformValue("atlas", 0);
        m_instProg->setUniformValue("bild", bx, by, bw, bh);
        m_instProg->setUniformValue("h", h);
        for (int i = 0; i < 8; ++i) m_instProg->enableAttributeArray(i);
        m_eckenVbo.bind();
        m_instProg->setAttributeBuffer(0, GL_FLOAT, 0, 2, 2 * sizeof(float));
        m_flockenVbo.bind();
        static const int lage[7] = {0, 2, 4, 6, 8, 10, 14}, breite[7] = {2, 2, 2, 2, 2, 4, 1};
        for (int i = 0; i < 7; ++i) {
            m_instProg->setAttributeBuffer(i + 1, GL_FLOAT, lage[i] * int(sizeof(float)), breite[i], INST_WERTE * sizeof(float));
            f->glVertexAttribDivisor(GLuint(i + 1), 1);
        }
        f->glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(m_flockenZahl));
        // Teiler zurücksetzen: die Attribut-Plätze teilen sich alle Programme
        for (int i = 1; i < 8; ++i) f->glVertexAttribDivisor(GLuint(i), 0);
        for (int i = 0; i < 8; ++i) m_instProg->disableAttributeArray(i);
        m_flockenVbo.release();
        m_instProg->release();
        return;
    }
    m_fortProg->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_atlasTextur);
    m_fortProg->setUniformValue("atlas", 0);
    m_fortProg->setUniformValue("bild", bx, by, bw, bh);
    m_fortProg->setUniformValue("h", h);
    m_flockenVbo.bind();
    m_flockenIndex.bind();
    for (int i = 0; i < 6; ++i) m_fortProg->enableAttributeArray(i);
    const int schritt = FORT_WERTE * sizeof(float);
    for (size_t von = 0; von < m_flockenZahl; von += FLOCKEN_JE_AUFRUF) {
        const int anzahl = int(std::min<size_t>(FLOCKEN_JE_AUFRUF, m_flockenZahl - von));
        const int basis = int(von * 4) * schritt;
        static const int lage[6] = {0, 2, 4, 6, 8, 10}, breite[6] = {2, 2, 2, 2, 2, 3};
        for (int i = 0; i < 6; ++i) m_fortProg->setAttributeBuffer(i, GL_FLOAT, basis + lage[i] * int(sizeof(float)), breite[i], schritt);
        glDrawElements(GL_TRIANGLES, anzahl * 6, GL_UNSIGNED_SHORT, nullptr);
    }
    for (int i = 0; i < 6; ++i) m_fortProg->disableAttributeArray(i);
    m_flockenIndex.release();
    m_flockenVbo.release();
    m_fortProg->release();
}

// Schneedecken als Bänder zwischen der geglätteten Oberkante und der Fensterkante
void Overlay::deckenSammeln(float bx, float by, float bw, float bh, double dpr, float weich) {
    for (const Decke &d : m_sim->decken()) deckeSammeln(d, bx, by, bw, bh, dpr, weich);
}

// fallende Schneebretter: wie eine Decke ohne Fenster darunter (rundum weich), in jedem Bild neu
void Overlay::brockenSammeln(float bx, float by, float bw, float bh, double dpr, float weich) {
    for (const Brocken &b : m_sim->brocken()) {
        m_brockenDecke.x = b.x;
        m_brockenDecke.y = b.y;
        m_brockenDecke.spaltenBreite = b.spaltenBreite;
        m_brockenDecke.hoehe = b.hoehe;
        m_brockenDecke.sichtbar.assign(b.hoehe.size(), 1);
        deckeSammeln(m_brockenDecke, bx, by, bw, bh, dpr, weich);
    }
}

void Overlay::deckeSammeln(const Decke &d, float bx, float by, float bw, float bh, double dpr, float weich) {
    // Pixelraster des Monitors: Kanten auf ganze Gerätepixel runden (scharf bei 1,5×)
    auto px = [dpr](float v) { return float(std::round(v * dpr) / dpr); };
    const float fest = 4.f; // Abstand zur Unterseite, ab dem nichts mehr ausgeblendet wird
    {
        if (d.y < by || d.y > by + bh + 1 || d.x - 4 > bx + bw || d.x + d.breite() + 4 < bx) return;
        m_punkte.clear();
        m_enden.clear();
        deckenProfilAnhaengen(d, m_punkte, m_enden);
        const float y = px(float(d.y));
        const float a = 0.97f;
        size_t i = 0;
        for (size_t ende : m_enden) {
            const size_t anfang = i;
            i = ende;
            if (m_punkte[ende - 1].x < bx || m_punkte[anfang].x > bx + bw) continue;
            // Dreiecksband: je Punkt eine Ecke oben und eine unten, jeweils mit Tiefe unter der
            // Oberfläche und Abstand zur Unterseite. Stücke werden mit doppelten Ecken verkettet.
            const size_t alt = m_daten.size();
            const bool verketten = alt > 0;
            m_daten.resize(alt + ((ende - anfang) * 2 + (verketten ? 2 : 0)) * 5);
            float *z = m_daten.data() + alt;
            for (size_t k = anfang; k < ende; ++k) {
                const ProfilPunkt &p = m_punkte[k];
                const bool frei = p.unten > 0 || p.oben <= 0; // Überhang oder auslaufende Spitze: auch unten weich
                const float yo = y - p.oben - weich, yu = y + p.unten + (frei ? weich : 0.f);
                const float uu = frei ? -weich : fest;
                const float oben[5] = {p.x, yo, -weich, uu + (yu - yo), a}, unten[5] = {p.x, yu, -weich + (yu - yo), uu, a};
                if (k == anfang && verketten) {
                    z = std::copy_n(z - 5, 5, z); // letzte Ecke des vorigen Stücks noch einmal
                    z = std::copy_n(oben, 5, z);  // erste Ecke dieses Stücks doppelt
                }
                z = std::copy_n(oben, 5, z);
                z = std::copy_n(unten, 5, z);
            }
        }
    }
}

Overlay::Last Overlay::lastAbholen() {
    Last l;
    if (m_bilder) l = {m_sammelNs / 1e6 / m_bilder, m_zeichenNs / 1e6 / m_bilder, m_tauschNs / 1e6 / m_bilder,
                       m_aufbauten ? m_aufbauNs / 1e6 / m_aufbauten : 0, int(m_fahrzeugEcken + GLint(m_flockenZahl * 4)), m_instanzen};
    m_aufbauNs = 0;
    m_aufbauten = 0;
    m_sammelNs = m_zeichenNs = m_eckenSumme = m_tauschNs = 0;
    m_bilder = 0;
    return l;
}

void Overlay::paintGL() {
    QElapsedTimer stoppuhr;
    stoppuhr.start();
    const double jetzt = uhrSekunden();
    m_bildBeginn = jetzt;
    rasterNachfuehren(jetzt);
    m_bildzeiten.bild(jetzt);
    if (vorDemBild) vorDemBild(jetzt); // die App rechnet die Simulation weiter, wenn ein Schritt fällig ist
    else { ++m_eigenerTakt.schritt; ++m_eigenerTakt.flockenSchritt; m_eigenerTakt.schrittUhr = jetzt; }
    const qint64 vorher = stoppuhr.nsecsElapsed(); // Simulationsschritt zählt nicht zum Zeichnen dieser Ebene
    const qreal dpr = devicePixelRatio();
    glViewport(0, 0, int(std::lround(width() * dpr)), int(std::lround(height() * dpr)));
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    atlasLaden(dpr);

    const QRect g = m_bildschirm->geometry();
    const float bx = g.x(), by = g.y(), bw = g.width(), bh = g.height();
    const float weich = float(0.6 + 0.4 / dpr);
    const float fuer[5] = {bx, by, bw, bh, float(dpr)};
    const bool anders = !std::equal(fuer, fuer + 5, m_deckenFuer);

    // Decken, Brocken und Räumdienst: nach jedem Simulationsschritt neu (sie bewegen sich mit der Simulation)
    if (m_takt->schritt != m_teileGebaut || anders) {
        m_teileGebaut = m_takt->schritt;
        // Deckenform nur in jedem vierten Schritt (Ruhe: 7,5-mal je s, die Decken wachsen dabei um Bruchteile
        // eines Pixels) – sofort aber, wenn sich Fenster, Monitore oder Skalierung geändert haben oder
        // Schnee abgefallen ist (deckenStand)
        if (m_sim->deckenStand() != m_deckenStand || ++m_deckenAlter >= 4 || anders) {
            m_daten.clear();
            deckenSammeln(bx, by, bw, bh, dpr, weich);
            m_deckenEnde = m_daten.size();
            m_deckenStand = m_sim->deckenStand();
            m_deckenAlter = 0;
            std::copy_n(fuer, 5, m_deckenFuer);
        }
        m_daten.resize(m_deckenEnde);
        brockenSammeln(bx, by, bw, bh, dpr, weich);
        haufenSammeln(bx, by, bw, bh, dpr, weich);
        m_deckenEcken = GLint(m_daten.size() / 5);
        // Räumdienst (Hubschrauber, Fahrzeuge, Bälle, Staub): hinter den fallenden Flocken, vor den Decken
        if (m_szene && !m_szene->teile.empty()) {
            if (fahrzeugAtlasLaden(dpr)) fahrzeugeSammeln(bx, by, bw, bh);
        }
        m_fahrzeugEcken = GLint(m_daten.size() / 5);
        // Sprechblasen: liegen im selben Puffer, werden aber erst nach den Flocken (ganz vorn) gezeichnet
        if (m_szene && !m_szene->blasen.empty()) blasenSammeln(bx, by, bw, bh, dpr);
        m_blasenEcken = GLint(m_daten.size() / 5);
        if (!m_daten.empty()) {
            m_vbo.bind();
            m_vbo.allocate(m_daten.data(), int(m_daten.size() * sizeof(float)));
            m_vbo.release();
        }
        m_eckenSumme += qint64(m_daten.size() / 5);
    }
    // Flocken: höchstens 30-mal je s neu, dazwischen schreibt der Shader sie fort
    if (m_takt->flockenSchritt != m_flockenGebaut || anders) {
        m_flockenGebaut = m_takt->flockenSchritt;
        m_flockenUhr = m_takt->schrittUhr;
        const qint64 a = stoppuhr.nsecsElapsed();
        flockenBauen(bx, by, bw, bh);
        if (m_flockenZahl) {
            m_flockenVbo.bind();
            m_flockenVbo.allocate(m_flocken.data(), int(m_flocken.size() * sizeof(float)));
            m_flockenVbo.release();
        }
        m_aufbauNs += stoppuhr.nsecsElapsed() - a;
        ++m_aufbauten;
        m_eckenSumme += qint64(m_flockenZahl * 4);
    }
    const qint64 gesammelt = stoppuhr.nsecsElapsed();
    m_sammelNs += gesammelt - vorher;
    ++m_bilder;

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // vormultipliziertes Alpha
    auto zeichnen = [&](QOpenGLShaderProgram *p, GLenum art, GLint von, GLint anzahl) {
        p->setUniformValue("bild", bx, by, bw, bh);
        p->enableAttributeArray(0);
        p->enableAttributeArray(1);
        p->setAttributeBuffer(0, GL_FLOAT, 0, 2, 5 * sizeof(float));
        p->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 3, 5 * sizeof(float));
        glDrawArrays(art, von, anzahl);
        p->disableAttributeArray(0);
        p->disableAttributeArray(1);
        p->release();
    };
    // erst die Decken, dann der Räumdienst, dann die Flocken davor
    if (m_fahrzeugEcken > 0) {
        m_vbo.bind();
        if (m_deckenEcken > 0) {
            m_deckenProg->bind();
            m_deckenProg->setUniformValue("weich", weich);
            m_deckenProg->setUniformValue("korn", float(std::max(1.0, std::floor(dpr + 0.5))));
            m_deckenProg->setUniformValue("zeit", float((m_uhr.elapsed() % 3600000) / 1000.0));
            zeichnen(m_deckenProg, GL_TRIANGLE_STRIP, 0, m_deckenEcken);
        }
        if (m_fahrzeugEcken > m_deckenEcken) {
            m_flockenProg->bind();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_fahrzeugTextur);
            m_flockenProg->setUniformValue("atlas", 0);
            zeichnen(m_flockenProg, GL_TRIANGLES, m_deckenEcken, m_fahrzeugEcken - m_deckenEcken);
        }
        m_vbo.release();
    }
    // Zeit seit dem Simulationsstand der Flocken (in Simulationszeit, also mit Zeitlupe); höchstens 0,15 s
    const float h = float(std::clamp((jetzt - m_flockenUhr) * m_takt->zeitlupe, 0.0, 0.15));
    flockenZeichnen(bx, by, bw, bh, h);
    // Sprechblasen über allem
    if (m_blasenEcken > m_fahrzeugEcken) {
        m_vbo.bind();
        m_flockenProg->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_blasenTextur);
        m_flockenProg->setUniformValue("atlas", 0);
        zeichnen(m_flockenProg, GL_TRIANGLES, m_fahrzeugEcken, m_blasenEcken - m_fahrzeugEcken);
        m_vbo.release();
    }
    m_zeichenNs += stoppuhr.nsecsElapsed() - gesammelt;
    m_zeichnenEnde = uhrSekunden();
}
