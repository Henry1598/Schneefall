#include "einstellungen.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>

namespace schnee {
namespace {

std::string trimmen(const std::string &s) {
    const char *ws = " \t\r\n";
    size_t a = s.find_first_not_of(ws);
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(ws);
    return s.substr(a, b - a + 1);
}

bool zahl(const std::string &t, double &aus) {
    double v = 0;
    auto [p, ec] = std::from_chars(t.data(), t.data() + t.size(), v);
    if (ec != std::errc() || p != t.data() + t.size()) return false;
    aus = v;
    return true;
}

bool wahrheit(std::string t, bool &aus) {
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (t == "true" || t == "ja" || t == "1" || t == "an" || t == "yes") { aus = true; return true; }
    if (t == "false" || t == "nein" || t == "0" || t == "aus" || t == "no") { aus = false; return true; }
    return false;
}

void setzen(double &ziel, const std::string &wert, double min, double max) {
    double v;
    if (zahl(wert, v)) ziel = std::clamp(v, min, max);
}

std::string fmt(double v) {
    char buf[64];
    auto [p, ec] = std::to_chars(buf, buf + sizeof buf, v);
    return std::string(buf, p);
}

} // namespace

Programmeinstellungen einstellungenAusText(const std::string &text) {
    Programmeinstellungen p;
    std::istringstream in(text);
    std::string zeile;
    while (std::getline(in, zeile)) {
        zeile = trimmen(zeile);
        if (zeile.empty() || zeile[0] == '#' || zeile[0] == '[') continue;
        size_t gl = zeile.find('=');
        if (gl == std::string::npos) continue;
        std::string k = trimmen(zeile.substr(0, gl));
        std::string w = trimmen(zeile.substr(gl + 1));
        if (k == "menge") setzen(p.sim.menge, w, 0.0, 10.0);
        else if (k == "wind") setzen(p.sim.wind, w, -200.0, 200.0);
        else if (k == "flockengroesse") setzen(p.sim.groesse, w, 1.0, 12.0);
        else if (k == "max_deckenhoehe") setzen(p.sim.maxHoehe, w, 2.0, 80.0);
        else if (k == "fallgeschwindigkeit") setzen(p.sim.fallGeschw, w, 5.0, 300.0);
        else if (k == "fps") { double v = p.fps; setzen(v, w, 10.0, 120.0); p.fps = int(v); }
        else if (k == "an") wahrheit(w, p.an);
        else if (k == "vor_fenstern") wahrheit(w, p.sim.vorFenstern);
        else if (k == "vollbild_pausieren") wahrheit(w, p.vollbildPausieren);
        else if (k == "ton") wahrheit(w, p.ton);
        else if (k == "lautstaerke") { double v = p.lautstaerke; setzen(v, w, 0.0, 100.0); p.lautstaerke = int(std::lround(v)); }
        else if (k == "flockendetail") {
            std::transform(w.begin(), w.end(), w.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (w == "realistisch") p.realistisch = true;
            else if (w == "einfach") p.realistisch = false;
        }
    }
    return p;
}

std::string einstellungenAlsText(const Programmeinstellungen &p) {
    std::ostringstream o;
    o << "# Schneefall – Einstellungen (wird vom Steuerfenster überschrieben)\n"
      << "# Menge: Flocken je 10 000 px² (0 … 10)\n"
      << "menge=" << fmt(p.sim.menge) << "\n"
      << "# Wind in px/s, negativ = nach links, positiv = nach rechts (-200 … 200)\n"
      << "wind=" << fmt(p.sim.wind) << "\n"
      << "# mittlerer Flockendurchmesser in px (1 … 12)\n"
      << "flockengroesse=" << fmt(p.sim.groesse) << "\n"
      << "# höchste Schneedecke auf Fenstern in px (2 … 80)\n"
      << "max_deckenhoehe=" << fmt(p.sim.maxHoehe) << "\n"
      << "# mittlere Fallgeschwindigkeit in px/s (5 … 300)\n"
      << "fallgeschwindigkeit=" << fmt(p.sim.fallGeschw) << "\n"
      << "# Bilder pro Sekunde (10 … 120)\n"
      << "fps=" << p.fps << "\n"
      << "# Schnee an (true) oder pausiert (false)\n"
      << "an=" << (p.an ? "true" : "false") << "\n"
      << "# Schnee fällt vor den Fenstern (true) oder nur über der Arbeitsfläche, also scheinbar dahinter (false)\n"
      << "vor_fenstern=" << (p.sim.vorFenstern ? "true" : "false") << "\n"
      << "# bei aktivem Vollbildfenster (Spiele, Videos) pausieren\n"
      << "vollbild_pausieren=" << (p.vollbildPausieren ? "true" : "false") << "\n"
      << "# Flockendetail: realistisch (Schneekristalle, nahe Flocken groß) oder einfach (weiche Punkte)\n"
      << "flockendetail=" << (p.realistisch ? "realistisch" : "einfach") << "\n"
      << "# Geräusche (Hubschrauber, Motor, …) an (true) oder aus (false)\n"
      << "ton=" << (p.ton ? "true" : "false") << "\n"
      << "# Lautstärke der Geräusche (0 … 100)\n"
      << "lautstaerke=" << p.lautstaerke << "\n";
    return o.str();
}

} // namespace schnee
