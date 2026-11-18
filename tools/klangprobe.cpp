// Hörprobe ohne die App.
//   klangprobe                      spielt jeden Klang nacheinander kurz ab (Audiogerät, Lautstärke --laut N)
//   klangprobe --wav datei.wav      rendert alle Klänge nacheinander offline in eine WAV-Datei
//   klangprobe --wav-ordner ordner  eine WAV je Klang (+ alle.wav)
//   klangprobe --cpu                misst die CPU-Last der Synthese (Rotor + Motor)
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QTimer>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include "klang.h"

namespace {
struct Schritt {
    const char *name;
    double dauer;                       // Sekunden
    void (*start)(Klang &);
    void (*takt)(Klang &, double t);    // wird alle 33 ms aufgerufen (Dauerklänge bewegen)
};
const Schritt schritte[] = {
    {"rotor", 8.0, [](Klang &) {}, [](Klang &k, double t) { k.dauerklang(1, KlangArt::Rotor, 100 + 1700 * t / 8.0, 100, 1.0); }},
    {"motor", 6.0, [](Klang &) {}, [](Klang &k, double t) { k.dauerklang(2, KlangArt::Motor, 960, 600, t < 2 ? 0.0 : t < 3 ? (t - 2) : t < 5 ? 1.0 : 0.0); }},
    {"rueckfahrwarner", 4.0, [](Klang &) {}, [](Klang &k, double) { k.dauerklang(3, KlangArt::Rueckfahrwarner, 960, 600, 0); }},
    {"rutsch", 3.0, [](Klang &k) { k.ereignis(KlangArt::Rutsch, 500, 300, 0.4); k.ereignis(KlangArt::Rutsch, 1500, 300, 1.8); }, [](Klang &, double) {}},
    {"plumps", 2.0, [](Klang &k) { k.ereignis(KlangArt::Plumps, 300, 900, 1.0); k.ereignis(KlangArt::Plumps, 1600, 900, 0.5); }, [](Klang &, double) {}},
    {"wurf-treffer", 3.0, [](Klang &k) { k.ereignis(KlangArt::Wurf, 400, 500, 1.0); }, [](Klang &k, double t) {
        static bool g = false; if (t > 0.35 && !g) { g = true; k.ereignis(KlangArt::Treffer, 1500, 500, 1.0); } }},
    {"murren", 3.0, [](Klang &k) { k.ereignis(KlangArt::Murren, 960, 500, 1.0); }, [](Klang &k, double t) {
        static bool g = false; if (t > 1.2 && !g) { g = true; k.ereignis(KlangArt::Murren, 1400, 500, 1.2); } }},
};

void wavSchreiben(const std::string &pfad, const std::vector<float> &s) {
    FILE *f = std::fopen(pfad.c_str(), "wb");
    if (!f) { std::perror(pfad.c_str()); return; }
    uint32_t daten = uint32_t(s.size() * 2), rate = Klang::Abtastrate, bps = rate * 4, riff = 36 + daten;
    uint16_t fmt = 1, ch = 2, ba = 4, bits = 16; uint32_t fl = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fl, 4, 1, f); std::fwrite(&fmt, 2, 1, f); std::fwrite(&ch, 2, 1, f); std::fwrite(&rate, 4, 1, f);
    std::fwrite(&bps, 4, 1, f); std::fwrite(&ba, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&daten, 4, 1, f);
    for (float v : s) { int16_t i = int16_t(std::lrintf(std::max(-1.0f, std::min(1.0f, v)) * 32767.0f)); std::fwrite(&i, 2, 1, f); }
    std::fclose(f);
}

// rendert einen Schritt offline; hängt Stille von 0,5 s an
std::vector<float> schrittRendern(const Schritt &st) {
    Klang k(false);
    k.setzeAn(true); k.setzeLautstaerke(0.8); k.setzeWeltbreite(0, 1920);
    st.start(k);
    std::vector<float> aus;
    const int block = Klang::Abtastrate / 30; // 33 ms
    std::vector<float> b(size_t(block) * 2);
    double t = 0;
    bool gestoppt = false;
    while (t < st.dauer + 0.5) {
        if (t < st.dauer) st.takt(k, t);
        else if (!gestoppt) { k.dauerklangStopp(1); k.dauerklangStopp(2); k.dauerklangStopp(3); gestoppt = true; }
        k.render(b.data(), block);
        aus.insert(aus.end(), b.begin(), b.end());
        t += double(block) / Klang::Abtastrate;
    }
    return aus;
}
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    std::string wav, ordner;
    bool cpu = false;
    double laut = 0.4;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--wav") && i + 1 < argc) wav = argv[++i];
        else if (!std::strcmp(argv[i], "--wav-ordner") && i + 1 < argc) ordner = argv[++i];
        else if (!std::strcmp(argv[i], "--cpu")) cpu = true;
        else if (!std::strcmp(argv[i], "--laut") && i + 1 < argc) laut = std::atof(argv[++i]) / 100.0;
    }
    if (cpu) {
        Klang k(false);
        k.setzeAn(true); k.setzeLautstaerke(0.5); k.setzeWeltbreite(0, 1920);
        const double sek = 20.0;
        const int block = 480;
        std::vector<float> b(size_t(block) * 2);
        timespec a, e;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &a);
        for (int i = 0; i < int(sek * Klang::Abtastrate / block); ++i) {
            if (i % 3 == 0) {
                double x = 100 + 1700 * ((i / 3) % 400) / 400.0;
                k.dauerklang(1, KlangArt::Rotor, x, 100, 1.0);
                k.dauerklang(2, KlangArt::Motor, 960, 600, 0.5 + 0.5 * ((i / 90) % 2));
            }
            k.render(b.data(), block);
        }
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &e);
        double s = double(e.tv_sec - a.tv_sec) + double(e.tv_nsec - a.tv_nsec) * 1e-9;
        std::printf("Rotor+Motor: %.3f s CPU für %.0f s Ton = %.2f %% eines Kerns\n", s, sek, 100.0 * s / sek);
        return 0;
    }
    if (!wav.empty() || !ordner.empty()) {
        std::vector<float> alle;
        if (!ordner.empty()) QDir().mkpath(QString::fromStdString(ordner));
        for (const Schritt &st : schritte) {
            auto s = schrittRendern(st);
            if (!ordner.empty()) wavSchreiben(ordner + "/" + st.name + ".wav", s);
            alle.insert(alle.end(), s.begin(), s.end());
        }
        if (!wav.empty()) wavSchreiben(wav, alle);
        else wavSchreiben(ordner + "/alle.wav", alle);
        return 0;
    }
    // Live abspielen (Gerät)
    Klang k(true);
    k.setzeAn(true); k.setzeLautstaerke(laut); k.setzeWeltbreite(0, 1920);
    size_t idx = 0;
    QElapsedTimer uhr;
    QTimer t;
    t.setInterval(33);
    bool neu = true;
    QObject::connect(&t, &QTimer::timeout, [&] {
        if (idx >= sizeof schritte / sizeof *schritte) { app.quit(); return; }
        const Schritt &st = schritte[idx];
        if (neu) { std::printf("%s …\n", st.name); std::fflush(stdout); uhr.start(); st.start(k); neu = false; }
        double s = uhr.elapsed() / 1000.0;
        if (s < st.dauer) st.takt(k, s);
        else { k.dauerklangStopp(1); k.dauerklangStopp(2); if (s > st.dauer + 0.6) { ++idx; neu = true; } }
    });
    t.start();
    return app.exec();
}
