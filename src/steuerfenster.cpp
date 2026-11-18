#include "steuerfenster.h"
#include "app.h"
#include "sprueche.h"
#include "plattform/autostart.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using namespace schnee;

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
    m_ton = new QCheckBox(QStringLiteral("Geräusche"), this);
    m_ton->setToolTip(QStringLiteral("Hubschrauber, Motoren, Schneebälle … als selbst erzeugte Geräusche (Standard: aus)."));
    m_laut = new QSlider(Qt::Horizontal, this);
    m_laut->setRange(0, 100);
    m_laut->setMinimumWidth(220);
    m_lautWert = new QLabel(this);
    m_lautWert->setMinimumWidth(70);
    m_lautWert->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_bildrate = new QComboBox(this);
    for (const char *t : {"Auto (wie der Monitor)", "30 Bilder/s", "60 Bilder/s", "120 Bilder/s", "144 Bilder/s"}) m_bildrate->addItem(QString::fromUtf8(t));
    m_bildrate->setObjectName(QStringLiteral("bildrate"));
    m_bildrate->setToolTip(QStringLiteral("Wie oft der Schnee je Sekunde neu gezeichnet wird, im Takt des Monitors. Auto: so oft wie der Monitor "
                                          "(z. B. 144 oder 165 Bilder/s, am flüssigsten). Eine Obergrenze zeichnet gleichmäßig jedes 2., 3. … "
                                          "Monitorbild und spart CPU (schneefall und KWin)."));
    g->addWidget(new QLabel(QStringLiteral("Bildrate"), this), 7, 0);
    g->addWidget(m_bildrate, 7, 1, 1, 2);
    g->addWidget(m_ton, 6, 0);
    g->addWidget(m_laut, 6, 1);
    g->addWidget(m_lautWert, 6, 2);
    connect(m_ton, &QCheckBox::toggled, this, [this] { uebernehmen(); });
    connect(m_laut, &QSlider::valueChanged, this, [this] { uebernehmen(); });
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

    // Gruppe „Räumdienst“: Hubschrauber bringt ein Räumfahrzeug, Schneebälle
    auto *raeumGruppe = new QGroupBox(QStringLiteral("Räumdienst"), this);
    auto *rg = new QGridLayout(raeumGruppe);
    m_raeumAn = new QCheckBox(QStringLiteral("Bei viel Schnee von selbst räumen"), this);
    m_raeumAn->setObjectName(QStringLiteral("raeumdienst"));
    m_raeumAn->setToolTip(QStringLiteral("Liegt auf einem Fenster genug Schnee, fliegt ein Hubschrauber ein Räumfahrzeug ein. "
                                         "Es schiebt den Schnee über die Fensterkante und wird wieder abgeholt."));
    rg->addWidget(m_raeumAn, 0, 0, 1, 3);
    m_raeumenAb = regler(QStringLiteral("Räumen ab"), 30, 100, 1.0, QStringLiteral(" %"), rg, 1,
                         QStringLiteral("Anteil der Höchstmenge (max. Deckenhöhe), ab dem ein Einsatz gebucht wird."));
    m_fzGroesse = regler(QStringLiteral("Fahrzeuggröße"), 5, 30, 0.1, QStringLiteral("×"), rg, 2);
    m_gleichzeitig = regler(QStringLiteral("Einsätze zugleich"), 1, 3, 1.0, QString(), rg, 3,
                            QStringLiteral("Weitere Fenster warten der Reihe nach."));
    m_trefferStreik = regler(QStringLiteral("Treffer bis Streik"), 1, 10, 1.0, QString(), rg, 4,
                             QStringLiteral("Nach so vielen Schneebällen bleibt das Fahrzeug stehen und der Fahrer beschwert sich."));
    m_taskleiste = new QCheckBox(QStringLiteral("Taskleiste und Boden räumen (kleiner Pflug)"), this);
    m_taskleiste->setObjectName(QStringLiteral("taskleiste_raeumen"));
    rg->addWidget(m_taskleiste, 5, 0, 1, 3);
    m_anfassbar = new QCheckBox(QStringLiteral("Schnee anfassbar (Schneebälle formen)"), this);
    m_anfassbar->setObjectName(QStringLiteral("schnee_anfassbar"));
    m_anfassbar->setToolTip(QStringLiteral("Auf Schnee drücken und ziehen formt einen Schneeball, loslassen wirft ihn. Dazu nimmt Schneefall "
                                           "Mausklicks auf Schnee (ab 6 px Höhe) und auf Fahrzeugen an. Aus: alle Klicks gehen durch wie früher."));
    rg->addWidget(m_anfassbar, 6, 0, 1, 3);
    auto *jetzt = new QPushButton(QIcon::fromTheme(QStringLiteral("go-jump")), QStringLiteral("Jetzt räumen"), this);
    jetzt->setObjectName(QStringLiteral("jetzt_raeumen"));
    jetzt->setToolTip(QStringLiteral("Startet sofort einen Einsatz am Fenster mit dem meisten Schnee."));
    rg->addWidget(jetzt, 7, 0, 1, 3);
    rg->addWidget(new QLabel(QStringLiteral("Ausdrucksweise"), this), 8, 0);
    m_ausdruck = new QComboBox(this);
    m_ausdruck->setObjectName(QStringLiteral("sprueche_max_stufe"));
    for (int st = SPRUCH_STUFE_MIN; st <= SPRUCH_STUFE_MAX; ++st) m_ausdruck->addItem(QString::fromUtf8(stufenName(st)));
    m_ausdruck->setToolTip(QStringLiteral("Wie derb Fahrer und Pilot schimpfen dürfen, wenn sie getroffen werden. "
                                          "Mit jedem Treffer wird es lauter – höchstens bis zu dieser Stufe."));
    rg->addWidget(m_ausdruck, 8, 1, 1, 2);
    connect(m_ausdruck, &QComboBox::currentIndexChanged, this, [this] { uebernehmen(); });
    rg->setRowStretch(9, 1);
    connect(jetzt, &QPushButton::clicked, m_app, [this] { m_app->Raeumen(); });
    for (QCheckBox *c : {m_raeumAn, m_taskleiste, m_anfassbar}) connect(c, &QCheckBox::toggled, this, [this] { uebernehmen(); });

    auto *gruppen = new QHBoxLayout;
    gruppen->addWidget(fensterGruppe);
    gruppen->addWidget(raeumGruppe);
    haupt->addLayout(gruppen);
    m_alle = {&m_menge, &m_wind, &m_groesse, &m_hoehe, &m_fall, &m_schwelle, &m_rutschAb, &m_rutschTempo, &m_wolkeDauer, &m_wolkeLiegen, &m_bodenMax,
              &m_raeumenAb, &m_fzGroesse, &m_gleichzeitig, &m_trefferStreik};

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
    connect(m_bildrate, &QComboBox::currentIndexChanged, this, [this] { uebernehmen(); });
    connect(m_autostart, &QCheckBox::toggled, this, [this](bool an) {
        if (m_setzt) return;
        plattform::autostartSetzen(an, QCoreApplication::applicationFilePath());
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
    {
        // eigener Wert aus der Datei (z. B. 45): als zusätzlicher Eintrag, damit er nicht verstellt wird
        static const int raten[] = {0, 30, 60, 120, 144};
        int i = int(std::find(std::begin(raten), std::end(raten), p.bildrate) - std::begin(raten));
        while (m_bildrate->count() > 5) m_bildrate->removeItem(5);
        if (i == 5) m_bildrate->addItem(QStringLiteral("%1 Bilder/s").arg(p.bildrate), p.bildrate);
        m_bildrate->setCurrentIndex(i);
    }
    m_autostart->setChecked(plattform::autostartAktiv());
    m_ton->setChecked(p.ton);
    m_laut->setValue(p.lautstaerke);
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
    m_raeumAn->setChecked(p.raeum.an);
    m_taskleiste->setChecked(p.raeum.taskleiste);
    m_anfassbar->setChecked(p.raeum.anfassbar);
    setze(m_raeumenAb, p.raeum.raeumenAb * 100);
    setze(m_fzGroesse, p.raeum.fahrzeugGroesse);
    setze(m_gleichzeitig, p.raeum.gleichzeitig);
    setze(m_trefferStreik, p.raeum.trefferBisStreik);
    m_ausdruck->setCurrentIndex(std::clamp(p.spruecheMaxStufe, SPRUCH_STUFE_MIN, SPRUCH_STUFE_MAX) - SPRUCH_STUFE_MIN);
    m_setzt = false;
    uebernehmen(); // nur Beschriftungen, da Werte gleich
}

void Steuerfenster::uebernehmen() {
    for (Regler *r : m_alle) {
        if (!r->slider) continue; // noch im Aufbau
        double w = r->slider->value() * r->faktor;
        QString t = QString::number(w, 'g', 6) + r->einheit;
        if (r == &m_wind) t = (w < 0 ? QStringLiteral("← ") : w > 0 ? QStringLiteral("→ ") : QString()) + QString::number(std::abs(w)) + r->einheit;
        r->wert->setText(t);
    }
    m_lautWert->setText(QString::number(m_laut->value()) + QStringLiteral(" %"));
    if (m_setzt) return;
    const Programmeinstellungen &alt = m_app->einstellungen();
    Programmeinstellungen p = alt;
    p.an = m_an->isChecked();
    p.vollbildPausieren = m_vollbild->isChecked();
    p.sim.modus = Fenstermodus(std::clamp(m_modus->currentIndex(), 0, 2));
    p.realistisch = m_detail->currentIndex() == 1;
    {
        static const int raten[] = {0, 30, 60, 120, 144};
        const int i = m_bildrate->currentIndex();
        p.bildrate = i >= 0 && i < 5 ? raten[i] : m_bildrate->currentData().toInt();
    }
    p.ton = m_ton->isChecked();
    p.lautstaerke = m_laut->value();
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
    p.raeum.an = m_raeumAn->isChecked();
    p.raeum.taskleiste = m_taskleiste->isChecked();
    p.raeum.anfassbar = m_anfassbar->isChecked();
    lies(m_raeumenAb, p.raeum.raeumenAb, 100.0);
    lies(m_fzGroesse, p.raeum.fahrzeugGroesse);
    p.raeum.gleichzeitig = m_gleichzeitig.slider->value();
    p.raeum.trefferBisStreik = m_trefferStreik.slider->value();
    p.spruecheMaxStufe = m_ausdruck->currentIndex() + SPRUCH_STUFE_MIN;
    if (einstellungenAlsText(p) == einstellungenAlsText(alt)) return;
    m_app->einstellungenAendern(p);
}
