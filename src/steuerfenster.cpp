#include "steuerfenster.h"
#include "app.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSaveFile>
#include <QSlider>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <cmath>

using namespace schnee;

QString Steuerfenster::autostartPfad() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart/schneefall.desktop");
}

Steuerfenster::Regler Steuerfenster::regler(const QString &name, int min, int max, double faktor, const QString &einheit,
                                            QGridLayout *g, int zeile) {
    auto *l = new QLabel(name, this);
    auto *s = new QSlider(Qt::Horizontal, this);
    s->setRange(min, max);
    s->setMinimumWidth(220);
    auto *w = new QLabel(this);
    w->setMinimumWidth(70);
    w->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    g->addWidget(l, zeile, 0);
    g->addWidget(s, zeile, 1);
    g->addWidget(w, zeile, 2);
    m_einheiten[zeile] = einheit;
    connect(s, &QSlider::valueChanged, this, [this] { uebernehmen(); });
    return {s, w, faktor};
}

Steuerfenster::Steuerfenster(App *app) : m_app(app) {
    setWindowTitle(QStringLiteral("Schneefall"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("weather-snow")));
    setAttribute(Qt::WA_QuitOnClose, false);

    auto *haupt = new QVBoxLayout(this);
    m_an = new QCheckBox(QStringLiteral("Schnee an"), this);
    QFont f = m_an->font();
    f.setBold(true);
    m_an->setFont(f);
    haupt->addWidget(m_an);

    auto *g = new QGridLayout;
    m_menge = regler(QStringLiteral("Menge"), 0, 100, 0.05, QString(), g, 0);          // 0 … 5
    m_wind = regler(QStringLiteral("Wind ← →"), -150, 150, 1.0, QStringLiteral(" px/s"), g, 1);
    m_groesse = regler(QStringLiteral("Flockengröße"), 2, 20, 0.5, QStringLiteral(" px"), g, 2); // 1 … 10
    m_hoehe = regler(QStringLiteral("Max. Deckenhöhe"), 2, 60, 1.0, QStringLiteral(" px"), g, 3);
    m_fall = regler(QStringLiteral("Fallgeschwindigkeit"), 10, 200, 1.0, QStringLiteral(" px/s"), g, 4);
    m_detail = new QComboBox(this);
    m_detail->addItem(QStringLiteral("Einfach (weiche Punkte)"));
    m_detail->addItem(QStringLiteral("Realistisch (Schneekristalle)"));
    m_detail->setToolTip(QStringLiteral("Realistisch: sechszählige Schneekristalle, nahe Flocken groß und schnell, ferne klein und blass. "
                                        "Einfach: alle Flocken als weiche Punkte."));
    g->addWidget(new QLabel(QStringLiteral("Flockendetail"), this), 5, 0);
    g->addWidget(m_detail, 5, 1, 1, 2);
    haupt->addLayout(g);

    auto *abschuetteln = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-clear-all")), QStringLiteral("Schnee abschütteln"), this);
    haupt->addWidget(abschuetteln);
    connect(abschuetteln, &QPushButton::clicked, m_app, &App::Abschuetteln);

    m_vorFenstern = new QCheckBox(QStringLiteral("Schnee fällt vor Fenstern"), this);
    m_vorFenstern->setToolTip(QStringLiteral("Aus: fallender Schnee ist nur über der Arbeitsfläche zu sehen, als fiele er hinter den Fenstern. "
                                             "Die Schneedecken auf den Fenstern bleiben sichtbar."));
    haupt->addWidget(m_vorFenstern);
    m_vollbild = new QCheckBox(QStringLiteral("Bei Vollbild pausieren"), this);
    m_autostart = new QCheckBox(QStringLiteral("Mit dem System starten"), this);
    haupt->addWidget(m_vollbild);
    haupt->addWidget(m_autostart);

    auto *unten = new QHBoxLayout;
    unten->addStretch();
    auto *schliessen = new QPushButton(QStringLiteral("Schließen"), this);
    auto *beenden = new QPushButton(QIcon::fromTheme(QStringLiteral("application-exit")), QStringLiteral("Beenden"), this);
    unten->addWidget(schliessen);
    unten->addWidget(beenden);
    haupt->addLayout(unten);
    connect(schliessen, &QPushButton::clicked, this, &QWidget::hide);
    connect(beenden, &QPushButton::clicked, m_app, &App::Beenden);

    connect(m_an, &QCheckBox::toggled, this, [this] { uebernehmen(); });
    connect(m_vollbild, &QCheckBox::toggled, this, [this] { uebernehmen(); });
    connect(m_vorFenstern, &QCheckBox::toggled, this, [this] { uebernehmen(); });
    connect(m_detail, &QComboBox::currentIndexChanged, this, [this] { uebernehmen(); });
    connect(m_autostart, &QCheckBox::toggled, this, [this](bool an) {
        if (m_setzt) return;
        const QString pfad = autostartPfad();
        if (an) {
            QDir().mkpath(QFileInfo(pfad).absolutePath());
            QSaveFile d(pfad);
            if (d.open(QIODevice::WriteOnly)) {
                d.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=Schneefall\n"
                                       "Comment=Schnee über allen Fenstern\nExec=%1\nIcon=weather-snow\n"
                                       "X-KDE-autostart-phase=2\n")
                            .arg(QCoreApplication::applicationFilePath()).toUtf8());
                d.commit();
            }
        } else {
            // nur genau diese eine, von Schneefall angelegte Datei entfernen
            QFile::remove(pfad);
        }
    });
    ausEinstellungen();
}

void Steuerfenster::ausEinstellungen() {
    m_setzt = true;
    const Programmeinstellungen &p = m_app->einstellungen();
    m_an->setChecked(p.an);
    m_vollbild->setChecked(p.vollbildPausieren);
    m_vorFenstern->setChecked(p.sim.vorFenstern);
    m_detail->setCurrentIndex(p.realistisch ? 1 : 0);
    m_autostart->setChecked(QFile::exists(autostartPfad()));
    auto setze = [](Regler &r, double wert) { r.slider->setValue(int(std::lround(wert / r.faktor))); };
    setze(m_menge, p.sim.menge);
    setze(m_wind, p.sim.wind);
    setze(m_groesse, p.sim.groesse);
    setze(m_hoehe, p.sim.maxHoehe);
    setze(m_fall, p.sim.fallGeschw);
    m_setzt = false;
    uebernehmen(); // nur Beschriftungen, da Werte gleich
}

void Steuerfenster::uebernehmen() {
    Regler *alle[5] = {&m_menge, &m_wind, &m_groesse, &m_hoehe, &m_fall};
    for (int i = 0; i < 5; ++i) {
        double w = alle[i]->slider->value() * alle[i]->faktor;
        QString t = QString::number(w, 'g', 3) + m_einheiten[i];
        if (i == 1) t = (w < 0 ? QStringLiteral("← ") : w > 0 ? QStringLiteral("→ ") : QString()) + QString::number(std::abs(w)) + m_einheiten[i];
        alle[i]->wert->setText(t);
    }
    if (m_setzt) return;
    Programmeinstellungen p = m_app->einstellungen();
    p.an = m_an->isChecked();
    p.vollbildPausieren = m_vollbild->isChecked();
    p.sim.vorFenstern = m_vorFenstern->isChecked();
    p.realistisch = m_detail->currentIndex() == 1;
    p.sim.menge = m_menge.slider->value() * m_menge.faktor;
    p.sim.wind = m_wind.slider->value() * m_wind.faktor;
    p.sim.groesse = m_groesse.slider->value() * m_groesse.faktor;
    p.sim.maxHoehe = m_hoehe.slider->value() * m_hoehe.faktor;
    p.sim.fallGeschw = m_fall.slider->value() * m_fall.faktor;
    const Programmeinstellungen &alt = m_app->einstellungen();
    if (p.an == alt.an && p.realistisch == alt.realistisch && p.vollbildPausieren == alt.vollbildPausieren && p.sim.vorFenstern == alt.sim.vorFenstern && p.sim.menge == alt.sim.menge && p.sim.wind == alt.sim.wind
        && p.sim.groesse == alt.sim.groesse && p.sim.maxHoehe == alt.sim.maxHoehe && p.sim.fallGeschw == alt.sim.fallGeschw)
        return;
    m_app->einstellungenAendern(p);
}
