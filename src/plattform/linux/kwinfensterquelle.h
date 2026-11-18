// Linux-Fensterquelle: das KWin-Skript (kwin/contents/code/main.js) schickt die Fensterliste als JSON an den
// D-Bus-Dienst org.schneefall (Methode Fenster), bei Änderung nach höchstens 15 ms und alle 2 s als Puls.
#pragma once
#include "plattform/fensterquelle.h"

namespace plattform {

// JSON-Liste des KWin-Skripts: [{id, x, y, w, h, s (Stapelplatz), min, fs, akt, dock}, …]; Fenster ohne id entfallen
std::vector<schnee::Fenster> fensterAusKwinJson(const QString &json);

class KwinFensterquelle : public Fensterquelle {
public:
    void starten(Empfaenger empfaenger) override { m_empfaenger = std::move(empfaenger); }
    QString diagnose() const override { return m_letzte; }
    // vom D-Bus-Dienst (Aufruf Fenster aus dem KWin-Skript); unveränderte Liste (Puls) wird nicht gemeldet
    void listeEmpfangen(const QString &json);

private:
    Empfaenger m_empfaenger;
    QString m_letzte;
};

} // namespace plattform
