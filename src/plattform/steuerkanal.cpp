#include "plattform/steuerkanal.h"
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QStandardPaths>

namespace plattform {

namespace {
constexpr qint64 MAX_ZEILE = 64 * 1024; // längere Anfragen sind kein gültiger Befehl

QByteArray anfrageText(const QString &befehl, const QStringList &argumente) {
    QJsonObject o;
    o.insert(QStringLiteral("befehl"), befehl);
    o.insert(QStringLiteral("argumente"), QJsonArray::fromStringList(argumente));
    return QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n';
}

QByteArray antwortText(const Antwort &a) {
    QJsonObject o;
    o.insert(QStringLiteral("ergebnis"), a.ergebnis == Antwort::Ergebnis::Ok ? QStringLiteral("ok") : QStringLiteral("unbekannt"));
    if (a.text) o.insert(QStringLiteral("text"), *a.text);
    return QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n';
}

// läuft unter dem Namen eine Instanz, die Verbindungen annimmt?
bool instanzAntwortet(const QString &name) {
    QLocalSocket s;
    s.connectToServer(name);
    const bool da = s.waitForConnected(500);
    s.abort();
    return da;
}
} // namespace

struct Steuerkanal::Impl {
    QLocalServer server;
};

Steuerkanal::Steuerkanal(Befehlsziel ziel, QString name) : m_ziel(std::move(ziel)), m_name(std::move(name)) {}

Steuerkanal::~Steuerkanal() = default;

QString Steuerkanal::standardName() {
#ifdef Q_OS_WIN
    // benannte Pipe; der Pipe-Namensraum ist für alle Sitzungen gemeinsam, daher je Benutzer
    QString benutzer = qEnvironmentVariable("USERNAME");
    if (benutzer.isEmpty()) benutzer = QStringLiteral("unbekannt");
    return QStringLiteral("schneefall-") + benutzer;
#else
    // Laufzeitordner des Benutzers (/run/user/<uid>, nur für ihn zugänglich), nicht das gemeinsame /tmp
    QString ordner = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (ordner.isEmpty()) ordner = QDir::homePath();
    return ordner + QStringLiteral("/schneefall.sock");
#endif
}

bool Steuerkanal::anmelden() {
    if (m_impl) return true;
    if (instanzAntwortet(m_name)) return false;
    auto impl = std::make_unique<Impl>();
    impl->server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!impl->server.listen(m_name)) {
        // verwaister Socket einer abgestürzten Instanz (niemand antwortet, s. o.): ersetzen
        if (impl->server.serverError() != QAbstractSocket::AddressInUseError) return false;
        QLocalServer::removeServer(m_name);
        if (!impl->server.listen(m_name)) return false;
    }
    QLocalServer *server = &impl->server;
    QObject::connect(server, &QLocalServer::newConnection, server, [this, server] {
        while (QLocalSocket *s = server->nextPendingConnection()) {
            QObject::connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
            QObject::connect(s, &QLocalSocket::readyRead, s, [this, s] {
                if (!s->canReadLine()) {
                    if (s->bytesAvailable() > MAX_ZEILE) s->abort();
                    return;
                }
                const QByteArray zeile = s->readLine(MAX_ZEILE);
                const QJsonObject o = QJsonDocument::fromJson(zeile).object();
                const QString befehl = o.value(QStringLiteral("befehl")).toString();
                QStringList argumente;
                for (const QJsonValue &v : o.value(QStringLiteral("argumente")).toArray()) argumente << v.toString();
                // das Ziel darf das Programm beenden o. Ä.: Socket über QPointer absichern
                QPointer<QLocalSocket> sicher(s);
                const Antwort a = befehl.isEmpty() || !m_ziel ? Antwort::unbekannt() : m_ziel(befehl, argumente);
                if (!sicher) return;
                s->write(antwortText(a));
                s->flush();
                s->disconnectFromServer();
            });
        }
    });
    m_impl = std::move(impl);
    return true;
}

Antwort Steuerkanal::senden(const QString &befehl, const QStringList &argumente, const QString &name, int ms) {
    QLocalSocket s;
    s.connectToServer(name);
    if (!s.waitForConnected(ms)) return {};
    s.write(anfrageText(befehl, argumente));
    if (!s.waitForBytesWritten(ms)) return {};
    QByteArray zeile;
    while (!zeile.endsWith('\n')) {
        if (!s.bytesAvailable() && !s.waitForReadyRead(ms)) break;
        zeile += s.readAll();
        if (zeile.size() > 64 * 1024 * 1024) break;
    }
    const QJsonObject o = QJsonDocument::fromJson(zeile).object();
    const QString ergebnis = o.value(QStringLiteral("ergebnis")).toString();
    if (ergebnis == QLatin1String("ok")) {
        Antwort a = Antwort::ok();
        if (o.contains(QStringLiteral("text"))) a.text = o.value(QStringLiteral("text")).toString();
        return a;
    }
    if (ergebnis == QLatin1String("unbekannt")) return Antwort::unbekannt();
    return {};
}

} // namespace plattform
