#include "fahrzeugbild.h"
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <atomic>
#include <cmath>
#include <cstring>
#include <future>
#include <initializer_list>
#include <map>
#include <memory>
#include <thread>
#include <utility>
#include "fahrzeugmodell.h"

using namespace schnee;

namespace {
const double PI = 3.14159265358979;
const QColor RAND(22, 25, 31, 225);          // dunkler Umriss: hebt die Teile von jedem Hintergrund ab
const QColor GELB_HELL(255, 214, 74), GELB(243, 172, 28), GELB_DUNKEL(176, 112, 10);
const QColor ROT_HELL(236, 84, 70), ROT(196, 44, 38), ROT_DUNKEL(112, 20, 20);
const QColor STAHL_HELL(214, 221, 229), STAHL(132, 141, 152), STAHL_DUNKEL(58, 64, 72);
const QColor ORANGE_HELL(255, 150, 62), ORANGE(240, 104, 26), ORANGE_DUNKEL(158, 60, 10);

QPen stift(const QColor &c, double breite) {
    QPen p(c, breite);
    p.setCapStyle(Qt::RoundCap);
    p.setJoinStyle(Qt::RoundJoin);
    return p;
}

QLinearGradient verlauf(double x0, double y0, double x1, double y1, std::initializer_list<std::pair<double, QColor>> stufen) {
    QLinearGradient g(x0, y0, x1, y1);
    for (const auto &s : stufen) g.setColorAt(s.first, s.second);
    return g;
}

QColor mitAlpha(QColor c, int a) { c.setAlpha(a); return c; }

void flaeche(QPainter &p, const QPainterPath &pfad, const QBrush &fuellung, double rand = 0.7) {
    p.setPen(rand > 0 ? stift(RAND, rand) : QPen(Qt::NoPen));
    p.setBrush(fuellung);
    p.drawPath(pfad);
}

QPainterPath vieleck(std::initializer_list<QPointF> punkte) {
    QPainterPath pf;
    bool erster = true;
    for (const QPointF &q : punkte) {
        if (erster) pf.moveTo(q); else pf.lineTo(q);
        erster = false;
    }
    pf.closeSubpath();
    return pf;
}

QPainterPath rund(double x, double y, double w, double h, double r) {
    QPainterPath pf;
    pf.addRoundedRect(QRectF(x, y, w, h), r, r);
    return pf;
}

// Glas: oben hell (Himmel spiegelt sich), unten dunkel, schräger Lichtreflex
void glas(QPainter &p, const QPainterPath &pfad, const QRectF &r, int deckkraft = 235) {
    flaeche(p, pfad, verlauf(r.left(), r.top(), r.left() + r.width() * 0.25, r.bottom(),
                             {{0, QColor(214, 238, 255, deckkraft)}, {0.45, QColor(96, 150, 200, deckkraft)}, {1, QColor(26, 48, 76, deckkraft)}}), 0.6);
    p.save();
    p.setClipPath(pfad, Qt::IntersectClip);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 120));
    const double w = r.width(), h = r.height();
    p.drawPath(vieleck({{r.left() + w * 0.18, r.top()}, {r.left() + w * 0.36, r.top()}, {r.left() + w * 0.12, r.bottom()}, {r.left() - w * 0.06, r.bottom()}}));
    p.setBrush(QColor(255, 255, 255, 70));
    p.drawPath(vieleck({{r.left() + w * 0.46, r.top()}, {r.left() + w * 0.54, r.top()}, {r.left() + w * 0.3, r.bottom()}, {r.left() + w * 0.22, r.bottom()}}));
    Q_UNUSED(h);
    p.restore();
}

void leuchtpunkt(QPainter &p, double w, const QColor &farbe) {
    QRadialGradient g(w / 2, w / 2, w / 2);
    g.setColorAt(0, QColor(255, 255, 255, 255));
    g.setColorAt(0.18, mitAlpha(farbe.lighter(140), 255));
    g.setColorAt(0.45, mitAlpha(farbe, 150));
    g.setColorAt(1, mitAlpha(farbe, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(QRectF(0, 0, w, w));
}

// ---- Hubschrauber (Bug rechts) ----
// Hauptrotor: Blattkreis schräg von oben gesehen (wie die Fahrzeuge), Blätter verwischt
void heliRotor(QPainter &p, int bild) {
    const double mx = 88, my = 16, rx = 86, ry = 86 * std::sin(KAMERA_NEIGUNG);
    p.setPen(Qt::NoPen);
    QRadialGradient g(mx, my, rx);
    g.setColorAt(0, QColor(120, 128, 140, 70));
    g.setColorAt(0.75, QColor(120, 128, 140, 52));
    g.setColorAt(0.97, QColor(150, 158, 170, 70));
    g.setColorAt(1, QColor(120, 128, 140, 0));
    p.save();
    p.translate(mx, my);
    p.scale(1, ry / rx);
    p.translate(-mx, -my);
    p.setBrush(g);
    p.drawEllipse(QPointF(mx, my), rx, rx);
    // vier Blätter, jedes als Fächer aus blasser werdenden Strichen (Bewegungsunschärfe)
    for (int blatt = 0; blatt < 4; ++blatt) {
        for (int wisch = -5; wisch <= 5; ++wisch) {
            const double w = (bild * 22.5 + blatt * 90 + wisch * 3.2) * PI / 180;
            const int a = wisch == 0 ? 165 : std::max(12, 92 - std::abs(wisch) * 17);
            p.setPen(stift(QColor(34, 38, 46, a), wisch == 0 ? 2.6 : 3.4));
            p.drawLine(QPointF(mx, my), QPointF(mx + std::cos(w) * rx * 0.985, my + std::sin(w) * rx * 0.985));
            p.setPen(stift(QColor(230, 236, 244, a / 3), 1.0));
            p.drawLine(QPointF(mx + std::cos(w + 0.02) * 8, my + std::sin(w + 0.02) * 8), QPointF(mx + std::cos(w + 0.02) * rx * 0.97, my + std::sin(w + 0.02) * rx * 0.97));
        }
        const double w = (bild * 22.5 + blatt * 90) * PI / 180;
        p.setPen(stift(QColor(250, 200, 40, 190), 2.8));   // gelbe Blattspitze
        p.drawLine(QPointF(mx + std::cos(w) * rx * 0.9, my + std::sin(w) * rx * 0.9), QPointF(mx + std::cos(w) * rx * 0.985, my + std::sin(w) * rx * 0.985));
    }
    p.restore();
}

void haken(QPainter &p) {
    flaeche(p, rund(2, 0.3, 4, 3.2, 0.8), verlauf(2, 0, 6, 0, {{0, STAHL_HELL}, {1, STAHL_DUNKEL}}), 0.5);
    QPainterPath h;
    h.moveTo(4, 3.4);
    h.lineTo(4, 7);
    h.cubicTo(4, 11.6, 7.6, 11.2, 7, 8);
    p.setBrush(Qt::NoBrush);
    p.setPen(stift(RAND, 2.3));
    p.drawPath(h);
    p.setPen(stift(QColor(250, 196, 40), 1.3));
    p.drawPath(h);
}

void seil(QPainter &p) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(28, 30, 36));
    p.drawRect(QRectF(-1, 1, 6, 2));
    p.setBrush(QColor(120, 126, 136, 150));
    p.drawRect(QRectF(-1, 1.2, 6, 0.6)); // Glanz: Stahlseil
}

void schatten(QPainter &p) {
    p.save();
    p.scale(1, 12.0 / 64);
    QRadialGradient g(32, 32, 32);
    g.setColorAt(0, QColor(8, 14, 30, 190));
    g.setColorAt(0.6, QColor(8, 14, 30, 90));
    g.setColorAt(1, QColor(8, 14, 30, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(QRectF(0, 0, 64, 64));
    p.restore();
}

// ---- Pistenraupe / Bulldozer (Schild rechts) ----
void fahrer(QPainter &p) {
    QPainterPath jacke;
    jacke.moveTo(2, 18);
    jacke.cubicTo(1.6, 12, 3.4, 9.2, 6.6, 8.8);
    jacke.cubicTo(10.4, 9, 12.2, 12, 12, 18);
    jacke.closeSubpath();
    flaeche(p, jacke, verlauf(2, 9, 12, 18, {{0, QColor(255, 140, 40)}, {1, QColor(206, 84, 10)}}), 0.6);
    p.save();
    p.setClipPath(jacke, Qt::IntersectClip);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(236, 240, 180));
    p.drawRect(QRectF(0, 13.2, 14, 1.5));
    p.setBrush(QColor(200, 206, 214));
    p.drawRect(QRectF(0, 13.7, 14, 0.5));
    p.restore();
    // Kopf
    p.setPen(stift(RAND, 0.5));
    p.setBrush(QColor(240, 196, 160));
    p.drawEllipse(QPointF(7.4, 6.2), 3.1, 3.3);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(110, 74, 48));
    p.drawPath(vieleck({{6.4, 7.6}, {10.2, 7.2}, {9.6, 9.6}, {7.2, 9.6}}));  // Bart
    p.setBrush(QColor(30, 26, 26));
    p.drawEllipse(QPointF(9.0, 5.7), 0.55, 0.65);                             // Auge
    p.setPen(stift(QColor(60, 30, 20), 0.5));
    p.drawLine(QPointF(8.2, 4.5), QPointF(9.9, 5.0));                         // grimmige Braue
    // Helm mit Schirm
    QPainterPath helm;
    helm.moveTo(3.9, 5.0);
    helm.cubicTo(3.8, 0.6, 10.8, 0.4, 10.9, 4.4);
    helm.lineTo(12.6, 4.6);
    helm.lineTo(12.6, 5.4);
    helm.lineTo(3.9, 5.4);
    helm.closeSubpath();
    flaeche(p, helm, verlauf(4, 1, 11, 5, {{0, QColor(255, 255, 255)}, {1, QColor(206, 212, 222)}}), 0.5);
}

void arm(QPainter &p) {
    flaeche(p, rund(0.9, 0.4, 3.2, 8.6, 1.5), verlauf(0.9, 0, 4.1, 0, {{0, QColor(255, 140, 40)}, {1, QColor(206, 84, 10)}}), 0.5);
    p.setPen(stift(RAND, 0.5));
    p.setBrush(QColor(240, 196, 160));
    p.drawEllipse(QPointF(2.5, 9.9), 1.9, 1.9);   // Faust
    p.setPen(stift(QColor(170, 120, 90), 0.35));
    p.drawLine(QPointF(1.4, 10.3), QPointF(3.6, 10.3));
}

void schneeBuckel(QPainter &p, double w, double h, unsigned saat) {
    QPainterPath b;
    b.moveTo(0.6, h);
    const int n = 6;
    double x = 0.6;
    for (int i = 0; i < n; ++i) {
        saat = saat * 1664525u + 1013904223u;
        const double hoch = h * (0.25 + 0.6 * ((saat >> 16) & 255) / 255.0) * std::sin(PI * (i + 0.5) / n + 0.25);
        const double x1 = 0.6 + (w - 1.2) * (i + 1) / n;
        b.cubicTo(x + (x1 - x) * 0.2, h - hoch * 1.3, x1 - (x1 - x) * 0.2, h - hoch * 1.3, x1, h - (i + 1 < n ? hoch * 0.45 : 0));
        x = x1;
    }
    b.closeSubpath();
    p.setPen(stift(QColor(150, 176, 210, 150), 0.4));
    p.setBrush(verlauf(0, 0, 0, h, {{0, QColor(255, 255, 255)}, {1, QColor(214, 226, 242)}}));
    p.drawPath(b);
}

void klecks(QPainter &p) {
    QPainterPath k;
    unsigned saat = 99;
    for (int i = 0; i <= 14; ++i) {
        saat = saat * 1664525u + 1013904223u;
        const double w = i * 2 * PI / 14, r = 4.6 + 2.6 * ((saat >> 16) & 255) / 255.0;
        const QPointF q(8 + std::cos(w) * r, 8 + std::sin(w) * r);
        if (i == 0) k.moveTo(q); else k.lineTo(q);
    }
    k.closeSubpath();
    QRadialGradient g(6.5, 6, 8);
    g.setColorAt(0, QColor(255, 255, 255));
    g.setColorAt(1, QColor(206, 220, 240));
    p.setPen(stift(QColor(150, 176, 210, 170), 0.5));
    p.setBrush(g);
    p.drawPath(k);
}

// ---- kleiner Pflug für Taskleiste und Boden (Kommunalfahrzeug, Schild rechts) ----
void wolke(QPainter &p, const QColor &innen, const QColor &aussen) {
    QRadialGradient g(8, 8, 8);
    g.setColorAt(0, innen);
    g.setColorAt(0.5, mitAlpha(aussen, aussen.alpha() * 2 / 3));
    g.setColorAt(1, mitAlpha(aussen, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawEllipse(QRectF(0, 0, 16, 16));
}

// Schneeball: Kugel mit Licht von links oben, Dellen und Kristallen (unsymmetrisch, damit man die Drehung sieht)
void ball(QPainter &p) {
    const double m = 18, r = 16;
    QRadialGradient g(m - 5.5, m - 6.5, r * 1.45);
    g.setColorAt(0, QColor(255, 255, 255));
    g.setColorAt(0.45, QColor(236, 242, 250));
    g.setColorAt(0.8, QColor(196, 210, 230));
    g.setColorAt(1, QColor(160, 178, 206));
    p.setPen(stift(QColor(120, 142, 176, 200), 0.8));
    p.setBrush(g);
    p.drawEllipse(QPointF(m, m), r, r);
    p.setPen(Qt::NoPen);
    unsigned saat = 4711;
    for (int i = 0; i < 26; ++i) {
        saat = saat * 1664525u + 1013904223u;
        const double w = ((saat >> 8) & 1023) / 1023.0 * 2 * PI;
        saat = saat * 1664525u + 1013904223u;
        const double a = std::sqrt(((saat >> 8) & 1023) / 1023.0) * (r - 3);
        saat = saat * 1664525u + 1013904223u;
        const double gr = 0.7 + 1.9 * ((saat >> 8) & 255) / 255.0;
        const bool delle = i % 3 != 0;
        p.setBrush(delle ? QColor(150, 170, 200, 110) : QColor(255, 255, 255, 230));
        p.drawEllipse(QPointF(m + std::cos(w) * a, m + std::sin(w) * a), gr, gr * 0.8);
    }
    p.setBrush(QColor(140, 160, 194, 120));
    p.drawPath(vieleck({{m + 3, m + 6}, {m + 10, m + 3}, {m + 8, m + 9}})); // Kerbe
}

QFont blasenSchrift() {
    QFont f(QStringLiteral("Noto Sans"));
    f.setPixelSize(11);
    f.setBold(true);
    f.setHintingPreference(QFont::PreferNoHinting);
    return f;
}

void blase(QPainter &p, const QString &text, double w, double h) {
    QPainterPath b;
    b.addRoundedRect(QRectF(1, 1, w - 2, h - 9), 7, 7);
    QPainterPath zipfel = vieleck({{9, h - 9.5}, {19, h - 9.5}, {3, h - 0.8}});
    b = b.united(zipfel);
    p.setPen(stift(QColor(28, 32, 40), 1.3));
    p.setBrush(verlauf(0, 0, 0, h, {{0, QColor(255, 255, 255)}, {1, QColor(232, 238, 246)}}));
    p.drawPath(b);
    p.setFont(blasenSchrift());
    p.setPen(QColor(24, 28, 36));
    p.drawText(QRectF(1, 1, w - 2, h - 10), Qt::AlignCenter, text);
}

// ---- 3D-Ansichten fürs Wenden ----
// Seitenbild als Textur: genau die Malerei der Seitenansicht, damit die Ansicht bei Gier 0 dem gewohnten Bild gleicht
struct Vorlage {
    Teil teil;
    float w, h, px, py;
};
const Vorlage VORLAGEN[] = {
    {Teil::HeliRotor0, 176, 32, 88, 16}, {Teil::HeliRotor1, 176, 32, 88, 16}, {Teil::HeliRotor2, 176, 32, 88, 16}, {Teil::HeliRotor3, 176, 32, 88, 16},
    {Teil::LichtRot, 16, 16, 8, 8}, {Teil::LichtGruen, 16, 16, 8, 8}, {Teil::LichtWeiss, 16, 16, 8, 8}, {Teil::LichtOrange, 16, 16, 8, 8},
    {Teil::Haken, 8, 12, 4, 1}, {Teil::Seil, 4, 4, 2, 2}, {Teil::Schatten, 64, 12, 32, 6},
    {Teil::Fahrer, 14, 18, 7, 18}, {Teil::Arm, 5, 12, 2.5f, 1.5f},
    {Teil::SchneeDach, 30, 8, 15, 8}, {Teil::SchneeHaube, 24, 6, 12, 6}, {Teil::Klecks, 16, 16, 8, 8},
    {Teil::Rauch, 16, 16, 8, 8}, {Teil::Staub, 16, 16, 8, 8}, {Teil::Ball, 36, 36, 18, 18},
};

void malen(QPainter &p, Teil t) {
    switch (t) {
    case Teil::HeliRotor0: heliRotor(p, 0); break;
    case Teil::HeliRotor1: heliRotor(p, 1); break;
    case Teil::HeliRotor2: heliRotor(p, 2); break;
    case Teil::HeliRotor3: heliRotor(p, 3); break;
    case Teil::LichtRot: leuchtpunkt(p, 16, QColor(255, 40, 30)); break;
    case Teil::LichtGruen: leuchtpunkt(p, 16, QColor(40, 240, 90)); break;
    case Teil::LichtWeiss: leuchtpunkt(p, 16, QColor(235, 244, 255)); break;
    case Teil::LichtOrange: leuchtpunkt(p, 16, QColor(255, 150, 20)); break;
    case Teil::Haken: haken(p); break;
    case Teil::Seil: seil(p); break;
    case Teil::Schatten: schatten(p); break;
    case Teil::Fahrer: fahrer(p); break;
    case Teil::Arm: arm(p); break;
    case Teil::SchneeDach: schneeBuckel(p, 30, 8, 7); break;
    case Teil::SchneeHaube: schneeBuckel(p, 24, 6, 21); break;
    case Teil::Klecks: klecks(p); break;
    case Teil::Rauch: wolke(p, QColor(120, 120, 126, 190), QColor(86, 86, 92, 150)); break;
    case Teil::Staub: wolke(p, QColor(255, 255, 255, 235), QColor(236, 243, 252, 190)); break;
    case Teil::Ball: ball(p); break;
    default: break;
    }
}
} // namespace

FahrzeugAtlas fahrzeugAtlasErzeugen(double dpr, double groesse) {
    struct Auftrag { float w = 0, h = 0, px = 0, py = 0; double skala = 1; int art = 0; QString text; const Modell *modell = nullptr; double gier = 0; };
    std::vector<Auftrag> auftraege;
    auftraege.resize(size_t(teilAnzahl()));
    for (const Vorlage &v : VORLAGEN) auftraege[size_t(v.teil)] = {v.w, v.h, v.px, v.py, dpr * groesse, 1, QString(), nullptr, 0};
    const QFontMetricsF masse(blasenSchrift());
    for (int i = 0; i < spruchAnzahl(); ++i) {
        const QString text = QString::fromUtf8(spruchText(i));
        const float w = float(std::ceil(masse.horizontalAdvance(text)) + 22);
        auftraege[size_t(int(Teil::Blase0) + i)] = {w, 31, 3, 30.2f, dpr * blasenSkala(groesse), 2, text, nullptr, 0};
    }
    // 3D-Ansichten: jede Schicht jedes Fahrzeugs je Winkelstufe. Bei sehr großen Fahrzeugen etwas gröber
    // gerastert (höchstens 3 px je Einheit), damit der Atlas nicht ausufert.
    const double skala3d = std::min(dpr * groesse, 3.0);
    std::vector<Modell> modelle;
    modelle.reserve(40);
    auto schicht = [&](Teil basis, int je, int nummer, Modell modell, const Rahmen &r, bool nurSeiten = false) {
        modelle.push_back(std::move(modell));
        const Modell *m = &modelle.back();
        if (nurSeiten) {
            for (int k = 0; k < 2; ++k) auftraege[size_t(int(basis) + k)] = {r.w, r.h, r.px, r.py, skala3d, 3, QString(), m, k * PI};
            return;
        }
        for (int stufe = 0; stufe < WINKELSTUFEN; ++stufe)
            auftraege[size_t(int(basis) + stufe * je + nummer)] = {r.w, r.h, r.px, r.py, skala3d, 3, QString(), m, stufenWinkel(stufe)};
    };
    schicht(Teil::RaupeKoerper, 1, 0, raupeKoerper(true), RAHMEN_RAUPE);
    schicht(Teil::RaupeLeer, 1, 0, raupeKoerper(false), RAHMEN_RAUPE, true);
    for (int ph = 0; ph < KETTENPHASEN; ++ph) {
        schicht(Teil::RaupeKetteR, KETTENPHASEN, ph, raupeKette(1, ph), RAHMEN_KETTE);
        schicht(Teil::RaupeKetteL, KETTENPHASEN, ph, raupeKette(-1, ph), RAHMEN_KETTE);
    }
    schicht(Teil::RaupeSchild, 1, 0, raupeSchild(), RAHMEN_RSCHILD);
    schicht(Teil::PflugKoerper, 1, 0, pflugKoerper(true), RAHMEN_PFLUG);
    schicht(Teil::PflugLeer, 1, 0, pflugKoerper(false), RAHMEN_PFLUG, true);
    for (int ph = 0; ph < RADPHASEN; ++ph) schicht(Teil::PflugRad, RADPHASEN, ph, pflugRad(ph), RAHMEN_RAD);
    schicht(Teil::PflugSchild, 1, 0, pflugSchild(), RAHMEN_PSCHILD);
    schicht(Teil::HeliKoerper, 1, 0, heliKoerper(), RAHMEN_HELI);

    // einfache Regalpackung
    const int rand = 2, maxBreite = skala3d > 2 ? 4096 : 2048;
    FahrzeugAtlas a;
    a.felder.resize(auftraege.size());
    int x = rand, y = rand, zeile = 0, breite = 0;
    for (size_t i = 0; i < auftraege.size(); ++i) {
        const Auftrag &t = auftraege[i];
        const int w = int(std::ceil(t.w * t.skala)), h = int(std::ceil(t.h * t.skala));
        if (x + w + rand > maxBreite) { x = rand; y += zeile + rand; zeile = 0; }
        a.felder[i] = {x, y, w, h, t.w, t.h, t.px, t.py};
        x += w + rand;
        zeile = std::max(zeile, h);
        breite = std::max(breite, x);
    }
    // gemalte Teile (Rotor, Lichter, Schnee, Sprechblasen …)
    QImage gemalt(breite, y + zeile + rand, QImage::Format_ARGB32_Premultiplied);
    gemalt.fill(Qt::transparent);
    {
        QPainter p(&gemalt);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        for (size_t i = 0; i < auftraege.size(); ++i) {
            const Auftrag &t = auftraege[i];
            if (t.art != 1 && t.art != 2) continue;
            const FahrzeugFeld &f = a.felder[i];
            p.save();
            p.setClipRect(QRect(f.x, f.y, f.w, f.h));
            p.translate(f.x, f.y);
            p.scale(f.w / double(t.w), f.h / double(t.h));
            if (t.art == 2) blase(p, t.text, t.w, t.h);
            else malen(p, Teil(i));
            p.restore();
        }
    }
    a.bild = gemalt.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    // 3D-Ansichten auf mehreren Kernen rastern und in den Atlas kopieren
    std::vector<size_t> ansichten;
    for (size_t i = 0; i < auftraege.size(); ++i)
        if (auftraege[i].art == 3) ansichten.push_back(i);
    const int abtastung = skala3d <= 2.2 ? 3 : 2;
    std::atomic<size_t> naechste{0};
    uchar *ziel = a.bild.bits();
    const qsizetype zeilenBytes = a.bild.bytesPerLine();
    auto arbeiten = [&] {
        for (size_t k; (k = naechste.fetch_add(1)) < ansichten.size();) {
            const Auftrag &t = auftraege[ansichten[k]];
            const FahrzeugFeld &f = a.felder[ansichten[k]];
            const Ansichtsbild b = rendern(*t.modell, t.gier, f.w / double(t.w), f.w, f.h, t.px * f.w / double(t.w), t.py * f.h / double(t.h), abtastung);
            for (int zl = 0; zl < f.h; ++zl) std::memcpy(ziel + (f.y + zl) * zeilenBytes + f.x * 4, &b.rgba[size_t(zl) * f.w * 4], size_t(f.w) * 4);
        }
    };
    const unsigned kerne = std::clamp(std::thread::hardware_concurrency() / 2, 1u, 8u);
    std::vector<std::thread> helfer;
    for (unsigned i = 1; i < kerne; ++i) helfer.emplace_back(arbeiten);
    arbeiten();
    for (std::thread &t : helfer) t.join();
    return a;
}

// Der Atlas wird im Hintergrund gemalt (dauert einen Moment): bis er fertig ist, gibt es nullptr.
const FahrzeugAtlas *fahrzeugAtlasFuer(double dpr, double groesse) {
    struct Eintrag { std::future<FahrzeugAtlas> zukunft; std::unique_ptr<FahrzeugAtlas> fertig; };
    static std::map<std::pair<int, int>, Eintrag> lager;
    const std::pair<int, int> schluessel(int(std::lround(dpr * 100)), int(std::lround(groesse * 100)));
    auto it = lager.find(schluessel);
    if (it == lager.end()) {
        // Größe am Regler verstellt: fertige alte Bilder nicht ewig aufheben
        if (lager.size() > 5)
            for (auto alt = lager.begin(); alt != lager.end();) alt = alt->second.fertig ? lager.erase(alt) : std::next(alt);
        it = lager.emplace(schluessel, Eintrag{}).first;
        it->second.zukunft = std::async(std::launch::async, [dpr, groesse] { return fahrzeugAtlasErzeugen(dpr, groesse); });
    }
    Eintrag &e = it->second;
    if (!e.fertig && e.zukunft.valid() && e.zukunft.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        e.fertig = std::make_unique<FahrzeugAtlas>(e.zukunft.get());
    return e.fertig.get();
}
