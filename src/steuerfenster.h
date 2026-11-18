// Kleines Steuerfenster: Schalter, Regler, Abschütteln, Autostart, Beenden.
#pragma once
#include <QWidget>
#include <vector>

class App;
class QCheckBox;
class QComboBox;
class QSlider;
class QLabel;

class Steuerfenster : public QWidget {
public:
    explicit Steuerfenster(App *app);
    void ausEinstellungen(); // Bedienelemente auf den aktuellen Stand bringen

private:
    struct Regler { QSlider *slider = nullptr; QLabel *wert = nullptr; double faktor = 1; QString einheit; };
    Regler regler(const QString &name, int min, int max, double faktor, const QString &einheit, class QGridLayout *g, int zeile,
                  const QString &tipp = QString());
    void uebernehmen();
    void statistikAnzeigen();     // Werte der Gruppe „Statistik“ auffrischen (1×/s, solange sichtbar)
    void statistikZuruecksetzen(); // mit Rückfrage

    App *m_app;
    QLabel *m_stat[12] = {};      // Wertefelder der Gruppe „Statistik“
    QLabel *m_statSeit = nullptr;
    QCheckBox *m_an;
    QCheckBox *m_vollbild;
    QComboBox *m_modus; // Fenstermodus: 0 = fest, 1 = dahinter, 2 = davor
    QCheckBox *m_autostart;
    QLabel *m_autostartHinweis; // erscheint, wenn sich der Autostart nicht einrichten ließ
    QCheckBox *m_ton;
    QSlider *m_laut;
    QLabel *m_lautWert;
    QComboBox *m_detail; // Flockendetail: 0 = einfach, 1 = realistisch
    QComboBox *m_bildrate; // Auto, 30, 60, 120, 144 (+ eigener Wert aus der Datei)
    Regler m_menge, m_wind, m_groesse, m_hoehe, m_fall;
    // Gruppe „Fenster“
    Regler m_schwelle, m_rutschAb, m_rutschTempo, m_wolkeDauer, m_wolkeLiegen, m_bodenMax;
    // Gruppe „Räumdienst“
    QCheckBox *m_raeumAn, *m_taskleiste, *m_anfassbar;
    Regler m_raeumenAb, m_fzGroesse, m_gleichzeitig, m_trefferStreik;
    QComboBox *m_ausdruck; // Ausdrucksweise der Sprüche: Index 0 … 4 = Stufe 1 … 5
    std::vector<Regler *> m_alle;
    bool m_setzt = false;
};
