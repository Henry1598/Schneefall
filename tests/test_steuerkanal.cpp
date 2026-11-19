// Steuerkanal (QLocalServer/QLocalSocket): Befehle an das laufende Programm und Einzelinstanz.
// Plattformneutral – läuft unter Linux und Windows. Der Server läuft in einem eigenen Thread, damit der
// blockierende Client (wie die Befehlszeile) im Test-Thread warten kann.
#include <QtTest>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QLockFile>
#include <QProcess>
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
    Steuerkanal::Anmeldung ergebnis = Steuerkanal::Anmeldung::Fehler;
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
                if (b == QLatin1String("Gross")) return Antwort::ok(QString(5 * 1024 * 1024, QLatin1Char('x'))); // 5 MiB
                return Antwort::unbekannt();
            }, name);
            ergebnis = kanal->anmelden();
            angemeldet = ergebnis == Steuerkanal::Anmeldung::Ok;
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
    void init() {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

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
        QCOMPARE(zweite.anmelden(), Steuerkanal::Anmeldung::LaeuftSchon);
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
        verwaistenSocketAnlegen();
        Server s(name());
        QVERIFY(s.angemeldet);
        QCOMPARE(Steuerkanal::senden(QStringLiteral("Status"), {}, name()).ergebnis, Antwort::Ergebnis::Ok);
#endif
    }
    // ---- Sicherheitsprüfung N2: Verbindungen begrenzt ----
    void leerlaufendeVerbindungWirdNachZweiSekundenGetrennt() {
        Server s(name());
        QLocalSocket c;
        c.connectToServer(name());
        QVERIFY(c.waitForConnected(1000));
        QElapsedTimer uhr;
        uhr.start();
        QVERIFY(c.waitForDisconnected(5000)); // schickt nichts – der Server trennt
        QVERIFY2(uhr.elapsed() >= 1500 && uhr.elapsed() < 4000, qPrintable(QString::number(uhr.elapsed())));
    }
    void hoechstens16GleichzeitigeVerbindungen() {
        Server s(name());
        std::vector<std::unique_ptr<QLocalSocket>> leer;
        for (int i = 0; i < 16; ++i) {
            leer.push_back(std::make_unique<QLocalSocket>());
            leer.back()->connectToServer(name());
            QVERIFY(leer.back()->waitForConnected(1000));
        }
        QTest::qWait(300); // der Server hat alle angenommen
        QLocalSocket zuViel;
        zuViel.connectToServer(name());
        QVERIFY(zuViel.waitForConnected(1000));
        QElapsedTimer uhr;
        uhr.start();
        QVERIFY(zuViel.state() == QLocalSocket::UnconnectedState || zuViel.waitForDisconnected(1000)); // sofort getrennt
        QVERIFY(uhr.elapsed() < 1000);
        for (auto &c : leer) QCOMPARE(c->state(), QLocalSocket::ConnectedState); // die ersten bleiben (bis zur Zeitgrenze)
        leer.clear();
        QTest::qWait(300);
        QCOMPARE(Steuerkanal::senden(QStringLiteral("Status"), {}, name()).ergebnis, Antwort::Ergebnis::Ok);
    }
    void nurEinBefehlJeVerbindung() {
        Server s(name());
        QLocalSocket c;
        c.connectToServer(name());
        QVERIFY(c.waitForConnected(1000));
        c.write("{\"befehl\":\"Umschalten\",\"argumente\":[]}\n{\"befehl\":\"Umschalten\",\"argumente\":[]}\n");
        QVERIFY(c.waitForBytesWritten(1000));
        QVERIFY(c.waitForReadyRead(2000));
        c.write("{\"befehl\":\"Umschalten\",\"argumente\":[]}\n");
        c.waitForBytesWritten(500);
        if (c.state() != QLocalSocket::UnconnectedState) c.waitForDisconnected(1000);
        s.stop();
        QCOMPARE(s.empfangen, QStringList{QStringLiteral("Umschalten")});
    }

    // ---- Sicherheitsprüfung N3: Antworten begrenzt, Ausgabe ohne Steuerzeichen ----
    void zuGrosseAntwortWirdVerworfen() {
        Server s(name());
        QVERIFY(Steuerkanal::senden(QStringLiteral("Gross"), {}, name()).ergebnis != Antwort::Ergebnis::Ok);
    }
    void terminalTextOhneSteuerzeichen() {
        const QString roh = QStringLiteral("a\x1b[2Jb\x07" "c\n\td\x7f") + QChar(0x9b) + QStringLiteral("e\r");
        QCOMPARE(plattform::terminalText(roh), QStringLiteral("a?[2Jb?c\n\td??e?"));
        QCOMPARE(plattform::terminalText(QStringLiteral("Schnee ❄ ok\n")), QStringLiteral("Schnee ❄ ok\n"));
        // Nachprüfung R6: Bidi-Steuerung (U+202E, U+2066), Zeilen-/Absatztrenner (U+2028, U+2029)
        const QString bidi = QStringLiteral("a") + QChar(0x202e) + QStringLiteral("b") + QChar(0x2066) + QStringLiteral("c") + QChar(0x2028) +
                             QStringLiteral("d") + QChar(0x2029) + QStringLiteral("e");
        QCOMPARE(plattform::terminalText(bidi), QStringLiteral("a?b?c?d?e"));
        // Formatzeichen außerhalb der BMP (Tag-Zeichen U+E0041) als ganzer Codepunkt; Emoji außerhalb der BMP bleiben
        const char32_t tag[] = {U'x', 0xE0041, U'y', 0x1F328, 0};
        QCOMPARE(plattform::terminalText(QString::fromUcs4(tag)), QStringLiteral("x?y") + QString::fromUcs4(U"\U0001F328"));
    }
    // Nachprüfung R6: die Frist gilt für den ganzen Aufruf, nicht je Lesevorgang (Server tröpfelt Bytes)
    void tropfenderServerHaeltDenClientNichtFest() {
        QThread thread;
        QLocalServer server;
        QTimer tropfen;
        QLocalSocket *verbunden = nullptr;
        QVERIFY(server.listen(name()));
        server.moveToThread(&thread);
        tropfen.moveToThread(&thread);
        QObject::connect(&server, &QLocalServer::newConnection, &server, [&] { verbunden = server.nextPendingConnection(); });
        QObject::connect(&tropfen, &QTimer::timeout, &server, [&] { if (verbunden) { verbunden->write("x"); verbunden->flush(); } });
        thread.start();
        QMetaObject::invokeMethod(&tropfen, [&] { tropfen.start(100); }, Qt::BlockingQueuedConnection);
        QElapsedTimer uhr;
        uhr.start();
        const Antwort a = Steuerkanal::senden(QStringLiteral("Status"), {}, name(), 800);
        const qint64 dauer = uhr.elapsed();
        QMetaObject::invokeMethod(&server, [&] { tropfen.stop(); delete verbunden; server.close(); server.moveToThread(qApp->thread()); tropfen.moveToThread(qApp->thread()); }, Qt::BlockingQueuedConnection);
        thread.quit();
        thread.wait();
        QCOMPARE(a.ergebnis, Antwort::Ergebnis::NichtErreicht);
        QVERIFY2(dauer < 1500, qPrintable(QString::number(dauer)));
    }
    // Windows ohne eigene SID (Nachprüfung R6) bzw. allgemein: ohne Namen kein Kanal – Fehler, nicht „läuft schon“
    void ohneNamenFehler() {
        Steuerkanal k([](const QString &, const QStringList &) { return Antwort::ok(); }, QString());
        QCOMPARE(k.anmelden(), Steuerkanal::Anmeldung::Fehler);
        QCOMPARE(Steuerkanal::senden(QStringLiteral("Status"), {}, QString(), 300).ergebnis, Antwort::Ergebnis::NichtErreicht);
    }
    // Nachprüfung R5: ohne Laufzeitordner ein eigener 0700-Ordner unter /tmp, nur wenn er wirklich uns gehört
    void privaterOrdnerNurWennSicher() {
#ifndef Q_OS_UNIX
        QSKIP("nur Unix");
#else
        const QString basis = m_dir->path();
        const QString o = plattform::privaterOrdner(basis);
        QVERIFY(!o.isEmpty());
        QVERIFY(o.startsWith(basis));
        QCOMPARE(QFileInfo(o).permissions() & (QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther),
                 QFile::Permissions());
        QCOMPARE(plattform::privaterOrdner(basis), o); // zweiter Aufruf: derselbe Ordner
        // für andere offen: nicht verwenden
        QVERIFY(QFile::setPermissions(o, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther));
        QVERIFY(plattform::privaterOrdner(basis).isEmpty());
        // symbolischer Link an seiner Stelle: nicht verwenden
        const QString basis2 = m_dir->filePath(QStringLiteral("zwei"));
        QVERIFY(QDir().mkpath(basis2 + QStringLiteral("/ziel")));
        const QString erwartet = QFileInfo(plattform::privaterOrdner(basis2)).fileName();
        QVERIFY(QDir(plattform::privaterOrdner(basis2)).removeRecursively());
        QVERIFY(QFile::link(basis2 + QStringLiteral("/ziel"), basis2 + QLatin1Char('/') + erwartet));
        QVERIFY(plattform::privaterOrdner(basis2).isEmpty());
#endif
    }

    // ---- Sicherheitsprüfung M2/N5: Einzelinstanz über eine Sperrdatei ----
    void sperrdateiWirdGehalten() {
        Server s(name());
        QVERIFY(s.angemeldet);
        QLockFile fremd(Steuerkanal::sperrPfad(name()));
        QVERIFY(!fremd.tryLock(0));
        QCOMPARE(fremd.error(), QLockFile::LockFailedError);
    }
    // eine andere Instanz hält die Sperre, lauscht aber (noch) nicht: läuft schon – ihr Socket wird nicht entfernt
    void gehalteneSperreOhneSocketHeisstLaeuftSchon() {
#ifndef Q_OS_UNIX
        QSKIP("verwaister Socket nur mit Unix-Sockets");
#else
        verwaistenSocketAnlegen();
        QLockFile andere(Steuerkanal::sperrPfad(name()));
        andere.setStaleLockTime(0);
        QVERIFY(andere.tryLock(0));
        Steuerkanal k([](const QString &, const QStringList &) { return Antwort::ok(); }, name());
        QCOMPARE(k.anmelden(), Steuerkanal::Anmeldung::LaeuftSchon);
        QVERIFY(QFile::exists(name())); // removeServer nur mit gehaltener Sperre
#endif
    }
    // Absturz: Sperrdatei eines beendeten Prozesses bleibt liegen – die neue Instanz übernimmt
    void verwaisteSperreNachAbsturzWirdUebernommen() {
        QProcess p;
#ifdef Q_OS_WIN
        p.start(QStringLiteral("cmd"), {QStringLiteral("/c"), QStringLiteral("exit")});
#else
        p.start(QStringLiteral("true"), QStringList{});
#endif
        QVERIFY(p.waitForStarted(3000));
        const qint64 totePid = p.processId();
        QVERIFY(p.waitForFinished(3000));
        QFile sperre(Steuerkanal::sperrPfad(name()));
        QVERIFY(sperre.open(QIODevice::WriteOnly));
        sperre.write(QByteArray::number(totePid) + '\n' + QCoreApplication::applicationName().toUtf8() + '\n' + QSysInfo::machineHostName().toUtf8() + '\n');
        sperre.close();
        Server s(name());
        QCOMPARE(s.ergebnis, Steuerkanal::Anmeldung::Ok);
        QCOMPARE(Steuerkanal::senden(QStringLiteral("Status"), {}, name()).ergebnis, Antwort::Ergebnis::Ok);
    }
    // Lauschen scheitert (hier: Socket-Pfad zu lang): Fehler, nicht „läuft schon“
    void scheiterndesLauschenIstEinFehler() {
#ifndef Q_OS_UNIX
        QSKIP("Pfadlänge nur bei Unix-Sockets begrenzt");
#else
        const QString zuLang = m_dir->filePath(QString(200, QLatin1Char('s')) + QStringLiteral(".sock"));
        Steuerkanal k([](const QString &, const QStringList &) { return Antwort::ok(); }, zuLang);
        QCOMPARE(k.anmelden(), Steuerkanal::Anmeldung::Fehler);
        QVERIFY(!k.fehlertext().isEmpty());
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
#ifdef Q_OS_UNIX
    // Socket-Datei, an der niemand lauscht (wie nach einem Absturz)
    void verwaistenSocketAnlegen() {
        const QByteArray pfad = QFile::encodeName(name());
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        QVERIFY(fd >= 0);
        sockaddr_un adr{};
        adr.sun_family = AF_UNIX;
        qstrncpy(adr.sun_path, pfad.constData(), sizeof adr.sun_path);
        QCOMPARE(::bind(fd, reinterpret_cast<sockaddr *>(&adr), sizeof adr), 0);
        ::close(fd); // ohne unlink: verwaist
        QVERIFY(QFile::exists(name()));
    }
#endif
    QString name() const {
#ifdef Q_OS_WIN
        return QStringLiteral("schneefall-test-%1").arg(QCoreApplication::applicationPid()); // benannte Pipe
#else
        return m_dir->filePath(QStringLiteral("sf.sock"));
#endif
    }
    std::unique_ptr<QTemporaryDir> m_dir;
};

QTEST_GUILESS_MAIN(TestSteuerkanal)
#include "test_steuerkanal.moc"
