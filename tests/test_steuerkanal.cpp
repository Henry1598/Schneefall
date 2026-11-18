// Steuerkanal (QLocalServer/QLocalSocket): Befehle an das laufende Programm und Einzelinstanz.
// Plattformneutral – läuft unter Linux und Windows. Der Server läuft in einem eigenen Thread, damit der
// blockierende Client (wie die Befehlszeile) im Test-Thread warten kann.
#include <QtTest>
#include <QTemporaryDir>
#include <QThread>
#include "plattform/steuerkanal.h"
#ifdef Q_OS_UNIX
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

using plattform::Antwort;
using plattform::Steuerkanal;

namespace {
// Steuerkanal in einem eigenen Thread mit eigener Ereignisschleife
struct Server {
    QThread thread;
    QObject kontext;
    std::unique_ptr<Steuerkanal> kanal;
    bool angemeldet = false;
    QStringList empfangen; // „befehl arg1 arg2“ (nur im Server-Thread geschrieben, nach stop() gelesen)
    explicit Server(const QString &name) {
        kontext.moveToThread(&thread);
        thread.start();
        QMetaObject::invokeMethod(&kontext, [this, name] {
            kanal = std::make_unique<Steuerkanal>([this](const QString &b, const QStringList &a) -> Antwort {
                empfangen << (QStringList{b} + a).join(QLatin1Char(' '));
                if (b == QLatin1String("Status")) return Antwort::ok(QStringLiteral("laeuft=1\nzweite Zeile"));
                if (b == QLatin1String("Umschalten")) return Antwort::ok();
                if (b == QLatin1String("Zeitlupe") && a.size() == 1) return Antwort::ok();
                return Antwort::unbekannt();
            }, name);
            angemeldet = kanal->anmelden();
        }, Qt::BlockingQueuedConnection);
    }
    ~Server() { stop(); }
    void stop() {
        if (!thread.isRunning()) return;
        QMetaObject::invokeMethod(&kontext, [this] { kanal.reset(); }, Qt::BlockingQueuedConnection);
        thread.quit();
        thread.wait();
    }
};
} // namespace

class TestSteuerkanal : public QObject {
    Q_OBJECT
private slots:
    void init() { QVERIFY(m_dir.isValid()); }

    void befehlKommtAnUndAntwortZurueck() {
        Server s(name());
        QVERIFY(s.angemeldet);
        const Antwort a = Steuerkanal::senden(QStringLiteral("Status"), {}, name());
        QCOMPARE(a.ergebnis, Antwort::Ergebnis::Ok);
        QVERIFY(a.text.has_value());
        QCOMPARE(*a.text, QStringLiteral("laeuft=1\nzweite Zeile")); // mehrzeiliger Text kommt unverändert an
    }
    void befehlOhneRueckgabeHatKeinenText() {
        Server s(name());
        const Antwort a = Steuerkanal::senden(QStringLiteral("Umschalten"), {}, name());
        QCOMPARE(a.ergebnis, Antwort::Ergebnis::Ok);
        QVERIFY(!a.text.has_value());
    }
    void argumenteKommenAn() {
        Server s(name());
        QCOMPARE(Steuerkanal::senden(QStringLiteral("Zeitlupe"), {QStringLiteral("0.25")}, name()).ergebnis, Antwort::Ergebnis::Ok);
        s.stop();
        QCOMPARE(s.empfangen, QStringList{QStringLiteral("Zeitlupe 0.25")});
    }
    void unbekannterBefehlWirdGemeldet() {
        Server s(name());
        QCOMPARE(Steuerkanal::senden(QStringLiteral("Gibtsnicht"), {}, name()).ergebnis, Antwort::Ergebnis::Unbekannt);
    }
    void ohneLaufendeInstanzNichtErreicht() {
        const Antwort a = Steuerkanal::senden(QStringLiteral("Status"), {}, name(), 500);
        QCOMPARE(a.ergebnis, Antwort::Ergebnis::NichtErreicht);
    }
    void zweiteInstanzBekommtDenKanalNicht() {
        Server erste(name());
        QVERIFY(erste.angemeldet);
        Steuerkanal zweite([](const QString &, const QStringList &) { return Antwort::ok(); }, name());
        QVERIFY(!zweite.anmelden());
        // die erste antwortet weiter
        QCOMPARE(Steuerkanal::senden(QStringLiteral("Status"), {}, name()).ergebnis, Antwort::Ergebnis::Ok);
    }
    void nachDemBeendenIstDerKanalWiederFrei() {
        {
            Server erste(name());
            QVERIFY(erste.angemeldet);
        }
        Server zweite(name());
        QVERIFY(zweite.angemeldet);
    }
    // abgestürzte Instanz: der Socket liegt noch da, niemand lauscht – die neue Instanz übernimmt
    void verwaisterSocketWirdErsetzt() {
#ifndef Q_OS_UNIX
        QSKIP("nur mit Unix-Sockets");
#else
        const QByteArray pfad = QFile::encodeName(name());
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        QVERIFY(fd >= 0);
        sockaddr_un adr{};
        adr.sun_family = AF_UNIX;
        qstrncpy(adr.sun_path, pfad.constData(), sizeof adr.sun_path);
        QCOMPARE(::bind(fd, reinterpret_cast<sockaddr *>(&adr), sizeof adr), 0);
        ::close(fd); // ohne unlink: verwaist
        QVERIFY(QFile::exists(name()));
        Server s(name());
        QVERIFY(s.angemeldet);
        QCOMPARE(Steuerkanal::senden(QStringLiteral("Status"), {}, name()).ergebnis, Antwort::Ergebnis::Ok);
#endif
    }
    void standardNameIstJeBenutzer() {
        const QString n = Steuerkanal::standardName();
        QVERIFY(!n.isEmpty());
#ifdef Q_OS_UNIX
        QVERIFY2(!n.startsWith(QDir::tempPath()), qPrintable(n)); // nicht im gemeinsamen /tmp
#endif
    }

private:
    QString name() const {
#ifdef Q_OS_WIN
        return QStringLiteral("schneefall-test-%1").arg(QCoreApplication::applicationPid()); // benannte Pipe
#else
        return m_dir.filePath(QStringLiteral("sf.sock"));
#endif
    }
    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(TestSteuerkanal)
#include "test_steuerkanal.moc"
