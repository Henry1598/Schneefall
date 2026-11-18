// Schneefall-Hauptobjekt: Simulation, Ebenen je Monitor, D-Bus-Dienst org.schneefall,
// Symbol im Systemabschnitt und Steuerfenster.
#pragma once
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include "einstellungen.h"
#include "klang.h"
#include "raeumdienst.h"
#include "raeumszene.h"
#include "schnee.h"

class Overlay;
class QScreen;
class QSystemTrayIcon;
class QMenu;
class QAction;
class Steuerfenster;

class App : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.schneefall")
public:
    App();
    ~App() override;
    // registriert den D-Bus-Dienst; false, wenn schon eine Instanz läuft
    bool dienstAnmelden();
    void starten();

    const schnee::Programmeinstellungen &einstellungen() const { return m_einst; }
    void einstellungenAendern(const schnee::Programmeinstellungen &p); // wirkt sofort, speichert verzögert
    static QString konfigPfad();
    Klang &klang() { return m_klang; } // Geräusche (Teil B ruft ereignis()/dauerklang() hier auf)

public Q_SLOTS:
    Q_SCRIPTABLE void Fenster(const QString &json);
    Q_SCRIPTABLE void Steuerfenster();
    Q_SCRIPTABLE void Beenden();
    Q_SCRIPTABLE void Umschalten();          // Schnee an/aus (Programm läuft weiter)
    Q_SCRIPTABLE void Abschuetteln();
    Q_SCRIPTABLE QString Status();
    Q_SCRIPTABLE QString Fensterliste(); // Diagnose: zuletzt vom KWin-Skript empfangene Liste
    Q_SCRIPTABLE QString Raeumen();      // „Jetzt räumen“: Einsatz am Fenster mit dem meisten Schnee, gibt dessen ID zurück
    // wirft einen Schneeball (globale Koordinaten, px/s, Menge bis 220) – zum Spielen und Testen ohne Maus
    Q_SCRIPTABLE void Werfen(double x, double y, double vx, double vy, double menge);

private:
    void bildschirmHinzu(QScreen *s);
    void bildschirmWeg(QScreen *s);
    void bildschirmeMelden();
    void sichtbarkeitPruefen();
    void takt();
    void laden();
    void speichern();
    void trayAufbauen();
    void klaengeWeitergeben();
    void eingabeAktualisieren();
    void maus(int art, double x, double y); // 0 = gedrückt, 1 = bewegt, 2 = losgelassen

    schnee::Simulation m_sim;
    schnee::Programmeinstellungen m_einst;
    Klang m_klang;
    schnee::Raeumdienst m_raeum{m_sim};
    schnee::Szene m_szene;        // was vom Räumdienst zu zeichnen ist (einmal je Takt für alle Ebenen)
    QElapsedTimer m_mausUhr;
    int m_eingabeTakt = 0;
    std::vector<schnee::Fenster> m_fenster;
    QString m_letzteListe;
    QHash<QScreen *, Overlay *> m_ebenen;
    QTimer m_takt;
    QTimer m_speicherTimer;
    QElapsedTimer m_uhr;
    QFileSystemWatcher m_beobachter;
    bool m_laeuft = false;
    bool m_vollbild = false;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_anAktion = nullptr;
    QPointer<::Steuerfenster> m_steuer;
    // Lastmessung (für Status)
    qint64 m_rechenNs = 0;
    int m_rechenBilder = 0;
};
