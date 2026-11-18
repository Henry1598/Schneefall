// Steuerkanal: Einzelinstanz und Befehle an das laufende Programm (--status, --raeumen, --umschalten, …).
//
// Plattformneutral über QLocalServer/QLocalSocket (Linux: Unix-Socket im Laufzeitordner des Benutzers,
// Windows: benannte Pipe je Benutzer-SID und Sitzung). Protokoll: je Verbindung eine Zeile JSON
// {"befehl":…, "argumente":[…]}, Antwort eine Zeile {"ergebnis":"ok"|"unbekannt", "text":…}.
//
// Grenzen (Sicherheitsprüfung): Anfrage bis 64 KiB, je Verbindung ein Befehl und höchstens 2 s, höchstens
// 16 Verbindungen gleichzeitig; der Client nimmt Antworten bis 4 MiB an.
//
// Einzelinstanz: eine Sperrdatei (QLockFile) neben dem Socket bzw. unter %LOCALAPPDATA%\Schneefall\ entscheidet.
// Nur wer sie hält, darf einen verwaisten Socket entfernen; die Sperre eines abgestürzten Prozesses (PID lebt
// nicht mehr) wird übernommen.
//
// Dazu der Systemkanal der Plattform: unter Linux der D-Bus-Dienst org.schneefall (das KWin-Skript schickt
// dort die Fensterliste hin, qdbus6 kann Befehle senden); unter Windows gibt es keinen.
#pragma once
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>
#include <optional>

namespace plattform {

class Fensterquelle;

struct Antwort {
    enum class Ergebnis { Ok, Unbekannt, NichtErreicht };
    Ergebnis ergebnis = Ergebnis::NichtErreicht;
    std::optional<QString> text; // leer bei Befehlen ohne Rückgabewert
    static Antwort ok(std::optional<QString> t = std::nullopt) { return {Ergebnis::Ok, std::move(t)}; }
    static Antwort unbekannt() { return {Ergebnis::Unbekannt, std::nullopt}; }
};

// führt einen Befehl im laufenden Programm aus (GUI-Thread); unbekannter Befehl oder falsche Argumente → Unbekannt
using Befehlsziel = std::function<Antwort(const QString &befehl, const QStringList &argumente)>;

// Text fürs Terminal: Steuerzeichen außer Zeilenumbruch und Tabulator (ESC, BEL, CR, DEL, C1 …) werden zu „?“
QString terminalText(const QString &text);

class Steuerkanal {
public:
    enum class Anmeldung { Ok, LaeuftSchon, Fehler };
    explicit Steuerkanal(Befehlsziel ziel, QString name = standardName());
    ~Steuerkanal();
    Steuerkanal(const Steuerkanal &) = delete;
    Steuerkanal &operator=(const Steuerkanal &) = delete;
    // Einzelinstanz: LaeuftSchon, wenn eine andere Instanz die Sperre hält oder unter dem Namen antwortet
    // (z. B. eine ältere Version ohne Sperre). Mit gehaltener Sperre wird ein verwaister Socket ersetzt.
    // Fehler: Sperre oder Lauschen unmöglich (Text in fehlertext()). Nur der eigene Benutzer kann sich verbinden.
    Anmeldung anmelden();
    QString fehlertext() const { return m_fehler; }
    QString name() const { return m_name; }
    // Sperrdatei zum Namen (Linux: <socket>.lock, Windows: %LOCALAPPDATA%\Schneefall\<name>.lock)
    static QString sperrPfad(const QString &name);
    // Name je Benutzer (Linux: $XDG_RUNTIME_DIR/schneefall.sock, Windows: schneefall-<Benutzer-SID>-<Sitzung>)
    static QString standardName();
    // Client: Befehl senden und auf die Antwort warten (blockiert höchstens ms). NichtErreicht, wenn keine Instanz läuft.
    static Antwort senden(const QString &befehl, const QStringList &argumente = {}, const QString &name = standardName(), int ms = 3000);

private:
    struct Impl;
    Befehlsziel m_ziel;
    QString m_name;
    QString m_fehler;
    std::unique_ptr<Impl> m_impl;
};

// Systemkanal der Plattform (Linux: D-Bus org.schneefall mit allen Befehlen und Fenster(json) fürs KWin-Skript)
class Systemkanal {
public:
    virtual ~Systemkanal() = default;
    // false, wenn der Kanal schon einer anderen Instanz gehört oder nicht verfügbar ist, wo er gebraucht wird
    virtual bool anmelden() = 0;
};
// quelle: die Fensterquelle der App (Linux: bekommt die Fensterliste aus dem D-Bus-Aufruf Fenster)
std::unique_ptr<Systemkanal> systemkanalErzeugen(Befehlsziel ziel, Fensterquelle &quelle);
// Client über den Systemkanal (Linux: D-Bus, z. B. ältere laufende Version); std::nullopt = gibt es nicht
std::optional<Antwort> systemkanalSenden(const QString &befehl, const QStringList &argumente);

} // namespace plattform
