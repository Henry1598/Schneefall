#include "steuerfenster.h"
#include "app.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSaveFile>
#include <QSlider>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace schnee;

QString Steuerfenster::autostartPfad() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart/schneefall.desktop");
}

Steuerfenster::Regler Steuerfenster::regler(const QString &name, int min, int max, double faktor, const QString &einheit,
                                            QGridLayout *g, int zeile, const QString &tipp) {
    auto *l = new QLabel(name, this);
    l->setToolTip(tipp);
    auto *s = new QSlider(Qt::Horizontal, this);
    s->setRange(min, max);
    s->setMinimumWidth(220);
    auto *w = new QLabel(this);
    w->setMinimumWidth(70);
    w->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    g->addWidget(l, zeile, 0);
    g->addWidget(s, zeile, 1);
    g->addWidget(w, zeile, 2);
    s->setToolTip(tipp);
    connect(s, &QSlider::valueChanged, this, [this] { uebernehmen(); });
    return {s, w, faktor, einheit};
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
    m_hoehe = regler(QStringLiteral("Max. Deckenhöhe"), 2, 80, 1.0, QStringLiteral(" px"), g, 3,
                     QStringLiteral("So hoch kann sich Schnee auf Fenstern sammeln."));
    m_fall = regler(QStringLiteral("Fallgeschwindigkeit"), 10, 200, 1.0, QStringLiteral(" px/s"), g, 4);
    m_detail = new QComboBox(this);
    m_detail->addItem(QStringLiteral("Einfach (weiche Punkte)"));
    m_detail->addItem(QStringLiteral("Realistisch (Schneekristalle)"));
    m_detail->setToolTip(QStringLiteral("Realistisch: sechszählige Schneekristalle, nahe Flocken groß und schnell, ferne klein und blass. "
                                        "Einfach: alle Flocken als weiche Punkte."));
    m_detail->setObjectName(QStringLiteral("flockendetail"));
    g->addWidget(new QLabel(QStringLiteral("Flockendetail"), this), 5, 0);
    g->addWidget(m_detail, 5, 1, 1, 2);
    haupt->addLayout(g);

    // Gruppe „Fenster“: wie Schnee und Fenster zusammenspielen
    auto *fensterGruppe = new QGroupBox(QStringLiteral("Fenster"), this);
    auto *fg = new QGridLayout(fensterGruppe);
    m_modus = new QComboBox(this);
    m_modus->setObjectName(QStringLiteral("fenstermodus"));
    m_modus->addItem(QStringLiteral("Fest (Schnee landet auf Fenstern)"));
    m_modus->addItem(QStringLiteral("Dahinter (fällt hinter Fenstern)"));
    m_modus->addItem(QStringLiteral("Davor (fällt vor Fenstern)"));
    m_modus->setToolTip(QStringLiteral("Fest: Fenster sind feste Flächen. Schnee landet überall, wo ein Fenster von oben getroffen werden kann, "
                                       "prallt seitlich ab und fällt nie davor oder dahinter. Unter Fenstern liegt ein Schneeschatten."));
    fg->addWidget(new QLabel(QStringLiteral("Fenstermodus"), this), 0, 0);
    fg->addWidget(m_modus, 0, 1, 1, 2);
    m_schwelle = regler(QStringLiteral("Schüttel-Schwelle"), 1, 100, 1000.0, QStringLiteral(" px/s²"), fg, 1,
                        QStringLiteral("Ab dieser Beschleunigung beim Ziehen fällt Schnee vom Fenster. Klein = empfindlich."));
    m_rutschAb = regler(QStringLiteral("Rutsch ab"), 0, 120, 1.0, QStringLiteral(" min"), fg, 2,
                        QStringLiteral("War ein Fenster so lange offen (oder ist die Decke fast voll), rutscht der Schnee beim Minimieren "
                                       "oder Schließen als Brett ab. Sonst rieselt er als Wolke herunter."));
    m_rutschTempo = regler(QStringLiteral("Rutschtempo"), 2, 30, 0.1, QStringLiteral("×"), fg, 3);
    m_wolkeDauer = regler(QStringLiteral("Wolkendauer"), 1, 60, 1.0, QStringLiteral(" s"), fg, 4);
    m_wolkeLiegen = regler(QStringLiteral("Wolke bleibt liegen"), 0, 100, 1.0, QStringLiteral(" %"), fg, 5,
                           QStringLiteral("So viel der Schneewolke landet unten, der Rest löst sich auf."));
    m_bodenMax = regler(QStringLiteral("Max. Boden/Taskleiste"), 2, 80, 1.0, QStringLiteral(" px"), fg, 6);
    haupt->addWidget(fensterGruppe);
    m_alle = {&m_menge, &m_wind, &m_groesse, &m_hoehe, &m_fall, &m_schwelle, &m_rutschAb, &m_rutschTempo, &m_wolkeDauer, &m_wolkeLiegen, &m_bodenMax};

    auto *abschuetteln = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-clear-all")), QStringLiteral("Schnee abschütteln"), this);
    haupt->addWidget(abschuetteln);
    connect(abschuetteln, &QPushButton::clicked, m_app, &App::Abschuetteln);

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
    connect(m_modus, &QComboBox::currentIndexChanged, this, [this] { uebernehmen(); });
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
    m_modus->setCurrentIndex(int(p.sim.modus));
    m_detail->setCurrentIndex(p.realistisch ? 1 : 0);
    m_autostart->setChecked(QFile::exists(autostartPfad()));
    auto setze = [](Regler &r, double wert) { r.slider->setValue(int(std::lround(wert / r.faktor))); };
    setze(m_menge, p.sim.menge);
    setze(m_wind, p.sim.wind);
    setze(m_groesse, p.sim.groesse);
    setze(m_hoehe, p.sim.maxHoehe);
    setze(m_fall, p.sim.fallGeschw);
    setze(m_schwelle, p.sim.schuettelnSchwelle);
    setze(m_rutschAb, p.sim.rutschAbMinuten);
    setze(m_rutschTempo, p.sim.rutschTempo);
    setze(m_wolkeDauer, p.sim.wolkeDauer);
    setze(m_wolkeLiegen, p.sim.wolkeLiegen * 100);
    setze(m_bodenMax, p.sim.bodenMax);
    m_setzt = false;
    uebernehmen(); // nur Beschriftungen, da Werte gleich
}

void Steuerfenster::uebernehmen() {
    for (Regler *r : m_alle) {
        if (!r->slider) continue; // noch im Aufbau
        double w = r->slider->value() * r->faktor;
        QString t = QString::number(w, 'g', 4) + r->einheit;
        if (r == &m_wind) t = (w < 0 ? QStringLiteral("← ") : w > 0 ? QStringLiteral("→ ") : QString()) + QString::number(std::abs(w)) + r->einheit;
        r->wert->setText(t);
    }
    if (m_setzt) return;
    const Programmeinstellungen &alt = m_app->einstellungen();
    Programmeinstellungen p = alt;
    p.an = m_an->isChecked();
    p.vollbildPausieren = m_vollbild->isChecked();
    p.sim.modus = Fenstermodus(std::clamp(m_modus->currentIndex(), 0, 2));
    p.realistisch = m_detail->currentIndex() == 1;
    // Reglerwerte nur übernehmen, wenn der Regler bewegt wurde: so bleiben krumme Werte aus der Datei erhalten
    auto lies = [](const Regler &r, double &ziel, double skala = 1.0) {
        if (r.slider->value() != int(std::lround(ziel * skala / r.faktor))) ziel = r.slider->value() * r.faktor / skala;
    };
    lies(m_menge, p.sim.menge);
    lies(m_wind, p.sim.wind);
    lies(m_groesse, p.sim.groesse);
    lies(m_hoehe, p.sim.maxHoehe);
    lies(m_fall, p.sim.fallGeschw);
    lies(m_schwelle, p.sim.schuettelnSchwelle);
    lies(m_rutschAb, p.sim.rutschAbMinuten);
    lies(m_rutschTempo, p.sim.rutschTempo);
    lies(m_wolkeDauer, p.sim.wolkeDauer);
    lies(m_wolkeLiegen, p.sim.wolkeLiegen, 100.0);
    lies(m_bodenMax, p.sim.bodenMax);
    if (einstellungenAlsText(p) == einstellungenAlsText(alt)) return;
    m_app->einstellungenAendern(p);
}
