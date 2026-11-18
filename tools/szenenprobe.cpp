// Malt den Räumdienst ohne Bildschirm in PNG-Dateien (zum Begutachten der Grafik):
//   build/szenenprobe <ordner> [dpr] [fahrzeug_groesse]
// Atlas.png zeigt alle Einzelteile, die übrigen Bilder Stationen eines Einsatzes.
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <cstdio>
#include "fahrzeugbild.h"
#include "raeumdienst.h"
#include "raeumszene.h"

using namespace schnee;

static void deckeMalen(QPainter &p, const Decke &d) {
    QPainterPath pf;
    pf.moveTo(d.x, d.y);
    for (size_t i = 0; i < d.hoehe.size(); ++i) pf.lineTo(d.x + (i + 0.5) * d.spaltenBreite, d.y - (d.sichtbar[i] ? d.hoehe[i] : 0));
    pf.lineTo(d.x + d.breite(), d.y);
    pf.closeSubpath();
    QLinearGradient g(0, d.y - 20, 0, d.y);
    g.setColorAt(0, Qt::white);
    g.setColorAt(1, QColor(200, 214, 236));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawPath(pf);
}

static void bild(const QString &datei, const Simulation &sim, const Raeumdienst &rd, double zeit, double dpr, QRectF ausschnitt) {
    const FahrzeugAtlas &a = fahrzeugAtlasFuer(dpr, rd.einstellungen().fahrzeugGroesse);
    QImage img(int(ausschnitt.width() * dpr), int(ausschnitt.height() * dpr), QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.scale(dpr, dpr);
    p.translate(-ausschnitt.topLeft());
    // Hintergrund: halb heller, halb dunkler Schreibtisch, Fenster als Flächen
    QLinearGradient hg(ausschnitt.left(), 0, ausschnitt.right(), 0);
    hg.setColorAt(0, QColor(40, 60, 96));
    hg.setColorAt(0.5, QColor(96, 130, 170));
    hg.setColorAt(1, QColor(226, 232, 240));
    p.fillRect(ausschnitt, hg);
    for (const Decke &d : sim.decken()) {
        if (!d.boden) {
            p.fillRect(QRectF(d.x, d.y, d.breite(), 400), QColor(46, 50, 58));
            p.fillRect(QRectF(d.x, d.y, d.breite(), 26), QColor(70, 76, 88));
        }
        deckeMalen(p, d);
    }
    Szene sz;
    szeneBauen(rd, sim, zeit, sz);
    for (const Decke &h : sz.haufen) deckeMalen(p, h);
    for (const Brocken &b : sim.brocken()) {
        Decke d;
        d.x = b.x; d.y = b.y; d.spaltenBreite = b.spaltenBreite; d.hoehe = b.hoehe; d.sichtbar.assign(b.hoehe.size(), 1);
        deckeMalen(p, d);
    }
    for (const Zeichenteil &z : sz.teile) {
        const FahrzeugFeld &f = a.felder[z.teil];
        p.save();
        p.translate(z.x, z.y);
        p.rotate(z.winkel * 180 / 3.14159265);
        p.scale(z.sx, z.sy);
        p.translate(-f.px, -f.py);
        p.setOpacity(z.alpha);
        p.drawImage(QRectF(0, 0, f.bw, f.bh), a.bild, QRectF(f.x, f.y, f.w, f.h));
        p.restore();
    }
    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    for (const Flocke &f : sim.flocken()) p.drawEllipse(QPointF(f.x, f.y), f.groesse / 2, f.groesse / 2);
    p.end();
    img.save(datei);
    std::printf("%s (%zu Teile)\n", qPrintable(datei), sz.teile.size());
}

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    const QString ordner = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral(".");
    const double dpr = argc > 2 ? atof(argv[2]) : 2.0, groesse = argc > 3 ? atof(argv[3]) : 1.0;
    {
        const FahrzeugAtlas a = fahrzeugAtlasErzeugen(dpr * 2, groesse);
        QImage auf(a.bild.size(), QImage::Format_ARGB32_Premultiplied);
        auf.fill(QColor(90, 120, 160));
        QPainter p(&auf);
        p.drawImage(0, 0, a.bild);
        p.end();
        auf.save(ordner + "/atlas.png");
        std::printf("Atlas %dx%d\n", a.bild.width(), a.bild.height());
    }
    Einstellungen e;
    e.wind = 0;
    e.maxHoehe = 18;
    Simulation sim(e);
    Raeumdienst rd(sim);
    RaeumEinstellungen re;
    re.an = false;
    re.fahrzeugGroesse = groesse;
    rd.setEinstellungen(re);
    sim.setBildschirme({{0, 0, 1400, 800}});
    Fenster f;
    f.id = "a"; f.r = {300, 420, 800, 300}; f.stapel = 1;
    sim.setFenster({f});
    auto fuellen = [&](const char *id, double h) {
        const Decke *d = sim.decke(id);
        for (size_t i = 0; i < d->hoehe.size(); ++i) sim.schneeHinzufuegen(id, d->x + (i + 0.5) * d->spaltenBreite, h);
    };
    fuellen("a", 15);
    fuellen("boden:0", 12);
    double zeit = 0;
    auto laufen = [&](double s) { for (double t = 0; t < s; t += 1.0 / 30) { sim.schritt(1.0 / 30, true); rd.schritt(1.0 / 30); zeit += 1.0 / 30; } };
    auto bis = [&](auto bedingung) { for (int i = 0; i < 30 * 300 && !bedingung(); ++i) laufen(1.0 / 30); };
    laufen(1);
    rd.buchen("a");
    laufen(0.1);
    const Einsatz &E = rd.einsaetze().front();
    const QRectF alles(0, 0, 1400, 800);
    auto nah = [&](double w = 420, double h = 260) { return QRectF(E.fz.x - w / 2, 420 - h + 50, w, h); };
    laufen(3.2);
    bild(ordner + "/01_anflug.png", sim, rd, zeit, dpr, QRectF(E.heli.x - 210, E.heli.y - 110, 420, 260));
    bis([&] { return E.phase == Phase::Absenken; });
    laufen(0.4);
    bild(ordner + "/02_absenken.png", sim, rd, zeit, dpr, nah(420, 300));
    bis([&] { return E.phase == Phase::Ausklinken; });
    laufen(0.15);
    bild(ordner + "/03_aufgesetzt.png", sim, rd, zeit, dpr, nah(420, 300));
    bis([&] { return E.fz.phase == FahrPhase::Schieben; });
    laufen(2.2);
    bild(ordner + "/04_schieben.png", sim, rd, zeit, dpr, nah());
    bild(ordner + "/04_schieben_gesamt.png", sim, rd, zeit, 1.0, alles);
    laufen(2.0);
    bild(ordner + "/05_kante.png", sim, rd, zeit, dpr, nah());
    // Schneebälle bis zum Streik
    rd.werfen(E.fz.x - 240, 330, 620, -260, 150);
    laufen(0.35);
    bild(ordner + "/06_ball.png", sim, rd, zeit, dpr, nah(520, 300));
    bis([&] { return rd.baelle().empty(); });
    laufen(0.1);
    bild(ordner + "/07_treffer.png", sim, rd, zeit, dpr, nah());
    for (int i = 0; i < 2; ++i) { rd.werfen(E.fz.x - 10 + 20 * i, E.fz.y - 30, 0, 0, 90); laufen(0.1); }
    laufen(0.8);
    bild(ordner + "/08_streik.png", sim, rd, zeit, dpr, nah());
    laufen(0.15);
    bild(ordner + "/08_streik2.png", sim, rd, zeit, dpr, nah());
    bis([&] { return E.fz.phase == FahrPhase::Zurueck; });
    laufen(0.8);
    bild(ordner + "/09_zurueck.png", sim, rd, zeit, dpr, nah());
    bis([&] { return E.fz.phase == FahrPhase::Wenden; });
    laufen(0.25);
    bild(ordner + "/10_wenden.png", sim, rd, zeit, dpr, nah());
    bis([&] { return E.phase == Phase::Anheben; });
    laufen(0.5);
    bild(ordner + "/11_anheben.png", sim, rd, zeit, dpr, nah(420, 320));
    bis([&] { return rd.einsaetze().empty(); });
    // kleiner Pflug auf dem Boden
    rd.buchen("boden:0");
    laufen(0.1);
    const Einsatz &P = rd.einsaetze().front();
    bis([&] { return P.fz.phase == FahrPhase::Schieben; });
    laufen(2.5);
    bild(ordner + "/12_pflug.png", sim, rd, zeit, dpr, QRectF(P.fz.x - 180, 610, 420, 190));
    for (int i = 0; i < 3; ++i) { rd.werfen(P.fz.x, P.fz.y - 12, 0, 0, 90); laufen(0.1); }
    laufen(0.9);
    bild(ordner + "/13_pflug_streik.png", sim, rd, zeit, dpr, QRectF(P.fz.x - 180, 610, 420, 190));
    return 0;
}
