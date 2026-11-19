// Windows-Steuerkanal, Sicherheitsprüfung R1–R3: eigene Pipe mit DACL nur für den eigenen Benutzer, Besitzer
// ausdrücklich der Benutzer, vorab belegte Pipe → klarer Fehler, Client prüft den Besitzer des Pipe-Objekts.
// Fremde Benutzer werden mit einer Pipe nachgebildet, deren Besitzer die Administratorgruppe ist (geht nur erhöht,
// sonst übersprungen); der echte Test mit einem zweiten Konto steht in der README (Windows-Backend).
#include <QtTest>
#include <QLocalSocket>
#include <QThread>
#include "plattform/steuerkanal.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>

using plattform::Antwort;
using plattform::Steuerkanal;

namespace {
struct Server {
    QThread thread;
    QObject kontext;
    std::unique_ptr<Steuerkanal> kanal;
    Steuerkanal::Anmeldung ergebnis = Steuerkanal::Anmeldung::Fehler;
    QString fehler;
    explicit Server(const QString &name) {
        kontext.moveToThread(&thread);
        thread.start();
        QMetaObject::invokeMethod(&kontext, [this, name] {
            kanal = std::make_unique<Steuerkanal>([](const QString &, const QStringList &) { return Antwort::ok(QStringLiteral("ja")); }, name);
            ergebnis = kanal->anmelden();
            fehler = kanal->fehlertext();
        }, Qt::BlockingQueuedConnection);
    }
    ~Server() {
        QMetaObject::invokeMethod(&kontext, [this] { kanal.reset(); }, Qt::BlockingQueuedConnection);
        thread.quit();
        thread.wait();
    }
};

QString sidText(PSID sid) {
    LPWSTR t = nullptr;
    if (!sid || !ConvertSidToStringSidW(sid, &t)) return {};
    const QString s = QString::fromWCharArray(t);
    LocalFree(t);
    return s;
}

QString eigeneSid() {
    HANDLE token = nullptr;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    DWORD n = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &n);
    QByteArray b(int(n), 0);
    GetTokenInformation(token, TokenUser, b.data(), n, &n);
    CloseHandle(token);
    return sidText(reinterpret_cast<TOKEN_USER *>(b.data())->User.Sid);
}

bool erhoeht() {
    HANDLE token = nullptr;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    TOKEN_ELEVATION e{};
    DWORD n = 0;
    GetTokenInformation(token, TokenElevation, &e, sizeof e, &n);
    CloseHandle(token);
    return e.TokenIsElevated;
}

DWORD eigeneIntegritaet() {
    HANDLE token = nullptr;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    DWORD n = 0;
    GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &n);
    QByteArray b(int(n), 0);
    GetTokenInformation(token, TokenIntegrityLevel, b.data(), n, &n);
    CloseHandle(token);
    PSID sid = reinterpret_cast<TOKEN_MANDATORY_LABEL *>(b.data())->Label.Sid;
    return *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1);
}

// Pipe wie ein „fremder“ Prozess: SDDL frei wählbar (z. B. Besitzer Administratoren, Zugriff für alle)
HANDLE fremdePipe(const QString &name, const QString &sddl, bool erste = true) {
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(reinterpret_cast<LPCWSTR>(sddl.utf16()), SDDL_REVISION_1, &sd, nullptr))
        return INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES sa{sizeof sa, sd, FALSE};
    const std::wstring pfad = (QStringLiteral("\\\\.\\pipe\\") + name).toStdWString();
    HANDLE h = CreateNamedPipeW(pfad.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (erste ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0),
                                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 4096, 4096, 0, &sa);
    LocalFree(sd);
    return h;
}
} // namespace

class TestPipeWin : public QObject {
    Q_OBJECT
private slots:
    void init() { ++m_nr; }

    // R1: Besitzer = eigener Benutzer, DACL genau ein Eintrag für ihn
    void pipeGehoertNurDemEigenenBenutzer() {
        Server s(name());
        QCOMPARE(s.ergebnis, Steuerkanal::Anmeldung::Ok);
        const std::wstring pfad = (QStringLiteral("\\\\.\\pipe\\") + name()).toStdWString();
        PSID besitzer = nullptr;
        PACL dacl = nullptr;
        PSECURITY_DESCRIPTOR sd = nullptr;
        // als Client öffnen (nur Lesen der Sicherheitsdaten), dann am Objekt nachsehen
        HANDLE h = CreateFileW(pfad.c_str(), READ_CONTROL, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        QVERIFY(h != INVALID_HANDLE_VALUE);
        QCOMPARE(GetSecurityInfo(h, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &besitzer, nullptr, &dacl,
                                 nullptr, &sd),
                 DWORD(ERROR_SUCCESS));
        CloseHandle(h);
        QCOMPARE(sidText(besitzer), eigeneSid());
        QVERIFY(dacl);
        QCOMPARE(int(dacl->AceCount), 1);
        void *ace = nullptr;
        QVERIFY(GetAce(dacl, 0, &ace));
        auto *erlaubt = static_cast<ACCESS_ALLOWED_ACE *>(ace);
        QCOMPARE(int(erlaubt->Header.AceType), int(ACCESS_ALLOWED_ACE_TYPE));
        QCOMPARE(sidText(&erlaubt->SidStart), eigeneSid());
        LocalFree(sd);
        QCOMPARE(Steuerkanal::senden(QStringLiteral("Status"), {}, name()).ergebnis, Antwort::Ergebnis::Ok);
    }

    // R3: läuft der Server über Mittel (erhöht), trägt die Pipe seine Integritätsstufe mit „kein Schreiben von unten“ –
    // ein normal (Mittel) gestarteter Prozess kann eine erhöhte Instanz dann nicht fernsteuern
    void pipeTraegtEigeneIntegritaet() {
        Server s(name());
        QCOMPARE(s.ergebnis, Steuerkanal::Anmeldung::Ok);
        const std::wstring pfad = (QStringLiteral("\\\\.\\pipe\\") + name()).toStdWString();
        HANDLE h = CreateFileW(pfad.c_str(), READ_CONTROL, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        QVERIFY(h != INVALID_HANDLE_VALUE);
        PACL sacl = nullptr;
        PSECURITY_DESCRIPTOR sd = nullptr;
        QCOMPARE(GetSecurityInfo(h, SE_KERNEL_OBJECT, LABEL_SECURITY_INFORMATION, nullptr, nullptr, nullptr, &sacl, &sd), DWORD(ERROR_SUCCESS));
        CloseHandle(h);
        DWORD stufe = SECURITY_MANDATORY_MEDIUM_RID; // ohne Kennzeichnung gilt Mittel
        DWORD maske = 0;
        for (int i = 0; sacl && i < int(sacl->AceCount); ++i) {
            void *ace = nullptr;
            if (!GetAce(sacl, DWORD(i), &ace) || static_cast<ACE_HEADER *>(ace)->AceType != SYSTEM_MANDATORY_LABEL_ACE_TYPE) continue;
            auto *l = static_cast<SYSTEM_MANDATORY_LABEL_ACE *>(ace);
            PSID sid = &l->SidStart;
            stufe = *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1);
            maske = l->Mask;
        }
        LocalFree(sd);
        const DWORD eigen = eigeneIntegritaet();
        // immer ausdrücklich gekennzeichnet (auch bei Mittel), nichts von unten: schreiben, lesen, ausführen
        QCOMPARE(stufe, eigen);
        QCOMPARE(maske & (SYSTEM_MANDATORY_LABEL_NO_WRITE_UP | SYSTEM_MANDATORY_LABEL_NO_READ_UP | SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP),
                 DWORD(SYSTEM_MANDATORY_LABEL_NO_WRITE_UP | SYSTEM_MANDATORY_LABEL_NO_READ_UP | SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP));
        QCOMPARE(Steuerkanal::senden(QStringLiteral("Status"), {}, name()).ergebnis, Antwort::Ergebnis::Ok);
    }

    // R1: Name vorab belegt (offene DACL, weitere Instanzen möglich) → Start scheitert mit klarer Meldung
    void vorabBelegtePipeIstEinFehler() {
        if (!erhoeht()) QSKIP("fremden Besitzer (Administratoren) nachbilden geht nur erhöht");
        HANDLE fremd = fremdePipe(name(), QStringLiteral("O:BAD:(A;;GA;;;WD)"));
        QVERIFY(fremd != INVALID_HANDLE_VALUE);
        {
            Server s(name());
            QCOMPARE(s.ergebnis, Steuerkanal::Anmeldung::Fehler);
            QVERIFY2(s.fehler.contains(QStringLiteral("fremdem Prozess belegt")), qPrintable(s.fehler));
        }
        CloseHandle(fremd);
    }

    // R2: Client verbindet sich nicht mit einer Pipe, die einem anderen Besitzer gehört, und schickt nichts
    void clientLehntFremdenBesitzerAb() {
        if (!erhoeht()) QSKIP("Besitzer Administratoren setzen geht nur erhöht");
        HANDLE fremd = fremdePipe(name(), QStringLiteral("O:BAD:(A;;GA;;;WD)"));
        QVERIFY2(fremd != INVALID_HANDLE_VALUE, "Pipe mit Besitzer Administratoren");
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        ConnectNamedPipe(fremd, &ov);
        const Antwort a = Steuerkanal::senden(QStringLiteral("Beenden"), {}, name(), 1500);
        QCOMPARE(a.ergebnis, Antwort::Ergebnis::NichtErreicht);
        QVERIFY(a.text && a.text->contains(QStringLiteral("fremden Benutzer")));
        // beim „Fremden“ ist nichts angekommen
        DWORD verfuegbar = 0;
        PeekNamedPipe(fremd, nullptr, 0, nullptr, &verfuegbar, nullptr);
        QCOMPARE(verfuegbar, DWORD(0));
        CloseHandle(ov.hEvent);
        CloseHandle(fremd);
    }

    // die eigene Instanz läuft: zweite Anmeldung = läuft schon (Sperre), Befehle gehen
    void zweiteInstanzLaeuftSchon() {
        Server a(name());
        QCOMPARE(a.ergebnis, Steuerkanal::Anmeldung::Ok);
        Server b(name());
        QCOMPARE(b.ergebnis, Steuerkanal::Anmeldung::LaeuftSchon);
        QCOMPARE(*Steuerkanal::senden(QStringLiteral("Status"), {}, name()).text, QStringLiteral("ja"));
    }

    // viele Verbindungen nacheinander: nach jeder steht die nächste Pipe-Instanz bereit
    void vieleVerbindungenNacheinander() {
        Server s(name());
        for (int i = 0; i < 40; ++i) QCOMPARE(Steuerkanal::senden(QStringLiteral("Status"), {}, name()).ergebnis, Antwort::Ergebnis::Ok);
    }

    // Frist im Client: Server nimmt an, antwortet aber nie
    void fristImClient() {
        HANDLE stumm = fremdePipe(name(), QStringLiteral("O:%1D:(A;;GA;;;%1)").arg(eigeneSid()));
        QVERIFY(stumm != INVALID_HANDLE_VALUE);
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        ConnectNamedPipe(stumm, &ov);
        QElapsedTimer t;
        t.start();
        QCOMPARE(Steuerkanal::senden(QStringLiteral("Status"), {}, name(), 1000).ergebnis, Antwort::Ergebnis::NichtErreicht);
        QVERIFY(t.elapsed() < 2500);
        CloseHandle(ov.hEvent);
        CloseHandle(stumm);
    }

private:
    QString name() const { return QStringLiteral("schneefall-pipetest-%1-%2").arg(QCoreApplication::applicationPid()).arg(m_nr); }
    int m_nr = 0;
};

QTEST_GUILESS_MAIN(TestPipeWin)
#include "test_pipe_win.moc"
