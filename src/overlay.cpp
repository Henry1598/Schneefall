#include "overlay.h"
#include <LayerShellQt/Window>
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

Overlay::Overlay(QScreen *bildschirm, const Simulation *sim, const bool *realistisch)
    : QOpenGLWindow(QOpenGLWindow::NoPartialUpdate), m_bildschirm(bildschirm), m_sim(sim), m_realistisch(realistisch) {
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
    m_uhr.start();
}

Overlay::~Overlay() {
    makeCurrent();
    m_vbo.destroy();
    if (m_atlasTextur) glDeleteTextures(1, &m_atlasTextur);
    delete m_flockenProg;
    delete m_deckenProg;
    doneCurrent();
}

void Overlay::anBildschirmAnpassen() { resize(m_bildschirm->geometry().size()); }

QOpenGLShaderProgram *Overlay::programm(const char *fragment) {
    auto *p = new QOpenGLShaderProgram;
    p->addShaderFromSourceCode(QOpenGLShader::Vertex, VERTEX);
    p->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment);
    p->bindAttributeLocation("pos", 0);
    p->bindAttributeLocation("uva", 1);
    if (!p->link()) qWarning("Schneefall: Shader-Fehler: %s", qPrintable(p->log()));
    return p;
}

void Overlay::initializeGL() {
    initializeOpenGLFunctions();
    m_flockenProg = programm(FLOCKEN);
    m_deckenProg = programm(DECKEN);
    m_vbo.create();
    m_vbo.setUsagePattern(QOpenGLBuffer::StreamDraw);
    m_atlasTextur = 0; // neuer Kontext: Textur neu anlegen
    m_atlasDpr = 0;
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

// Hängt ein Viereck als zwei Dreiecke an: Ecken in der Reihenfolge links oben, rechts oben,
// links unten, rechts unten, je Ecke x, y und drei Werte fürs Programm.
void Overlay::viereck(const float e[4][5]) {
    static const int folge[6] = {0, 1, 2, 1, 3, 2};
    const size_t alt = m_daten.size();
    m_daten.resize(alt + 30);
    float *z = m_daten.data() + alt;
    for (int k : folge) z = std::copy_n(e[k], 5, z);
}

// Jede Flocke ist ein gedrehtes, beim Taumeln gestauchtes Viereck mit einem Bild aus dem Atlas
void Overlay::flockenSammeln(float bx, float by, float bw, float bh) {
    const bool realistisch = *m_realistisch;
    const double grund = m_sim->einstellungen().groesse;
    const float aw = float(m_plan.breite), ah = float(m_plan.hoehe);
    for (const Flocke &f : m_sim->flocken()) {
        const float fx = float(f.x), fy = float(f.y);
        // grob aussortieren (größte Flocke: 64 px), bevor das Aussehen berechnet wird
        if (fx + 33 < bx || fx - 33 > bx + bw || fy + 33 < by || fy - 33 > by + bh) continue;
        const Flockenbild b = flockenbild(f, grund, realistisch);
        const float r = b.durchmesser * 0.5f;
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
        float ax = r, ay = 0, qx = 0, qy = r; // Achsen des Vierecks
        if (b.sprite != Sprite::Punkt) {
            const float c = kosinusSchnell(b.winkel), s = sinusSchnell(b.winkel);
            ax = c * r; ay = s * r;
            qx = -s * r * b.stauchung; qy = c * r * b.stauchung;
        }
        const float a = b.deckkraft;
        const float e[4][5] = {{fx - ax - qx, fy - ay - qy, u0, v0, a}, {fx + ax - qx, fy + ay - qy, u1, v0, a},
                               {fx - ax + qx, fy - ay + qy, u0, v1, a}, {fx + ax + qx, fy + ay + qy, u1, v1, a}};
        viereck(e);
    }
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
    if (m_bilder) l = {m_sammelNs / 1e6 / m_bilder, m_zeichenNs / 1e6 / m_bilder, int(m_eckenSumme / m_bilder)};
    m_sammelNs = m_zeichenNs = m_eckenSumme = 0;
    m_bilder = 0;
    return l;
}

void Overlay::paintGL() {
    QElapsedTimer stoppuhr;
    stoppuhr.start();
    const qreal dpr = devicePixelRatio();
    glViewport(0, 0, int(std::lround(width() * dpr)), int(std::lround(height() * dpr)));
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    atlasLaden(dpr);

    const QRect g = m_bildschirm->geometry();
    const float bx = g.x(), by = g.y(), bw = g.width(), bh = g.height();
    const float weich = float(0.6 + 0.4 / dpr);

    // Deckenform nur in jedem vierten Bild neu berechnen – sofort aber, wenn sich Fenster,
    // Monitore oder Skalierung geändert haben oder Schnee abgefallen ist
    const float fuer[5] = {bx, by, bw, bh, float(dpr)};
    if (m_sim->deckenStand() != m_deckenStand || ++m_deckenAlter >= 4 || !std::equal(fuer, fuer + 5, m_deckenFuer)) {
        m_daten.clear();
        deckenSammeln(bx, by, bw, bh, dpr, weich);
        m_deckenEnde = m_daten.size();
        m_deckenStand = m_sim->deckenStand();
        m_deckenAlter = 0;
        std::copy_n(fuer, 5, m_deckenFuer);
    }
    m_daten.resize(m_deckenEnde);
    brockenSammeln(bx, by, bw, bh, dpr, weich);
    const size_t deckenEnde = m_daten.size();
    flockenSammeln(bx, by, bw, bh);
    const qint64 gesammelt = stoppuhr.nsecsElapsed();
    m_sammelNs += gesammelt;
    m_eckenSumme += qint64(m_daten.size() / 5);
    ++m_bilder;
    if (m_daten.empty()) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // vormultipliziertes Alpha
    m_vbo.bind();
    m_vbo.allocate(m_daten.data(), int(m_daten.size() * sizeof(float)));
    const GLint deckenEcken = GLint(deckenEnde / 5), alleEcken = GLint(m_daten.size() / 5);
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
    // erst die Decken, dann die Flocken davor
    if (deckenEcken > 0) {
        m_deckenProg->bind();
        m_deckenProg->setUniformValue("weich", weich);
        m_deckenProg->setUniformValue("korn", float(std::max(1.0, std::floor(dpr + 0.5))));
        m_deckenProg->setUniformValue("zeit", float((m_uhr.elapsed() % 3600000) / 1000.0));
        zeichnen(m_deckenProg, GL_TRIANGLE_STRIP, 0, deckenEcken);
    }
    if (alleEcken > deckenEcken) {
        m_flockenProg->bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_atlasTextur);
        m_flockenProg->setUniformValue("atlas", 0);
        zeichnen(m_flockenProg, GL_TRIANGLES, deckenEcken, alleEcken - deckenEcken);
    }
    m_vbo.release();
    m_zeichenNs += stoppuhr.nsecsElapsed() - gesammelt;
}
