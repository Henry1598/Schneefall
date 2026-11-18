#include "plattform/linux/kwinfensterquelle.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace plattform {

std::vector<schnee::Fenster> fensterAusKwinJson(const QString &json) {
    const QJsonArray liste = QJsonDocument::fromJson(json.toUtf8()).array();
    std::vector<schnee::Fenster> f;
    f.reserve(size_t(liste.size()));
    for (const QJsonValue &v : liste) {
        QJsonObject o = v.toObject();
        schnee::Fenster w;
        w.id = o.value(QStringLiteral("id")).toString().toStdString();
        w.r = {o.value(QStringLiteral("x")).toDouble(), o.value(QStringLiteral("y")).toDouble(),
               o.value(QStringLiteral("w")).toDouble(), o.value(QStringLiteral("h")).toDouble()};
        w.stapel = o.value(QStringLiteral("s")).toInt();
        w.minimiert = o.value(QStringLiteral("min")).toBool();
        w.vollbild = o.value(QStringLiteral("fs")).toBool();
        w.aktiv = o.value(QStringLiteral("akt")).toBool();
        w.dock = o.value(QStringLiteral("dock")).toBool();
        if (!w.id.empty()) f.push_back(std::move(w));
    }
    return f;
}

void KwinFensterquelle::listeEmpfangen(const QString &json) {
    if (!m_empfaenger) return;          // noch nicht gestartet: der nächste Puls bringt die Liste
    if (json == m_letzte) return;       // Puls ohne Änderung
    m_letzte = json;
    m_empfaenger(fensterAusKwinJson(json));
}

std::unique_ptr<Fensterquelle> fensterquelleErzeugen() { return std::make_unique<KwinFensterquelle>(); }

} // namespace plattform
