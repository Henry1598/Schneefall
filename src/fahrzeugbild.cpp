#include "fahrzeugbild.h"
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QTransform>
#include "fahrzeugmodell.h"
#include <cmath>
#include <initializer_list>
#include <map>
#include <utility>

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
void heliRumpf(QPainter &p) {
    // Heckausleger
    flaeche(p, vieleck({{72, 21}, {10, 24.5}, {8, 29}, {72, 37}}), verlauf(0, 21, 0, 37, {{0, ROT_HELL}, {0.5, ROT}, {1, ROT_DUNKEL}}));
    p.setPen(stift(QColor(255, 255, 255, 210), 1.6));
    p.drawLine(QPointF(14, 27.2), QPointF(66, 30)); // weißer Zierstreifen
    // Seitenleitwerk und Höhenflosse
    flaeche(p, vieleck({{3, 3}, {11, 3}, {19, 25}, {6, 29}}), verlauf(0, 3, 0, 29, {{0, ROT_HELL}, {1, ROT_DUNKEL}}));
    flaeche(p, vieleck({{5, 8}, {12.5, 8}, {13.5, 11}, {5.6, 11}}), QColor(250, 250, 250), 0);
    flaeche(p, vieleck({{16, 26}, {34, 27.5}, {34, 29.5}, {16, 29.5}}), verlauf(0, 26, 0, 30, {{0, STAHL_HELL}, {1, STAHL_DUNKEL}}), 0.5);
    // Triebwerksverkleidung mit Lufteinlass und Auspuff, Rotormast
    flaeche(p, rund(84.5, 2.5, 5, 12, 1), verlauf(84, 0, 90, 0, {{0, STAHL_DUNKEL}, {0.5, STAHL_HELL}, {1, STAHL_DUNKEL}}), 0.5);
    flaeche(p, rund(66, 10, 44, 15, 6), verlauf(0, 10, 0, 25, {{0, QColor(120, 128, 138)}, {0.5, QColor(74, 80, 90)}, {1, QColor(40, 44, 52)}}));
    flaeche(p, rund(97, 13, 9, 6, 2.5), QColor(18, 20, 24), 0.4);
    flaeche(p, rund(66, 14.5, 5, 4.5, 1.5), QColor(30, 26, 24), 0.4);
    // Rumpf
    QPainterPath rumpf;
    rumpf.moveTo(60, 25);
    rumpf.cubicTo(64, 13, 100, 11, 118, 14.5);
    rumpf.cubicTo(137, 18, 147, 28, 145, 36);
    rumpf.cubicTo(143, 44, 122, 48, 96, 48);
    rumpf.lineTo(74, 46.5);
    rumpf.cubicTo(65, 45, 59, 37, 60, 25);
    flaeche(p, rumpf, verlauf(0, 12, 0, 48, {{0, ROT_HELL}, {0.35, ROT}, {0.8, ROT_DUNKEL}, {1, QColor(70, 12, 14)}}), 0.8);
    p.save();
    p.setClipPath(rumpf, Qt::IntersectClip);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(250, 250, 250, 235));
    p.drawRect(QRectF(58, 35.5, 92, 4.2)); // weißer Bauchstreifen
    p.setBrush(QColor(255, 255, 255, 60));
    p.drawEllipse(QRectF(64, 9, 70, 9));   // Glanz auf dem Rücken
    p.restore();
    // Kanzel aus Glas mit Pilot
    QPainterPath kanzel;
    kanzel.moveTo(103, 15.5);
    kanzel.cubicTo(124, 14, 141, 22, 144, 33);
    kanzel.lineTo(119, 34);
    kanzel.lineTo(103, 31);
    kanzel.closeSubpath();
    glas(p, kanzel, QRectF(103, 14, 41, 20));
    p.save();
    p.setClipPath(kanzel, Qt::IntersectClip);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 24, 30, 200));
    p.drawEllipse(QRectF(116, 20, 7.5, 7.5));           // Helm
    p.drawPath(vieleck({{114, 34}, {116, 27}, {124, 27}, {127, 34}}));
    p.setBrush(QColor(255, 255, 255, 150));
    p.drawEllipse(QRectF(121, 22, 2.2, 2.6));           // Visier-Glanz
    p.restore();
    p.setPen(stift(RAND, 0.6));
    p.drawLine(QPointF(119, 34), QPointF(117, 15));     // Kanzelstrebe
    // Seitenfenster und Schiebetür
    glas(p, rund(84, 18, 15, 12, 2.5), QRectF(84, 18, 15, 12));
    p.setPen(stift(mitAlpha(RAND, 120), 0.45));
    p.setBrush(Qt::NoBrush);
    p.drawPath(rund(81.5, 16.5, 20, 29, 2));
    // Kufen
    p.setPen(stift(RAND, 2.6));
    p.drawLine(QPointF(84, 46.5), QPointF(80, 53.5));
    p.drawLine(QPointF(113, 46.5), QPointF(117, 53.5));
    QPainterPath kufe;
    kufe.moveTo(66, 54);
    kufe.lineTo(128, 54);
    kufe.quadTo(133, 54, 135.5, 50);
    p.setBrush(Qt::NoBrush);
    p.drawPath(kufe);
    p.setPen(stift(STAHL_HELL, 1.3));
    p.drawLine(QPointF(84, 46.5), QPointF(80, 53.5));
    p.drawLine(QPointF(113, 46.5), QPointF(117, 53.5));
    p.drawPath(kufe);
    // Lastgeschirr unter dem Bauch
    flaeche(p, rund(88.5, 46.5, 7, 3.2, 1), QColor(40, 44, 52), 0.5);
    // Schnee auf Ausleger und Rücken
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 215));
    p.drawPath(rund(20, 23.2, 40, 1.8, 0.9));
    p.drawPath(rund(70, 9.2, 12, 1.9, 0.9));
}

// Rotor von der Seite: durchscheinende Scheibe, Blätter als verwischte Striche
void heliRotor(QPainter &p, int bild) {
    const double mx = 88, my = 7, rx = 86, ry = 5.2;
    p.setPen(Qt::NoPen);
    QRadialGradient g(mx, my, rx);
    g.setColorAt(0, QColor(120, 128, 140, 80));
    g.setColorAt(0.8, QColor(120, 128, 140, 56));
    g.setColorAt(1, QColor(120, 128, 140, 0));
    p.setBrush(g);
    p.drawEllipse(QRectF(mx - rx, my - ry, 2 * rx, 2 * ry));
    for (int blatt = 0; blatt < 4; ++blatt) {
        for (int wisch = -3; wisch <= 3; ++wisch) {
            const double w = (bild * 30 + blatt * 90 + wisch * 5) * PI / 180;
            const int a = wisch == 0 ? 150 : 80 - std::abs(wisch) * 20;
            p.setPen(stift(QColor(34, 38, 46, a), wisch == 0 ? 1.5 : 2.0));
            p.drawLine(QPointF(mx, my), QPointF(mx + std::cos(w) * rx * 0.985, my + std::sin(w) * ry * 0.985));
            // heller Glanz auf dem Blatt: auch vor dunklem Hintergrund zu sehen
            p.setPen(stift(QColor(226, 232, 240, a / 2), 0.7));
            p.drawLine(QPointF(mx, my - 0.5), QPointF(mx + std::cos(w) * rx * 0.985, my - 0.5 + std::sin(w) * ry * 0.985));
        }
        // gelbe Blattspitze
        const double w = (bild * 30 + blatt * 90) * PI / 180;
        p.setPen(stift(QColor(250, 200, 40, 170), 1.5));
        p.drawLine(QPointF(mx + std::cos(w) * rx * 0.9, my + std::sin(w) * ry * 0.9), QPointF(mx + std::cos(w) * rx * 0.985, my + std::sin(w) * ry * 0.985));
    }
    flaeche(p, rund(mx - 4, my - 2.2, 8, 4.4, 2), QColor(48, 52, 60), 0.5);
}

void heckrotor(QPainter &p, int bild) {
    const double m = 13, r = 12;
    p.setPen(Qt::NoPen);
    QRadialGradient g(m, m, r);
    g.setColorAt(0, QColor(70, 76, 86, 60));
    g.setColorAt(0.85, QColor(70, 76, 86, 50));
    g.setColorAt(1, QColor(70, 76, 86, 0));
    p.setBrush(g);
    p.drawEllipse(QRectF(m - r, m - r, 2 * r, 2 * r));
    for (int blatt = 0; blatt < 2; ++blatt)
        for (int wisch = -3; wisch <= 3; ++wisch) {
            const double w = (bild * 60 + blatt * 180 + wisch * 9) * PI / 180;
            p.setPen(stift(QColor(34, 38, 46, wisch == 0 ? 150 : 75 - std::abs(wisch) * 18), 1.6));
            p.drawLine(QPointF(m, m), QPointF(m + std::cos(w) * (r - 0.8), m + std::sin(w) * (r - 0.8)));
        }
    flaeche(p, rund(m - 1.6, m - 1.6, 3.2, 3.2, 1.6), QColor(60, 64, 72), 0.4);
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
void raupeUnterwagen(QPainter &p) {
    // dunkles Band, auf dem die Kettenglieder laufen (füllt die Lücken zwischen den Gliedern)
    QPainterPath band;
    band.moveTo(12, 0.9);
    band.lineTo(57, 1.9);
    band.arcTo(QRectF(57 - 7.6, 9.4 - 7.6, 15.2, 15.2), 90, -180);
    band.lineTo(12, 16.9);
    band.arcTo(QRectF(12 - 8, 8.8 - 8, 16, 16), 270, -180);
    p.setPen(stift(QColor(20, 22, 26), 2.6));
    p.setBrush(QColor(30, 33, 39));
    p.drawPath(band);
    // Laufwerksträger mit Schrauben
    flaeche(p, rund(17, 5, 36, 7, 2), verlauf(0, 5, 0, 12, {{0, QColor(96, 103, 114)}, {1, QColor(50, 55, 63)}}), 0.5);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(170, 178, 188));
    for (double x = 21; x < 52; x += 7) p.drawEllipse(QPointF(x, 8.5), 0.8, 0.8);
    p.setBrush(mitAlpha(GELB, 255));
    p.drawPath(rund(24, 2.6, 22, 2.2, 1)); // Kettenschutz in Wagenfarbe
}

void kettenglied(QPainter &p) {
    flaeche(p, rund(0.3, 0.4, 4.6, 2.1, 0.6), verlauf(0, 0, 0, 2.6, {{0, QColor(128, 136, 148)}, {1, QColor(58, 63, 72)}}), 0.45);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(22, 24, 28));
    p.drawRect(QRectF(1.7, 2.2, 1.8, 0.8)); // Steg greift in den Schnee
}

void rolle(QPainter &p, double d, int loecher, bool zaehne) {
    const double m = d / 2, r = m - 0.7;
    if (zaehne) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(44, 48, 56));
        for (int i = 0; i < 9; ++i) {
            const double w = i * 2 * PI / 9;
            p.drawPath(vieleck({{m + std::cos(w - 0.2) * (r - 0.6), m + std::sin(w - 0.2) * (r - 0.6)},
                                {m + std::cos(w) * (r + 0.7), m + std::sin(w) * (r + 0.7)},
                                {m + std::cos(w + 0.2) * (r - 0.6), m + std::sin(w + 0.2) * (r - 0.6)}}));
        }
    }
    QRadialGradient g(m - r * 0.3, m - r * 0.35, r * 1.3);
    g.setColorAt(0, QColor(150, 158, 170));
    g.setColorAt(1, QColor(52, 57, 66));
    p.setPen(stift(RAND, 0.5));
    p.setBrush(g);
    p.drawEllipse(QPointF(m, m), r - (zaehne ? 0.5 : 0), r - (zaehne ? 0.5 : 0));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(24, 26, 31));
    for (int i = 0; i < loecher; ++i) {
        const double w = i * 2 * PI / loecher + 0.4;
        p.drawEllipse(QPointF(m + std::cos(w) * r * 0.55, m + std::sin(w) * r * 0.55), r * 0.17, r * 0.17);
    }
    p.setBrush(mitAlpha(GELB, 255));
    p.drawEllipse(QPointF(m, m), r * 0.26, r * 0.26);
    // eine helle Marke, damit man die Drehung sieht
    p.setBrush(QColor(235, 240, 246));
    p.drawEllipse(QPointF(m + r * 0.78, m), r * 0.1 + 0.25, r * 0.1 + 0.25);
}

void raupeAufbau(QPainter &p) {
    // Heckgewicht und Tank
    flaeche(p, rund(2.5, 21, 8, 13, 1.5), verlauf(0, 21, 0, 34, {{0, GELB}, {1, GELB_DUNKEL}}));
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 3; ++i) { // Warnschraffur am Heck
        p.setBrush(QColor(26, 28, 32));
        p.drawPath(vieleck({{3.2, 24.0 + i * 3.4}, {5.6, 24.0 + i * 3.4}, {3.2, 26.4 + i * 3.4}}));
    }
    // Motorhaube mit Kühlergrill, Lüftungsschlitzen und Glanzkante
    QPainterPath haube = vieleck({{31, 17}, {54, 18}, {58, 22}, {58, 34}, {31, 34}});
    flaeche(p, haube, verlauf(0, 17, 0, 34, {{0, GELB_HELL}, {0.3, GELB}, {1, GELB_DUNKEL}}));
    flaeche(p, rund(54.8, 22.5, 2.8, 10.5, 0.6), QColor(34, 36, 42), 0.4);
    p.setPen(stift(QColor(60, 44, 10, 170), 0.55));
    for (double x = 37; x < 52; x += 2.6) p.drawLine(QPointF(x, 22.5), QPointF(x, 29.5));
    p.setPen(stift(QColor(255, 244, 190, 210), 0.7));
    p.drawLine(QPointF(32.5, 18.2), QPointF(53.2, 19));
    // Auspuff mit Regenklappe, Luftfilter
    flaeche(p, rund(39.6, 5, 2.8, 13, 1), verlauf(39.6, 0, 42.4, 0, {{0, QColor(150, 156, 166)}, {0.5, QColor(92, 98, 108)}, {1, QColor(40, 43, 50)}}), 0.5);
    p.setPen(stift(RAND, 0.9));
    p.drawLine(QPointF(39, 4.8), QPointF(43.4, 3.6));
    flaeche(p, rund(46, 13.2, 4.5, 4.6, 1.2), verlauf(0, 13, 0, 18, {{0, QColor(96, 102, 112)}, {1, QColor(44, 48, 55)}}), 0.5);
    // Kabine: Rückwand, Innenraum (dunkel), Dach mit Überstand
    flaeche(p, rund(8, 5, 24, 29, 1.6), verlauf(0, 5, 0, 34, {{0, GELB_HELL}, {0.4, GELB}, {1, GELB_DUNKEL}}));
    flaeche(p, rund(10.2, 7, 19.6, 14, 1.2), verlauf(0, 7, 0, 21, {{0, QColor(52, 66, 84)}, {1, QColor(20, 26, 36)}}), 0.5);
    flaeche(p, rund(17.5, 15, 6, 6.5, 1.2), QColor(14, 16, 20), 0);   // Sitzlehne
    flaeche(p, rund(5.5, 2.8, 29.5, 3.4, 1.4), verlauf(0, 2.8, 0, 6.2, {{0, QColor(250, 250, 250)}, {1, QColor(176, 184, 194)}}));
    // Öse für den Lasthaken, Sockel der Rundumleuchte
    p.setPen(stift(RAND, 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(30, 1.6), 1.5, 1.5);
    p.setPen(stift(STAHL_HELL, 0.9));
    p.drawEllipse(QPointF(30, 1.6), 1.5, 1.5);
    flaeche(p, rund(12, 1.6, 4, 1.6, 0.4), QColor(40, 43, 50), 0.4);
    flaeche(p, rund(12.4, -0.9 + 0.6, 3.2, 2.4, 1.1), verlauf(0, 0, 0, 2, {{0, QColor(255, 190, 90)}, {1, QColor(226, 110, 20)}}), 0.4);
}

// vordere Kabinenschicht: Scheibe mit Reflex, Holme, Tür mit Griff (verdeckt den sitzenden Fahrer bis zur Brust)
void raupeKabine(QPainter &p) {
    glas(p, rund(2.2, 3, 19.6, 14, 1.2), QRectF(2.2, 3, 19.6, 14), 105);
    p.setPen(stift(GELB_DUNKEL, 1.1));
    p.drawLine(QPointF(12.4, 3), QPointF(12.4, 17));                 // Mittelholm
    flaeche(p, rund(0.6, 17, 22.8, 13, 1.2), verlauf(0, 17, 0, 30, {{0, GELB}, {1, GELB_DUNKEL}}), 0.6);
    p.setPen(stift(mitAlpha(RAND, 150), 0.5));
    p.setBrush(Qt::NoBrush);
    p.drawPath(rund(3, 18.5, 12, 10.5, 0.8));                         // Türfuge
    p.setPen(stift(QColor(30, 32, 38), 1.0));
    p.drawLine(QPointF(11, 21), QPointF(13.6, 21));                   // Griff
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(26, 28, 32));
    p.drawPath(rund(16.6, 20.5, 5, 5, 0.8));                          // Typenschild
    p.setBrush(GELB_HELL);
    p.drawPath(vieleck({{17.4, 24.6}, {19.1, 21.4}, {20.8, 24.6}}));
}

// Fahrer (schaut nach rechts): Warnjacke mit Reflexstreifen, Helm, Bart
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

void raupeSchild(QPainter &p) {
    // Schubarm und Hydraulikzylinder
    flaeche(p, vieleck({{0.6, 18.3}, {14.6, 21}, {14.6, 24.2}, {0.6, 21.7}}), verlauf(0, 18, 0, 24, {{0, GELB}, {1, GELB_DUNKEL}}), 0.6);
    p.setPen(stift(RAND, 2.2));
    p.drawLine(QPointF(3.6, 12.6), QPointF(14, 18.6));
    p.setPen(stift(STAHL_HELL, 1.1));
    p.drawLine(QPointF(3.6, 12.6), QPointF(14, 18.6));
    p.setPen(stift(QColor(60, 64, 72), 1.9));
    p.drawLine(QPointF(3.6, 12.6), QPointF(8, 15.1));
    flaeche(p, rund(0.4, 18.4, 3.4, 3.4, 1.7), STAHL_DUNKEL, 0.5);
    // gewölbtes Schild: hinten Wagenfarbe, vorn blanker Stahl
    QPainterPath ruecken;
    ruecken.moveTo(14.4, 1.2);
    ruecken.quadTo(10.6, 14, 15.6, 27);
    ruecken.lineTo(18.4, 27);
    ruecken.quadTo(13.6, 14, 17.2, 1.2);
    ruecken.closeSubpath();
    flaeche(p, ruecken, verlauf(11, 0, 18, 0, {{0, GELB_DUNKEL}, {1, GELB}}), 0.6);
    QPainterPath blatt;
    blatt.moveTo(17.2, 1.2);
    blatt.quadTo(13.6, 14, 18.4, 27);
    blatt.lineTo(21.2, 27);
    blatt.quadTo(16.8, 14, 19.6, 1.2);
    blatt.closeSubpath();
    flaeche(p, blatt, verlauf(0, 1, 0, 27, {{0, STAHL_HELL}, {0.55, STAHL}, {1, STAHL_DUNKEL}}), 0.6);
    p.setPen(stift(QColor(255, 255, 255, 190), 0.6));
    p.drawLine(QPointF(18.6, 3), QPointF(17.6, 10));
    flaeche(p, rund(15.2, 26, 6.4, 1.8, 0.5), QColor(36, 39, 45), 0.4);  // Schürfleiste
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
void pflugRumpf(QPainter &p) {
    // Überrollbügel mit Öse und Leuchtensockel
    p.setBrush(Qt::NoBrush);
    QPainterPath buegel;
    buegel.moveTo(9, 20);
    buegel.lineTo(9, 5);
    buegel.quadTo(9, 2.6, 12, 2.6);
    buegel.lineTo(24, 2.6);
    p.setPen(stift(RAND, 2.5));
    p.drawPath(buegel);
    p.setPen(stift(QColor(70, 76, 86), 1.4));
    p.drawPath(buegel);
    p.setPen(stift(RAND, 1.8));
    p.drawEllipse(QPointF(22, 1.5), 1.2, 1.2);
    p.setPen(stift(STAHL_HELL, 0.8));
    p.drawEllipse(QPointF(22, 1.5), 1.2, 1.2);
    flaeche(p, rund(9.6, 0.2, 2.8, 2.6, 1.1), verlauf(0, 0, 0, 2.8, {{0, QColor(255, 190, 90)}, {1, QColor(226, 110, 20)}}), 0.4);
    // Sitz
    flaeche(p, rund(10.5, 11, 4, 9, 1.5), QColor(30, 32, 38), 0.5);
    flaeche(p, rund(11, 17.5, 10, 3, 1.2), QColor(40, 43, 50), 0.5);
    // Rumpf mit Haube, Kotflügeln und Scheinwerfer
    QPainterPath rumpf;
    rumpf.moveTo(5, 24.5);
    rumpf.lineTo(5.5, 19.5);
    rumpf.quadTo(11, 18.6, 22, 19);
    rumpf.lineTo(23.5, 12.2);
    rumpf.quadTo(31, 11, 38.5, 13.5);
    rumpf.lineTo(40, 24.5);
    rumpf.closeSubpath();
    flaeche(p, rumpf, verlauf(0, 11, 0, 25, {{0, ORANGE_HELL}, {0.4, ORANGE}, {1, ORANGE_DUNKEL}}), 0.7);
    p.setPen(stift(QColor(255, 226, 190, 200), 0.6));
    p.drawLine(QPointF(25, 12.9), QPointF(37.4, 14));
    flaeche(p, rund(37.2, 15.2, 2.6, 2.6, 1.2), QColor(255, 248, 200), 0.4);
    flaeche(p, rund(36.6, 19, 3.6, 4.6, 0.6), QColor(34, 36, 42), 0.4);
    p.setPen(stift(QColor(90, 36, 6, 170), 0.5));
    for (double x = 27; x < 35; x += 2.4) p.drawLine(QPointF(x, 16), QPointF(x, 21));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(250, 250, 250));
    p.drawRect(QRectF(7, 21.4, 14, 1.2));          // Reflexstreifen
    // Lenksäule
    p.setPen(stift(RAND, 1.5));
    p.drawLine(QPointF(24.5, 13), QPointF(21.5, 8.6));
    p.drawLine(QPointF(19.6, 9.6), QPointF(23, 7.4));
}

void rad(QPainter &p) {
    const double m = 6, r = 5.6;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 21, 25));
    p.drawEllipse(QPointF(m, m), r, r);
    p.setPen(stift(QColor(58, 60, 68), 0.7));
    for (int i = 0; i < 12; ++i) { // Stollen
        const double w = i * 2 * PI / 12;
        p.drawLine(QPointF(m + std::cos(w) * (r - 1.5), m + std::sin(w) * (r - 1.5)), QPointF(m + std::cos(w + 0.12) * (r - 0.2), m + std::sin(w + 0.12) * (r - 0.2)));
    }
    QRadialGradient g(m - 1, m - 1, 3.4);
    g.setColorAt(0, QColor(236, 240, 246));
    g.setColorAt(1, QColor(120, 128, 140));
    p.setPen(stift(RAND, 0.4));
    p.setBrush(g);
    p.drawEllipse(QPointF(m, m), 2.9, 2.9);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(40, 43, 50));
    for (int i = 0; i < 3; ++i) {
        const double w = i * 2 * PI / 3;
        p.drawEllipse(QPointF(m + std::cos(w) * 1.6, m + std::sin(w) * 1.6), 0.5, 0.5);
    }
    p.setBrush(ORANGE);
    p.drawEllipse(QPointF(m + 1.2, m + 1.0), 0.45, 0.45); // Ventilkappe: zeigt die Drehung
}

void pflugSchild(QPainter &p) {
    flaeche(p, vieleck({{0.4, 10.6}, {7.4, 12}, {7.4, 14.4}, {0.4, 13.2}}), verlauf(0, 10, 0, 15, {{0, QColor(84, 90, 100)}, {1, QColor(40, 43, 50)}}), 0.5);
    QPainterPath blatt;
    blatt.moveTo(7.6, 1);
    blatt.quadTo(5, 9, 8.2, 17);
    blatt.lineTo(11.2, 17);
    blatt.quadTo(8, 9, 10.2, 1);
    blatt.closeSubpath();
    flaeche(p, blatt, verlauf(0, 1, 0, 17, {{0, QColor(255, 196, 120)}, {0.5, ORANGE}, {1, ORANGE_DUNKEL}}), 0.6);
    p.setPen(stift(QColor(255, 255, 255, 170), 0.5));
    p.drawLine(QPointF(9.4, 2.4), QPointF(8.7, 7));
    flaeche(p, rund(7.6, 16.2, 4, 1.5, 0.4), QColor(36, 39, 45), 0.3);
}

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
QImage seitenbild(int bild, double skala) {
    static const QSizeF groessen[SeitenbildAnzahl] = {{60, 34}, {70, 18}, {150, 58}, {44, 28}, {12, 12}};
    const QSizeF g = groessen[bild];
    QImage img(int(std::ceil(g.width() * skala)), int(std::ceil(g.height() * skala)), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(skala, skala);
    auto bei = [&p](double x, double y, auto malen) { p.save(); p.translate(x, y); malen(); p.restore(); };
    switch (bild) {
    case BildRaupeAufbau:
        raupeAufbau(p);
        bei(13, 8, [&] { fahrer(p); });
        bei(8, 4, [&] { raupeKabine(p); });
        break;
    case BildRaupeKette:
        raupeUnterwagen(p);
        bei(4.5, 1.5, [&] { rolle(p, 15, 5, true); });
        bei(50, 2.5, [&] { rolle(p, 14, 4, false); });
        for (double x : {-13.0, -5.5, 2.0, 9.5}) bei(x + 31, 9.4, [&] { rolle(p, 8, 0, false); });
        break;
    case BildHeli: heliRumpf(p); break;
    case BildPflug: pflugRumpf(p); break;
    case BildRad: rad(p); break;
    }
    return img;
}

QColor abgetoent(QColor c, double hell) {
    if (hell <= 1) return QColor(int(c.red() * hell), int(c.green() * hell), int(c.blue() * hell), c.alpha());
    const double w = std::min(1.0, (hell - 1) * 0.9);
    return QColor(int(c.red() + (255 - c.red()) * w), int(c.green() + (255 - c.green()) * w), int(c.blue() + (255 - c.blue()) * w), c.alpha());
}

// malt das Modell im Gierwinkel; der Maler steht in Einheiten mit dem Modellursprung im Nullpunkt
void modellMalen(QPainter &p, const Modell &m, double gier, const std::vector<QImage> &texturen, double texturSkala) {
    for (const Sichtflaeche &v : ansicht(m, gier)) {
        const Flaeche &f = *v.quelle;
        QPolygonF vieleck;
        for (const P2 &e : v.ecken) vieleck << QPointF(e.x, e.y);
        QColor grund = QColor::fromRgba(f.farbe);
        bool bemalt = false;
        if (f.textur >= 0 && f.ecken.size() >= 3) {
            // Seitenbild liegt in der x-y-Ebene: Bildpunkt (u, v) = (x + tu, tv - y). Je Fläche ist die Abbildung
            // Bild → Leinwand affin; steht die Fläche fast quer zur Bildebene, taugt sie nicht (dann einfarbig).
            const size_t n = f.ecken.size(), i1 = std::max<size_t>(1, n / 3), i2 = std::max<size_t>(2, 2 * n / 3);
            const P3 &a = f.ecken[0], &b = f.ecken[i1], &c = f.ecken[i2];
            const double u0 = a.x + f.tu, v0 = f.tv - a.y, du1 = b.x - a.x, dv1 = -(b.y - a.y), du2 = c.x - a.x, dv2 = -(c.y - a.y);
            const double det = du1 * dv2 - du2 * dv1;
            const double nz = std::abs(normale(f).z);
            const QImage &bild = texturen[size_t(f.textur)];
            if (nz > 0.3 && std::abs(det) > 1e-6) {
                const QPointF A = vieleck[0], B = vieleck[int(i1)], C = vieleck[int(i2)];
                const double xu = ((B.x() - A.x()) * dv2 - (C.x() - A.x()) * dv1) / det, xv = ((C.x() - A.x()) * du1 - (B.x() - A.x()) * du2) / det;
                const double yu = ((B.y() - A.y()) * dv2 - (C.y() - A.y()) * dv1) / det, yv = ((C.y() - A.y()) * du1 - (B.y() - A.y()) * du2) / det;
                QBrush pinsel(bild);
                pinsel.setTransform(QTransform(xu / texturSkala, yu / texturSkala, xv / texturSkala, yv / texturSkala,
                                               A.x() - xu * u0 - xv * v0, A.y() - yu * u0 - yv * v0));
                p.setPen(QPen(pinsel, 0.35));
                p.setBrush(pinsel);
                p.drawPolygon(vieleck);
                // Licht: abgewandte Flächen dunkler, dem Licht zugewandte heller
                p.setPen(Qt::NoPen);
                p.setBrush(v.hell < 1 ? QColor(10, 14, 24, int(std::min(1.0, (1 - v.hell) * 1.1) * 255)) : QColor(255, 255, 255, int(std::min(0.5, (v.hell - 1) * 0.7) * 255)));
                p.drawPolygon(vieleck);
                bemalt = true;
            } else {
                // Farbe des Seitenbilds an dieser Stelle übernehmen (z. B. Rot des Rumpfs, Blau der Kanzel)
                P3 mitte;
                for (const P3 &e : f.ecken) { mitte.x += e.x / f.ecken.size(); mitte.y += e.y / f.ecken.size(); }
                const QPoint q(int((mitte.x + f.tu) * texturSkala), int((f.tv - mitte.y) * texturSkala));
                if (bild.rect().contains(q) && bild.pixelColor(q).alpha() > 220) grund = bild.pixelColor(q);
            }
        }
        if (bemalt) continue;
        const QColor farbe = grund.alpha() < 255 ? grund : abgetoent(grund, v.hell);
        p.setPen(grund.alpha() < 255 ? QPen(Qt::NoPen) : QPen(farbe.darker(106), 0.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(farbe);
        p.drawPolygon(vieleck);
    }
}

struct Vorlage {
    Teil teil;
    float w, h, px, py;
};
const Vorlage VORLAGEN[] = {
    {Teil::HeliRumpf, 150, 58, 92, 47},
    {Teil::HeliRotor0, 176, 14, 88, 7}, {Teil::HeliRotor1, 176, 14, 88, 7}, {Teil::HeliRotor2, 176, 14, 88, 7},
    {Teil::Heckrotor0, 26, 26, 13, 13}, {Teil::Heckrotor1, 26, 26, 13, 13}, {Teil::Heckrotor2, 26, 26, 13, 13},
    {Teil::LichtRot, 16, 16, 8, 8}, {Teil::LichtGruen, 16, 16, 8, 8}, {Teil::LichtWeiss, 16, 16, 8, 8}, {Teil::LichtOrange, 16, 16, 8, 8},
    {Teil::Haken, 8, 12, 4, 1}, {Teil::Seil, 4, 4, 2, 2}, {Teil::Schatten, 64, 12, 32, 6},
    {Teil::RaupeUnterwagen, 70, 18, 35, 18}, {Teil::Kettenglied, 5.2f, 3, 2.6f, 1.5f}, {Teil::Laufrolle, 8, 8, 4, 4},
    {Teil::Antriebsrad, 15, 15, 7.5f, 7.5f}, {Teil::Leitrad, 14, 14, 7, 7}, {Teil::RaupeAufbau, 60, 34, 30, 34},
    {Teil::RaupeGlas, 24, 30, 0, 0}, {Teil::Fahrer, 14, 18, 7, 18}, {Teil::Arm, 5, 12, 2.5f, 1.5f}, {Teil::RaupeSchild, 22, 28, 2, 20},
    {Teil::SchneeDach, 30, 8, 15, 8}, {Teil::SchneeHaube, 24, 6, 12, 6}, {Teil::Klecks, 16, 16, 8, 8},
    {Teil::PflugRumpf, 44, 28, 22, 28}, {Teil::Rad, 12, 12, 6, 6}, {Teil::PflugSchild, 12, 18, 1, 12},
    {Teil::Rauch, 16, 16, 8, 8}, {Teil::Staub, 16, 16, 8, 8}, {Teil::Ball, 36, 36, 18, 18},
};

void malen(QPainter &p, Teil t) {
    switch (t) {
    case Teil::HeliRumpf: heliRumpf(p); break;
    case Teil::HeliRotor0: heliRotor(p, 0); break;
    case Teil::HeliRotor1: heliRotor(p, 1); break;
    case Teil::HeliRotor2: heliRotor(p, 2); break;
    case Teil::Heckrotor0: heckrotor(p, 0); break;
    case Teil::Heckrotor1: heckrotor(p, 1); break;
    case Teil::Heckrotor2: heckrotor(p, 2); break;
    case Teil::LichtRot: leuchtpunkt(p, 16, QColor(255, 40, 30)); break;
    case Teil::LichtGruen: leuchtpunkt(p, 16, QColor(40, 240, 90)); break;
    case Teil::LichtWeiss: leuchtpunkt(p, 16, QColor(235, 244, 255)); break;
    case Teil::LichtOrange: leuchtpunkt(p, 16, QColor(255, 150, 20)); break;
    case Teil::Haken: haken(p); break;
    case Teil::Seil: seil(p); break;
    case Teil::Schatten: schatten(p); break;
    case Teil::RaupeUnterwagen: raupeUnterwagen(p); break;
    case Teil::Kettenglied: kettenglied(p); break;
    case Teil::Laufrolle: rolle(p, 8, 0, false); break;
    case Teil::Antriebsrad: rolle(p, 15, 5, true); break;
    case Teil::Leitrad: rolle(p, 14, 4, false); break;
    case Teil::RaupeAufbau: raupeAufbau(p); break;
    case Teil::RaupeGlas: raupeKabine(p); break;
    case Teil::Fahrer: fahrer(p); break;
    case Teil::Arm: arm(p); break;
    case Teil::RaupeSchild: raupeSchild(p); break;
    case Teil::SchneeDach: schneeBuckel(p, 30, 8, 7); break;
    case Teil::SchneeHaube: schneeBuckel(p, 24, 6, 21); break;
    case Teil::Klecks: klecks(p); break;
    case Teil::PflugRumpf: pflugRumpf(p); break;
    case Teil::Rad: rad(p); break;
    case Teil::PflugSchild: pflugSchild(p); break;
    case Teil::Rauch: wolke(p, QColor(120, 120, 126, 190), QColor(86, 86, 92, 150)); break;
    case Teil::Staub: wolke(p, QColor(255, 255, 255, 235), QColor(236, 243, 252, 190)); break;
    case Teil::Ball: ball(p); break;
    default: break;
    }
}
} // namespace

FahrzeugAtlas fahrzeugAtlasErzeugen(double dpr, double groesse) {
    struct Auftrag { float w, h, px, py; double skala; int teil; QString text; const Modell *modell = nullptr; double gier = 0; };
    std::vector<Auftrag> auftraege;
    auftraege.resize(size_t(teilAnzahl()));
    for (const Vorlage &v : VORLAGEN) auftraege[size_t(v.teil)] = {v.w, v.h, v.px, v.py, dpr * groesse, int(v.teil), QString()};
    const QFontMetricsF masse(blasenSchrift());
    for (int i = 0; i < spruchAnzahl(); ++i) {
        const QString text = QString::fromUtf8(spruchText(i));
        const float w = float(std::ceil(masse.horizontalAdvance(text)) + 22);
        auftraege[size_t(int(Teil::Blase0) + i)] = {w, 31, 3, 30.2f, dpr * blasenSkala(groesse), int(Teil::Blase0), text};
    }
    // 3D-Ansichten fürs Wenden: je Winkelstufe ein Bild (Raupe und Pflug in zwei Phasen). Bei sehr großen
    // Fahrzeugen etwas gröber gerastert – sie sind nur kurz und in Bewegung zu sehen.
    const double skala3d = std::min(dpr * groesse, 3.0);
    const Modell modelle[5] = {raupenModell(0), raupenModell(1), pflugModell(0), pflugModell(1), heliModell()};
    std::vector<QImage> texturen;
    for (int b = 0; b < SeitenbildAnzahl; ++b) texturen.push_back(seitenbild(b, skala3d));
    for (int stufe = 0; stufe < WINKELSTUFEN; ++stufe) {
        const double gier = stufenWinkel(stufe);
        for (int phase = 0; phase < 2; ++phase) {
            auftraege[size_t(int(Teil::Raupe3D) + stufe * 2 + phase)] = {RAHMEN_RAUPE.w, RAHMEN_RAUPE.h, RAHMEN_RAUPE.px, RAHMEN_RAUPE.py, skala3d,
                                                                         int(Teil::Raupe3D), QString(), &modelle[phase], gier};
            auftraege[size_t(int(Teil::Pflug3D) + stufe * 2 + phase)] = {RAHMEN_PFLUG.w, RAHMEN_PFLUG.h, RAHMEN_PFLUG.px, RAHMEN_PFLUG.py, skala3d,
                                                                         int(Teil::Pflug3D), QString(), &modelle[2 + phase], gier};
        }
        auftraege[size_t(int(Teil::Heli3D) + stufe)] = {RAHMEN_HELI.w, RAHMEN_HELI.h, RAHMEN_HELI.px, RAHMEN_HELI.py, skala3d, int(Teil::Heli3D),
                                                        QString(), &modelle[4], gier};
    }
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
    QImage bild(breite, y + zeile + rand, QImage::Format_ARGB32_Premultiplied);
    bild.fill(Qt::transparent);
    QPainter p(&bild);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    for (size_t i = 0; i < auftraege.size(); ++i) {
        const Auftrag &t = auftraege[i];
        const FahrzeugFeld &f = a.felder[i];
        p.save();
        p.setClipRect(QRect(f.x, f.y, f.w, f.h));
        p.translate(f.x, f.y);
        p.scale(f.w / double(t.w), f.h / double(t.h));
        if (t.modell) {
            p.translate(t.px, t.py);
            modellMalen(p, *t.modell, t.gier, texturen, skala3d);
        } else if (t.teil == int(Teil::Blase0)) blase(p, t.text, t.w, t.h);
        else malen(p, Teil(t.teil));
        p.restore();
    }
    p.end();
    a.bild = bild.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    return a;
}

const FahrzeugAtlas &fahrzeugAtlasFuer(double dpr, double groesse) {
    static std::map<std::pair<int, int>, FahrzeugAtlas> lager;
    const std::pair<int, int> schluessel(int(std::lround(dpr * 100)), int(std::lround(groesse * 100)));
    auto it = lager.find(schluessel);
    if (it == lager.end()) {
        if (lager.size() > 6) lager.clear(); // Größe am Regler verstellt: alte Bilder nicht ewig aufheben
        it = lager.emplace(schluessel, fahrzeugAtlasErzeugen(dpr, groesse)).first;
    }
    return it->second;
}
