#include "plattform/steuerkanal.h"
#include <QDeadlineTimer>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>

#ifdef Q_OS_WIN
#include <algorithm>
#include <string>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sddl.h>
#endif

namespace plattform {

namespace {
constexpr qint64 MAX_ZEILE = 64 * 1024;          // längere Anfragen sind kein gültiger Befehl
constexpr int VERBINDUNG_MS = 2000;              // so lange darf eine Verbindung offen bleiben
constexpr int MAX_VERBINDUNGEN = 16;             // gleichzeitig offene Verbindungen
constexpr qsizetype MAX_ANTWORT = 4 * 1024 * 1024; // Client: größere Antworten werden verworfen

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

#ifdef Q_OS_WIN
// ---- Windows: benannte Pipe absichern ----
// Kann unter Linux nicht übersetzt werden; wird im Windows-Backend-Schritt in der VM gebaut und geprüft.
struct Griff {
    HANDLE h = nullptr;
    ~Griff() {
        if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
};

// SID des Benutzers, dem der Prozess gehört, als Text („S-1-5-21-…“); leer bei Fehler
QString benutzerSid(HANDLE prozess) {
    Griff token;
    if (!OpenProcessToken(prozess, TOKEN_QUERY, &token.h)) return {};
    DWORD laenge = 0;
    GetTokenInformation(token.h, TokenUser, nullptr, 0, &laenge);
    if (laenge == 0) return {};
    std::vector<BYTE> puffer(laenge);
    if (!GetTokenInformation(token.h, TokenUser, puffer.data(), laenge, &laenge)) return {};
    LPWSTR text = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<const TOKEN_USER *>(puffer.data())->User.Sid, &text)) return {};
    const QString sid = QString::fromWCharArray(text);
    LocalFree(text);
    return sid;
}

QString eigeneSid() {
    static const QString sid = benutzerSid(GetCurrentProcess());
    return sid;
}

// Client: Pipe nur mit Identifikationsrecht öffnen (ein fremder Server kann sich nicht als dieser Benutzer
// ausgeben) und prüfen, dass der Server-Prozess demselben Benutzer gehört (gegen eine vorab von einem anderen
// Benutzer belegte Pipe). Bei Erfolg übernimmt der QLocalSocket den Griff.
bool sicherVerbinden(QLocalSocket &s, const QString &name, int ms) {
    const std::wstring pfad = (QStringLiteral("\\\\.\\pipe\\") + name).toStdWString();
    const QDeadlineTimer frist(ms);
    HANDLE h = INVALID_HANDLE_VALUE;
    for (;;) {
        h = CreateFileW(pfad.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
        if (h != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_PIPE_BUSY || frist.hasExpired()) return false;
        if (!WaitNamedPipeW(pfad.c_str(), DWORD(std::max<qint64>(1, frist.remainingTime())))) return false;
    }
    ULONG pid = 0;
    bool vertraut = GetNamedPipeServerProcessId(h, &pid) != 0;
    if (vertraut) {
        Griff prozess;
        prozess.h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        vertraut = prozess.h && !eigeneSid().isEmpty() && benutzerSid(prozess.h) == eigeneSid();
    }
    if (!vertraut || !s.setSocketDescriptor(qintptr(h), QLocalSocket::ConnectedState, QIODevice::ReadWrite)) {
        CloseHandle(h);
        return false;
    }
    return true;
}
// TODO (Windows-Backend, in der VM): die Server-Pipe selbst anlegen – CreateNamedPipeW mit
// FILE_FLAG_FIRST_PIPE_INSTANCE | PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_REJECT_REMOTE_CLIENTS und einer DACL
// nur für den eigenen Benutzer – und an QLocalServer übergeben (QLocalServer::listen(qintptr)). Prüfen, ob Qt das
// unter Windows unterstützt; sonst bleibt QLocalServer::listen(name) mit UserAccessOption. Bis dahin schützen die
// Sperrdatei (Einzelinstanz), der Name aus SID + Sitzung und die Server-Prüfung im Client (sicherVerbinden).
#endif

// Client-Verbindung (Windows: abgesichert, s. o.)
bool verbinden(QLocalSocket &s, const QString &name, int ms) {
#ifdef Q_OS_WIN
    return sicherVerbinden(s, name, ms);
#else
    s.connectToServer(name);
    return s.waitForConnected(ms);
#endif
}

// läuft unter dem Namen eine Instanz, die Verbindungen annimmt?
bool instanzAntwortet(const QString &name) {
    QLocalSocket s;
    const bool da = verbinden(s, name, 500);
    s.abort();
    return da;
}
} // namespace

QString terminalText(const QString &text) {
    QString t = text;
    for (QChar &c : t)
        if (c.category() == QChar::Other_Control && c != QLatin1Char('\n') && c != QLatin1Char('\t')) c = QLatin1Char('?');
    return t;
}

struct Steuerkanal::Impl {
    QLockFile sperre;
    QLocalServer server;
    int offen = 0; // offene Verbindungen
    explicit Impl(const QString &sperrPfad) : sperre(sperrPfad) {}
};

Steuerkanal::Steuerkanal(Befehlsziel ziel, QString name) : m_ziel(std::move(ziel)), m_name(std::move(name)) {}

Steuerkanal::~Steuerkanal() = default;

QString Steuerkanal::standardName() {
#ifdef Q_OS_WIN
    // Pipe-Namensraum ist für alle Benutzer und Sitzungen gemeinsam: Name aus Benutzer-SID und Sitzungsnummer
    // (der Benutzername ist nicht eindeutig, z. B. lokales Konto und Domänenkonto gleichen Namens)
    DWORD sitzung = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sitzung);
    QString sid = eigeneSid();
    if (sid.isEmpty()) sid = qEnvironmentVariable("USERNAME", QStringLiteral("unbekannt"));
    return QStringLiteral("schneefall-%1-%2").arg(sid).arg(quint32(sitzung));
#else
    // Laufzeitordner des Benutzers (/run/user/<uid>, nur für ihn zugänglich), nicht das gemeinsame /tmp
    QString ordner = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (ordner.isEmpty()) ordner = QDir::homePath();
    return ordner + QStringLiteral("/schneefall.sock");
#endif
}

QString Steuerkanal::sperrPfad(const QString &name) {
#ifdef Q_OS_WIN
    QString basis = qEnvironmentVariable("LOCALAPPDATA");
    if (basis.isEmpty()) basis = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return QDir::cleanPath(basis + QStringLiteral("/Schneefall/") + name + QStringLiteral(".lock"));
#else
    return name + QStringLiteral(".lock");
#endif
}

Steuerkanal::Anmeldung Steuerkanal::anmelden() {
    if (m_impl) return Anmeldung::Ok;
    m_fehler.clear();
    auto impl = std::make_unique<Impl>(sperrPfad(m_name));
    // Sperre: nur verwaist, wenn der haltende Prozess nicht mehr läuft (kein Ablauf nach Zeit – eine laufende
    // Instanz hält sie beliebig lange)
    QDir().mkpath(QFileInfo(sperrPfad(m_name)).absolutePath());
    impl->sperre.setStaleLockTime(0);
    if (!impl->sperre.tryLock(0)) {
        if (impl->sperre.error() == QLockFile::LockFailedError) return Anmeldung::LaeuftSchon;
        m_fehler = QStringLiteral("Sperrdatei %1 lässt sich nicht anlegen").arg(sperrPfad(m_name));
        return Anmeldung::Fehler;
    }
    // ältere Version ohne Sperre läuft noch?
    if (instanzAntwortet(m_name)) return Anmeldung::LaeuftSchon;
    impl->server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!impl->server.listen(m_name)) {
        // verwaister Socket einer abgestürzten Instanz: wir halten die Sperre und niemand antwortet – ersetzen
        if (impl->server.serverError() == QAbstractSocket::AddressInUseError) {
            QLocalServer::removeServer(m_name);
            impl->server.listen(m_name);
        }
        if (!impl->server.isListening()) {
            m_fehler = QStringLiteral("Steuerkanal %1: %2").arg(m_name, impl->server.errorString());
            return Anmeldung::Fehler;
        }
    }
    Impl *ip = impl.get();
    QLocalServer *server = &impl->server;
    QObject::connect(server, &QLocalServer::newConnection, server, [this, ip, server] {
        while (QLocalSocket *s = server->nextPendingConnection()) {
            if (ip->offen >= MAX_VERBINDUNGEN) {
                s->abort();
                s->deleteLater();
                continue;
            }
            ++ip->offen;
            QObject::connect(s, &QObject::destroyed, server, [ip] { --ip->offen; });
            QObject::connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
            // jede Verbindung höchstens VERBINDUNG_MS lang (auch wer nichts oder keine ganze Zeile schickt)
            QTimer::singleShot(VERBINDUNG_MS, s, [s] {
                s->abort();
                s->deleteLater();
            });
            QObject::connect(s, &QLocalSocket::readyRead, s, [this, s] {
                if (!s->canReadLine()) {
                    if (s->bytesAvailable() > MAX_ZEILE) s->abort();
                    return;
                }
                // ein Befehl je Verbindung: weitere Zeilen werden nicht mehr gelesen
                QObject::disconnect(s, &QLocalSocket::readyRead, s, nullptr);
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
    return Anmeldung::Ok;
}

Antwort Steuerkanal::senden(const QString &befehl, const QStringList &argumente, const QString &name, int ms) {
    QLocalSocket s;
    if (!verbinden(s, name, ms)) return {};
    s.write(anfrageText(befehl, argumente));
    if (!s.waitForBytesWritten(ms)) return {};
    QByteArray zeile;
    while (!zeile.endsWith('\n')) {
        if (!s.bytesAvailable() && !s.waitForReadyRead(ms)) break;
        zeile += s.read(MAX_ANTWORT + 1 - zeile.size());
        if (zeile.size() > MAX_ANTWORT) return {}; // zu groß: verwerfen
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
