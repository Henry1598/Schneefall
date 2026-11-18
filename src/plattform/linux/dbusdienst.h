// Linux-Systemkanal: D-Bus-Dienst org.schneefall auf „/“, Schnittstelle org.schneefall.
// Methoden und Signaturen wie bisher (KWin-Skript ruft Fenster(s), qdbus6/busctl die übrigen).
#pragma once
#include <QObject>
#include "plattform/steuerkanal.h"

namespace plattform {

class KwinFensterquelle;

class DbusDienst : public QObject, public Systemkanal {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.schneefall")
public:
    // quelle: bekommt die Fensterliste aus Fenster(s); nullptr = Fensterlisten werden ignoriert
    DbusDienst(Befehlsziel ziel, KwinFensterquelle *quelle);
    ~DbusDienst() override;
    bool anmelden() override;

public Q_SLOTS:
    Q_SCRIPTABLE void Fenster(const QString &json);
    Q_SCRIPTABLE void Steuerfenster();
    Q_SCRIPTABLE void Beenden();
    Q_SCRIPTABLE void Umschalten();
    Q_SCRIPTABLE void Abschuetteln();
    Q_SCRIPTABLE QString Status();
    Q_SCRIPTABLE QString Fensterliste();
    Q_SCRIPTABLE QString Raeumen();
    Q_SCRIPTABLE void Werfen(double x, double y, double vx, double vy, double menge);
    Q_SCRIPTABLE void Zeitlupe(double faktor);

private:
    QString text(const QString &befehl, const QStringList &argumente = {});
    Befehlsziel m_ziel;
    KwinFensterquelle *m_quelle;
    bool m_angemeldet = false;
};

} // namespace plattform
