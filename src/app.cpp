#include "app.h"
#include "overlay.h"
#include "steuerfenster.h"
#include "sprueche.h"
#include "plattform/pfade.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QPainter>
#include <QSaveFile>
#include <QScreen>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <cmath>

using namespace schnee;

namespace {
QIcon flockenSymbol() {
    QIcon thema = QIcon::fromTheme(QStringLiteral("weather-snow"));
    QIcon icon;
    for (int g : {16, 22, 24, 32, 48, 64}) {
        QPixmap pm(g, g);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QPen stift(QColor(120, 190, 255), std::max(1.0, g / 14.0), Qt::SolidLine, Qt::RoundCap);
        p.setPen(stift);
        p.translate(g / 2.0, g / 2.0);
        double r = g * 0.44;
        for (int i = 0; i < 6; ++i) {
            p.drawLine(QPointF(0, 0), QPointF(0, -r));
            p.drawLine(QPointF(0, -r * 0.6), QPointF(-r * 0.25, -r * 0.82));
            p.drawLine(QPointF(0, -r * 0.6), QPointF(r * 0.25, -r * 0.82));
            p.rotate(60);
        }
        icon.addPixmap(pm);
    }
    Q_UNUSED(thema);
    return icon;
}
// eine Zeile Bildzeiten für --status: Monitorrate, Soll, gemessene Bilder/s, Abstände, Jitter, verpasste Bilder, Verteilung
QString bildzeitenText(double hz, double soll, const Bildzeiten::Bericht &z) {
    QStringList stufen;
    for (int n : z.stufen) stufen << QString::number(n);
    return QStringLiteral("hz=%1 soll_ms=%2 bilder=%3 fps=%4 abstand_ms=%5 jitter_ms=%6 p50=%7 p95=%8 p99=%9 max=%10 verpasst=%11 "
                          "verteilung[<0.5|0.5-0.9|0.9-1.1|1.1-1.5|1.5-2.5|>2.5]=%12")
        .arg(hz, 0, 'f', 2).arg(soll * 1000, 0, 'f', 2).arg(z.bilder).arg(z.fps, 0, 'f', 1).arg(z.mittelMs, 0, 'f', 2)
        .arg(z.jitterMs, 0, 'f', 2).arg(z.p50Ms, 0, 'f', 2).arg(z.p95Ms, 0, 'f', 2).arg(z.p99Ms, 0, 'f', 2).arg(z.maxMs, 0, 'f', 2)
        .arg(z.verpasst).arg(stufen.join(QLatin1Char('|')));
}
} // namespace

App::App(std::unique_ptr<plattform::Fensterquelle> quelle) : m_quelle(quelle ? std::move(quelle) : plattform::fensterquelleErzeugen()) {
    m_mausUhr.start();
    m_takt.setTimerType(Qt::PreciseTimer);
    connect(&m_takt, &QTimer::timeout, this, &App::takt);
    m_speicherTimer.setSingleShot(true);
    m_speicherTimer.setInterval(400);
    connect(&m_speicherTimer, &QTimer::timeout, this, &App::speichern);
    m_statistikTimer.setInterval(30000);
    connect(&m_statistikTimer, &QTimer::timeout, this, [this] { if (m_statistik.geaendert()) m_statistik.speichern(); });
    m_statistikTimer.start();
    connect(&m_beobachter, &QFileSystemWatcher::fileChanged, this, [this] {
        // von Hand bearbeitet: neu laden (nach atomarem Ersetzen erneut beobachten)
        QTimer::singleShot(200, this, [this] { laden(); });
    });
    m_quelle->starten([this](std::vector<schnee::Fenster> liste) { fensterSetzen(std::move(liste)); });
}

App::~App() {
    // zuerst die Kanäle schließen: danach kommt kein Befehl mehr an
    m_steuerkanal.reset();
    m_systemkanal.reset();
    if (m_speicherTimer.isActive()) speichern();
    statistikAbholen();
    m_statistik.speichern();
    qDeleteAll(m_ebenen);
    delete m_steuer;
    delete m_menu;
}

QString App::konfigPfad() { return plattform::konfigPfad(); }

bool App::dienstAnmelden() {
    auto ziel = [this](const QString &name, const QStringList &argumente) { return befehl(name, argumente); };
    // Linux: D-Bus org.schneefall (Einzelinstanz wie bisher; das KWin-Skript schickt dorthin die Fensterliste)
    m_systemkanal = plattform::systemkanalErzeugen(ziel, *m_quelle);
    if (m_systemkanal && !m_systemkanal->anmelden()) {
        m_systemkanal.reset();
        return false;
    }
    // plattformneutral: lokaler Socket bzw. benannte Pipe
    m_steuerkanal = std::make_unique<plattform::Steuerkanal>(ziel);
    if (!m_steuerkanal->anmelden()) {
        m_steuerkanal.reset();
        m_systemkanal.reset();
        return false;
    }
    return true;
}

plattform::Antwort App::befehl(const QString &name, const QStringList &argumente) {
    using plattform::Antwort;
    // Zahlen-Argumente: alle müssen gültige, endliche Zahlen sein (nan/inf nicht), sonst ist der Befehl unbekannt
    std::vector<double> z;
    for (const QString &a : argumente) {
        bool ok = false;
        z.push_back(a.toDouble(&ok));
        if (!ok || !std::isfinite(z.back())) return Antwort::unbekannt();
    }
    const size_t n = z.size();
    if (name == QLatin1String("Status") && n == 0) return Antwort::ok(Status());
    if (name == QLatin1String("Fensterliste") && n == 0) return Antwort::ok(Fensterliste());
    if (name == QLatin1String("Raeumen") && n == 0) return Antwort::ok(Raeumen());
    if (name == QLatin1String("Steuerfenster") && n == 0) { Steuerfenster(); return Antwort::ok(); }
    if (name == QLatin1String("Beenden") && n == 0) { Beenden(); return Antwort::ok(); }
    if (name == QLatin1String("Umschalten") && n == 0) { Umschalten(); return Antwort::ok(); }
    if (name == QLatin1String("Abschuetteln") && n == 0) { Abschuetteln(); return Antwort::ok(); }
    if (name == QLatin1String("Werfen") && n == 5) { Werfen(z[0], z[1], z[2], z[3], z[4]); return Antwort::ok(); }
    if (name == QLatin1String("Zeitlupe") && n == 1) { Zeitlupe(z[0]); return Antwort::ok(); }
    return Antwort::unbekannt();
}

void App::laden() {
    QFile f(konfigPfad());
    if (f.open(QIODevice::ReadOnly)) {
        const std::string text = f.readAll().toStdString();
        m_einst = einstellungenAusText(text);
        // neue Schlüssel ergänzen bzw. vor_fenstern entfernen; vorhandene Werte bleiben, wie sie sind
        if (einstellungenBrauchtErgaenzung(text)) {
            f.close();
            speichern();
        }
    } else {
        m_einst = Programmeinstellungen{};
        speichern(); // Datei mit Standardwerten und Erklärungen anlegen
    }
    if (!m_beobachter.files().contains(konfigPfad()) && QFile::exists(konfigPfad())) m_beobachter.addPath(konfigPfad());
    m_sim.setEinstellungen(m_einst.sim);
    m_raeum.setEinstellungen(m_einst.raeum);
    Sprueche::instanz().setzeMaxStufe(m_einst.spruecheMaxStufe);   // auch nach Änderung der Datei von Hand
    m_eingabeFaellig = 0;
    for (Overlay *o : std::as_const(m_ebenen)) o->setzeObergrenze(m_einst.bildrate);
    m_klang.setzeLautstaerke(m_einst.lautstaerke / 100.0);
    m_klang.setzeAn(m_einst.ton);
    if (m_anAktion) m_anAktion->setChecked(m_einst.an);
    if (m_steuer) m_steuer->ausEinstellungen();
    sichtbarkeitPruefen();
}

void App::speichern() {
    m_speicherTimer.stop();
    QDir().mkpath(QFileInfo(konfigPfad()).absolutePath());
    std::string text = einstellungenAlsText(m_einst);
    QFile alt(konfigPfad());
    if (alt.open(QIODevice::ReadOnly) && alt.readAll().toStdString() == text) return;
    alt.close();
    QSaveFile f(konfigPfad());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(text.data(), qint64(text.size()));
        f.commit();
    }
    // nach dem Ersetzen wieder beobachten
    m_beobachter.removePath(konfigPfad());
    m_beobachter.addPath(konfigPfad());
}

void App::einstellungenAendern(const Programmeinstellungen &p) {
    m_einst = p;
    Sprueche::instanz().setzeMaxStufe(m_einst.spruecheMaxStufe);
    m_sim.setEinstellungen(m_einst.sim);
    m_raeum.setEinstellungen(m_einst.raeum);
    m_eingabeFaellig = 0;
    m_klang.setzeLautstaerke(m_einst.lautstaerke / 100.0);
    m_klang.setzeAn(m_einst.ton);
    for (Overlay *o : std::as_const(m_ebenen)) o->setzeObergrenze(m_einst.bildrate);
    if (m_anAktion) m_anAktion->setChecked(m_einst.an);
    sichtbarkeitPruefen();
    m_speicherTimer.start();
}

void App::starten() {
    laden();
    // die 1000 Sprüche für Fahrer und Pilot; die Ausdrucksweise folgt der Einstellung
    static_assert(int(Redner::Pilot) == int(Sprecher::Pilot) && int(Redeanlass::HeliAbflug) == int(Anlass::HeliAbflug) && int(Redeanlass::Gruss) == int(Anlass::Gruss));
    Sprueche::instanz().setzeMaxStufe(m_einst.spruecheMaxStufe);
    m_raeum.setSpruchquelle([](Redner wer, Redeanlass anlass, int trefferzahl) {
        return Sprueche::instanz().waehleBeiTreffer(Sprecher(int(wer)), Anlass(int(anlass)), trefferzahl).toStdString();
    });
    for (QScreen *s : QGuiApplication::screens()) bildschirmHinzu(s);
    connect(qApp, &QGuiApplication::screenAdded, this, &App::bildschirmHinzu);
    connect(qApp, &QGuiApplication::screenRemoved, this, &App::bildschirmWeg);
    bildschirmeMelden();
    m_sim.vorfuellen(); // sofort überall Schnee, nicht erst ein Vorhang von oben
    trayAufbauen();
    sichtbarkeitPruefen();
}

void App::trayAufbauen() {
    m_tray = new QSystemTrayIcon(flockenSymbol(), this);
    m_tray->setToolTip(m_statistik.kurzText());
    m_menu = new QMenu;
    m_anAktion = m_menu->addAction(QStringLiteral("Schnee an"));
    m_anAktion->setCheckable(true);
    m_anAktion->setChecked(m_einst.an);
    connect(m_anAktion, &QAction::triggered, this, [this](bool an) {
        Programmeinstellungen p = m_einst;
        p.an = an;
        einstellungenAendern(p);
        if (m_steuer) m_steuer->ausEinstellungen();
    });
    m_menu->addAction(QStringLiteral("Steuerfenster …"), this, &App::Steuerfenster);
    m_menu->addAction(QStringLiteral("Schnee abschütteln"), this, &App::Abschuetteln);
    m_menu->addAction(QStringLiteral("Jetzt räumen"), this, [this] { Raeumen(); });
    m_menu->addSeparator();
    m_menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), QStringLiteral("Beenden"), this, &App::Beenden);
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) {
            if (m_steuer && m_steuer->isVisible()) m_steuer->hide();
            else Steuerfenster();
        }
    });
    m_tray->show();
}

void App::bildschirmHinzu(QScreen *s) {
    if (m_ebenen.contains(s)) return;
    auto *o = new Overlay(s, &m_sim, &m_einst.realistisch, &m_szene, &m_einst.raeum.fahrzeugGroesse, &m_taktstand);
    o->mausziel = [this](int art, double x, double y) { maus(art, x, y); };
    o->vorDemBild = [this](double jetzt) { bildKommt(jetzt); };
    o->setzeObergrenze(m_einst.bildrate);
    m_ebenen.insert(s, o);
    connect(s, &QScreen::geometryChanged, this, [this, o] {
        o->anBildschirmAnpassen();
        bildschirmeMelden();
    });
    bildschirmeMelden();
    if (m_laeuft) o->zeigen(true);
}

void App::bildschirmWeg(QScreen *s) {
    if (Overlay *o = m_ebenen.take(s)) delete o;
    bildschirmeMelden();
}

void App::bildschirmeMelden() {
    std::vector<Rechteck> b;
    for (QScreen *s : m_ebenen.keys()) {
        QRect g = s->geometry();
        b.push_back({double(g.x()), double(g.y()), double(g.width()), double(g.height())});
    }
    // feste Reihenfolge (links oben zuerst), damit "boden:<i>" stabil bleibt
    std::sort(b.begin(), b.end(), [](const Rechteck &a, const Rechteck &c) { return a.x != c.x ? a.x < c.x : a.y < c.y; });
    m_sim.setBildschirme(b);
    if (!b.empty()) {
        double l = b.front().x, r = l;
        for (const Rechteck &q : b) { l = std::min(l, q.x); r = std::max(r, q.rechts()); }
        m_klang.setzeWeltbreite(l, r);
    }
}

void App::sichtbarkeitPruefen() {
    bool soll = m_einst.an && !(m_einst.vollbildPausieren && m_vollbild);
    if (soll == m_laeuft) return;
    m_laeuft = soll;
    if (soll) {
        m_taktstand.schrittUhr = uhrSekunden(); // die Pause zählt nicht als Simulationszeit
        for (Overlay *o : std::as_const(m_ebenen)) o->zeigen(true);
        m_takt.start(100);
    } else {
        m_takt.stop();
        for (Overlay *o : std::as_const(m_ebenen)) o->zeigen(false);
        // Pause oder Vollbild: der Einsatz bleibt stehen, die Geräusche verstummen
        m_raeum.klaengeAnhalten();
        klaengeWeitergeben();
    }
}

// Bewegung, die über das Fortschreiben im Shader hinausgeht: dann rechnet die Simulation in jedem
// Monitorbild (Fenster ziehen, fallende Schneebretter, Räumdienst, Schneebälle, Maus gedrückt)
bool App::inBewegung(double jetzt) const {
    return m_raeum.etwasLos() || !m_sim.brocken().empty() || m_mausGedrueckt || jetzt - m_fensterUhr < 0.5;
}

void App::bildKommt(double jetzt) {
    if (!m_laeuft) return;
    // Ruhe: 30 Schritte je s, die Flocken schreibt der Shader fort. Bewegung (Räumdienst, Schneeball, gezogenes
    // Fenster): etwas öfter als die Bildrate. Die Monitore zeichnen zeitversetzt; ein Schritt je Bild jedes Monitors
    // wären bei vier Monitoren über 150 Schritte je s. So ist der Stand in jedem Bild höchstens einen
    // Dreiviertel-Bildabstand alt (bei 80 px/s rund 1 px), und der Einsatz kostet spürbar weniger.
    const double rate = m_einst.bildrate > 0 ? m_einst.bildrate : 320;
    const double faellig = inBewegung(jetzt) ? 1.0 / (rate * 1.25) : 1.0 / 30 - 0.002;
    if (jetzt - m_taktstand.schrittUhr >= faellig) schritt(jetzt);
}

void App::takt() {
    const double jetzt = uhrSekunden();
    if (jetzt - m_taktstand.schrittUhr >= 0.09) schritt(jetzt);
}

void App::schritt(double jetzt) {
    double dt = std::clamp(jetzt - m_taktstand.schrittUhr, 0.0, 0.1);
    dt *= m_zeitlupe;
    QElapsedTimer t;
    t.start();
    m_sim.schritt(dt, true);
    m_raeum.schritt(dt);
    statistikAbholen();
    if (m_raeum.etwasLos()) szeneBauen(m_raeum, m_sim, m_mausUhr.elapsed() / 1000.0, m_szene);
    else m_szene.leeren();
    klaengeWeitergeben();
    // Eingaberegion: alle halbe Sekunde, mit Fahrzeug unterwegs öfter
    if (jetzt >= m_eingabeFaellig) {
        m_eingabeFaellig = jetzt + (m_raeum.einsaetze().empty() ? 0.5 : 0.125);
        eingabeAktualisieren();
    }
    m_taktstand.schrittUhr = jetzt;
    m_taktstand.zeitlupe = m_zeitlupe;
    ++m_taktstand.schritt;
    ++m_schritte;
    // Flocken höchstens 30-mal je s neu aufbauen lassen (dazwischen schreibt der Shader sie fort)
    if (jetzt - m_flockenUhr >= 1.0 / 30 - 0.002) {
        m_flockenUhr = jetzt;
        ++m_taktstand.flockenSchritt;
        ++m_flockenAufbauten;
    }
    m_rechenNs += t.nsecsElapsed();
    ++m_rechenBilder;
}

void App::fensterSetzen(std::vector<schnee::Fenster> f) {
    m_fenster = std::move(f);
    // Zeitpunkt in Simulationszeit: daraus misst der Kern, wie schnell und ruckartig ein Fenster gezogen wird
    const double jetzt = uhrSekunden();
    m_sim.setFenster(m_fenster, m_sim.zeit() + (m_laeuft ? std::clamp(jetzt - m_taktstand.schrittUhr, 0.0, 0.1) * m_zeitlupe : 0.0));
    m_fensterUhr = jetzt; // Fenster bewegen sich: Decken in jedem Bild nachführen
    m_vollbild = vollbildAktiv(m_fenster);
    sichtbarkeitPruefen();
}

void App::Steuerfenster() {
    if (!m_steuer) m_steuer = new ::Steuerfenster(this);
    m_steuer->ausEinstellungen();
    m_steuer->show();
    m_steuer->raise();
    m_steuer->activateWindow();
}

void App::Beenden() { QTimer::singleShot(0, qApp, &QCoreApplication::quit); }

void App::Umschalten() {
    Programmeinstellungen p = m_einst;
    p.an = !p.an;
    einstellungenAendern(p);
    if (m_steuer) m_steuer->ausEinstellungen();
}

void App::Abschuetteln() { m_sim.abschuetteln(); }

QString App::Status() {
    double ms = m_rechenBilder ? m_rechenNs / 1e6 / m_rechenBilder : 0;
    m_rechenNs = 0;
    m_rechenBilder = 0;
    const double jetzt = uhrSekunden(), spanne = std::max(1e-3, jetzt - m_statusUhr);
    const QString takt = QStringLiteral("bildrate=%1 schritte_je_s=%2 flockenaufbau_je_s=%3 bewegung=%4\n")
                             .arg(m_einst.bildrate > 0 ? QString::number(m_einst.bildrate) : QStringLiteral("auto"))
                             .arg(m_schritte / spanne, 0, 'f', 1).arg(m_flockenAufbauten / spanne, 0, 'f', 1).arg(inBewegung(jetzt));
    m_statusUhr = jetzt;
    m_schritte = m_flockenAufbauten = 0;
    QStringList b;
    for (auto it = m_ebenen.cbegin(); it != m_ebenen.cend(); ++it) {
        const Overlay::Last l = it.value()->lastAbholen();
        const Bildzeiten::Bericht z = it.value()->bildzeitenAbholen();
        b << QStringLiteral("%1 %2x%3+%4+%5 dpr=%6 sichtbar=%7 sammeln_ms=%8 zeichnen_ms=%9 ecken=%10")
                 .arg(it.key()->name())
                 .arg(it.key()->geometry().width()).arg(it.key()->geometry().height())
                 .arg(it.key()->geometry().x()).arg(it.key()->geometry().y())
                 .arg(it.value()->devicePixelRatio()).arg(it.value()->isVisible())
                 .arg(l.sammelnMs, 0, 'f', 3).arg(l.zeichnenMs, 0, 'f', 3).arg(l.ecken)
          + QStringLiteral(" tausch_ms=%1 flockenaufbau_ms=%2 instanzen=%3").arg(l.tauschMs, 0, 'f', 3).arg(l.flockenaufbauMs, 0, 'f', 3).arg(l.instanzen)
          + QLatin1Char(' ') + bildzeitenText(it.value()->monitorHz(), it.value()->sollAbstand(), z)
          + QStringLiteral(" teiler=%1").arg(it.value()->teiler());
    }
    int decken = 0;
    double schnee = 0;
    for (const Decke &d : m_sim.decken()) {
        ++decken;
        for (float h : d.hoehe) schnee += h;
    }
    // Räumdienst: je Einsatz Fenster, Phase, Fahrzeugphase und Lage
    static const char *const phasen[] = {"Anflug", "Schweben", "Absenken", "Ausklinken", "Raeumen", "Rueckkehr", "Einhaken", "Anheben", "Abflug", "Fertig"};
    static const char *const fahrphasen[] = {"AmSeil", "Aufgesetzt", "Wenden", "Senken", "Schieben", "Kippen", "Zurueck", "Bereit", "Streik", "Beleidigt"};
    QStringList einsaetze;
    for (const Einsatz &e : m_raeum.einsaetze())
        einsaetze << QStringLiteral("einsatz=%1 fenster=%2 phase=%3 fahrzeug=%4 heli=%5,%6 fz=%7,%8 wulst=%9 treffer=%10 spruch=%11 pilot=%12")
                         .arg(e.nr).arg(QString::fromStdString(e.id), QLatin1String(phasen[int(e.phase)]), QLatin1String(fahrphasen[int(e.fz.phase)]))
                         .arg(e.heli.x, 0, 'f', 0).arg(e.heli.y, 0, 'f', 0).arg(e.fz.x, 0, 'f', 0).arg(e.fz.y, 0, 'f', 0)
                         .arg(e.fz.wulst, 0, 'f', 0).arg(e.fz.trefferGesamt)
                         .arg(e.fz.blase.sichtbar() ? QString::fromStdString(e.fz.blase.text).replace(QLatin1Char(' '), QLatin1Char('_')) : QStringLiteral("-"),
                              e.heli.blase.sichtbar() ? QString::fromStdString(e.heli.blase.text).replace(QLatin1Char(' '), QLatin1Char('_')) : QStringLiteral("-"));
    return QStringLiteral("laeuft=%1 an=%2 vollbild=%3 flocken=%4 fenster=%5 decken=%6 schnee=%7 rechenzeit_ms=%8 brocken=%9 einsaetze=%11 wartend=%12 baelle=%13 eingabeflaechen=%14\n%10%15")
        .arg(m_laeuft).arg(m_einst.an).arg(m_vollbild).arg(m_sim.flocken().size()).arg(m_fenster.size())
        .arg(decken).arg(schnee, 0, 'f', 0).arg(ms, 0, 'f', 3).arg(m_sim.brocken().size()).arg(takt + b.join(QLatin1Char('\n')))
        .arg(m_raeum.einsaetze().size()).arg(m_raeum.warteschlange().size()).arg(m_raeum.baelle().size() + (m_raeum.ballInHand() ? 1 : 0))
        .arg(m_raeum.eingabeRechtecke().size()).arg((einsaetze.isEmpty() ? QString() : QLatin1Char('\n') + einsaetze.join(QLatin1Char('\n'))) + QLatin1Char('\n') + m_statistik.statusZeile());
}

QString App::Fensterliste() { return m_quelle->diagnose(); }

QString App::Raeumen() { return QString::fromStdString(m_raeum.jetztRaeumen()); }

void App::Zeitlupe(double faktor) {
    if (!std::isfinite(faktor)) return; // std::clamp ließe NaN durch
    m_zeitlupe = std::clamp(faktor, 0.02, 1.0);
}

void App::Werfen(double x, double y, double vx, double vy, double menge) { m_raeum.werfen(x, y, vx, vy, menge); }

void App::statistikAbholen() {
    const std::vector<Statistikereignis> e = m_raeum.statistikAbholen();
    if (e.empty()) return;
    m_statistik.verbuchen(e);
    if (m_tray) {
        const QString t = m_statistik.kurzText();
        if (m_tray->toolTip() != t) m_tray->setToolTip(t);
    }
}

void App::statistikZuruecksetzen() {
    m_raeum.statistikAbholen();   // noch nicht verbuchte Ereignisse gehören zur alten Zählung
    m_statistik.zuruecksetzen();
    m_statistik.speichern();
    if (m_tray) m_tray->setToolTip(m_statistik.kurzText());
}

void App::klaengeWeitergeben() {
    static_assert(int(Geraeusch::Rotor) == int(KlangArt::Rotor) && int(Geraeusch::Murren) == int(KlangArt::Murren)
                  && int(Geraeusch::Rutsch) == int(KlangArt::Rutsch) && int(Geraeusch::Treffer) == int(KlangArt::Treffer));
    for (const Klangereignis &k : m_raeum.klaengeAbholen()) {
        const KlangArt art = KlangArt(int(k.art));
        switch (k.typ) {
        case Klangereignis::Einmal: m_klang.ereignis(art, k.x, k.y, k.staerke); break;
        case Klangereignis::Dauer: m_klang.dauerklang(k.id, art, k.x, k.y, k.staerke); break;
        case Klangereignis::Stopp: m_klang.dauerklangStopp(k.id); break;
        }
    }
}

void App::eingabeAktualisieren() {
    const std::vector<Rechteck> r = m_raeum.eingabeRechtecke();
    for (Overlay *o : std::as_const(m_ebenen)) o->eingabeSetzen(r);
}

void App::maus(int art, double x, double y) {
    const double t = m_mausUhr.elapsed() / 1000.0;
    m_mausGedrueckt = art != 2;
    if (art == 0) m_raeum.mausDruck(x, y, t);
    else if (art == 1) m_raeum.mausBewegt(x, y, t);
    else m_raeum.mausLos(x, y, t);
}
