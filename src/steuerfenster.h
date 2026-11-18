// Kleines Steuerfenster: Schalter, Regler, Abschütteln, Autostart, Beenden.
#pragma once
#include <QWidget>

class App;
class QCheckBox;
class QSlider;
class QLabel;

class Steuerfenster : public QWidget {
public:
    explicit Steuerfenster(App *app);
    void ausEinstellungen(); // Bedienelemente auf den aktuellen Stand bringen
    static QString autostartPfad();

private:
    struct Regler { QSlider *slider; QLabel *wert; double faktor; };
    Regler regler(const QString &name, int min, int max, double faktor, const QString &einheit, class QGridLayout *g, int zeile);
    void uebernehmen();

    App *m_app;
    QCheckBox *m_an;
    QCheckBox *m_vollbild;
    QCheckBox *m_vorFenstern;
    QCheckBox *m_autostart;
    Regler m_menge, m_wind, m_groesse, m_hoehe, m_fall;
    QString m_einheiten[5];
    bool m_setzt = false;
};
