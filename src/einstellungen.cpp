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

const char *modusText(Fenstermodus m) {
    switch (m) {
    case Fenstermodus::Dahinter: return "dahinter";
    case Fenstermodus::Davor: return "davor";
    case Fenstermodus::Fest: break;
    }
    return "fest";
}

} // namespace

bool einstellungenBrauchtErgaenzung(const std::string &text) {
    for (const char *k : {"fenstermodus", "schuetteln_schwelle", "rutsch_ab_minuten", "rutsch_tempo", "wolke_dauer", "wolke_liegen", "boden_max"})
        if (text.find(std::string("\n") + k + "=") == std::string::npos && text.rfind(std::string(k) + "=", 0) != 0) return true;
    return text.find("vor_fenstern") != std::string::npos;
}

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
        // vor_fenstern (bis 18.11.) wird nicht mehr gelesen: Henry will jetzt „fest“, beim Speichern verschwindet er
        else if (k == "fenstermodus") {
            std::transform(w.begin(), w.end(), w.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (w == "fest") p.sim.modus = Fenstermodus::Fest;
            else if (w == "dahinter") p.sim.modus = Fenstermodus::Dahinter;
            else if (w == "davor") p.sim.modus = Fenstermodus::Davor;
        }
        else if (k == "schuetteln_schwelle") setzen(p.sim.schuettelnSchwelle, w, 1000.0, 100000.0);
        else if (k == "rutsch_ab_minuten") setzen(p.sim.rutschAbMinuten, w, 0.0, 120.0);
        else if (k == "rutsch_tempo") setzen(p.sim.rutschTempo, w, 0.2, 3.0);
        else if (k == "wolke_dauer") setzen(p.sim.wolkeDauer, w, 1.0, 60.0);
        else if (k == "wolke_liegen") { double v = p.sim.wolkeLiegen * 100; setzen(v, w, 0.0, 100.0); p.sim.wolkeLiegen = v / 100; }
        else if (k == "boden_max") setzen(p.sim.bodenMax, w, 2.0, 80.0);
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
      << "# Fenster: fest (Schnee landet auf jedem sichtbaren Fenster, nie davor oder dahinter),\n"
      << "# dahinter (fällt scheinbar hinter den Fenstern) oder davor (fällt vor den Fenstern)\n"
      << "fenstermodus=" << modusText(p.sim.modus) << "\n"
      << "# Schütteln: ab dieser Beschleunigung eines gezogenen Fensters in px/s² fällt Schnee ab (1000 … 100000)\n"
      << "schuetteln_schwelle=" << fmt(p.sim.schuettelnSchwelle) << "\n"
      << "# Minimieren/Schließen: war das Fenster so viele Minuten offen (oder ist die Decke fast voll),\n"
      << "# rutscht der Schnee als Brett ab, sonst rieselt er als Wolke herunter (0 … 120)\n"
      << "rutsch_ab_minuten=" << fmt(p.sim.rutschAbMinuten) << "\n"
      << "# Tempo des Schneebretts (0.2 … 3, 1 = normal)\n"
      << "rutsch_tempo=" << fmt(p.sim.rutschTempo) << "\n"
      << "# so viele Sekunden hält sich die Schneewolke (1 … 60)\n"
      << "wolke_dauer=" << fmt(p.sim.wolkeDauer) << "\n"
      << "# so viel Prozent der Wolke bleiben unten liegen, der Rest löst sich auf (0 … 100)\n"
      << "wolke_liegen=" << fmt(std::round(p.sim.wolkeLiegen * 1000) / 10) << "\n"
      << "# höchste Schneedecke auf Boden und Taskleiste in px (2 … 80)\n"
      << "boden_max=" << fmt(p.sim.bodenMax) << "\n"
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
