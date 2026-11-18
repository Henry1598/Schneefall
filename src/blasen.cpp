#include "blasen.h"
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace {
QFont schrift() {
    QFont f(QStringLiteral("Noto Sans"));
    f.setPixelSize(11);
    f.setBold(true);
    f.setHintingPreference(QFont::PreferNoHinting);
    return f;
}
}

Blasenpool::Blasenpool(int plaetze) { m_plaetze.resize(size_t(std::max(1, plaetze))); }

const Blasenfeld &Blasenpool::feld(const std::string &text, bool gespiegelt, double pxJeEinheit) {
    if (pxJeEinheit != m_skala || m_bild.isNull()) {
        // andere Skalierung (Monitor, Fahrzeuggröße): alles neu
        m_skala = pxJeEinheit;
        m_bild = QImage(int(std::ceil(PLATZ_BREITE * m_skala)), int(std::ceil(PLATZ_HOEHE * m_skala)) * int(m_plaetze.size()), QImage::Format_RGBA8888_Premultiplied);
        m_bild.fill(Qt::transparent);
        for (Platz &p : m_plaetze) p = Platz{};
        m_geaendert = true;
    }
    Platz *frei = nullptr;
    for (Platz &p : m_plaetze) {
        if (p.belegt && p.gespiegelt == gespiegelt && p.text == text) { p.zuletzt = m_bildNr; return p.feld; }
        // Ersatz: unbenutzt, sonst der am längsten nicht gezeigte – aber keiner, der in diesem Bild schon dran ist
        if (p.belegt && p.zuletzt == m_bildNr) continue;
        if (!frei || !p.belegt || (frei->belegt && p.zuletzt < frei->zuletzt)) frei = !frei || frei->belegt ? &p : frei;
    }
    if (!frei) frei = &m_plaetze.front();   // mehr Blasen zugleich als Plätze: kommt praktisch nicht vor
    frei->text = text;
    frei->gespiegelt = gespiegelt;
    frei->belegt = true;
    frei->zuletzt = m_bildNr;
    malen(*frei, int(frei - m_plaetze.data()));
    return frei->feld;
}

void Blasenpool::malen(Platz &platz, int nr) {
    const QString text = QString::fromStdString(platz.text);
    const QFontMetricsF masse(schrift());
    const QRectF satz = masse.boundingRect(QRectF(0, 0, TEXT_BREITE, 1000), Qt::AlignHCenter | Qt::TextWordWrap, text);
    const double w = std::min(double(PLATZ_BREITE), std::max(44.0, std::ceil(satz.width()) + 24));
    const double h = std::min(double(PLATZ_HOEHE), std::ceil(satz.height()) + 13 + 9);   // Rand + Schwanz
    const int zeile = int(std::ceil(PLATZ_HOEHE * m_skala));
    const QRect ganz(0, nr * zeile, m_bild.width(), zeile);
    QPainter p(&m_bild);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(ganz, Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setClipRect(ganz);
    p.translate(0, ganz.y());
    p.scale(m_skala, m_skala);
    if (platz.gespiegelt) { p.translate(w, 0); p.scale(-1, 1); }   // nur die Form spiegeln, den Text danach gerade malen
    QPainterPath form;
    form.addRoundedRect(QRectF(1, 1, w - 2, h - 9), 7, 7);
    QPainterPath schwanz;
    schwanz.moveTo(9, h - 9.5);
    schwanz.lineTo(19, h - 9.5);
    schwanz.lineTo(3, h - 0.8);
    schwanz.closeSubpath();
    form = form.united(schwanz);
    QLinearGradient verlauf(0, 0, 0, h);
    verlauf.setColorAt(0, QColor(255, 255, 255));
    verlauf.setColorAt(1, QColor(232, 238, 246));
    p.setPen(QPen(QColor(28, 32, 40), 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(verlauf);
    p.drawPath(form);
    if (platz.gespiegelt) { p.scale(-1, 1); p.translate(-w, 0); }
    p.setFont(schrift());
    p.setPen(QColor(24, 28, 36));
    p.drawText(QRectF(1, 1, w - 2, h - 10), Qt::AlignCenter | Qt::TextWordWrap, text);
    p.end();
    platz.feld = {0, ganz.y(), int(std::ceil(w * m_skala)), int(std::ceil(h * m_skala)), float(w), float(h), float(platz.gespiegelt ? w - 3 : 3), float(h - 0.8)};
    ++m_gemalt;
    m_geaendert = true;
}
