// Linux-Fensterquelle: das KWin-Skript (kwin/contents/code/main.js) schickt die Fensterliste als JSON an den
// D-Bus-Dienst org.schneefall (Methode Fenster), bei Änderung nach höchstens 15 ms und alle 2 s als Puls.
#pragma once
#include "plattform/fensterquelle.h"

namespace plattform {

// JSON-Liste des KWin-Skripts: [{id, x, y, w, h, s (Stapelplatz), min, fs, akt, dock}, …].
// Die Daten kommen von außen (jeder Prozess der Sitzung kann Fenster(json) aufrufen), daher begrenzt:
// höchstens 1 MiB Text und die ersten 1000 Fenster; Fenster ohne id oder mit id über 128 Zeichen entfallen,
// ebenso Fenster mit nicht endlichen oder nicht-numerischen Zahlen; x/y werden auf ±100000, w/h auf 0..100000 geklemmt.
// Kaputte Daten geben eine leere Liste.
std::vector<schnee::Fenster> fensterAusKwinJson(const QString &json);

class KwinFensterquelle : public Fensterquelle {
public:
    void starten(Empfaenger empfaenger) override { m_empfaenger = std::move(empfaenger); }
    // die zuletzt übernommene (geprüfte) Liste, neu serialisiert – nie der Rohtext von außen
    QString diagnose() const override { return m_letzte; }
    // vom D-Bus-Dienst (Aufruf Fenster aus dem KWin-Skript); unveränderte Liste (Puls) wird nicht gemeldet,
    // zu große oder ungültige Nachrichten werden verworfen (der vorige Stand bleibt)
    void listeEmpfangen(const QString &json);

private:
    Empfaenger m_empfaenger;
    QString m_roh;    // zuletzt übernommene Nachricht (Vergleich für den Puls)
    QString m_letzte; // Diagnose
};

} // namespace plattform
