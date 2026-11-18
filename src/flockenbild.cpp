#include "flockenbild.h"
#include <algorithm>
#include <cmath>

namespace schnee {
namespace {
uint32_t mischen(uint32_t h) {
    h ^= h >> 16; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
    return h;
}
float anteil(uint32_t h) { return float(h & 0xffffu) / 65536.f; } // 0 … <1
} // namespace

float sinusSchnell(float winkel) {
    static const struct Tabelle {
        float w[1024];
        Tabelle() { for (int i = 0; i < 1024; ++i) w[i] = float(std::sin(i * 6.283185307179586 / 1024)); }
    } tabelle;
    return tabelle.w[int(std::lround(winkel * (1024.f / 6.2831853f))) & 1023];
}

Flockenbild flockenbild(const Flocke &f, double grundgroesse, bool realistisch) {
    Flockenbild b;
    if (!realistisch) {
        b.sprite = Sprite::Punkt;
        b.durchmesser = float(f.groesse + 1.6);
        b.deckkraft = 0.92f;
        return b;
    }
    // feste Zufallswerte je Flocke: Form, Anfangswinkel, Drehrichtung und -tempo, Taumeln
    const uint32_t h1 = mischen(f.los ^ 0x9e3779b9u), h2 = mischen(h1 + 0x7f4a7c15u), h3 = mischen(h2 + 0x1234567u);
    b.variante = int(h1 >> 16);
    const float tempo = (0.06f + 0.34f * anteil(h2)) * ((h2 >> 20) & 1u ? 1.f : -1.f);
    b.winkel = anteil(h1) * 6.2831853f + float(f.phase) * tempo;
    b.stauchung = 0.82f + 0.18f * kosinusSchnell(float(f.phase) * (0.5f + 0.6f * anteil(h3)) + anteil(h3 >> 16) * 6.2831853f);

    if (f.art == Flockenart::Pulver) {
        b.sprite = Sprite::Pulver;
        b.durchmesser = float(f.groesse) * (1.3f + 0.7f * anteil(h3)) + 0.5f;
        b.deckkraft = 0.9f;
        b.stauchung = 1.f;
        return b;
    }
    // Tiefe aus der Größe: neue Flocken sind 0,5- bis 1,5-mal so groß wie eingestellt
    const float g = float(std::max(0.5, grundgroesse));
    const float t = std::clamp(float(f.groesse) / g - 0.5f, 0.f, 0.9999f);
    if (t < 0.5f) { // fern: kleiner, blasser, weicher Punkt
        const float s = t / 0.5f;
        b.sprite = Sprite::Punkt;
        b.durchmesser = g * (0.6f + 0.8f * s) + 1.f;
        b.deckkraft = 0.45f + 0.4f * s;
        b.stauchung = 1.f;
    } else if (t < 0.88f) { // Mittelgrund: Kristallform beim Hinsehen erkennbar
        const float s = (t - 0.5f) / 0.38f;
        b.sprite = Sprite::Kristall;
        b.durchmesser = g * (2.5f + 2.5f * s);
        b.deckkraft = 0.88f + 0.08f * s;
    } else if (t < 0.985f) { // nah: groß und klar
        const float s = (t - 0.88f) / 0.105f;
        b.sprite = Sprite::Kristall;
        b.durchmesser = g * (5.f + 2.5f * s);
        b.deckkraft = 0.95f;
    } else { // ganz vorn: sehr groß, leicht unscharf
        const float s = (t - 0.985f) / 0.015f;
        b.sprite = Sprite::Weich;
        b.durchmesser = g * (8.f + 3.f * s);
        b.deckkraft = 0.62f;
    }
    b.durchmesser = std::min(b.durchmesser, 64.f);
    return b;
}

} // namespace schnee
