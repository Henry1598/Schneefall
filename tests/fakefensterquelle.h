// Nachgebildete Fensterquelle für Tests: die Testfälle bestimmen selbst, welche Fenster es gibt.
#pragma once
#include "plattform/fensterquelle.h"

class FakeFensterquelle : public plattform::Fensterquelle {
public:
    void starten(Empfaenger e) override { m_empfaenger = std::move(e); }
    QString diagnose() const override { return QStringLiteral("fake:%1").arg(m_gemeldet); }
    // wie ein Backend: eine neue Liste melden
    void melden(std::vector<schnee::Fenster> liste) {
        ++m_gemeldet;
        if (m_empfaenger) m_empfaenger(std::move(liste));
    }
    bool gestartet() const { return bool(m_empfaenger); }

private:
    Empfaenger m_empfaenger;
    int m_gemeldet = 0;
};

inline schnee::Fenster fakeFenster(const char *id, double x, double y, double w, double h, int stapel, bool vollbild = false, bool aktiv = false) {
    schnee::Fenster f;
    f.id = id;
    f.r = {x, y, w, h};
    f.stapel = stapel;
    f.vollbild = vollbild;
    f.aktiv = aktiv;
    return f;
}
