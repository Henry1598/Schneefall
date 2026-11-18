// Sprüche für Fahrer und Hubschrauberpilot: aus daten/sprueche.tsv (eingebettet per Qt-Ressource),
// Auswahl zufällig, ohne Wiederholung der letzten Sätze, Ausdrucksweise (Stufe 1–5) per Einstellung begrenzt.
#pragma once
#include <QByteArray>
#include <QString>
#include <deque>
#include <map>
#include <random>
#include <vector>

namespace schnee {

enum class Sprecher { Fahrer, Pilot };
enum class Anlass { Treffer, Streik, BeleidigtAbflug, Gruss, HeliTreffer, HeliAbholen, HeliAbflug };

constexpr int SPRUCH_STUFE_MIN = 1, SPRUCH_STUFE_MAX = 5;
constexpr int SPRUCH_FENSTER = 50; // so viele zuletzt gesagte Sätze je Sprecher werden nicht wiederholt

// Eskalation: Stufe ergibt sich aus der Trefferzahl (1. Treffer = Stufe 1, dann steigend), gedeckelt durch die Einstellung
int spruchStufeFuerTreffer(int trefferzahl, int maxStufe);
const char *stufenName(int stufe); // „freundlich“ … „richtig vulgär“

class Sprueche {
public:
    Sprueche();                          // beginnt mit den eingebauten Ersatzsätzen
    // Datei/Text laden; bei Fehler oder ohne gültige Zeile bleiben die bisherigen Sätze. Rückgabe: Zahl gültiger Zeilen.
    int ladeText(const QByteArray &tsv);
    int ladeDatei(const QString &pfad);  // auch ":/daten/sprueche.tsv"
    int ladeStandard();                  // Ressource, sonst Datei neben dem Programm / unter ../daten; sonst Ersatzsätze
    int anzahl() const;
    int anzahl(Sprecher s, Anlass a, int stufe) const;
    bool eingebaut() const { return m_eingebaut; } // true: nur die Ersatzsätze sind aktiv

    void setzeMaxStufe(int s) { m_maxStufe = s < SPRUCH_STUFE_MIN ? SPRUCH_STUFE_MIN : s > SPRUCH_STUFE_MAX ? SPRUCH_STUFE_MAX : s; }
    int maxStufe() const { return m_maxStufe; }
    void setzeSamen(unsigned s) { m_zufall.seed(s); }

    // Zufälliger Satz zum Anlass; die gewünschte Stufe wird auf maxStufe() begrenzt. Gibt es auf der Stufe nichts,
    // wird die nächstniedrigere (dann höhere) genommen. Leer nur, wenn es für Sprecher/Anlass gar nichts gibt.
    QString waehle(Sprecher s, Anlass a, int stufe);
    // wie waehle(), aber die Stufe kommt aus der Trefferzahl (Eskalation)
    QString waehleBeiTreffer(Sprecher s, Anlass a, int trefferzahl) { return waehle(s, a, spruchStufeFuerTreffer(trefferzahl, m_maxStufe)); }

    static Sprueche &instanz(); // gemeinsame, einmal geladene Sammlung für das Programm

private:
    struct Schluessel {
        int sprecher, anlass, stufe;
        bool operator<(const Schluessel &o) const {
            return sprecher != o.sprecher ? sprecher < o.sprecher : anlass != o.anlass ? anlass < o.anlass : stufe < o.stufe;
        }
    };
    void ersatzLaden();
    std::map<Schluessel, std::vector<QString>> m_tabelle;
    std::deque<QString> m_zuletzt[2]; // je Sprecher
    int m_maxStufe = 3;
    bool m_eingebaut = true;
    std::mt19937 m_zufall;
};

} // namespace schnee
