// Statistik: zählt Räumeinsätze, Schneebälle, Treffer und Streiks und speichert sie dauerhaft.
// Nur Qt Core, keine Oberfläche und nichts Plattformabhängiges (gilt unverändert für Linux und Windows).
// Dateiformat: Textdatei „schluessel=wert“ je Zeile (schneefall-statistik.conf neben schneefall.conf).
#pragma once
#include <QDateTime>
#include <QString>
#include <vector>
#include "raeumdienst.h"

namespace schnee {

struct Zaehlerstand {
    qint64 einsaetzeGestartet = 0, einsaetzeAbgeschlossen = 0, einsaetzeAbgebrochen = 0;
    qint64 pflugGestartet = 0, pflugAbgeschlossen = 0;   // davon der kleine Pflug (Taskleiste und Boden)
    double schneeGeraeumt = 0;                           // Summe der abgeworfenen Mengen (Einheiten wie gesamtSchnee())
    qint64 baelleGeworfen = 0;
    qint64 trefferFahrzeug = 0, trefferHeli = 0, baelleDaneben = 0;
    qint64 streiks = 0, beleidigteAbfluege = 0, sprueche = 0;
    QDateTime seit;                                      // Beginn der Zählung (UTC)

    qint64 trefferGesamt() const { return trefferFahrzeug + trefferHeli; }
    // Treffer je entschiedenem Wurf (Treffer + daneben) in Prozent; -1, solange noch nichts entschieden ist
    double trefferquote() const;
    bool operator==(const Zaehlerstand &o) const;
};

class Statistik {
public:
    // pfad leer = Standardpfad (standardPfad()); die Datei wird sofort geladen, fehlende oder kaputte Datei = bei null beginnen
    explicit Statistik(const QString &pfad = QString());
    static QString standardPfad();

    const Zaehlerstand &stand() const { return m_z; }
    const QString &pfad() const { return m_pfad; }

    void verbuchen(const Statistikereignis &e);
    void verbuchen(const std::vector<Statistikereignis> &e);
    void zuruecksetzen();

    bool geaendert() const { return m_geaendert; }
    // schreibt atomar (QSaveFile), aber nur bei Änderung; false bei Schreibfehler
    bool speichern();
    // lädt die Datei neu; false, wenn sie fehlt oder unbrauchbar ist (dann steht alles auf null)
    bool laden();

    static QString alsText(const Zaehlerstand &z);
    static bool ausText(const QString &text, Zaehlerstand &z);
    // eine Zeile für --status: statistik=geraeumt:12,abgebrochen:1,...
    QString statusZeile() const;
    // kurzer Text für den Tooltip des Symbols
    QString kurzText() const;

private:
    QString m_pfad;
    Zaehlerstand m_z;
    bool m_geaendert = false;
};

} // namespace schnee
