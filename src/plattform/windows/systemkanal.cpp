// Windows-Systemkanal: gibt es nicht – Einzelinstanz und Befehle laufen allein über den Steuerkanal
// (QLocalServer, benannte Pipe je Benutzer, siehe plattform/steuerkanal.cpp).
#include "plattform/steuerkanal.h"

namespace plattform {

std::unique_ptr<Systemkanal> systemkanalErzeugen(Befehlsziel, Fensterquelle &) { return nullptr; }

std::optional<Antwort> systemkanalSenden(const QString &, const QStringList &) { return std::nullopt; }

} // namespace plattform
