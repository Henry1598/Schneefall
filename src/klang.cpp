#include "klang.h"
#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

namespace {
constexpr double SR = Klang::Abtastrate;
constexpr float PI2 = 6.28318530718f;

// schnelle Sinusnäherung, Phase 0..1 (Fehler < 0,1 %)
inline float sinus(float ph) {
    float x = ph - std::floor(ph) - 0.5f; // -0.5 .. 0.5
    float y = 8.0f * x * (1.0f - 2.0f * std::fabs(x)); // Parabel, = -sin
    y = 0.225f * (y * std::fabs(y) - y) + y;
    return -y;
}

struct Rausch {
    uint32_t s = 2463534242u;
    float next() { // -1 .. 1
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return float(int32_t(s)) * (1.0f / 2147483648.0f);
    }
};

struct Tiefpass {
    float y = 0;
    float proc(float x, float k) { y += k * (x - y); return y; }
};
inline float koeff(float hz) { return 1.0f - std::exp(-PI2 * hz / float(SR)); }

// Zweipol-Resonator (Formant)
struct Formant {
    float a1 = 0, a2 = 0, g = 0, y1 = 0, y2 = 0;
    void setze(float f, float bw) {
        float r = std::exp(-3.14159265f * bw / float(SR));
        a1 = -2.0f * r * std::cos(PI2 * f / float(SR));
        a2 = r * r;
        g = 1.0f - r;
    }
    float proc(float x) {
        float y = g * x - a1 * y1 - a2 * y2;
        y2 = y1; y1 = y;
        return y;
    }
};

// Pegel -> (links, rechts): Equal-Power-Panning, zu den Rändern etwas leiser
struct Pan { float l, r; };
Pan panVon(double x, double links, double rechts, double basis = 1.0) {
    double breite = std::max(1.0, rechts - links);
    double p = std::clamp((x - links) / breite, 0.0, 1.0) * 2.0 - 1.0; // -1 .. 1
    double g = basis * (1.0 - 0.3 * std::fabs(p));
    double w = (p + 1.0) * 0.78539816339; // 0 .. pi/2
    return {float(g * std::cos(w)), float(g * std::sin(w))};
}

bool istDauerart(KlangArt a) { return a == KlangArt::Rotor || a == KlangArt::Motor || a == KlangArt::Rueckfahrwarner; }

struct Stimme {
    KlangArt art = KlangArt::Plumps;
    int id = -1;              // >= 0: Dauerklang
    bool dauer = false;
    bool ausklingen = false;  // Dauerklang: Stopp angefordert
    long long alter = 0;      // Reihenfolge für Stimmenklau
    // Lautstärke-/Panning-Ziele (werden pro Sample geglättet)
    float zl = 0, zr = 0, gl = 0, gr = 0;
    float hue = 0;            // Ein-/Ausblendhülle 0..1
    float hueSchritt = 1.0f / (0.05f * float(SR));
    // Einmalklang
    long long pos = 0, laenge = 0;
    float staerke = 1;
    // Dauerklang: Parameter
    float last = 0, dopplerZiel = 1, doppler = 1, freq = 0;
    double letztesX = 0;
    long long letzteZeit = -1;
    bool hatX = false;
    // Synthesezustand
    Rausch n;
    float ph = 0, ph2 = 0, ph3 = 0, e = 0, e2 = 0;
    int takt = 0;
    Tiefpass lp1, lp2, lp3, hp;
    Formant f1, f2, f3;
    float p0 = 1, p1 = 1;     // freie Zufallswerte je Stimme
};

inline float maxf(float a, float b) { return a > b ? a : b; }

// ---- Dauerklänge: ein Sample ----
inline float rotor(Stimme &s) {
    float d = s.doppler;
    float fb = s.freq * d;              // Blattfolge
    s.ph += fb / float(SR);
    if (s.ph >= 1.0f) {
        s.ph -= 1.0f;
        s.takt ^= 1;
        s.e = s.takt ? 1.0f : 0.72f;     // abwechselnd stärkere/schwächere Schläge
    }
    s.e *= 0.9992f;                     // Abklingen eines Schlags
    s.ph2 += 62.0f * d / float(SR);
    float thump = s.lp1.proc(s.e, 0.02f) * (sinus(s.ph2) * 0.8f + 0.35f * s.lp2.proc(s.n.next(), 0.15f));
    float rumpel = s.lp3.proc(s.n.next(), koeff(140.0f)) * (0.5f + 0.5f * s.e) * 0.9f;
    s.ph3 += 1650.0f * d / float(SR);
    float whine = 0.05f * sinus(s.ph3) + 0.02f * sinus(s.ph3 * 2.0f + 0.1f) + 0.012f * sinus(s.ph3 * 0.5f);
    whine *= 0.8f + 0.2f * sinus(s.ph * 0.25f);
    return 0.75f * thump + 0.25f * rumpel + whine;
}

inline float motor(Stimme &s) {
    float d = s.doppler;
    s.ph += s.freq * d / float(SR);     // Zündfolge
    if (s.ph >= 1.0f) {
        s.ph -= 1.0f;
        s.e = 0.75f + 0.25f * s.n.next(); // leichte Unregelmäßigkeit (Dieselnageln)
        s.e = std::fabs(s.e);
    }
    s.e *= 0.9965f - 0.0015f * s.last;
    float saege = 2.0f * s.ph - 1.0f;
    float k = koeff(260.0f + 900.0f * s.last);
    float ton = s.lp1.proc(saege, k);
    ton = s.lp2.proc(ton, k);
    float knack = s.lp3.proc(s.n.next(), koeff(1100.0f + 1500.0f * s.last)) * s.e;
    float brumm = 0.55f * sinus(s.ph * 0.5f);   // halbe Zündfrequenz: unrunder Lauf
    return (0.9f * ton + 0.35f * knack * (0.6f + 0.6f * s.last) + 0.18f * brumm) * (0.8f + 0.5f * s.last) * 0.8f;
}

inline float warner(Stimme &s) {
    // 1 Hz, halbe Zeit an; Pieps ~ 1 kHz mit etwas Oberton
    s.ph += 1.0f / float(SR);
    if (s.ph >= 1.0f) s.ph -= 1.0f;
    float an = s.ph < 0.5f ? 1.0f : 0.0f;
    s.e += 0.004f * 5.0f * (an - s.e) * (an > s.e ? 1.0f : 0.8f); // ~ 5 ms Ein-/Ausblenden
    s.ph2 += 1000.0f / float(SR);
    s.ph2 -= std::floor(s.ph2);
    float t = sinus(s.ph2) + 0.3f * sinus(s.ph2 * 3.0f) + 0.1f * sinus(s.ph2 * 5.0f);
    return 0.7f * s.e * t;
}

// ---- Einmalklänge ----
inline float rutsch(Stimme &s) {
    float t = float(s.pos) / float(s.laenge);
    float hue = std::sin(3.14159265f * std::pow(t, 0.6f));
    hue *= hue;
    float roh = s.n.next();
    float k = koeff(500.0f + 1800.0f * hue);
    float x = s.lp1.proc(roh, k);
    x -= s.hp.proc(x, koeff(180.0f));
    // Körnigkeit
    float koernig = 0.75f + 0.25f * s.lp2.proc(s.n.next(), 0.003f) * 8.0f;
    return 2.4f * x * hue * koernig;
}

inline float plumps(Stimme &s) {
    float t = float(s.pos) / float(SR);
    float f = (48.0f + 85.0f * std::exp(-t * 18.0f)) * (1.15f - 0.25f * std::min(s.staerke, 1.5f));
    s.ph += f / float(SR);
    float ton = sinus(s.ph) * std::exp(-t * 11.0f);
    float rausch = s.lp1.proc(s.n.next(), koeff(500.0f)) * std::exp(-t * 28.0f);
    return 2.0f * ton + 1.4f * rausch;
}

inline float wurf(Stimme &s) {
    float t = float(s.pos) / float(s.laenge);
    float hue = std::sin(3.14159265f * t);
    hue = hue * hue * (1.0f - 0.35f * t);
    float f = 500.0f + 2600.0f * std::sin(3.14159265f * std::pow(t, 0.7f));
    float x = s.lp1.proc(s.n.next(), koeff(f));
    x = s.lp2.proc(x, koeff(f * 1.3f));
    return 3.2f * x * hue;
}

inline float treffer(Stimme &s) {
    float t = float(s.pos) / float(SR);
    s.ph += (85.0f - 25.0f * std::min(t * 8.0f, 1.0f)) / float(SR);
    float ton = sinus(s.ph) * std::exp(-t * 30.0f);
    float dumpf = s.lp1.proc(s.n.next(), koeff(650.0f)) * std::exp(-t * 26.0f);
    // Knirschen: kurze Körner im gefilterten Rauschen
    float korn = s.lp2.proc(s.n.next(), 0.0004f * 70.0f);
    float knirsch = (korn > 0.25f ? 1.0f : 0.0f) * s.lp3.proc(s.n.next(), koeff(2200.0f)) * std::exp(-t * 18.0f) * 1.2f;
    return 0.9f * ton + 1.3f * dumpf + 0.6f * knirsch;
}

inline float murren(Stimme &s) {
    float t = float(s.pos) / float(SR);
    float T = float(s.laenge) / float(SR);
    float u = t / T;
    // zwei Silben „grum-mel“
    auto beule = [](float x) { return x <= 0 || x >= 1 ? 0.0f : std::sin(3.14159265f * x); };
    float hue = beule(u / 0.52f) + 0.75f * beule((u - 0.45f) / 0.55f);
    float f0 = (112.0f - 30.0f * u) * s.p0 * (1.0f + 0.015f * sinus(t * 5.5f));
    s.ph += f0 / float(SR);
    s.ph -= std::floor(s.ph);
    float quelle = (2.0f * s.ph - 1.0f) * 0.8f + 0.25f * s.n.next();
    // „r“: Knurren am Anfang der ersten Silbe
    if (u < 0.2f) quelle *= 0.65f + 0.35f * sinus(t * 32.0f);
    if ((s.pos & 31) == 0) {
        float v = std::clamp((u - 0.3f) / 0.4f, 0.0f, 1.0f); // „u“ -> „e/ö“ in der zweiten Silbe
        s.f1.setze(330.0f + 120.0f * v, 90.0f);
        s.f2.setze(780.0f + 650.0f * v, 120.0f);
        s.f3.setze(2400.0f + 100.0f * v, 220.0f);
    }
    float y = 2.2f * s.f1.proc(quelle) + 1.6f * s.f2.proc(quelle) + 0.5f * s.f3.proc(quelle);
    return 0.1f * y * hue;
}

float einmalLaenge(KlangArt a, float st) {
    switch (a) {
    case KlangArt::Rutsch: return 0.35f + 1.0f * std::clamp(st, 0.1f, 2.0f);
    case KlangArt::Plumps: return 0.45f;
    case KlangArt::Wurf: return 0.28f;
    case KlangArt::Treffer: return 0.28f;
    case KlangArt::Murren: return 0.75f;
    case KlangArt::Rueckfahrwarner: return 0.5f;
    default: return 1.5f;       // Rotor/Motor als Einmalklang: kurzer Schwall
    }
}

float grundfrequenz(KlangArt a, float last) {
    switch (a) {
    case KlangArt::Rotor: return 14.0f + 6.0f * std::clamp(last, 0.0f, 1.0f);
    case KlangArt::Motor: return 24.0f + 50.0f * std::clamp(last, 0.0f, 1.0f); // 720 … 2200 U/min, 4 Zylinder
    case KlangArt::Rueckfahrwarner: return 1000.0f;
    default: return 0;
    }
}
} // namespace

// ---------------- Audioausgabe ----------------
class KlangAusgabe : public QIODevice {
public:
    explicit KlangAusgabe(Klang *k) : m_k(k) { open(QIODevice::ReadOnly); }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return 1 << 20; }
protected:
    qint64 readData(char *data, qint64 maxlen) override {
        int frames = int(maxlen / 4);
        if (frames <= 0) return 0;
        m_puffer.resize(size_t(frames) * 2);
        m_k->render(m_puffer.data(), frames);
        auto *o = reinterpret_cast<int16_t *>(data);
        for (size_t i = 0; i < m_puffer.size(); ++i) o[i] = int16_t(std::lrintf(m_puffer[i] * 32767.0f));
        return qint64(frames) * 4;
    }
    qint64 writeData(const char *, qint64) override { return -1; }
private:
    Klang *m_k;
    std::vector<float> m_puffer;
};

struct Klang::Impl {
    bool mitGeraet;
    mutable std::mutex m;
    std::vector<Stimme> stimmen;
    bool an = false;
    float lautstaerke = 0.5f;
    double links = 0, rechts = 1920;
    float master = 0, masterZiel = 0, lautGeglaettet = 0;
    long long zeit = 0, zaehler = 0;
    long long stillSeit = 0;  // aufeinanderfolgende stille Samples
    // Gerät
    std::unique_ptr<QAudioSink> sink;
    std::unique_ptr<KlangAusgabe> ausgabe;
    QTimer *waechter = nullptr;

    Stimme *dauerStimme(int id) {
        for (auto &s : stimmen) if (s.dauer && s.id == id && !s.ausklingen) return &s;
        return nullptr;
    }
    int zaehleDauer() const { int n = 0; for (auto &s : stimmen) if (s.dauer && !s.ausklingen) ++n; return n; }
    int zaehleEinmal() const { int n = 0; for (auto &s : stimmen) if (!s.dauer && s.pos < s.laenge - 240) ++n; return n; }

    void setzePan(Stimme &s, double x, double basis = 1.0) {
        Pan p = panVon(x, links, rechts, basis);
        s.zl = p.l; s.zr = p.r;
    }
};

Klang::Klang(bool mitGeraet, QObject *parent) : QObject(parent), d(new Impl) {
    d->mitGeraet = mitGeraet;
    if (mitGeraet) {
        d->waechter = new QTimer(this);
        d->waechter->setInterval(250);
        connect(d->waechter, &QTimer::timeout, this, [this] {
            // Gerät schließen, wenn aus und ausgeblendet oder lange still (keine CPU im Leerlauf)
            if (!d->sink) return;
            bool zu;
            {
                std::lock_guard<std::mutex> lk(d->m);
                zu = d->stimmen.empty() && (d->stillSeit > 2 * Abtastrate || (!d->an && d->master <= 1e-4f));
            }
            if (zu) {
                d->sink->stop();
                d->sink.reset();
                d->ausgabe.reset();
            }
        });
        d->waechter->start();
    }
}

Klang::~Klang() {
    if (d->sink) d->sink->stop();
}

bool Klang::istAn() const { std::lock_guard<std::mutex> lk(d->m); return d->an; }
bool Klang::geraetOffen() const { return d->sink != nullptr; }

void Klang::setzeAn(bool an) {
    std::lock_guard<std::mutex> lk(d->m);
    d->an = an;
    d->masterZiel = an ? 1.0f : 0.0f;
    if (!an) {
        // laufende Dauerklänge ausblenden lassen, Einmalklänge verklingen mit dem Master
        for (auto &s : d->stimmen) if (s.dauer) s.ausklingen = true;
    }
}

void Klang::setzeLautstaerke(double l) {
    std::lock_guard<std::mutex> lk(d->m);
    d->lautstaerke = float(std::clamp(l, 0.0, 1.0));
}

void Klang::setzeWeltbreite(double links, double rechts) {
    std::lock_guard<std::mutex> lk(d->m);
    d->links = links;
    d->rechts = std::max(rechts, links + 1.0);
}

void Klang::geraetSicherstellen() {
    if (!d->mitGeraet || d->sink) return;
    QAudioFormat f;
    f.setSampleRate(Abtastrate);
    f.setChannelCount(2);
    f.setSampleFormat(QAudioFormat::Int16);
    QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (dev.isNull() || !dev.isFormatSupported(f)) return;
    d->ausgabe = std::make_unique<KlangAusgabe>(this);
    d->sink = std::make_unique<QAudioSink>(dev, f);
    d->sink->setBufferSize(Abtastrate / 10 * 4); // 100 ms
    d->sink->start(d->ausgabe.get());
}

void Klang::ereignis(KlangArt art, double x, double y, double staerke) {
    (void)y;
    {
        std::lock_guard<std::mutex> lk(d->m);
        if (!d->an) return;
        // Stimmenbegrenzung: die älteste Stimme schnell ausblenden
        while (d->zaehleEinmal() >= MaxEinmal) {
            Stimme *aelteste = nullptr;
            for (auto &s : d->stimmen)
                if (!s.dauer && s.pos < s.laenge - 240 && (!aelteste || s.alter < aelteste->alter)) aelteste = &s;
            if (!aelteste) break;
            aelteste->laenge = aelteste->pos + 240;
        }
        Stimme s;
        s.art = art;
        s.alter = ++d->zaehler;
        s.staerke = float(std::clamp(staerke, 0.0, 2.0));
        s.last = 0.5f;
        s.freq = grundfrequenz(art, 0.6f);
        s.laenge = std::max<long long>(480, (long long)(einmalLaenge(art, s.staerke) * SR));
        s.n.s = 0x9E3779B9u * uint32_t(d->zaehler) + 12345u;
        s.p0 = 0.92f + 0.16f * (0.5f + 0.5f * s.n.next());
        s.p1 = 0.5f + 0.5f * s.n.next();
        float basis = 0.35f + 0.65f * std::min(s.staerke, 1.2f) / 1.2f;
        if (art == KlangArt::Plumps || art == KlangArt::Treffer) basis = 0.5f + 0.5f * std::min(s.staerke, 1.2f) / 1.2f;
        d->setzePan(s, x, basis);
        s.gl = s.zl; s.gr = s.zr;
        s.hue = 1.0f;
        if (art == KlangArt::Murren) s.f1.setze(330, 90), s.f2.setze(780, 120), s.f3.setze(2400, 220);
        d->stimmen.push_back(s);
        d->stillSeit = 0;
    }
    geraetSicherstellen();
}

void Klang::dauerklang(int id, KlangArt art, double x, double y, double last) {
    (void)y;
    bool oeffnen = false;
    {
        std::lock_guard<std::mutex> lk(d->m);
        if (!d->an || !istDauerart(art)) return;
        last = std::clamp(last, 0.0, 1.0);
        Stimme *s = d->dauerStimme(id);
        if (!s) {
            if (d->zaehleDauer() >= MaxDauer) return;
            Stimme n;
            n.art = art; n.id = id; n.dauer = true;
            n.alter = ++d->zaehler;
            n.n.s = 0x85EBCA6Bu * uint32_t(id + 7) + 99u;
            n.hue = 0;
            n.e = (art == KlangArt::Rueckfahrwarner) ? 0.0f : 1.0f;
            d->stimmen.push_back(n);
            s = &d->stimmen.back();
            d->setzePan(*s, x);
            s->gl = s->zl; s->gr = s->zr;
            s->letztesX = x; s->hatX = true; s->letzteZeit = d->zeit;
            s->freq = grundfrequenz(art, float(last));
            oeffnen = true;
        } else if (s->art != art) {
            return; // gleiche id, andere Art: ignorieren
        }
        s->last = float(last);
        s->freq = grundfrequenz(art, s->last);
        d->setzePan(*s, x);
        if (art == KlangArt::Motor) { // Motor lauter bei Last
            s->zl *= 0.7f; s->zr *= 0.7f;
        } else if (art == KlangArt::Rueckfahrwarner) {
            s->zl *= 0.8f; s->zr *= 0.8f;
        }
        // Doppler aus der Radialgeschwindigkeit relativ zur Weltmitte (Zuhörer)
        double dt = double(d->zeit - s->letzteZeit) / SR;
        if (dt >= 0.01) {
            double mitte = 0.5 * (d->links + d->rechts);
            double v = (std::fabs(x - mitte) - std::fabs(s->letztesX - mitte)) / dt; // >0: entfernt sich
            const double c = 7000.0; // „Schallgeschwindigkeit“ in px/s
            double f = c / (c + std::clamp(v, -2500.0, 2500.0));
            s->dopplerZiel = float(std::clamp(f, 0.88, 1.12));
            s->letztesX = x;
            s->letzteZeit = d->zeit;
        }
        if (art == KlangArt::Rueckfahrwarner) s->dopplerZiel = 1.0f;
        d->stillSeit = 0;
    }
    if (oeffnen) geraetSicherstellen();
}

void Klang::dauerklangStopp(int id) {
    std::lock_guard<std::mutex> lk(d->m);
    if (Stimme *s = d->dauerStimme(id)) s->ausklingen = true;
}

int Klang::aktiveStimmen() const {
    std::lock_guard<std::mutex> lk(d->m);
    return int(d->stimmen.size());
}

double Klang::dauerFrequenz(int id) const {
    std::lock_guard<std::mutex> lk(d->m);
    for (auto &s : d->stimmen)
        if (s.dauer && s.id == id && !s.ausklingen) return double(s.freq) * double(s.doppler);
    return 0.0;
}

void Klang::render(float *aus, int frames) {
    std::lock_guard<std::mutex> lk(d->m);
    std::fill(aus, aus + size_t(frames) * 2, 0.0f);
    const bool nichts = d->stimmen.empty();
    if (nichts && d->master <= 1e-5f && d->masterZiel == 0.0f) {
        d->zeit += frames;
        d->stillSeit += frames;
        return;
    }
    const float masterSchritt = 1.0f / (0.04f * float(SR));
    const float glatt = 0.003f;
    for (auto &s : d->stimmen) {
        const bool dauer = s.dauer;
        for (int i = 0; i < frames; ++i) {
            float v = 0;
            float hue;
            if (dauer) {
                float ziel = s.ausklingen ? 0.0f : 1.0f;
                if (s.hue < ziel) s.hue = std::min(ziel, s.hue + s.hueSchritt);
                else if (s.hue > ziel) s.hue = std::max(ziel, s.hue - s.hueSchritt);
                s.gl += glatt * (s.zl - s.gl);
                s.gr += glatt * (s.zr - s.gr);
                s.doppler += 0.0006f * (s.dopplerZiel - s.doppler);
                hue = s.hue;
                if (hue <= 0.0f && s.ausklingen) break;
                switch (s.art) {
                case KlangArt::Rotor: v = rotor(s); break;
                case KlangArt::Motor: v = motor(s); break;
                default: v = warner(s); break;
                }
            } else {
                if (s.pos >= s.laenge) break;
                long long rest = s.laenge - s.pos;
                hue = std::min(1.0f, std::min(float(s.pos) / 144.0f, float(rest) / 240.0f));
                switch (s.art) {
                case KlangArt::Rutsch: v = rutsch(s); break;
                case KlangArt::Plumps: v = plumps(s); break;
                case KlangArt::Wurf: v = wurf(s); break;
                case KlangArt::Treffer: v = treffer(s); break;
                case KlangArt::Murren: v = murren(s); break;
                case KlangArt::Rueckfahrwarner: v = warner(s); break;
                case KlangArt::Rotor: v = rotor(s); break;
                case KlangArt::Motor: v = motor(s); break;
                }
                ++s.pos;
            }
            v *= hue;
            aus[2 * i] += v * s.gl;
            aus[2 * i + 1] += v * s.gr;
        }
    }
    // Master (Ein/Aus weich), Lautstärke geglättet, Begrenzer (tanh -> nie über 1,0)
    for (int i = 0; i < frames; ++i) {
        if (d->master < d->masterZiel) d->master = std::min(d->masterZiel, d->master + masterSchritt);
        else if (d->master > d->masterZiel) d->master = std::max(d->masterZiel, d->master - masterSchritt);
        d->lautGeglaettet += 0.002f * (d->lautstaerke - d->lautGeglaettet);
        float g = d->master * d->lautGeglaettet * 1.4f;
        for (int k = 0; k < 2; ++k) {
            float x = aus[2 * i + k] * g;
            aus[2 * i + k] = std::tanh(x);
        }
    }
    d->zeit += frames;
    // abgelaufene Stimmen entfernen
    d->stimmen.erase(std::remove_if(d->stimmen.begin(), d->stimmen.end(), [](const Stimme &s) {
        return s.dauer ? (s.ausklingen && s.hue <= 0.0f) : s.pos >= s.laenge;
    }), d->stimmen.end());
    d->stillSeit = d->stimmen.empty() ? d->stillSeit + frames : 0;
}
