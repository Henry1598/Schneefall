#include "statistik.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>
#include <cmath>

namespace schnee {

double Zaehlerstand::trefferquote() const {
    const qint64 n = trefferGesamt() + baelleDaneben;
    return n > 0 ? 100.0 * double(trefferGesamt()) / double(n) : -1;
}

bool Zaehlerstand::operator==(const Zaehlerstand &o) const {
    return einsaetzeGestartet == o.einsaetzeGestartet && einsaetzeAbgeschlossen == o.einsaetzeAbgeschlossen
        && einsaetzeAbgebrochen == o.einsaetzeAbgebrochen && pflugGestartet == o.pflugGestartet && pflugAbgeschlossen == o.pflugAbgeschlossen
        && schneeGeraeumt == o.schneeGeraeumt && baelleGeworfen == o.baelleGeworfen && trefferFahrzeug == o.trefferFahrzeug
        && trefferHeli == o.trefferHeli && baelleDaneben == o.baelleDaneben && streiks == o.streiks
        && beleidigteAbfluege == o.beleidigteAbfluege && sprueche == o.sprueche && seit == o.seit;
}

Statistik::Statistik(const QString &pfad) : m_pfad(pfad.isEmpty() ? standardPfad() : pfad) { laden(); }

QString Statistik::standardPfad() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/schneefall-statistik.conf");
}

void Statistik::verbuchen(const Statistikereignis &e) {
    using E = Statistikereignis;
    switch (e.art) {
    case E::EinsatzGestartet: ++m_z.einsaetzeGestartet; if (e.pflug) ++m_z.pflugGestartet; break;
    case E::EinsatzAbgeschlossen: ++m_z.einsaetzeAbgeschlossen; if (e.pflug) ++m_z.pflugAbgeschlossen; break;
    case E::EinsatzAbgebrochen: ++m_z.einsaetzeAbgebrochen; break;
    case E::SchneeGeraeumt: if (e.menge > 0 && std::isfinite(e.menge)) m_z.schneeGeraeumt += e.menge; break;
    case E::BallGeworfen: ++m_z.baelleGeworfen; break;
    case E::TrefferFahrzeug: ++m_z.trefferFahrzeug; break;
    case E::TrefferHeli: ++m_z.trefferHeli; break;
    case E::BallDaneben: ++m_z.baelleDaneben; break;
    case E::Streik: ++m_z.streiks; break;
    case E::BeleidigterAbflug: ++m_z.beleidigteAbfluege; break;
    case E::Spruch: ++m_z.sprueche; break;
    }
    m_geaendert = true;
}

void Statistik::verbuchen(const std::vector<Statistikereignis> &e) {
    for (const Statistikereignis &x : e) verbuchen(x);
}

void Statistik::zuruecksetzen() {
    m_z = Zaehlerstand{};
    m_z.seit = QDateTime::currentDateTimeUtc();
    m_geaendert = true;
}

QString Statistik::alsText(const Zaehlerstand &z) {
    return QStringLiteral(
               "# Schneefall-Statistik (wird vom Programm geschrieben; Löschen setzt die Zählung zurück)\n"
               "seit=%1\n"
               "einsaetze_gestartet=%2\neinsaetze_abgeschlossen=%3\neinsaetze_abgebrochen=%4\n"
               "pflug_gestartet=%5\npflug_abgeschlossen=%6\n"
               "schnee_geraeumt=%7\n"
               "baelle_geworfen=%8\ntreffer_fahrzeug=%9\ntreffer_hubschrauber=%10\nbaelle_daneben=%11\n"
               "streiks=%12\nbeleidigte_abfluege=%13\nsprueche=%14\n")
        .arg(z.seit.isValid() ? z.seit.toUTC().toString(Qt::ISODate) : QString())
        .arg(z.einsaetzeGestartet).arg(z.einsaetzeAbgeschlossen).arg(z.einsaetzeAbgebrochen)
        .arg(z.pflugGestartet).arg(z.pflugAbgeschlossen)
        .arg(z.schneeGeraeumt, 0, 'f', 1)
        .arg(z.baelleGeworfen).arg(z.trefferFahrzeug).arg(z.trefferHeli).arg(z.baelleDaneben)
        .arg(z.streiks).arg(z.beleidigteAbfluege).arg(z.sprueche);
}

bool Statistik::ausText(const QString &text, Zaehlerstand &z) {
    Zaehlerstand n;
    int erkannt = 0;
    auto zahl = [](const QString &w, qint64 &ziel) {
        bool ok = false;
        const qint64 v = w.trimmed().toLongLong(&ok);
        if (ok && v >= 0) { ziel = v; return true; }
        return false;
    };
    for (const QString &roh : text.split(QLatin1Char('\n'))) {
        const QString zeile = roh.trimmed();
        const int eq = zeile.indexOf(QLatin1Char('='));
        if (zeile.isEmpty() || zeile.startsWith(QLatin1Char('#')) || eq <= 0) continue;
        const QString k = zeile.left(eq).trimmed(), w = zeile.mid(eq + 1);
        bool ok = false;
        if (k == QLatin1String("seit")) {
            const QDateTime t = QDateTime::fromString(w.trimmed(), Qt::ISODate);
            if (t.isValid()) { n.seit = t.toUTC(); ok = true; }
        } else if (k == QLatin1String("schnee_geraeumt")) {
            const double v = w.trimmed().toDouble(&ok);
            if (ok && std::isfinite(v) && v >= 0) n.schneeGeraeumt = v; else ok = false;
        }
        else if (k == QLatin1String("einsaetze_gestartet")) ok = zahl(w, n.einsaetzeGestartet);
        else if (k == QLatin1String("einsaetze_abgeschlossen")) ok = zahl(w, n.einsaetzeAbgeschlossen);
        else if (k == QLatin1String("einsaetze_abgebrochen")) ok = zahl(w, n.einsaetzeAbgebrochen);
        else if (k == QLatin1String("pflug_gestartet")) ok = zahl(w, n.pflugGestartet);
        else if (k == QLatin1String("pflug_abgeschlossen")) ok = zahl(w, n.pflugAbgeschlossen);
        else if (k == QLatin1String("baelle_geworfen")) ok = zahl(w, n.baelleGeworfen);
        else if (k == QLatin1String("treffer_fahrzeug")) ok = zahl(w, n.trefferFahrzeug);
        else if (k == QLatin1String("treffer_hubschrauber")) ok = zahl(w, n.trefferHeli);
        else if (k == QLatin1String("baelle_daneben")) ok = zahl(w, n.baelleDaneben);
        else if (k == QLatin1String("streiks")) ok = zahl(w, n.streiks);
        else if (k == QLatin1String("beleidigte_abfluege")) ok = zahl(w, n.beleidigteAbfluege);
        else if (k == QLatin1String("sprueche")) ok = zahl(w, n.sprueche);
        if (ok) ++erkannt;
    }
    if (erkannt == 0) return false;
    if (!n.seit.isValid()) n.seit = QDateTime::currentDateTimeUtc();
    z = n;
    return true;
}

bool Statistik::laden() {
    m_z = Zaehlerstand{};
    m_geaendert = false;
    QFile f(m_pfad);
    bool ok = f.open(QIODevice::ReadOnly) && ausText(QString::fromUtf8(f.read(1 << 20)), m_z);
    if (!ok) {
        m_z = Zaehlerstand{};
        m_z.seit = QDateTime::currentDateTimeUtc();
        m_geaendert = f.exists();   // kaputte Datei: beim nächsten Speichern durch eine saubere ersetzen
    }
    return ok;
}

bool Statistik::speichern() {
    if (!m_geaendert && QFile::exists(m_pfad)) return true;
    QDir().mkpath(QFileInfo(m_pfad).absolutePath());
    QSaveFile f(m_pfad);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(alsText(m_z).toUtf8());
    if (!f.commit()) return false;
    m_geaendert = false;
    return true;
}

QString Statistik::statusZeile() const {
    const Zaehlerstand &z = m_z;
    const double q = z.trefferquote();
    return QStringLiteral("statistik=geraeumt:%1,gestartet:%2,abgebrochen:%3,pflug:%4,schnee:%5,baelle:%6,treffer:%7,heli:%8,daneben:%9,quote:%10,streiks:%11,beleidigt:%12,sprueche:%13")
        .arg(z.einsaetzeAbgeschlossen).arg(z.einsaetzeGestartet).arg(z.einsaetzeAbgebrochen).arg(z.pflugAbgeschlossen)
        .arg(z.schneeGeraeumt, 0, 'f', 0).arg(z.baelleGeworfen).arg(z.trefferFahrzeug).arg(z.trefferHeli).arg(z.baelleDaneben)
        .arg(q < 0 ? QStringLiteral("-") : QString::number(q, 'f', 0))
        .arg(z.streiks).arg(z.beleidigteAbfluege).arg(z.sprueche);
}

QString Statistik::kurzText() const {
    return QStringLiteral("Schneefall – %1× geräumt, %2 Treffer, %3 Streiks")
        .arg(m_z.einsaetzeAbgeschlossen).arg(m_z.trefferGesamt()).arg(m_z.streiks);
}

} // namespace schnee
