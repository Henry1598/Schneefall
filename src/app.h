// Schneefall-Hauptobjekt: Simulation, Ebenen je Monitor, Steuerkanal (Befehle, Einzelinstanz),
// Symbol im Systemabschnitt und Steuerfenster. Alles Plattformspezifische steckt in plattform/.
#pragma once
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <memory>
#include "bildtakt.h"
#include "einstellungen.h"
#include "klang.h"
#include "raeumdienst.h"
#include "raeumszene.h"
#include "schnee.h"
#include "statistik.h"
#include "plattform/fensterquelle.h"
#include "plattform/steuerkanal.h"

class Overlay;
class QScreen;
class QSystemTrayIcon;
class QMenu;
class QAction;
class Steuerfenster;

class App : public QObject {
    Q_OBJECT
public:
    // quelle: woher die Fensterliste kommt; nullptr = die der Plattform (Linux: KWin-Skript über D-Bus)
    explicit App(std::unique_ptr<plattform::Fensterquelle> quelle = nullptr);
    ~App() override;
    // meldet Steuerkanal (mit Sperrdatei) und Systemkanal (Linux: D-Bus org.schneefall) an:
    // Ok, LaeuftSchon (eine andere Instanz läuft) oder Fehler (Text in anmeldeFehler())
    plattform::Steuerkanal::Anmeldung dienstAnmelden();
    QString anmeldeFehler() const { return m_anmeldeFehler; }
    // Hinweis nach erfolgreicher Anmeldung (Windows: mit Administratorrechten gestartet), sonst leer
    QString anmeldeHinweis() const { return m_steuerkanal ? m_steuerkanal->hinweis() : QString(); }
    // ein Befehl aus Steuer- oder Systemkanal: Steuerfenster, Beenden, Umschalten, Abschuetteln, Status, Fensterliste,
    // Raeumen, Werfen x y vx vy menge, Zeitlupe faktor
    plattform::Antwort befehl(const QString &name, const QStringList &argumente);
    void starten();

    const schnee::Programmeinstellungen &einstellungen() const { return m_einst; }
    void einstellungenAendern(const schnee::Programmeinstellungen &p); // wirkt sofort, speichert verzögert
    static QString konfigPfad();
    schnee::Statistik &statistik() { return m_statistik; }   // Zähler (Räumeinsätze, Treffer, Streiks), Anzeige im Steuerfenster
    void statistikZuruecksetzen();
    Klang &klang() { return m_klang; } // Geräusche (Teil B ruft ereignis()/dauerklang() hier auf)

public Q_SLOTS:
    void Steuerfenster();
    void Beenden();
    void Umschalten();          // Schnee an/aus (Programm läuft weiter)
    void Abschuetteln();
    QString Status();
    QString Fensterliste(); // Diagnose: zuletzt von der Fensterquelle empfangene Liste (Rohdaten)
    QString Raeumen();      // „Jetzt räumen“: Einsatz am Fenster mit dem meisten Schnee, gibt dessen ID zurück
    // wirft einen Schneeball (globale Koordinaten, px/s, Menge bis 220) – zum Spielen und Testen ohne Maus
    void Werfen(double x, double y, double vx, double vy, double menge);
    // Zeitlupe zum Begutachten schneller Abläufe (z. B. der Wende): 0.2 = fünfmal langsamer, 1 = normal
    void Zeitlupe(double faktor);

private:
    void fensterSetzen(std::vector<schnee::Fenster> liste); // neue Liste von der Fensterquelle
    void bildschirmHinzu(QScreen *s);
    void bildschirmWeg(QScreen *s);
    void bildschirmeMelden();
    void sichtbarkeitPruefen();
    void takt();                 // Wächter: rechnet weiter, falls gerade keine Ebene zeichnet
    void bildKommt(double jetzt); // eine Ebene beginnt ein Bild: Simulationsschritt, wenn fällig
    void schritt(double jetzt);
    bool inBewegung(double jetzt) const;
    void laden();
    void speichern();
    void trayAufbauen();
    void statistikAbholen(); // Ereignisse des Räumdienstes verbuchen, Tooltip auffrischen
    void klaengeWeitergeben();
    void eingabeAktualisieren();
    void maus(int art, double x, double y); // 0 = gedrückt, 1 = bewegt, 2 = losgelassen

    schnee::Simulation m_sim;
    schnee::Programmeinstellungen m_einst;
    Klang m_klang;
    schnee::Raeumdienst m_raeum{m_sim};
    schnee::Statistik m_statistik;
    QTimer m_statistikTimer;       // schreibt die Statistik höchstens alle 30 s
    schnee::Szene m_szene;        // was vom Räumdienst zu zeichnen ist (einmal je Takt für alle Ebenen)
    QElapsedTimer m_mausUhr;
    double m_eingabeFaellig = 0;   // uhrSekunden(), ab wann die Eingaberegion wieder angepasst wird
    schnee::Taktstand m_taktstand; // gemeinsamer Simulationsstand für alle Ebenen
    double m_flockenUhr = -1;      // wann die Ebenen zuletzt ihre Flocken neu gebaut haben
    double m_fensterUhr = -1;      // wann sich zuletzt Fenster bewegt/geändert haben
    bool m_mausGedrueckt = false;
    int m_schritte = 0, m_flockenAufbauten = 0; // seit dem letzten --status
    double m_statusUhr = 0;
    double m_zeitlupe = 1.0;
    std::vector<schnee::Fenster> m_fenster;
    std::unique_ptr<plattform::Fensterquelle> m_quelle;
    std::unique_ptr<plattform::Systemkanal> m_systemkanal;
    std::unique_ptr<plattform::Steuerkanal> m_steuerkanal;
    QString m_anmeldeFehler;
    QHash<QScreen *, Overlay *> m_ebenen;
    QTimer m_takt;                 // Wächter (alle 100 ms), der eigentliche Takt kommt aus den Ebenen
    QTimer m_speicherTimer;
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
