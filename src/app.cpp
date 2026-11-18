#include "app.h"
#include "overlay.h"
#include "steuerfenster.h"

#include <QAction>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
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
} // namespace

App::App() {
    m_takt.setTimerType(Qt::PreciseTimer);
    connect(&m_takt, &QTimer::timeout, this, &App::takt);
    m_speicherTimer.setSingleShot(true);
    m_speicherTimer.setInterval(400);
    connect(&m_speicherTimer, &QTimer::timeout, this, &App::speichern);
    connect(&m_beobachter, &QFileSystemWatcher::fileChanged, this, [this] {
        // von Hand bearbeitet: neu laden (nach atomarem Ersetzen erneut beobachten)
        QTimer::singleShot(200, this, [this] { laden(); });
    });
}

App::~App() {
    if (m_speicherTimer.isActive()) speichern();
    qDeleteAll(m_ebenen);
    delete m_steuer;
    delete m_menu;
}

QString App::konfigPfad() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/schneefall.conf");
}

bool App::dienstAnmelden() {
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning("Schneefall: keine D-Bus-Sitzung");
        return false;
    }
    if (!bus.registerService(QStringLiteral("org.schneefall"))) return false;
    bus.registerObject(QStringLiteral("/"), this, QDBusConnection::ExportScriptableSlots);
    return true;
}

void App::laden() {
    QFile f(konfigPfad());
    if (f.open(QIODevice::ReadOnly)) {
        m_einst = einstellungenAusText(f.readAll().toStdString());
    } else {
        m_einst = Programmeinstellungen{};
        speichern(); // Datei mit Standardwerten und Erklärungen anlegen
    }
    if (!m_beobachter.files().contains(konfigPfad()) && QFile::exists(konfigPfad())) m_beobachter.addPath(konfigPfad());
    m_sim.setEinstellungen(m_einst.sim);
    m_takt.setInterval(std::max(1, 1000 / m_einst.fps));
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
    m_sim.setEinstellungen(m_einst.sim);
    m_takt.setInterval(std::max(1, 1000 / m_einst.fps));
    if (m_anAktion) m_anAktion->setChecked(m_einst.an);
    sichtbarkeitPruefen();
    m_speicherTimer.start();
}

void App::starten() {
    laden();
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
    m_tray->setToolTip(QStringLiteral("Schneefall"));
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
    auto *o = new Overlay(s, &m_sim);
    m_ebenen.insert(s, o);
    connect(s, &QScreen::geometryChanged, this, [this, o] {
        o->anBildschirmAnpassen();
        bildschirmeMelden();
    });
    bildschirmeMelden();
    if (m_laeuft) o->show();
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
}

void App::sichtbarkeitPruefen() {
    bool soll = m_einst.an && !(m_einst.vollbildPausieren && m_vollbild);
    if (soll == m_laeuft) return;
    m_laeuft = soll;
    if (soll) {
        for (Overlay *o : std::as_const(m_ebenen)) o->show();
        m_uhr.restart();
        m_takt.start(std::max(1, 1000 / m_einst.fps));
    } else {
        m_takt.stop();
        for (Overlay *o : std::as_const(m_ebenen)) o->hide();
    }
}

void App::takt() {
    double dt = std::min(0.1, m_uhr.restart() / 1000.0);
    QElapsedTimer t;
    t.start();
    m_sim.schritt(dt, true);
    m_rechenNs += t.nsecsElapsed();
    ++m_rechenBilder;
    for (Overlay *o : std::as_const(m_ebenen)) o->update();
}

void App::Fenster(const QString &json) {
    if (json == m_letzteListe) return; // Puls ohne Änderung
    m_letzteListe = json;
    const QJsonArray liste = QJsonDocument::fromJson(json.toUtf8()).array();
    std::vector<schnee::Fenster> f;
    f.reserve(size_t(liste.size()));
    for (const QJsonValue &v : liste) {
        QJsonObject o = v.toObject();
        schnee::Fenster w;
        w.id = o.value(QStringLiteral("id")).toString().toStdString();
        w.r = {o.value(QStringLiteral("x")).toDouble(), o.value(QStringLiteral("y")).toDouble(),
               o.value(QStringLiteral("w")).toDouble(), o.value(QStringLiteral("h")).toDouble()};
        w.stapel = o.value(QStringLiteral("s")).toInt();
        w.minimiert = o.value(QStringLiteral("min")).toBool();
        w.vollbild = o.value(QStringLiteral("fs")).toBool();
        w.aktiv = o.value(QStringLiteral("akt")).toBool();
        w.dock = o.value(QStringLiteral("dock")).toBool();
        if (!w.id.empty()) f.push_back(std::move(w));
    }
    m_fenster = std::move(f);
    m_sim.setFenster(m_fenster);
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
    QStringList b;
    for (auto it = m_ebenen.cbegin(); it != m_ebenen.cend(); ++it)
        b << QStringLiteral("%1 %2x%3+%4+%5 dpr=%6 sichtbar=%7")
                 .arg(it.key()->name())
                 .arg(it.key()->geometry().width()).arg(it.key()->geometry().height())
                 .arg(it.key()->geometry().x()).arg(it.key()->geometry().y())
                 .arg(it.value()->devicePixelRatio()).arg(it.value()->isVisible());
    int decken = 0;
    double schnee = 0;
    for (const Decke &d : m_sim.decken()) {
        ++decken;
        for (float h : d.hoehe) schnee += h;
    }
    return QStringLiteral("laeuft=%1 an=%2 vollbild=%3 flocken=%4 fenster=%5 decken=%6 schnee=%7 rechenzeit_ms=%8\n%9")
        .arg(m_laeuft).arg(m_einst.an).arg(m_vollbild).arg(m_sim.flocken().size()).arg(m_fenster.size())
        .arg(decken).arg(schnee, 0, 'f', 0).arg(ms, 0, 'f', 3).arg(b.join(QLatin1Char('\n')));
}

QString App::Fensterliste() { return m_letzteListe; }
