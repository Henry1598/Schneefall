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
#include <QSysInfo>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef Q_OS_WIN
#include <algorithm>
#include <string>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <thread>
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
// Der Pipe-Namensraum ist für alle Benutzer und Sitzungen gemeinsam. Deshalb legt Schneefall die Pipe selbst an
// (statt QLocalServer): erste Instanz mit FILE_FLAG_FIRST_PIPE_INSTANCE, Besitzer ausdrücklich der eigene Benutzer,
// DACL nur für ihn, PIPE_REJECT_REMOTE_CLIENTS. Hat ein fremder Prozess den Namen vorher belegt, scheitert das mit
// ERROR_ACCESS_DENIED – dann startet Schneefall nicht (statt Befehle an den Fremden zu schicken). Der Client prüft
// den Besitzer des Pipe-Objekts, der Server je Verbindung den Benutzer des Clients.
struct Griff {
    HANDLE h = nullptr;
    ~Griff() {
        if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
};

// SID des Benutzers eines Tokens (Bytes); leer bei Fehler
std::vector<BYTE> tokenSid(HANDLE token) {
    DWORD laenge = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &laenge);
    if (laenge == 0) return {};
    std::vector<BYTE> puffer(laenge);
    if (!GetTokenInformation(token, TokenUser, puffer.data(), laenge, &laenge)) return {};
    PSID sid = reinterpret_cast<const TOKEN_USER *>(puffer.data())->User.Sid;
    if (!IsValidSid(sid)) return {};
    const BYTE *b = static_cast<const BYTE *>(sid);
    return std::vector<BYTE>(b, b + GetLengthSid(sid));
}

// SID des Benutzers, dem der eigene Prozess gehört
const std::vector<BYTE> &eigeneSidBytes() {
    static const std::vector<BYTE> sid = [] {
        Griff token;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.h)) return std::vector<BYTE>{};
        return tokenSid(token.h);
    }();
    return sid;
}

bool istEigeneSid(PSID sid) {
    const auto &eigen = eigeneSidBytes();
    return sid && !eigen.empty() && IsValidSid(sid) && EqualSid(sid, const_cast<BYTE *>(eigen.data()));
}

QString eigeneSid() {
    static const QString text = [] {
        auto sid = eigeneSidBytes();
        if (sid.empty()) return QString();
        LPWSTR t = nullptr;
        if (!ConvertSidToStringSidW(sid.data(), &t)) return QString();
        const QString s = QString::fromWCharArray(t);
        LocalFree(t);
        return s;
    }();
    return text;
}

// läuft der eigene Prozess mit Administratorrechten (UAC-erhöht)?
bool istErhoeht() {
    Griff token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.h)) return false;
    TOKEN_ELEVATION e{};
    DWORD n = 0;
    return GetTokenInformation(token.h, TokenElevation, &e, sizeof e, &n) && e.TokenIsElevated;
}

// erhöht über UAC, d. h. es gibt daneben ein normales (gefiltertes) Token desselben Benutzers – nur dann laufen
// normale Konsolen mit niedrigerer Integritätsstufe (bei abgeschalteter UAC ist alles gleich hoch)
bool istUacErhoeht() {
    Griff token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.h)) return false;
    TOKEN_ELEVATION_TYPE t{};
    DWORD n = 0;
    return GetTokenInformation(token.h, TokenElevationType, &t, sizeof t, &n) && t == TokenElevationTypeFull;
}

// Integritätsstufe des eigenen Prozesses (Label-SID, z. B. S-1-16-12288 für Hoch); leer bei Fehler
std::vector<BYTE> eigeneIntegritaetSid(DWORD *rid = nullptr) {
    Griff token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.h)) return {};
    DWORD laenge = 0;
    GetTokenInformation(token.h, TokenIntegrityLevel, nullptr, 0, &laenge);
    std::vector<BYTE> puffer(laenge);
    if (laenge == 0 || !GetTokenInformation(token.h, TokenIntegrityLevel, puffer.data(), laenge, &laenge)) return {};
    PSID sid = reinterpret_cast<TOKEN_MANDATORY_LABEL *>(puffer.data())->Label.Sid;
    if (!IsValidSid(sid)) return {};
    const UCHAR anzahl = *GetSidSubAuthorityCount(sid);
    if (anzahl == 0) return {};
    if (rid) *rid = *GetSidSubAuthority(sid, anzahl - 1);
    const auto *b = static_cast<const BYTE *>(sid);
    return std::vector<BYTE>(b, b + GetLengthSid(sid));
}

// Sicherheitsbeschreibung der Pipe: Besitzer und einziger Berechtigter ist der eigene Benutzer. Läuft der Prozess
// über Mittel (erhöht), bekommt die Pipe zusätzlich seine Integritätsstufe als Kennzeichnung mit „kein Schreiben/Lesen
// von unten“ (R3): sonst könnte ein normal laufender Prozess desselben Benutzers die erhöhte Instanz fernsteuern –
// ohne Kennzeichnung gilt ein Objekt als Mittel.
struct PipeSicherheit {
    std::vector<BYTE> sid, acl, stufe, sacl;
    SECURITY_DESCRIPTOR sd{};
    SECURITY_ATTRIBUTES sa{};
    bool ok = false;
    PipeSicherheit() {
        sid = eigeneSidBytes();
        if (sid.empty()) return;
        acl.resize(sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + sid.size());
        PACL dacl = reinterpret_cast<PACL>(acl.data());
        ok = InitializeAcl(dacl, DWORD(acl.size()), ACL_REVISION) && AddAccessAllowedAce(dacl, ACL_REVISION, FILE_ALL_ACCESS, sid.data())
             && InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) && SetSecurityDescriptorOwner(&sd, sid.data(), FALSE)
             && SetSecurityDescriptorDacl(&sd, TRUE, dacl, FALSE);
        // Kennzeichnung immer (auch bei Mittel ausdrücklich); ohne ermittelbare Stufe keine Pipe (geschlossen scheitern)
        stufe = eigeneIntegritaetSid();
        if (stufe.empty()) ok = false;
        if (ok) {
            sacl.resize(sizeof(ACL) + sizeof(SYSTEM_MANDATORY_LABEL_ACE) + stufe.size());
            PACL s = reinterpret_cast<PACL>(sacl.data());
            ok = InitializeAcl(s, DWORD(sacl.size()), ACL_REVISION)
                 && AddMandatoryAce(s, ACL_REVISION, 0,
                                    SYSTEM_MANDATORY_LABEL_NO_WRITE_UP | SYSTEM_MANDATORY_LABEL_NO_READ_UP | SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP,
                                    stufe.data())
                 && SetSecurityDescriptorSacl(&sd, TRUE, s, FALSE);
        }
        sa.nLength = sizeof sa;
        sa.lpSecurityDescriptor = &sd;
        sa.bInheritHandle = FALSE;
    }
};

// Text zu einem Windows-Fehler (für Meldungen)
QString fehlerText(DWORD code) {
    wchar_t *t = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
                   reinterpret_cast<LPWSTR>(&t), 0, nullptr);
    QString s = t ? QString::fromWCharArray(t).trimmed() : QString();
    if (t) LocalFree(t);
    return QStringLiteral("%1 (Fehler %2)").arg(s).arg(code);
}

constexpr DWORD PIPE_PUFFER = 64 * 1024;

// Server: eine wartende Pipe-Instanz nach der anderen; verbundene Instanzen gehen an einen QLocalSocket
class PipeServer : public QObject {
public:
    enum class Start { Ok, FremdBelegt, Fehler };
    std::function<void(QLocalSocket *)> neueVerbindung;

    ~PipeServer() override {
        if (m_waechter.joinable()) {
            SetEvent(m_stopp);
            m_waechter.join();
        }
        if (m_stopp) CloseHandle(m_stopp);
        if (m_weiter) CloseHandle(m_weiter);
        if (m_pipe != INVALID_HANDLE_VALUE) {
            CancelIoEx(m_pipe, &m_ov);
            DWORD n = 0;
            GetOverlappedResult(m_pipe, &m_ov, &n, TRUE); // ausstehendes ConnectNamedPipe abschließen, bevor m_ov verschwindet
            CloseHandle(m_pipe);
        }
        if (m_ov.hEvent) CloseHandle(m_ov.hEvent);
    }

    Start lauschen(const QString &name, QString &fehler) {
        if (!m_sicherheit.ok) {
            fehler = QStringLiteral("Sicherheitsbeschreibung für die Pipe nicht erstellbar");
            return Start::Fehler;
        }
        m_pfad = (QStringLiteral("\\\\.\\pipe\\") + name).toStdWString();
        DWORD code = 0;
        if (!instanzAnlegen(true, code)) {
            if (code == ERROR_ACCESS_DENIED || code == ERROR_PIPE_BUSY) {
                fehler = QStringLiteral("Pipe %1 von fremdem Prozess belegt").arg(name);
                if (code == ERROR_ACCESS_DENIED && !istErhoeht())
                    fehler += QStringLiteral(" (oder Schneefall läuft bereits mit Administratorrechten)");
                return Start::FremdBelegt;
            }
            fehler = QStringLiteral("Pipe %1: %2").arg(name, fehlerText(code));
            return Start::Fehler;
        }
        m_ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        m_stopp = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        m_weiter = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!m_ov.hEvent || !m_stopp || !m_weiter) {
            fehler = QStringLiteral("Pipe %1: kein Ereignisobjekt").arg(name);
            return Start::Fehler;
        }
        warten();
        // Wächter-Thread: wartet auf „Client verbunden“ und meldet es dem Thread des Kanals; danach wartet er, bis
        // signalisiert() die nächste Instanz bereitgestellt hat. (QWinEventNotifier verlor hier unter Last Meldungen.)
        m_waechter = std::thread([this] {
            for (;;) {
                const HANDLE verbunden[2] = {m_stopp, m_ov.hEvent};
                if (WaitForMultipleObjects(2, verbunden, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) return;
                QMetaObject::invokeMethod(this, [this] { signalisiert(); }, Qt::QueuedConnection);
                const HANDLE weiter[2] = {m_stopp, m_weiter};
                if (WaitForMultipleObjects(2, weiter, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) return;
            }
        });
        return Start::Ok;
    }

private:
    bool instanzAnlegen(bool erste, DWORD &code) {
        const DWORD art = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (erste ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0);
        m_pipe = CreateNamedPipeW(m_pfad.c_str(), art, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                  PIPE_UNLIMITED_INSTANCES, PIPE_PUFFER, PIPE_PUFFER, 0, &m_sicherheit.sa);
        code = m_pipe == INVALID_HANDLE_VALUE ? GetLastError() : 0;
        if (m_pipe != INVALID_HANDLE_VALUE && !erste && !gehoertUns(m_pipe)) {
            // weitere Instanz auf einer Pipe, die nicht (mehr) uns gehört – nicht verwenden
            CloseHandle(m_pipe);
            m_pipe = INVALID_HANDLE_VALUE;
            code = ERROR_ACCESS_DENIED;
            qWarning("Steuerkanal: Pipe gehört einem fremden Benutzer – keine weitere Instanz");
        }
        return m_pipe != INVALID_HANDLE_VALUE;
    }

    static bool gehoertUns(HANDLE h) {
        PSID besitzer = nullptr;
        PSECURITY_DESCRIPTOR sd = nullptr;
        const bool ja = GetSecurityInfo(h, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION, &besitzer, nullptr, nullptr, nullptr, &sd) == ERROR_SUCCESS
                        && istEigeneSid(besitzer);
        if (sd) LocalFree(sd);
        return ja;
    }

    // auf den nächsten Client warten (überlappt; das Ereignis meldet der Wächter-Thread)
    void warten() {
        if (m_pipe == INVALID_HANDLE_VALUE) {
            DWORD code = 0;
            if (!instanzAnlegen(false, code)) {
                ResetEvent(m_ov.hEvent);
                QTimer::singleShot(200, this, [this] { warten(); }); // z. B. Ressourcen knapp: später erneut
                return;
            }
        }
        ResetEvent(m_ov.hEvent);
        m_sofort = false;
        if (ConnectNamedPipe(m_pipe, &m_ov)) {
            m_sofort = true;
            SetEvent(m_ov.hEvent);
        } else {
            const DWORD code = GetLastError();
            // Client war schneller als ConnectNamedPipe: verbunden, ohne dass die überlappte Operation läuft
            if (code == ERROR_PIPE_CONNECTED) {
                m_sofort = true;
                SetEvent(m_ov.hEvent);
            }
            else if (code != ERROR_IO_PENDING) {
                // z. B. ERROR_NO_DATA (Client war schon wieder weg): Instanz trennen und behalten – den eigenen Griff nie
                // schließen, sonst wäre der Name kurz frei und ein Fremder könnte ihn belegen
                ResetEvent(m_ov.hEvent);
                DisconnectNamedPipe(m_pipe);
                QTimer::singleShot(code == ERROR_NO_DATA ? 0 : 200, this, [this] { warten(); });
                return;
            }
        }
    }

    // im Thread des Kanals; gibt danach den Wächter wieder frei
    void signalisiert() {
        annehmen();
        SetEvent(m_weiter);
    }

    void annehmen() {
        if (m_pipe == INVALID_HANDLE_VALUE || WaitForSingleObject(m_ov.hEvent, 0) != WAIT_OBJECT_0) return;
        DWORD n = 0;
        const bool verbunden = m_sofort || GetOverlappedResult(m_pipe, &m_ov, &n, FALSE);
        m_sofort = false;
        if (!verbunden) {
            DisconnectNamedPipe(m_pipe); // Client schon wieder weg: Instanz wiederverwenden
            warten();
            return;
        }
        const HANDLE h = m_pipe;
        m_pipe = INVALID_HANDLE_VALUE;
        warten(); // gleich die nächste Instanz bereitstellen
        auto *s = new QLocalSocket(this);
        if (!s->setSocketDescriptor(qintptr(h), QLocalSocket::ConnectedState, QIODevice::ReadWrite)) {
            DisconnectNamedPipe(h);
            CloseHandle(h);
            delete s;
            return;
        }
        if (neueVerbindung) neueVerbindung(s);
        else s->abort();
    }

    PipeSicherheit m_sicherheit;
    std::wstring m_pfad;
    HANDLE m_pipe = INVALID_HANDLE_VALUE;
    OVERLAPPED m_ov{};
    bool m_sofort = false; // ConnectNamedPipe meldete „schon verbunden“ (m_ov gilt dann nicht)
    HANDLE m_stopp = nullptr, m_weiter = nullptr;
    std::thread m_waechter;
};

// Server, je Verbindung (Zusatzschutz zur DACL): gehört der Client dem eigenen Benutzer? Zuerst über Identifizieren
// (der Client öffnet mit SECURITY_IDENTIFICATION, die Anfrage ist gelesen).
bool clientIstEigen(QLocalSocket &s) {
    const HANDLE pipe = reinterpret_cast<HANDLE>(s.socketDescriptor());
    if (!pipe || pipe == INVALID_HANDLE_VALUE) return false;
    // kein Rückfall auf die Prozess-ID (nicht fälschungssicher): wer nicht identifizierbar ist, wird abgewiesen
    if (!ImpersonateNamedPipeClient(pipe)) return false;
    Griff token;
    const bool offen = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token.h);
    if (!RevertToSelf()) TerminateProcess(GetCurrentProcess(), 1); // nie unter fremdem Token weiterlaufen
    if (!offen) return false;
    auto sid = tokenSid(token.h);
    return !sid.empty() && istEigeneSid(sid.data());
}

// Client: Pipe nur mit Identifikationsrecht öffnen (ein fremder Server kann sich nicht als dieser Benutzer ausgeben)
// und prüfen, dass das Pipe-Objekt dem eigenen Benutzer gehört (R2: über den Besitzer, nicht über die PID – ein
// fremder Benutzer kann den Besitzer nicht auf unsere SID setzen). Bei Erfolg übernimmt der QLocalSocket den Griff.
// fehler: Grund, wenn keine Verbindung zustande kam und es mehr als „läuft nicht“ ist.
bool sicherVerbinden(QLocalSocket &s, const QString &name, int ms, QString *fehler = nullptr) {
    const std::wstring pfad = (QStringLiteral("\\\\.\\pipe\\") + name).toStdWString();
    const QDeadlineTimer frist(ms);
    HANDLE h = INVALID_HANDLE_VALUE;
    for (;;) {
        h = CreateFileW(pfad.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
        if (h != INVALID_HANDLE_VALUE) break;
        const DWORD code = GetLastError();
        if (code == ERROR_ACCESS_DENIED) {
            if (fehler)
                *fehler = istErhoeht() ? QStringLiteral("Zugriff auf den Steuerkanal verweigert: Die Pipe gehört einem fremden Prozess.")
                                       : QStringLiteral("Zugriff auf den Steuerkanal verweigert: Schneefall läuft wohl mit "
                                                        "Administratorrechten (dann den Befehl ebenfalls als Administrator "
                                                        "ausführen oder Schneefall normal starten) – oder ein fremder Prozess "
                                                        "belegt die Pipe.");
            return false;
        }
        if (code != ERROR_PIPE_BUSY || frist.hasExpired()) return false;
        if (!WaitNamedPipeW(pfad.c_str(), DWORD(std::max<qint64>(1, frist.remainingTime())))) return false;
    }
    PSID besitzer = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    const bool vertraut = GetSecurityInfo(h, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION, &besitzer, nullptr, nullptr, nullptr, &sd)
                              == ERROR_SUCCESS
                          && istEigeneSid(besitzer);
    if (sd) LocalFree(sd);
    if (!vertraut) {
        CloseHandle(h);
        if (fehler) *fehler = QStringLiteral("Die Pipe des Steuerkanals gehört einem fremden Benutzer – keine Befehle gesendet.");
        return false;
    }
    if (!s.setSocketDescriptor(qintptr(h), QLocalSocket::ConnectedState, QIODevice::ReadWrite)) {
        CloseHandle(h);
        return false;
    }
    return true;
}
#endif

// Server: Verbindung sofort hart trennen. Unter Windows zuerst DisconnectNamedPipe – sonst merkt der Client das
// Schließen der Server-Instanz nicht zuverlässig.
void trennen(QLocalSocket *s) {
#ifdef Q_OS_WIN
    const HANDLE h = reinterpret_cast<HANDLE>(s->socketDescriptor());
    if (h && h != INVALID_HANDLE_VALUE) DisconnectNamedPipe(h);
#endif
    s->abort();
}

// Client-Verbindung (Windows: abgesichert, s. o.)
bool verbinden(QLocalSocket &s, const QString &name, int ms, QString *fehler = nullptr) {
#ifdef Q_OS_WIN
    return sicherVerbinden(s, name, ms, fehler);
#else
    Q_UNUSED(fehler)
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
    // über Codepunkte (auch Formatzeichen außerhalb der BMP, z. B. Tag-Zeichen U+E0001 …)
    QList<uint> punkte = text.toUcs4();
    for (uint &c : punkte) {
        if (c == '\n' || c == '\t') continue;
        // Steuerzeichen, Format-/Bidi-Steuerung (U+202E, U+2066 …), Zeilen- und Absatztrenner, einzelne Surrogate
        const QChar::Category k = QChar::category(char32_t(c));
        if (k == QChar::Other_Control || k == QChar::Other_Format || k == QChar::Separator_Line || k == QChar::Separator_Paragraph
            || k == QChar::Other_Surrogate)
            c = '?';
    }
    return QString::fromUcs4(reinterpret_cast<const char32_t *>(punkte.constData()), punkte.size());
}

#ifdef Q_OS_UNIX
QString privaterOrdner(const QString &basis) {
    const QString ordner = basis + QStringLiteral("/schneefall-") + QString::number(::getuid());
    const QByteArray pfad = QFile::encodeName(ordner);
    if (::mkdir(pfad.constData(), 0700) != 0 && errno != EEXIST) return {};
    struct stat st {};
    if (::lstat(pfad.constData(), &st) != 0) return {};
    // kein Link, wirklich ein Ordner, uns gehörend und nur für uns zugänglich – sonst könnte ein anderer
    // Benutzer ihn vorab angelegt haben
    if (!S_ISDIR(st.st_mode) || st.st_uid != ::getuid() || (st.st_mode & 077) != 0) return {};
    return ordner;
}
#endif

struct Steuerkanal::Impl {
    QLockFile sperre;
#ifdef Q_OS_WIN
    PipeServer server; // eigene Pipe mit DACL (s. o.)
#else
    QLocalServer server;
#endif
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
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &sitzung)) return QString();
    const QString sid = eigeneSid();
    if (sid.isEmpty()) return QString(); // kein Rückfall auf den (nicht eindeutigen) Benutzernamen: anmelden() meldet Fehler
    return QStringLiteral("schneefall-%1-%2").arg(sid).arg(quint32(sitzung));
#else
    // Laufzeitordner des Benutzers (/run/user/<uid>, nur für ihn zugänglich), nicht das gemeinsame /tmp
    QString ordner = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (ordner.isEmpty()) ordner = privaterOrdner(QDir::tempPath()); // eigener 0700-Ordner unter /tmp
    if (!ordner.isEmpty()) return ordner + QStringLiteral("/schneefall.sock");
    // zuletzt das Home-Verzeichnis – je Rechner, falls es (NFS) mehrere Rechner teilen
    return QDir::homePath() + QStringLiteral("/.schneefall-") + QSysInfo::machineHostName() + QStringLiteral(".sock");
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
    if (m_name.isEmpty()) {
        m_fehler = QStringLiteral("kein Name für den Steuerkanal (Benutzer nicht ermittelbar)");
        return Anmeldung::Fehler;
    }
    auto impl = std::make_unique<Impl>(sperrPfad(m_name));
    // Sperre: nur verwaist, wenn der haltende Prozess nicht mehr läuft (kein Ablauf nach Zeit – eine laufende
    // Instanz hält sie beliebig lange)
#ifdef Q_OS_WIN
    // Über UAC erhöht keine Sperrdatei im Benutzerprofil (den Ordner kontrolliert jeder normale Prozess – Junction-
    // Angriffe auf Anlegen/Löschen mit Adminrechten). Die Einzelinstanz sichert dann FILE_FLAG_FIRST_PIPE_INSTANCE.
    const bool mitSperre = !istUacErhoeht();
#else
    const bool mitSperre = true;
#endif
    if (mitSperre) QDir().mkpath(QFileInfo(sperrPfad(m_name)).absolutePath());
    impl->sperre.setStaleLockTime(0);
    if (mitSperre && !impl->sperre.tryLock(0)) {
        if (impl->sperre.error() == QLockFile::LockFailedError) return Anmeldung::LaeuftSchon;
        m_fehler = QStringLiteral("Sperrdatei %1 lässt sich nicht anlegen").arg(sperrPfad(m_name));
        return Anmeldung::Fehler;
    }
    // ältere Version ohne Sperre läuft noch?
    if (instanzAntwortet(m_name)) return Anmeldung::LaeuftSchon;
    Impl *ip = impl.get();
    QObject *besitzer = &impl->server;
    // eine neue Verbindung: begrenzen, Frist setzen, genau eine Zeile lesen, Antwort schreiben
    auto annehmen = [this, ip, besitzer](QLocalSocket *s) {
        if (ip->offen >= MAX_VERBINDUNGEN) {
            trennen(s);
            s->deleteLater();
            return;
        }
        ++ip->offen;
        s->setReadBufferSize(MAX_ZEILE + 1); // Qt liest sonst unbegrenzt vor, bevor readyRead die Grenze prüft
        QObject::connect(s, &QObject::destroyed, besitzer, [ip] { --ip->offen; });
        QObject::connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
        // jede Verbindung höchstens VERBINDUNG_MS lang (auch wer nichts oder keine ganze Zeile schickt)
        QTimer::singleShot(VERBINDUNG_MS, s, [s] {
            trennen(s);
            s->deleteLater();
        });
        QObject::connect(s, &QLocalSocket::readyRead, s, [this, s] {
            if (!s->canReadLine()) {
                if (s->bytesAvailable() > MAX_ZEILE) trennen(s);
                return;
            }
            // ein Befehl je Verbindung: weitere Zeilen werden nicht mehr gelesen
            QObject::disconnect(s, &QLocalSocket::readyRead, s, nullptr);
#ifdef Q_OS_WIN
            // Zusatzschutz zur DACL: nur der eigene Benutzer (die Anfrage ist gelesen, Identifizieren geht jetzt)
            if (!clientIstEigen(*s)) {
                trennen(s);
                return;
            }
#endif
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
    };
#ifdef Q_OS_WIN
    impl->server.neueVerbindung = annehmen;
    QString fehler;
    if (impl->server.lauschen(m_name, fehler) != PipeServer::Start::Ok) {
        if (!mitSperre && instanzAntwortet(m_name)) return Anmeldung::LaeuftSchon; // eigene Instanz (geprüfter Besitzer)
        m_fehler = fehler;
        return Anmeldung::Fehler;
    }
    // R3: über UAC erhöht gestartet – Befehle aus einer normalen Konsole erreichen diese Instanz nicht (die Pipe trägt
    // die hohe Integritätsstufe, siehe PipeSicherheit). Bei abgeschalteter UAC läuft ohnehin alles gleich hoch.
    if (istUacErhoeht())
        m_hinweis = QStringLiteral("Schneefall läuft mit Administratorrechten. Befehle wie --status aus einer normalen "
                                   "Konsole erreichen diese Instanz nicht; besser ohne Administratorrechte starten.");
#else
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
    QLocalServer *server = &impl->server;
    QObject::connect(server, &QLocalServer::newConnection, server, [server, annehmen] {
        while (QLocalSocket *s = server->nextPendingConnection()) annehmen(s);
    });
#endif
    m_impl = std::move(impl);
    return Anmeldung::Ok;
}

Antwort Steuerkanal::senden(const QString &befehl, const QStringList &argumente, const QString &name, int ms) {
    if (name.isEmpty()) return {};
    // eine Frist für den ganzen Aufruf (ein tröpfelnder Server hält den Client nicht fest)
    const QDeadlineTimer frist(ms);
    auto rest = [&frist] { return int(std::max<qint64>(0, frist.remainingTime())); };
    QLocalSocket s;
    s.setReadBufferSize(MAX_ANTWORT + 1); // Qt liest sonst unbegrenzt vor
    QString fehler;
    if (!verbinden(s, name, rest(), &fehler)) {
        Antwort a; // NichtErreicht, mit Grund, wenn es mehr ist als „läuft nicht“
        if (!fehler.isEmpty()) a.text = fehler;
        return a;
    }
    s.write(anfrageText(befehl, argumente));
    if (!s.waitForBytesWritten(rest())) return {};
    QByteArray zeile;
    while (!zeile.endsWith('\n')) {
        if (frist.hasExpired()) return {};
        if (!s.bytesAvailable() && !s.waitForReadyRead(rest())) break;
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
