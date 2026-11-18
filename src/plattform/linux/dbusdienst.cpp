#include "plattform/linux/dbusdienst.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include "plattform/linux/kwinfensterquelle.h"

namespace plattform {

namespace {
const QString DIENST = QStringLiteral("org.schneefall");
QString zahl(double x) { return QString::number(x, 'g', 17); }
} // namespace

DbusDienst::DbusDienst(Befehlsziel ziel, KwinFensterquelle *quelle) : m_ziel(std::move(ziel)), m_quelle(quelle) {}

DbusDienst::~DbusDienst() {
    if (!m_angemeldet) return;
    auto bus = QDBusConnection::sessionBus();
    bus.unregisterObject(QStringLiteral("/"));
    bus.unregisterService(DIENST);
}

bool DbusDienst::anmelden() {
    if (m_angemeldet) return true;
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning("Schneefall: keine D-Bus-Sitzung");
        return false;
    }
    if (!bus.registerService(DIENST)) return false;
    bus.registerObject(QStringLiteral("/"), this, QDBusConnection::ExportScriptableSlots);
    m_angemeldet = true;
    return true;
}

QString DbusDienst::text(const QString &befehl, const QStringList &argumente) {
    const Antwort a = m_ziel ? m_ziel(befehl, argumente) : Antwort::unbekannt();
    return a.text.value_or(QString());
}

void DbusDienst::Fenster(const QString &json) {
    if (m_quelle) m_quelle->listeEmpfangen(json);
}
void DbusDienst::Steuerfenster() { text(QStringLiteral("Steuerfenster")); }
void DbusDienst::Beenden() { text(QStringLiteral("Beenden")); }
void DbusDienst::Umschalten() { text(QStringLiteral("Umschalten")); }
void DbusDienst::Abschuetteln() { text(QStringLiteral("Abschuetteln")); }
QString DbusDienst::Status() { return text(QStringLiteral("Status")); }
QString DbusDienst::Fensterliste() { return text(QStringLiteral("Fensterliste")); }
QString DbusDienst::Raeumen() { return text(QStringLiteral("Raeumen")); }
void DbusDienst::Werfen(double x, double y, double vx, double vy, double menge) {
    text(QStringLiteral("Werfen"), {zahl(x), zahl(y), zahl(vx), zahl(vy), zahl(menge)});
}
void DbusDienst::Zeitlupe(double faktor) { text(QStringLiteral("Zeitlupe"), {zahl(faktor)}); }

std::unique_ptr<Systemkanal> systemkanalErzeugen(Befehlsziel ziel, Fensterquelle &quelle) {
    return std::make_unique<DbusDienst>(std::move(ziel), dynamic_cast<KwinFensterquelle *>(&quelle));
}

// Client über D-Bus (z. B. eine laufende ältere Version ohne Steuerkanal-Socket)
std::optional<Antwort> systemkanalSenden(const QString &befehl, const QStringList &argumente) {
    QDBusInterface i(DIENST, QStringLiteral("/"), DIENST);
    if (!i.isValid()) return std::nullopt;
    QList<QVariant> werte;
    if (befehl == QLatin1String("Werfen") || befehl == QLatin1String("Zeitlupe")) {
        for (const QString &a : argumente) {
            bool ok = false;
            werte << a.toDouble(&ok);
            if (!ok) return Antwort::unbekannt();
        }
    } else if (!argumente.isEmpty()) {
        return Antwort::unbekannt();
    }
    const QDBusMessage r = i.callWithArgumentList(QDBus::Block, befehl, werte);
    if (r.type() == QDBusMessage::ErrorMessage) return Antwort::unbekannt();
    Antwort a = Antwort::ok();
    if (!r.arguments().isEmpty()) a.text = r.arguments().first().toString();
    return a;
}

} // namespace plattform
