#include "sprueche.h"
#include <QCoreApplication>
#include <QFile>
#include <QSet>
#include <QStringList>
#include <algorithm>

namespace schnee {
namespace {
const int MAX_ZEICHEN = 80; // Sprechblase: längere Zeilen aus der Datei werden ignoriert

bool sprecherVon(const QString &t, int &aus) {
    if (t == QLatin1String("fahrer")) aus = int(Sprecher::Fahrer);
    else if (t == QLatin1String("pilot")) aus = int(Sprecher::Pilot);
    else return false;
    return true;
}
bool anlassVon(const QString &t, int &aus) {
    static const char *const NAMEN[] = {"treffer", "streik", "beleidigt_abflug", "gruss", "heli_treffer", "heli_abholen", "heli_abflug"};
    for (int i = 0; i < 7; ++i)
        if (t == QLatin1String(NAMEN[i])) { aus = i; return true; }
    return false;
}
} // namespace

int spruchStufeFuerTreffer(int trefferzahl, int maxStufe) {
    const int deckel = std::clamp(maxStufe, SPRUCH_STUFE_MIN, SPRUCH_STUFE_MAX);
    return std::clamp(trefferzahl, SPRUCH_STUFE_MIN, deckel);
}

const char *stufenName(int stufe) {
    switch (std::clamp(stufe, SPRUCH_STUFE_MIN, SPRUCH_STUFE_MAX)) {
    case 1: return "freundlich";
    case 2: return "genervt";
    case 3: return "frech";
    case 4: return "derb";
    default: return "richtig vulgär";
    }
}

Sprueche::Sprueche() : m_zufall(std::random_device{}()) { ersatzLaden(); }

void Sprueche::ersatzLaden() {
    m_tabelle.clear();
    auto add = [this](Sprecher s, Anlass a, int st, const char *t) {
        m_tabelle[{int(s), int(a), st}].push_back(QString::fromUtf8(t));
    };
    add(Sprecher::Fahrer, Anlass::Treffer, 1, "Hey! Ich arbeite hier!");
    add(Sprecher::Fahrer, Anlass::Treffer, 2, "Wer war das?!");
    add(Sprecher::Fahrer, Anlass::Treffer, 3, "So nicht, Freundchen!");
    add(Sprecher::Fahrer, Anlass::Streik, 1, "Streik! Ich mach erst mal Pause.");
    add(Sprecher::Fahrer, Anlass::Streik, 2, "Das melde ich der Gewerkschaft!");
    add(Sprecher::Fahrer, Anlass::BeleidigtAbflug, 1, "Feierabend. Tschüss!");
    add(Sprecher::Fahrer, Anlass::BeleidigtAbflug, 2, "Mir reicht's! Ich fahr heim!");
    add(Sprecher::Fahrer, Anlass::Gruss, 1, "Moin!");
    add(Sprecher::Pilot, Anlass::HeliTreffer, 1, "Hoppla, hier fliegt ein Schneeball!");
    add(Sprecher::Pilot, Anlass::HeliTreffer, 2, "Hey, ich versuche hier zu fliegen!");
    add(Sprecher::Pilot, Anlass::HeliAbholen, 1, "Hubschrauber im Anflug, Einstieg bitte!");
    add(Sprecher::Pilot, Anlass::HeliAbflug, 1, "Abflug! Bis zum nächsten Schnee!");
    m_eingebaut = true;
}

int Sprueche::ladeText(const QByteArray &tsv) {
    std::map<Schluessel, std::vector<QString>> neu;
    QSet<QString> gesehen;
    int gueltig = 0;
    for (const QByteArray &roh : tsv.split('\n')) {
        const QString zeile = QString::fromUtf8(roh).trimmed();
        if (zeile.isEmpty() || zeile.startsWith(QLatin1Char('#'))) continue;
        const QStringList t = zeile.split(QLatin1Char('\t'));
        if (t.size() != 4) continue;
        int sp = 0, an = 0;
        bool ok = false;
        const int st = t[2].trimmed().toInt(&ok);
        const QString text = t[3].trimmed();
        if (!sprecherVon(t[0].trimmed(), sp) || !anlassVon(t[1].trimmed(), an) || !ok || st < SPRUCH_STUFE_MIN || st > SPRUCH_STUFE_MAX) continue;
        if (text.isEmpty() || text.size() > MAX_ZEICHEN) continue;
        if (gesehen.contains(QString::number(sp) + QLatin1Char('|') + text)) continue; // Dublette
        gesehen.insert(QString::number(sp) + QLatin1Char('|') + text);
        neu[{sp, an, st}].push_back(text);
        ++gueltig;
    }
    if (gueltig == 0) return 0;
    m_tabelle = std::move(neu);
    m_eingebaut = false;
    m_zuletzt[0].clear();
    m_zuletzt[1].clear();
    return gueltig;
}

int Sprueche::ladeDatei(const QString &pfad) {
    QFile f(pfad);
    if (!f.open(QIODevice::ReadOnly)) return 0;
    return ladeText(f.readAll());
}

int Sprueche::ladeStandard() {
    if (const int n = ladeDatei(QStringLiteral(":/daten/sprueche.tsv"))) return n;
    const QString dir = QCoreApplication::applicationDirPath();
    for (const QString &p : {dir + QStringLiteral("/sprueche.tsv"), dir + QStringLiteral("/daten/sprueche.tsv"),
                             dir + QStringLiteral("/../daten/sprueche.tsv"), dir + QStringLiteral("/../../daten/sprueche.tsv")})
        if (const int n = ladeDatei(p)) return n;
    return 0; // Ersatzsätze bleiben
}

int Sprueche::anzahl() const {
    int n = 0;
    for (const auto &[k, v] : m_tabelle) n += int(v.size());
    return n;
}

int Sprueche::anzahl(Sprecher s, Anlass a, int stufe) const {
    auto it = m_tabelle.find({int(s), int(a), stufe});
    return it == m_tabelle.end() ? 0 : int(it->second.size());
}

QString Sprueche::waehle(Sprecher s, Anlass a, int stufe) {
    const int wunsch = std::clamp(std::min(stufe, m_maxStufe), SPRUCH_STUFE_MIN, SPRUCH_STUFE_MAX);
    // erst abwärts bis Stufe 1, dann (Notfall) aufwärts bis zur Einstellungsgrenze – nie darüber
    const std::vector<QString> *quelle = nullptr;
    auto probiere = [&](int st) {
        auto it = m_tabelle.find({int(s), int(a), st});
        if (it != m_tabelle.end() && !it->second.empty()) { quelle = &it->second; return true; }
        return false;
    };
    for (int st = wunsch; st >= SPRUCH_STUFE_MIN && !quelle; --st) probiere(st);
    for (int st = wunsch + 1; st <= m_maxStufe && !quelle; ++st) probiere(st);
    if (!quelle) return QString();

    std::deque<QString> &zuletzt = m_zuletzt[int(s)];
    std::vector<const QString *> frei;
    for (const QString &t : *quelle)
        if (std::find(zuletzt.begin(), zuletzt.end(), t) == zuletzt.end()) frei.push_back(&t);
    const QString *wahl = nullptr;
    if (!frei.empty()) {
        wahl = frei[std::uniform_int_distribution<size_t>(0, frei.size() - 1)(m_zufall)];
    } else { // alles kürzlich gesagt: den am längsten zurückliegenden nehmen
        size_t best = zuletzt.size();
        for (const QString &t : *quelle) {
            const size_t pos = size_t(std::find(zuletzt.begin(), zuletzt.end(), t) - zuletzt.begin());
            if (pos < best) { best = pos; wahl = &t; }
        }
    }
    QString text = *wahl;
    zuletzt.erase(std::remove(zuletzt.begin(), zuletzt.end(), text), zuletzt.end());
    zuletzt.push_back(text);
    while (int(zuletzt.size()) > SPRUCH_FENSTER) zuletzt.pop_front();
    return text;
}

Sprueche &Sprueche::instanz() {
    static Sprueche s = [] { Sprueche x; x.ladeStandard(); return x; }();
    return s;
}

} // namespace schnee
