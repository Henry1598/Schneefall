#include "bildtakt.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace schnee {

double uhrSekunden() {
    using namespace std::chrono;
    static const steady_clock::time_point anfang = steady_clock::now();
    return duration<double>(steady_clock::now() - anfang).count();
}

void Bildzeiten::bild(double t) {
    const double d = t - m_letzt;
    // erstes Bild oder lange Lücke (Ebene verborgen, Pause): kein Abstand, die Messung setzt neu an
    if (m_letzt >= 0 && d >= 0 && d < std::max(0.25, 10 * m_soll)) m_abstaende.push_back(float(d));
    m_letzt = t;
}

Bildzeiten::Bericht Bildzeiten::abholen() {
    Bericht b;
    std::vector<float> a;
    a.swap(m_abstaende);
    b.bilder = int(a.size());
    if (a.empty()) return b;
    double summe = 0, quadrate = 0;
    for (float d : a) {
        summe += d;
        quadrate += double(d) * d;
        const double v = d / m_soll;
        const int stufe = v < 0.5 ? 0 : v < 0.9 ? 1 : v < 1.1 ? 2 : v < 1.5 ? 3 : v < 2.5 ? 4 : 5;
        ++b.stufen[size_t(stufe)];
        if (v > 1.5) ++b.verpasst;
    }
    const double n = double(a.size()), mittel = summe / n;
    b.mittelMs = mittel * 1000;
    b.jitterMs = std::sqrt(std::max(0.0, quadrate / n - mittel * mittel)) * 1000;
    b.fps = mittel > 0 ? 1 / mittel : 0;
    std::sort(a.begin(), a.end());
    auto quantil = [&a](double q) { return a[std::min(a.size() - 1, size_t(q * double(a.size())))] * 1000.0; };
    b.p50Ms = quantil(0.5);
    b.p95Ms = quantil(0.95);
    b.p99Ms = quantil(0.99);
    b.maxMs = a.back() * 1000.0;
    return b;
}

int bildTeiler(double monitorHz, int obergrenze) {
    if (obergrenze <= 0 || monitorHz <= 0) return 1;
    return std::max(1, int(std::ceil(monitorHz / obergrenze - 0.02)));
}

} // namespace schnee
