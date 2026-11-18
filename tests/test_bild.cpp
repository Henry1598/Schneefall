// Aussehen des Schnees: Kristallformen, Textur-Atlas, Form der Schneedecken.
#include <QtTest>
#include <cmath>
#include <set>
#include "deckenform.h"
#include "flockenbild.h"
#include "kristall.h"

using namespace schnee;

namespace {
const double PI = 3.14159265358979;

// Deckkraft an beliebiger Stelle (bilinear), außerhalb 0
double alphaBei(const Bild &b, double x, double y) {
    x -= 0.5; y -= 0.5;
    int x0 = int(std::floor(x)), y0 = int(std::floor(y));
    double fx = x - x0, fy = y - y0, s = 0;
    for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) {
            int px = x0 + dx, py = y0 + dy;
            if (px < 0 || py < 0 || px >= b.breite || py >= b.hoehe) continue;
            s += b.pixel(px, py)[3] / 255.0 * (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
        }
    return s;
}

// mittlere Abweichung der Deckkraft, wenn man das Bild um winkel um die Mitte dreht
double drehAbweichung(const Bild &b, double winkel) {
    const double m = b.breite / 2.0;
    double summe = 0;
    int n = 0;
    for (double r = 1; r < m; r += 0.75)
        for (double a = 0; a < 2 * PI; a += 0.5 / r) {
            double v = alphaBei(b, m + r * std::cos(a), m + r * std::sin(a));
            double w = alphaBei(b, m + r * std::cos(a + winkel), m + r * std::sin(a + winkel));
            summe += std::abs(v - w);
            ++n;
        }
    return summe / n;
}

double deckung(const Bild &b) {
    double s = 0;
    for (int y = 0; y < b.hoehe; ++y)
        for (int x = 0; x < b.breite; ++x) s += b.pixel(x, y)[3] / 255.0;
    return s / (b.breite * b.hoehe);
}

Decke decke(size_t spalten, float hoehe, bool boden = false) {
    Decke d;
    d.id = boden ? "boden:0" : "f";
    d.x = 100; d.y = 500; d.spaltenBreite = 3; d.boden = boden;
    d.hoehe.assign(spalten, hoehe);
    d.sichtbar.assign(spalten, 1);
    return d;
}
float obenBei(const std::vector<ProfilPunkt> &p, float x) {
    for (size_t i = 0; i + 1 < p.size(); ++i)
        if (x >= p[i].x && x <= p[i + 1].x) {
            float t = (x - p[i].x) / (p[i + 1].x - p[i].x);
            return p[i].oben + t * (p[i + 1].oben - p[i].oben);
        }
    return 0;
}
} // namespace

class TestBild : public QObject {
    Q_OBJECT
private slots:
    // --- Kristallform ---
    void kristallIstSechszaehligUndSpiegelsymmetrisch() {
        for (uint32_t saat = 0; saat < 40; ++saat) {
            Kristall k = kristallErzeugen(saat);
            QVERIFY(!k.striche.empty() || !k.sechsecke.empty());
            auto gleich = [](float a, float b) { return std::abs(a - b) < 1e-4f; };
            auto hat = [&](float x0, float y0, float x1, float y1) {
                for (const Strich &s : k.striche)
                    if ((gleich(s.x0, x0) && gleich(s.y0, y0) && gleich(s.x1, x1) && gleich(s.y1, y1))) return true;
                return false;
            };
            const float c = 0.5f, s60 = float(std::sin(PI / 3));
            for (const Strich &s : k.striche) {
                // um 60° gedreht und an der x-Achse gespiegelt gibt es denselben Strich wieder
                QVERIFY2(hat(s.x0 * c - s.y0 * s60, s.x0 * s60 + s.y0 * c, s.x1 * c - s.y1 * s60, s.x1 * s60 + s.y1 * c),
                         qPrintable(QString("Saat %1: Drehung fehlt").arg(saat)));
                QVERIFY2(hat(s.x0, -s.y0, s.x1, -s.y1), qPrintable(QString("Saat %1: Spiegelung fehlt").arg(saat)));
                QVERIFY(std::hypot(s.x0, s.y0) <= 1.0001f && std::hypot(s.x1, s.y1) <= 1.0001f);
            }
            for (const Sechseck &h : k.sechsecke) {
                bool gefunden = false;
                for (const Sechseck &g : k.sechsecke)
                    if (gleich(g.x, h.x * c - h.y * s60) && gleich(g.y, h.x * s60 + h.y * c) && gleich(g.radius, h.radius)) gefunden = true;
                QVERIFY(gefunden);
                QVERIFY(std::hypot(h.x, h.y) + h.radius <= 1.0001f);
            }
        }
    }
    void jedeSaatGibtEineAndereFormUndAlleArtenKommenVor() {
        std::set<Kristallart> arten;
        std::set<std::vector<int>> formen;
        for (uint32_t saat = 0; saat < 48; ++saat) {
            Kristall k = kristallErzeugen(saat);
            arten.insert(k.art);
            std::vector<int> f{int(k.art)};
            for (const Strich &s : k.striche) { f.push_back(int(s.x1 * 1000)); f.push_back(int(s.y1 * 1000)); f.push_back(int(s.b0 * 1000)); }
            for (const Sechseck &h : k.sechsecke) { f.push_back(int(h.radius * 1000)); f.push_back(int(h.x * 1000)); }
            formen.insert(f);
            // gleiche Saat, gleiche Form
            Kristall k2 = kristallErzeugen(saat);
            QCOMPARE(k2.striche.size(), k.striche.size());
        }
        QCOMPARE(arten.size(), size_t(3));
        QCOMPARE(formen.size(), size_t(48));
    }
    // --- gerastertes Bild ---
    void bildIstSechszaehlig_data() {
        QTest::addColumn<int>("groesse");
        QTest::newRow("96 px (nah, 5K-Monitor)") << 96;
        QTest::newRow("48 px") << 48;
        QTest::newRow("24 px") << 24;
    }
    void bildIstSechszaehlig() {
        QFETCH(int, groesse);
        for (uint32_t saat = 0; saat < 16; ++saat) {
            Bild b = kristallRastern(kristallErzeugen(saat), groesse);
            QCOMPARE(b.breite, groesse);
            QCOMPARE(b.hoehe, groesse);
            double d60 = drehAbweichung(b, PI / 3);
            double d30 = drehAbweichung(b, PI / 6);
            // 60° bildet den Kristall auf sich ab (bis auf das Pixelraster), 30° deutlich nicht (sonst wäre es nur ein Klecks)
            QVERIFY2(d60 < 0.08, qPrintable(QString("Saat %1: Abweichung bei 60° %2").arg(saat).arg(d60)));
            QVERIFY2(d30 > 2 * d60 && d30 > 0.1, qPrintable(QString("Saat %1: 30° %2, 60° %3").arg(saat).arg(d30).arg(d60)));
            // oben/unten spiegelbildlich
            for (int y = 0; y < groesse / 2; ++y)
                for (int x = 0; x < groesse; ++x)
                    QVERIFY(std::abs(b.pixel(x, y)[3] - b.pixel(x, groesse - 1 - y)[3]) <= 2);
        }
    }
    void bildIstWeissMitBlaeulicherKanteUndFreiemRand() {
        for (uint32_t saat = 0; saat < 16; ++saat) {
            const int g = 96;
            Bild b = kristallRastern(kristallErzeugen(saat), g);
            int weiss = 0, blaeulich = 0, sichtbar = 0;
            for (int y = 0; y < g; ++y)
                for (int x = 0; x < g; ++x) {
                    const uint8_t *p = b.pixel(x, y);
                    if (x == 0 || y == 0 || x == g - 1 || y == g - 1) QCOMPARE(int(p[3]), 0); // nichts blutet ins Nachbarfeld
                    QVERIFY(p[0] <= p[3] && p[1] <= p[3] && p[2] <= p[3]);                    // vormultipliziert
                    if (p[3] < 40) continue;
                    ++sichtbar;
                    double r = double(p[0]) / p[3], gr = double(p[1]) / p[3], bl = double(p[2]) / p[3];
                    QVERIFY2(r >= 0.72 && gr >= r - 0.01 && bl >= gr - 0.01, "Schnee ist weiß bis zart bläulich, nie farbig oder dunkel");
                    if (r > 0.97) ++weiss;
                    if (r < 0.92 && bl > 0.97) ++blaeulich;
                }
            QVERIFY(sichtbar > g * g / 25);
            QVERIFY2(weiss > sichtbar / 10, "weißer Kern fehlt");
            QVERIFY2(blaeulich > sichtbar / 10, "bläuliche Kante fehlt");
            QVERIFY(b.pixel(g / 2, g / 2)[3] > 180); // Mitte ist Eis
        }
    }
    void kleineKristalleBleibenErkennbarStattZuVerklumpen() {
        // bei 12 Gerätepixeln bleiben sechs Arme mit Lücken dazwischen
        for (uint32_t saat = 0; saat < 16; ++saat) {
            Bild b = kristallRastern(kristallErzeugen(saat), 12);
            double d = deckung(b);
            QVERIFY2(d > 0.08 && d < 0.6, qPrintable(QString("Saat %1: Deckung %2").arg(saat).arg(d)));
        }
    }
    void unscharferKristallIstWeicherAberGleichGross() {
        Kristall k = kristallErzeugen(3);
        Bild scharf = kristallRastern(k, 48), weich = kristallRastern(k, 48, 2.0f);
        int harteKanten = 0, weicheKanten = 0;
        for (int y = 0; y < 48; ++y)
            for (int x = 1; x < 48; ++x) {
                if (std::abs(scharf.pixel(x, y)[3] - scharf.pixel(x - 1, y)[3]) > 120) ++harteKanten;
                if (std::abs(weich.pixel(x, y)[3] - weich.pixel(x - 1, y)[3]) > 120) ++weicheKanten;
            }
        QVERIFY(harteKanten > 20);
        QCOMPARE(weicheKanten, 0);
        for (int i = 0; i < 48; ++i) QCOMPARE(int(weich.pixel(i, 0)[3]) + int(weich.pixel(0, i)[3]), 0);
    }
    void pulverIstKeinKristall() {
        std::set<double> verschieden;
        for (uint32_t saat = 0; saat < 8; ++saat) {
            Bild b = pulverRastern(saat, 24);
            QVERIFY(deckung(b) > 0.1);
            for (int i = 0; i < 24; ++i) QCOMPARE(int(b.pixel(i, 0)[3]) + int(b.pixel(0, i)[3]) + int(b.pixel(i, 23)[3]) + int(b.pixel(23, i)[3]), 0);
            // unregelmäßig: eine 60°-Drehung ändert das Bild spürbar
            QVERIFY2(drehAbweichung(b, PI / 3) > 0.03, qPrintable(QString("Saat %1").arg(saat)));
            verschieden.insert(deckung(b));
        }
        QVERIFY(verschieden.size() >= 7);
    }
    // --- Atlas ---
    void atlasIstScharfFuerJedeSkalierung_data() {
        QTest::addColumn<double>("dpr");
        QTest::newRow("1") << 1.0;
        QTest::newRow("1,25") << 1.25;
        QTest::newRow("1,5 (Henrys 5K-Monitor)") << 1.5;
        QTest::newRow("2") << 2.0;
    }
    void atlasIstScharfFuerJedeSkalierung() {
        QFETCH(double, dpr);
        AtlasPlan p = atlasPlanen(dpr);
        QVERIFY(p.varianten >= 12);
        QVERIFY(p.stufen.size() >= 5);
        QCOMPARE(p.kristalle.size(), p.stufen.size() * size_t(p.varianten));
        QVERIFY(p.breite <= 4096 && p.hoehe <= 4096);
        std::vector<AtlasFeld> alle = p.kristalle;
        alle.insert(alle.end(), p.weich.begin(), p.weich.end());
        alle.insert(alle.end(), p.pulver.begin(), p.pulver.end());
        alle.push_back(p.punkt);
        QVERIFY(!p.weich.empty() && p.pulver.size() >= 6);
        for (size_t i = 0; i < alle.size(); ++i) {
            const AtlasFeld &a = alle[i];
            QVERIFY(a.groesse > 0 && a.x >= 0 && a.y >= 0 && a.x + a.groesse <= p.breite && a.y + a.groesse <= p.hoehe);
            for (size_t j = i + 1; j < alle.size(); ++j) {
                const AtlasFeld &b = alle[j];
                bool getrennt = a.x + a.groesse <= b.x || b.x + b.groesse <= a.x || a.y + a.groesse <= b.y || b.y + b.groesse <= a.y;
                QVERIFY2(getrennt, "Atlasfelder überlappen");
            }
        }
        for (size_t s = 0; s < p.stufen.size(); ++s) {
            if (s) QVERIFY(p.stufen[s] > p.stufen[s - 1]);
            // jedes logische Pixel hat mindestens dpr Gerätepixel: nie hochskaliert, also scharf
            for (int v = 0; v < p.varianten; ++v) QVERIFY(p.kristall(int(s), v).groesse >= p.stufen[s] * dpr - 1e-9);
        }
        // Stufenwahl: kleinste Stufe, die reicht
        QCOMPARE(p.stufeFuer(0.5), 0);
        QCOMPARE(p.stufeFuer(p.stufen[2]), 2);
        QCOMPARE(p.stufeFuer(p.stufen[2] + 0.01), 3);
        QCOMPARE(p.stufeFuer(1e6), int(p.stufen.size()) - 1);
    }
    void atlasBildEnthaeltJedeFormMitFreiemRand() {
        AtlasPlan p = atlasPlanen(1.5);
        Bild b = atlasErzeugen(p);
        QCOMPARE(b.breite, p.breite);
        QCOMPARE(b.hoehe, p.hoehe);
        auto pruefe = [&](const AtlasFeld &f) {
            long summe = 0;
            for (int y = 0; y < f.groesse; ++y)
                for (int x = 0; x < f.groesse; ++x) {
                    int a = b.pixel(f.x + x, f.y + y)[3];
                    if (x == 0 || y == 0 || x == f.groesse - 1 || y == f.groesse - 1) { if (a) return false; }
                    summe += a;
                }
            return summe > 255L * f.groesse * f.groesse / 30;
        };
        for (const AtlasFeld &f : p.kristalle) QVERIFY(pruefe(f));
        for (const AtlasFeld &f : p.weich) QVERIFY(pruefe(f));
        for (const AtlasFeld &f : p.pulver) QVERIFY(pruefe(f));
        QVERIFY(pruefe(p.punkt));
        // dieselbe Variante zeigt in jeder Größe denselben Kristall, verschiedene Varianten verschiedene
        const AtlasFeld &gross = p.kristall(int(p.stufen.size()) - 1, 0), &gross2 = p.kristall(int(p.stufen.size()) - 1, 1);
        int unterschied = 0;
        for (int y = 0; y < gross.groesse; ++y)
            for (int x = 0; x < gross.groesse; ++x)
                if (std::abs(b.pixel(gross.x + x, gross.y + y)[3] - b.pixel(gross2.x + x, gross2.y + y)[3]) > 100) ++unterschied;
        QVERIFY(unterschied > 50);
    }
    // --- Erscheinungsbild einer Flocke ---
    void kleineFlockenSindFernePunkteGrosseNaheKristalle() {
        const double grund = 4;
        auto mit = [](double groesse, uint32_t los = 7) { Flocke f{0, 0, 0, 30, groesse, 0}; f.los = los; return f; };
        Flockenbild fern = flockenbild(mit(2.2), grund, true), mitte = flockenbild(mit(4.6), grund, true),
                    nah = flockenbild(mit(5.6), grund, true), ganzNah = flockenbild(mit(5.99), grund, true);
        QCOMPARE(fern.sprite, Sprite::Punkt);
        QCOMPARE(mitte.sprite, Sprite::Kristall);
        QCOMPARE(nah.sprite, Sprite::Kristall);
        QCOMPARE(ganzNah.sprite, Sprite::Weich); // ganz vorn: groß und leicht unscharf
        // die Kristallform braucht Platz: mittlere mindestens ~9 px, nahe deutlich größer
        QVERIFY(fern.durchmesser < 6);
        QVERIFY(mitte.durchmesser >= 9);
        QVERIFY(nah.durchmesser >= 18 && nah.durchmesser > mitte.durchmesser);
        QVERIFY(ganzNah.durchmesser > nah.durchmesser && ganzNah.durchmesser <= 64);
        // fern = blasser
        QVERIFY(fern.deckkraft < mitte.deckkraft && mitte.deckkraft <= nah.deckkraft);
        QVERIFY(ganzNah.deckkraft < nah.deckkraft);
        // etwa die Hälfte aller Flocken sind Punkte, der Rest Kristalle
        int punkte = 0;
        for (int i = 0; i < 1000; ++i)
            if (flockenbild(mit(grund * (0.5 + i / 1000.0)), grund, true).sprite == Sprite::Punkt) ++punkte;
        QVERIFY(punkte >= 400 && punkte <= 600);
    }
    void jedeFlockeHatEigeneFormUndDrehtSichLangsam() {
        std::set<int> varianten;
        std::set<int> winkel;
        for (uint32_t los = 1; los < 200; ++los) {
            Flocke f{0, 0, 0, 30, 5, 0};
            f.los = los * 2654435761u;
            Flockenbild a = flockenbild(f, 4, true);
            varianten.insert(a.variante);
            winkel.insert(int(a.winkel * 20));
            QVERIFY(a.variante >= 0);
            QVERIFY(a.stauchung >= 0.6f && a.stauchung <= 1.f); // kippt nur leicht, die Form bleibt lesbar
            // eine Sekunde später (phase läuft mit 1,7/s): leicht gedreht, nicht herumgewirbelt
            f.phase += 1.7;
            Flockenbild b = flockenbild(f, 4, true);
            QCOMPARE(b.variante, a.variante);
            QCOMPARE(b.durchmesser, a.durchmesser);
            QVERIFY(std::abs(b.winkel - a.winkel) > 0.05f && std::abs(b.winkel - a.winkel) < 1.0f);
        }
        QVERIFY(varianten.size() >= 12);
        QVERIFY(winkel.size() > 50);
    }
    void schnelleWinkelfunktionenSindGenauGenug() {
        for (float w = -20; w < 20; w += 0.0137f) {
            QVERIFY(std::abs(sinusSchnell(w) - std::sin(w)) < 0.004f);
            QVERIFY(std::abs(kosinusSchnell(w) - std::cos(w)) < 0.004f);
        }
    }
    void pulverIstNieEinKristallUndEinfachZeigtNurPunkte() {
        for (double g = 2; g <= 6; g += 0.25) {
            Flocke f{0, 0, 0, 30, g, 0};
            f.los = uint32_t(g * 1000);
            f.art = Flockenart::Pulver;
            Flockenbild p = flockenbild(f, 4, true);
            QCOMPARE(p.sprite, Sprite::Pulver);
            QVERIFY(p.durchmesser >= g && p.durchmesser <= 3 * g);
            QCOMPARE(flockenbild(f, 4, false).sprite, Sprite::Punkt);
            f.art = Flockenart::Kristall;
            Flockenbild e = flockenbild(f, 4, false);
            QCOMPARE(e.sprite, Sprite::Punkt);
            QCOMPARE(e.durchmesser, float(g + 1.6)); // wie vor dem Umbau
        }
    }
    // --- Schneedecke ---
    void leereDeckeHatKeineForm() {
        QVERIFY(deckenProfil(decke(40, 0.f)).empty());
    }
    void deckeAufFensterHatRundeEndenMitKleinemUeberhang() {
        Decke d = decke(40, 12.f); // Fensterkante x = 100 … 220
        auto stuecke = deckenProfil(d);
        QCOMPARE(stuecke.size(), size_t(1));
        const auto &p = stuecke[0];
        for (size_t i = 1; i < p.size(); ++i) QVERIFY(p[i].x > p[i - 1].x);
        // läuft an beiden Enden auf null aus, kleiner Überhang über das Fensterende hinaus
        QCOMPARE(p.front().oben, 0.f);
        QCOMPARE(p.back().oben, 0.f);
        QVERIFY(p.front().x < 100 && p.front().x >= 100 - 5);
        QVERIFY(p.back().x > 220 && p.back().x <= 220 + 5);
        // in der Mitte volle Höhe, nie höher als der Schnee
        QVERIFY(std::abs(obenBei(p, 160) - 12) < 0.3);
        for (const ProfilPunkt &q : p) QVERIFY(q.oben <= 12.01f && q.oben >= 0 && q.unten >= 0);
        // gerundet statt Kasten: 1 px hinter dem Ende noch deutlich niedriger, danach stetig steigend
        float h1 = obenBei(p, p.front().x + 1), h3 = obenBei(p, p.front().x + 3), h6 = obenBei(p, p.front().x + 6);
        QVERIFY2(h1 > 1 && h1 < 8, qPrintable(QString::number(h1)));
        QVERIFY(h3 > h1 && h6 > h3);
        // der Überhang hängt etwas unter die Kante, aber nur außerhalb des Fensters
        bool haengt = false;
        for (const ProfilPunkt &q : p) {
            if (q.unten > 0) { haengt = true; QVERIFY(q.x < 100 || q.x > 220); QVERIFY(q.unten <= 3); }
        }
        QVERIFY(haengt);
    }
    void bodenUndTaskleisteHabenKeinenUeberhang() {
        for (int art = 0; art < 2; ++art) {
            Decke d = decke(40, 12.f, art == 0);
            d.dock = art == 1;
            auto stuecke = deckenProfil(d);
            QCOMPARE(stuecke.size(), size_t(1));
            for (const ProfilPunkt &q : stuecke[0]) {
                QVERIFY(q.x >= 100 && q.x <= 220);
                QCOMPARE(q.unten, 0.f);
            }
            // der Boden geht am Monitorrand in voller Höhe weiter (kein Loch zwischen zwei Monitoren)
            if (d.boden) QVERIFY(stuecke[0].front().oben > 8);
        }
    }
    void verdeckterTeilTrenntDieDeckeOhneUeberhang() {
        Decke d = decke(40, 10.f);
        for (size_t i = 15; i < 25; ++i) d.sichtbar[i] = 0; // x = 145 … 175 verdeckt
        auto stuecke = deckenProfil(d);
        QCOMPARE(stuecke.size(), size_t(2));
        QVERIFY(stuecke[0].back().x <= 145.01f);
        QVERIFY(stuecke[1].front().x >= 174.99f);
        QCOMPARE(stuecke[0].back().oben, 0.f);
        QCOMPARE(stuecke[0].back().unten, 0.f);
    }
    void stufenWerdenGeglaettet() {
        Decke d = decke(40, 4.f);
        for (size_t i = 20; i < 40; ++i) d.hoehe[i] = 10.f; // Stufe bei x = 160
        const auto p = deckenProfil(d)[0];
        float groessterSprung = 0;
        for (float x = 110; x < 210; x += 1) groessterSprung = std::max(groessterSprung, std::abs(obenBei(p, x + 1) - obenBei(p, x)));
        QVERIFY2(groessterSprung < 1.5f, qPrintable(QString::number(groessterSprung))); // roh wären es 6 px auf einmal
        QVERIFY(std::abs(obenBei(p, 130) - 4) < 0.2 && std::abs(obenBei(p, 190) - 10) < 0.2);
    }
    // Fehler 18.11. (Bildschirmfoto): beim Aufbau lag der Schnee als lauter einzelne Brocken da
    void lueckigeDeckeLiegtDurchgehendStattInBrocken() {
        Decke d = decke(40, 0.f);
        for (size_t i = 4; i < 36; ++i) d.hoehe[i] = (i % 3 == 0) ? 0.f : (i % 3 == 1 ? 5.f : 3.f); // jede dritte Spalte leer
        d.hoehe[20] = d.hoehe[21] = 0.f;                                                             // auch zwei leere nebeneinander
        auto stuecke = deckenProfil(d);
        QCOMPARE(stuecke.size(), size_t(1));
        const auto &p = stuecke[0];
        float tiefste = 100, groessterSprung = 0;
        for (float x = 100 + 7 * 3; x < 100 + 33 * 3; x += 0.5f) {
            tiefste = std::min(tiefste, obenBei(p, x));
            groessterSprung = std::max(groessterSprung, std::abs(obenBei(p, x + 0.5f) - obenBei(p, x)));
        }
        QVERIFY2(tiefste > 0.8f, qPrintable(QString::number(tiefste)));                 // keine Löcher bis auf die Kante
        QVERIFY2(groessterSprung < 0.5f, qPrintable(QString::number(groessterSprung))); // sanfte Wellen
        // vor und hinter dem Schnee bleibt die Kante frei
        QVERIFY(p.front().x >= 100 + 3 * 3 && p.back().x <= 100 + 37 * 3);
    }
    void breiteLueckeTrenntDieDecke() {
        Decke d = decke(40, 6.f);
        for (size_t i = 15; i < 22; ++i) d.hoehe[i] = 0.f; // 21 px blanke Kante
        auto stuecke = deckenProfil(d);
        QCOMPARE(stuecke.size(), size_t(2));
        QVERIFY(stuecke[0].back().x <= 100 + 16 * 3 && stuecke[1].front().x >= 100 + 21 * 3);
    }
    void einzelneFlockeGibtKleinenHuegel() {
        Decke d = decke(40, 0.f);
        d.hoehe[10] = 1.2f; // x = 130 … 133
        auto stuecke = deckenProfil(d);
        QCOMPARE(stuecke.size(), size_t(1));
        QVERIFY(stuecke[0].front().x >= 128 && stuecke[0].back().x <= 135);
        float gipfel = 0;
        for (const ProfilPunkt &q : stuecke[0]) gipfel = std::max(gipfel, q.oben);
        QVERIFY(gipfel > 0.6f && gipfel <= 1.2f);
    }
};

QTEST_GUILESS_MAIN(TestBild)
#include "test_bild.moc"
