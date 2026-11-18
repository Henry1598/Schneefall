#include "plattform/linux/kwinfensterquelle.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <algorithm>
#include <cmath>

namespace plattform {

namespace {
// Fenster(json) kann jeder Prozess der Sitzung aufrufen: Größe, Anzahl und Werte begrenzen
constexpr qsizetype MAX_JSON = 1024 * 1024; // Zeichen; größere Nachrichten werden verworfen
constexpr qsizetype MAX_FENSTER = 1000;
constexpr qsizetype MAX_ID = 128;
constexpr double MAX_LAGE = 100000;  // |x|, |y|
constexpr double MAX_GROESSE = 100000; // w, h

// Zahl aus dem Eintrag: fehlt = 0; keine Zahl oder nicht endlich = ungültig (std::nullopt)
std::optional<double> zahl(const QJsonObject &o, const QString &schluessel, double min, double max) {
    const QJsonValue v = o.value(schluessel);
    if (v.isUndefined() || v.isNull()) return 0.0;
    if (!v.isDouble()) return std::nullopt;
    const double x = v.toDouble();
    if (!std::isfinite(x)) return std::nullopt;
    return std::clamp(x, min, max);
}

std::optional<QJsonArray> pruefen(const QString &json) {
    if (json.size() > MAX_JSON) return std::nullopt;
    QJsonParseError fehler;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &fehler);
    if (fehler.error != QJsonParseError::NoError || !doc.isArray()) return std::nullopt;
    return doc.array();
}

std::vector<schnee::Fenster> ausListe(const QJsonArray &liste) {
    std::vector<schnee::Fenster> f;
    f.reserve(size_t(std::min(liste.size(), MAX_FENSTER)));
    for (qsizetype i = 0; i < liste.size() && i < MAX_FENSTER; ++i) {
        const QJsonObject o = liste.at(i).toObject();
        const QString id = o.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || id.size() > MAX_ID) continue;
        if (std::any_of(id.begin(), id.end(), [](QChar c) { return c.category() == QChar::Other_Control; })) continue; // KWin-ids sind UUIDs

        const auto x = zahl(o, QStringLiteral("x"), -MAX_LAGE, MAX_LAGE), y = zahl(o, QStringLiteral("y"), -MAX_LAGE, MAX_LAGE);
        const auto w = zahl(o, QStringLiteral("w"), 0, MAX_GROESSE), h = zahl(o, QStringLiteral("h"), 0, MAX_GROESSE);
        const auto s = zahl(o, QStringLiteral("s"), -1e6, 1e6);
        if (!x || !y || !w || !h || !s) continue;
        schnee::Fenster fw;
        fw.id = id.toStdString();
        fw.r = {*x, *y, *w, *h};
        fw.stapel = int(*s);
        fw.minimiert = o.value(QStringLiteral("min")).toBool();
        fw.vollbild = o.value(QStringLiteral("fs")).toBool();
        fw.aktiv = o.value(QStringLiteral("akt")).toBool();
        fw.dock = o.value(QStringLiteral("dock")).toBool();
        f.push_back(std::move(fw));
    }
    return f;
}

// für --fensterliste: die übernommene Liste neu serialisiert (QJsonDocument maskiert Steuerzeichen als \uXXXX)
QString diagnoseText(const std::vector<schnee::Fenster> &f) {
    QJsonArray a;
    for (const schnee::Fenster &w : f) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), QString::fromStdString(w.id));
        o.insert(QStringLiteral("x"), w.r.x);
        o.insert(QStringLiteral("y"), w.r.y);
        o.insert(QStringLiteral("w"), w.r.w);
        o.insert(QStringLiteral("h"), w.r.h);
        o.insert(QStringLiteral("s"), w.stapel);
        o.insert(QStringLiteral("min"), w.minimiert);
        o.insert(QStringLiteral("fs"), w.vollbild);
        o.insert(QStringLiteral("akt"), w.aktiv);
        o.insert(QStringLiteral("dock"), w.dock);
        a.append(o);
    }
    return QString::fromUtf8(QJsonDocument(a).toJson(QJsonDocument::Compact));
}
} // namespace

std::vector<schnee::Fenster> fensterAusKwinJson(const QString &json) {
    const auto liste = pruefen(json);
    return liste ? ausListe(*liste) : std::vector<schnee::Fenster>{};
}

void KwinFensterquelle::listeEmpfangen(const QString &json) {
    if (!m_empfaenger) return;          // noch nicht gestartet: der nächste Puls bringt die Liste
    if (json == m_roh) return;          // Puls ohne Änderung
    const auto liste = pruefen(json);
    if (!liste) return;                 // zu groß oder kein JSON-Array: verwerfen, nichts merken
    m_roh = json;
    std::vector<schnee::Fenster> f = ausListe(*liste);
    m_letzte = diagnoseText(f);
    m_empfaenger(std::move(f));
}

std::unique_ptr<Fensterquelle> fensterquelleErzeugen() { return std::make_unique<KwinFensterquelle>(); }

} // namespace plattform
