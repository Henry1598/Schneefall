// Räumdienst und Schneebälle: Zustandsautomat, Seilpendel, Mengenerhaltung, Ball/Treffer/Streik.
// Geprüft wird nur über die öffentliche Schnittstelle von Raeumdienst und Simulation.
#include <QtTest>
#include <cmath>
#include <set>
#include "raeumdienst.h"
#include "schnee.h"

using namespace schnee;

namespace {
Einstellungen ruhig() {
    Einstellungen e;
    e.wind = 0;
    e.ablageJeFlocke = 1.0;
    e.maxHoehe = 20;
    e.bodenMax = 20;
    e.spaltenBreite = 4;
    e.schmelzen = 0;
    return e;
}
Fenster fenster(const char *id, double x, double y, double w, double h, int stapel) {
    Fenster f; f.id = id; f.r = {x, y, w, h}; f.stapel = stapel; return f;
}
// Welt: ein Monitor 2000×1200, Fenster „a“ (600 breit) oben, optional breites Fenster „b“ darunter
struct Welt {
    Simulation sim{ruhig()};
    Raeumdienst rd{sim};
    std::vector<Fenster> fenster_;
    explicit Welt(bool mitB = false) {
        sim.setBildschirme({{0, 0, 2000, 1200}});
        fenster_.push_back(fenster("a", 700, 400, 600, 300, 2));
        if (mitB) fenster_.push_back(fenster("b", 100, 900, 1800, 200, 1));
        sim.setFenster(fenster_);
        RaeumEinstellungen e;
        e.an = false; // Tests buchen selbst, wo nichts anderes steht
        rd.setEinstellungen(e);
    }
    void fuellen(const char *id, double hoehe) {
        const Decke *d = sim.decke(id);
        for (size_t i = 0; i < d->hoehe.size(); ++i) sim.schneeHinzufuegen(id, d->x + (i + 0.5) * d->spaltenBreite, hoehe);
    }
    void laufen(double sekunden, double dt = 1.0 / 30) {
        for (double t = 0; t < sekunden; t += dt) { sim.schritt(dt, false); rd.schritt(dt); }
    }
    // läuft, bis die Bedingung gilt; false nach Ablauf der Zeit
    template <class B> bool bis(B bedingung, double maxSekunden = 300, double dt = 1.0 / 30) {
        for (double t = 0; t < maxSekunden; t += dt) {
            if (bedingung()) return true;
            sim.schritt(dt, false);
            rd.schritt(dt);
        }
        return bedingung();
    }
    bool buche(const char *id) { const bool ok = rd.buchen(id); laufen(1.0 / 30); return ok; }
    double alles() const {
        double m = sim.fallenderSchnee() + rd.getrageneMenge();
        for (const Decke &d : sim.decken()) m += sim.gesamtSchnee(d.id);
        return m;
    }
    const Einsatz *einsatz() const { return rd.einsaetze().empty() ? nullptr : &rd.einsaetze().front(); }
    bool raeumt() const { return einsatz() && einsatz()->phase == Phase::Raeumen && einsatz()->fz.phase == FahrPhase::Schieben; }
    void triff() { const Fahrzeug &f = einsatz()->fz; rd.werfen(f.x, f.y - 20, 0, 0, 60); laufen(0.05); }
};
}

class TestRaeumdienst : public QObject {
    Q_OBJECT
private slots:
    void seilpendelBleibtAmSeilUndKommtZurRuhe() {
        for (double dt : {1.0 / 120, 1.0 / 30, 0.1}) {
            Seilpendel p;
            p.laenge = 60;
            p.haengen(0, 0);
            double t = 0;
            // Aufhängung ruckt heftig hin und her
            for (; t < 10; t += dt) {
                const double ax = 200 * std::sin(t * 5) + (std::fmod(t, 1.0) < 0.5 ? 80 : -80), ay = 30 * std::sin(t * 3);
                p.schritt(dt, ax, ay);
                QVERIFY(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.vx) && std::isfinite(p.vy));
                QVERIFY2(std::hypot(p.x - ax, p.y - ay) <= p.laenge + 1e-6, "Last weiter weg als das Seil lang ist");
            }
            // danach steht die Aufhängung still: Pendel schwingt aus
            for (t = 0; t < 25; t += dt) p.schritt(dt, 0, 0);
            QVERIFY2(std::abs(p.winkel()) < 0.03, qPrintable(QString("dt=%1 winkel=%2").arg(dt).arg(p.winkel())));
            QVERIFY(std::abs(p.y - 60) < 0.5);
        }
    }
    void seilpendelSchaukeltSichNichtAuf() {
        Seilpendel p;
        p.laenge = 80;
        p.ax = 0; p.ay = 0;
        p.x = 80 * std::sin(1.0); p.y = 80 * std::cos(1.0); // 57° ausgelenkt
        double frueh = 0, spaet = 0;
        for (double t = 0; t < 8; t += 1.0 / 30) {
            p.schritt(1.0 / 30, 0, 0);
            (t < 2 ? frueh : spaet) = std::max(t < 2 ? frueh : spaet, std::abs(p.winkel()));
        }
        QVERIFY(frueh <= 1.0 + 1e-6);
        QVERIFY2(spaet < frueh * 0.6, qPrintable(QString("%1 → %2").arg(frueh).arg(spaet)));
    }
    void vollesFensterWirdVonSelbstGebucht() {
        Welt w;
        RaeumEinstellungen e;   // an, ab 80 %
        w.rd.setEinstellungen(e);
        w.fuellen("a", 10);     // 50 % von 20 px
        w.laufen(3);
        QVERIFY(w.rd.einsaetze().empty());
        w.fuellen("a", 7);      // 85 %
        QVERIFY(w.bis([&] { return !w.rd.einsaetze().empty(); }, 1.2));
        QCOMPARE(w.rd.einsaetze().size(), size_t(1));
        QCOMPARE(w.einsatz()->id, std::string("a"));
        QCOMPARE(w.einsatz()->phase, Phase::Anflug);
        QCOMPARE(w.einsatz()->fz.art, Fahrzeugart::Raupe);
        // kommt von außerhalb des Bildschirms, Fahrzeug hängt am Seil
        QVERIFY(w.einsatz()->heli.x < 0 || w.einsatz()->heli.x > 2000);
        QVERIFY(w.einsatz()->angehaengt);
    }
    void ablaufVomAnflugBisZumAbflug() {
        Welt w;
        w.fuellen("a", 16);
        QVERIFY(w.buche("a"));
        std::vector<Phase> folge;
        bool aufFenster = true;
        double hoechsterWulst = 0;
        QVERIFY(w.bis([&] {
            if (const Einsatz *e = w.einsatz()) {
                if (folge.empty() || folge.back() != e->phase) folge.push_back(e->phase);
                if (e->fz.phase != FahrPhase::AmSeil) {
                    aufFenster &= e->fz.x > 700 && e->fz.x < 1300 && std::abs(e->fz.y - 400) < 1e-6;
                    hoechsterWulst = std::max(hoechsterWulst, e->fz.wulst);
                }
            }
            return w.rd.einsaetze().empty();
        }, 240));
        const std::vector<Phase> soll = {Phase::Anflug, Phase::Schweben, Phase::Absenken, Phase::Ausklinken, Phase::Raeumen,
                                         Phase::Rueckkehr, Phase::Einhaken, Phase::Anheben, Phase::Abflug};
        QCOMPARE(folge, soll);
        QVERIFY(aufFenster);
        QVERIFY2(hoechsterWulst > 100, "vor dem Schild hat sich kein Schnee getürmt");
        // Fenster ist geräumt
        const Decke *d = w.sim.decke("a");
        QVERIFY2(w.sim.gesamtSchnee("a") / d->hoehe.size() < 1.5, qPrintable(QString::number(w.sim.gesamtSchnee("a") / d->hoehe.size())));
    }
    void schneeFaelltBeimRaeumenNachUntenUndBleibtErhalten() {
        Welt w(true);
        Einstellungen hoch = ruhig();
        hoch.maxHoehe = 300;   // das untere Fenster soll den ganzen Haufen halten können
        w.sim.setEinstellungen(hoch);
        w.fuellen("a", 16);
        w.laufen(1);
        const double vorher = w.alles(), aVorher = w.sim.gesamtSchnee("a"), bVorher = w.sim.gesamtSchnee("b");
        QVERIFY(w.buche("a"));
        double groessteAbweichung = 0;
        QVERIFY(w.bis([&] {
            groessteAbweichung = std::max(groessteAbweichung, std::abs(w.alles() - vorher));
            return w.rd.einsaetze().empty();
        }, 240));
        w.laufen(5); // letzte Brocken landen
        QVERIFY2(groessteAbweichung < vorher * 0.003, qPrintable(QString("Abweichung %1 von %2").arg(groessteAbweichung).arg(vorher)));
        const double aWeg = aVorher - w.sim.gesamtSchnee("a");
        QVERIFY(aWeg > aVorher * 0.9);
        // der Schnee liegt jetzt auf dem Fenster darunter
        QVERIFY2(std::abs((w.sim.gesamtSchnee("b") - bVorher) - aWeg) < aVorher * 0.05,
                 qPrintable(QString("b +%1, a -%2").arg(w.sim.gesamtSchnee("b") - bVorher).arg(aWeg)));
    }
    void warteschlangeDerReiheNach() {
        Welt w;
        w.fenster_.push_back(fenster("c", 100, 700, 400, 200, 3));
        w.fenster_.push_back(fenster("d", 1400, 700, 400, 200, 4));
        w.sim.setFenster(w.fenster_);
        for (const char *id : {"a", "c", "d"}) w.fuellen(id, 16);
        QVERIFY(w.rd.buchen("c"));
        QVERIFY(w.buche("a"));
        QVERIFY(w.rd.buchen("d"));
        QVERIFY(!w.buche("a"));            // schon gebucht
        QVERIFY(!w.rd.buchen("gibtsnicht"));
        w.laufen(0.1);
        QCOMPARE(w.rd.einsaetze().size(), size_t(1));   // höchstens einer zugleich
        QCOMPARE(w.einsatz()->id, std::string("c"));
        QCOMPARE(w.rd.warteschlange().size(), size_t(2));
        QVERIFY(!w.rd.buchen("c"));            // läuft gerade
        QVERIFY(w.bis([&] { return w.einsatz() && w.einsatz()->id != "c"; }, 240));
        QCOMPARE(w.einsatz()->id, std::string("a"));    // der Reihe nach
        QCOMPARE(w.rd.warteschlange().front(), std::string("d"));
        // zwei zugleich erlaubt: der Wartende startet sofort
        RaeumEinstellungen e = w.rd.einstellungen();
        e.gleichzeitig = 2;
        w.rd.setEinstellungen(e);
        w.laufen(0.1);
        QCOMPARE(w.rd.einsaetze().size(), size_t(2));
        QVERIFY(w.rd.warteschlange().empty());
    }
    void minimierenHebtDasFahrzeugSofortAb() {
        Welt w(true);
        w.fuellen("a", 16);
        w.buche("a");
        QVERIFY(w.bis([&] { return w.raeumt() && w.einsatz()->fz.wulst > 50; }, 120));
        const double y0 = w.einsatz()->fz.y, vorher = w.alles();
        w.fenster_[0].minimiert = true;
        w.sim.setFenster(w.fenster_);
        w.laufen(0.05);
        const Einsatz *e = w.einsatz();
        QVERIFY(e && e->abbruch && e->eilig && e->angehaengt);
        QCOMPARE(e->phase, Phase::Abflug);
        QCOMPARE(e->fz.phase, FahrPhase::AmSeil);
        QCOMPARE(e->fz.wulst, 0.0);            // der Wulst fällt herunter
        w.laufen(1.5);
        QVERIFY2(w.einsatz()->fz.y < y0 - 40, qPrintable(QString("y %1 → %2").arg(y0).arg(w.einsatz()->fz.y)));
        QVERIFY(w.bis([&] { return w.rd.einsaetze().empty(); }, 60));
        w.laufen(3);
        QVERIFY(std::abs(w.alles() - vorher) < vorher * 0.002);
    }
    void abbruchSchonImAnflugFliegtEinfachWeiter() {
        Welt w;
        w.fuellen("a", 16);
        w.buche("a");
        w.laufen(1);
        w.fenster_[0].minimiert = true;
        w.sim.setFenster(w.fenster_);
        w.laufen(0.1);
        QCOMPARE(w.einsatz()->phase, Phase::Abflug);
        QVERIFY(w.einsatz()->angehaengt);
        QVERIFY(w.bis([&] { return w.rd.einsaetze().empty(); }, 60));
    }
    void verschobenesFensterNimmtDasFahrzeugMit() {
        Welt w;
        w.fuellen("a", 16);
        w.buche("a");
        QVERIFY(w.bis([&] { return w.raeumt(); }, 120));
        const double x0 = w.einsatz()->fz.x;
        w.fenster_[0].r.x += 150;
        w.fenster_[0].r.y += 60;
        w.sim.setFenster(w.fenster_);
        w.laufen(1.0 / 30);
        QVERIFY(std::abs(w.einsatz()->fz.x - (x0 + 150)) < 6);
        QCOMPARE(w.einsatz()->fz.y, 460.0);
        QVERIFY(!w.einsatz()->abbruch);
    }
    void jetztRaeumenNimmtDasFensterMitDemMeistenSchnee() {
        Welt w;
        QCOMPARE(w.rd.jetztRaeumen(), std::string());   // nirgends Schnee
        w.fenster_.push_back(fenster("c", 100, 700, 400, 200, 3));
        w.sim.setFenster(w.fenster_);
        w.fuellen("a", 5);
        w.fuellen("c", 9);
        QCOMPARE(w.rd.jetztRaeumen(), std::string("c"));
        QCOMPARE(w.rd.einsaetze().size(), size_t(1));    // sofort, ohne Warteschlange und unter der Schwelle
        // schon belegt: der nächste Auftrag kommt nach vorn in die Warteschlange
        w.rd.buchen("gibtsnicht");
        QCOMPARE(w.rd.jetztRaeumen(), std::string("a"));
        QCOMPARE(w.rd.warteschlange().front(), std::string("a"));
    }
    void bodenBekommtDenKleinenPflug() {
        Welt w;
        RaeumEinstellungen e;
        e.taskleiste = false;
        w.rd.setEinstellungen(e);
        w.fuellen("boden:0", 18);
        w.laufen(1.5);
        QVERIFY(w.rd.einsaetze().empty());          // Taskleiste/Boden räumen ist aus
        e.taskleiste = true;
        w.rd.setEinstellungen(e);
        w.laufen(1.5);
        QCOMPARE(w.rd.einsaetze().size(), size_t(1));
        QCOMPARE(w.einsatz()->fz.art, Fahrzeugart::Pflug);
        QVERIFY(w.bis([&] { return w.rd.einsaetze().empty(); }, 600));
        QVERIFY(w.sim.gesamtSchnee("boden:0") / w.sim.decke("boden:0")->hoehe.size() < 2.5);
    }
    void ballWaechstBeimZiehenUeberSchnee() {
        Welt w(true);
        w.fuellen("b", 12);
        w.laufen(0.5);
        QVERIFY(!w.rd.mausDruck(500, 300, 0));      // freie Fläche: nichts anzufassen
        QVERIFY(!w.rd.ballInHand());
        const double vorher = w.sim.gesamtSchnee("b");
        QVERIFY(w.rd.mausDruck(500, 893, 0));       // mitten im Schnee auf „b“
        QVERIFY(w.rd.ballInHand());
        const double klein = w.rd.ballInHand()->menge, kleinR = w.rd.ballInHand()->radius();
        QVERIFY(klein > 0);
        for (int i = 1; i <= 40; ++i) w.rd.mausBewegt(500 + i * 5, 893, i * 0.01);
        const double gross = w.rd.ballInHand()->menge;
        QVERIFY2(gross > klein * 4, qPrintable(QString("%1 → %2").arg(klein).arg(gross)));
        QVERIFY(w.rd.ballInHand()->radius() > kleinR * 1.5);
        QCOMPARE(w.rd.ballInHand()->x, 700.0);
        QVERIFY(std::abs((vorher - w.sim.gesamtSchnee("b")) - gross) < 1e-3);   // genau so viel fehlt in der Decke
        // wächst nicht endlos und nicht in der Luft
        for (int i = 1; i <= 400; ++i) w.rd.mausBewegt(700 + (i % 2) * 300, 893, 1 + i * 0.01);
        const double voll = w.rd.ballInHand()->menge;
        QVERIFY(voll <= 220.0 + 1e-6);
        for (int i = 1; i <= 50; ++i) w.rd.mausBewegt(700 + i * 5, 600, 6 + i * 0.01);
        QCOMPARE(w.rd.ballInHand()->menge, voll);
    }
    void wurfFliegtImBogenUndZerplatztAufDerDecke() {
        Welt w(true);
        w.fuellen("b", 12);
        w.laufen(0.5);
        QVERIFY(w.rd.mausDruck(300, 893, 0));
        for (int i = 1; i <= 30; ++i) w.rd.mausBewegt(300 + i * 4, 893, i * 0.01);
        // Schwung holen: in 0,1 s 60 px nach rechts und 50 px nach oben
        for (int i = 1; i <= 10; ++i) w.rd.mausBewegt(420 + i * 6, 893 - i * 5, 1 + i * 0.01);
        const double vorher = w.alles();
        w.rd.mausLos(480, 843, 1.1);
        QVERIFY(!w.rd.ballInHand());
        QCOMPARE(w.rd.baelle().size(), size_t(1));
        QVERIFY(std::abs(w.rd.baelle()[0].vx - 600) < 80);
        QVERIFY(std::abs(w.rd.baelle()[0].vy + 500) < 80);
        // Bogen: erst hoch, dann runter; dreht sich dabei
        double hoechster = 1e9, weitester = 0;
        const double winkel0 = w.rd.baelle()[0].winkel;
        double winkel1 = winkel0;
        QVERIFY(w.bis([&] {
            if (!w.rd.baelle().empty()) {
                hoechster = std::min(hoechster, w.rd.baelle()[0].y);
                weitester = w.rd.baelle()[0].x;
                winkel1 = w.rd.baelle()[0].winkel;
            }
            return w.rd.baelle().empty();
        }, 10));
        QVERIFY(hoechster < 800);
        QVERIFY(weitester > 800);
        QVERIFY(std::abs(winkel1 - winkel0) > 1);
        QVERIFY(!w.rd.staub().empty());             // zerplatzt in Staub
        w.laufen(4);
        QVERIFY2(std::abs(w.alles() - vorher) < 2, qPrintable(QString("%1 statt %2, fallend %3").arg(w.alles()).arg(vorher).arg(w.sim.fallenderSchnee())));
        // und liegt wieder als Schnee da
        QVERIFY(w.sim.hoeheBei("b", weitester) > 12);
    }
    void zuVieleTrefferFuehrenZumStreikUndDannZumBeleidigtenAbflug() {
        Welt w;
        w.fuellen("a", 16);
        w.buche("a");
        QVERIFY(w.bis([&] { return w.raeumt(); }, 120));
        w.rd.klaengeAbholen();
        w.triff();
        w.triff();
        QCOMPARE(w.einsatz()->fz.phase, FahrPhase::Schieben);   // zwei Treffer hält er aus
        QVERIFY(std::abs(w.einsatz()->fz.wackeln) > 0.001);     // wackelt aber
        QVERIFY(!w.einsatz()->fz.kleckse.empty());              // und Schnee klebt dran
        w.triff();
        QCOMPARE(w.einsatz()->fz.phase, FahrPhase::Streik);
        const int spruch1 = w.einsatz()->fz.spruch;
        QVERIFY(spruch1 >= 0 && spruch1 < 8);
        QVERIFY(QString::fromUtf8(spruchText(spruch1)).size() > 4);
        bool murren = false;
        for (const Klangereignis &k : w.rd.klaengeAbholen()) murren |= k.typ == Klangereignis::Einmal && k.art == Geraeusch::Murren;
        QVERIFY(murren);
        w.laufen(1.5);
        const double x = w.einsatz()->fz.x;
        QVERIFY(std::abs(w.einsatz()->fz.v) < 0.5);             // steht
        QVERIFY(w.einsatz()->fz.raus > 0.9);                    // Fahrer lehnt sich raus
        w.laufen(2);
        QVERIFY(std::abs(w.einsatz()->fz.x - x) < 1.0);
        // nach der Pause räumt er weiter
        QVERIFY(w.bis([&] { return w.einsatz()->fz.phase == FahrPhase::Schieben; }, 8));
        QVERIFY(w.einsatz()->fz.raus < 0.1);
        QCOMPARE(w.einsatz()->fz.spruch, -1);
        w.laufen(1);
        QVERIFY(std::abs(w.einsatz()->fz.x - x) > 10);
        // wieder drei Treffer: jetzt reicht es ihm
        w.triff(); w.triff();
        QVERIFY(!w.einsatz()->beleidigt);
        w.triff();
        QCOMPARE(w.einsatz()->fz.phase, FahrPhase::Beleidigt);
        QVERIFY(w.einsatz()->beleidigt);
        QVERIFY(w.einsatz()->fz.spruch >= 8 && w.einsatz()->fz.spruch <= 10);
        QVERIFY(w.bis([&] { return w.einsatz() && w.einsatz()->phase == Phase::Abflug; }, 60));
        QVERIFY(w.sim.gesamtSchnee("a") > 500);                 // Arbeit bleibt liegen
        QVERIFY(w.bis([&] { return w.rd.einsaetze().empty(); }, 60));
        // beleidigt: kommt so schnell nicht wieder, auch wenn das Fenster voll ist
        RaeumEinstellungen e;
        w.rd.setEinstellungen(e);
        w.fuellen("a", 19);
        w.laufen(5);
        QVERIFY(w.rd.einsaetze().empty());
    }
    void trefferWaehrendDesStreiksMachenIhnSofortBeleidigt() {
        Welt w;
        RaeumEinstellungen e;
        e.an = false;
        e.trefferBisStreik = 1;
        w.rd.setEinstellungen(e);
        w.fuellen("a", 16);
        w.buche("a");
        QVERIFY(w.bis([&] { return w.raeumt(); }, 120));
        w.triff();
        QCOMPARE(w.einsatz()->fz.phase, FahrPhase::Streik);
        const int s1 = w.einsatz()->fz.spruch;
        w.triff();
        QCOMPARE(w.einsatz()->fz.phase, FahrPhase::Streik);
        QVERIFY(w.einsatz()->fz.spruch != s1);                  // neuer Spruch
        w.triff();
        QCOMPARE(w.einsatz()->fz.phase, FahrPhase::Beleidigt);
    }
    void anfassbarIstNurHoherSchneeUndDasFahrzeug() {
        Welt w(true);
        w.fuellen("a", 4);
        QVERIFY(w.rd.eingabeRechtecke().empty());               // dünner Schnee: alles klickt durch
        const Decke *b = w.sim.decke("b");
        for (int i = 100; i < 150; ++i) w.sim.schneeHinzufuegen("b", b->x + (i + 0.5) * b->spaltenBreite, 12);
        auto r = w.rd.eingabeRechtecke();
        QCOMPARE(r.size(), size_t(1));
        QVERIFY(std::abs(r[0].x - 500) < 1 && std::abs(r[0].w - 200) < 1);
        QVERIFY(r[0].y >= 900 - 15 && r[0].unten() <= 900 + 1e-6 && r[0].h <= 15);
        // Fahrzeug ist anfassbar
        w.fuellen("a", 12);
        w.buche("a");
        QVERIFY(w.bis([&] { return w.raeumt(); }, 120));
        const Fahrzeug &f = w.einsatz()->fz;
        bool drin = false;
        for (const Rechteck &q : w.rd.eingabeRechtecke()) drin |= q.enthaelt(f.x, f.y - 20);
        QVERIFY(drin);
        QVERIFY(w.rd.mausDruck(f.x, f.y - 20, 0));              // anstupsen
        QVERIFY(!w.rd.ballInHand());
        // abgeschaltet: gar keine Eingabefläche
        RaeumEinstellungen e = w.rd.einstellungen();
        e.anfassbar = false;
        w.rd.setEinstellungen(e);
        QVERIFY(w.rd.eingabeRechtecke().empty());
    }
    void dauerklaengeLaufenUndEndenMitDemEinsatz() {
        Welt w;
        w.fuellen("a", 16);
        w.buche("a");
        w.laufen(0.2);
        std::set<int> rotor, motor, warner, gestoppt;
        auto sammeln = [&] {
            for (const Klangereignis &k : w.rd.klaengeAbholen()) {
                if (k.typ == Klangereignis::Dauer) {
                    QVERIFY(k.staerke >= 0 && k.staerke <= 1);
                    (k.art == Geraeusch::Rotor ? rotor : k.art == Geraeusch::Motor ? motor : warner).insert(k.id);
                    gestoppt.erase(k.id);
                } else if (k.typ == Klangereignis::Stopp) gestoppt.insert(k.id);
            }
        };
        QVERIFY(w.bis([&] { sammeln(); return w.rd.einsaetze().empty(); }, 240));
        w.laufen(0.1);
        sammeln();
        QCOMPARE(rotor.size(), size_t(1));
        QCOMPARE(motor.size(), size_t(1));
        QCOMPARE(warner.size(), size_t(1));       // beim Zurücksetzen piept er
        for (int id : rotor) QVERIFY(gestoppt.count(id));
        for (int id : motor) QVERIFY(gestoppt.count(id));
        for (int id : warner) QVERIFY(gestoppt.count(id));
        // Pause: alles Laufende wird gestoppt
        w.fuellen("a", 16);
        w.rd.jetztRaeumen();
        w.laufen(0.2);
        w.rd.klaengeAbholen();
        w.rd.klaengeAnhalten();
        int stopps = 0;
        for (const Klangereignis &k : w.rd.klaengeAbholen()) stopps += k.typ == Klangereignis::Stopp;
        QCOMPARE(stopps, 1);
    }
    void wendenIstEineDrehungAufDerStelle() {
        Welt w;
        w.fuellen("a", 16);
        w.buche("a");
        // erste Wende: nach der rechten Bahn, zurück in der Mitte
        QVERIFY(w.bis([&] { return w.einsatz() && w.einsatz()->fz.phase == FahrPhase::Wenden && w.einsatz()->fz.dreht; }, 120));
        const Fahrzeug &f = w.einsatz()->fz;
        const double x0 = f.x, kette0 = f.kette, dreh0 = f.drehweg;
        QCOMPARE(f.blick, 1);
        QVERIFY(f.schild > 0.97);                      // Schild ist angehoben
        w.rd.klaengeAbholen();
        double zeit = 0, letzter = f.gier, groessterSchritt = 0, hoechsteLast = 0;
        bool front = false, heck = false, monoton = true, staub = false;
        while (f.phase == FahrPhase::Wenden && zeit < 10) {
            w.laufen(1.0 / 30);
            zeit += 1.0 / 30;
            if (f.phase != FahrPhase::Wenden) break;
            monoton &= f.gier >= letzter - 1e-9;
            groessterSchritt = std::max(groessterSchritt, f.gier - letzter);
            letzter = f.gier;
            front |= std::abs(f.gier - M_PI / 2) < 0.15;
            heck |= std::abs(f.gier - 3 * M_PI / 2) < 0.15;
            hoechsteLast = std::max(hoechsteLast, f.last);
            staub |= !w.rd.staub().empty();
            QVERIFY(std::abs(f.x - x0) < 1e-6);        // dreht auf der Stelle
            QVERIFY(std::abs(f.v) < 1e-6);
        }
        QVERIFY2(zeit >= 1.5 && zeit <= 2.6, qPrintable(QString::number(zeit)));   // so lange braucht ein Kettenfahrzeug
        QVERIFY(monoton);
        QVERIFY2(groessterSchritt < 0.12, "Drehung ruckt (Winkelsprung)");          // gleitend, kein Umklappen
        QVERIFY(front && !heck);                       // von rechts nach links über die Frontansicht
        QCOMPARE(f.blick, -1);
        QVERIFY(std::abs(f.gier - M_PI) < 1e-9);
        QVERIFY(std::abs(f.drehweg - dreh0) > 30);     // die Ketten sind gegenläufig gelaufen (rechte vor, linke zurück) …
        QCOMPARE(f.kette, kette0);                     // … ohne dass das Fahrzeug Strecke gemacht hat
        QVERIFY2(hoechsteLast > 0.55, qPrintable(QString::number(hoechsteLast)));   // Motor arbeitet
        QVERIFY(staub);                                // Ketten wirbeln Schnee auf
        bool motorLaut = false;
        for (const Klangereignis &k : w.rd.klaengeAbholen()) motorLaut |= k.typ == Klangereignis::Dauer && k.art == Geraeusch::Motor && k.staerke > 0.5;
        QVERIFY(motorLaut);
        // der nächste Einsatz dreht andersherum: über die Heckansicht
        QVERIFY(w.bis([&] { return w.rd.einsaetze().empty(); }, 120));
        w.fuellen("a", 16);
        QVERIFY(!w.rd.jetztRaeumen().empty());
        QVERIFY(w.bis([&] { return w.einsatz() && w.einsatz()->fz.phase == FahrPhase::Wenden && w.einsatz()->fz.dreht; }, 120));
        const Fahrzeug &f2 = w.einsatz()->fz;
        front = heck = false;
        while (w.einsatz() && f2.phase == FahrPhase::Wenden) {
            w.laufen(1.0 / 30);
            front |= std::abs(f2.gier - M_PI / 2) < 0.15;
            heck |= std::abs(f2.gier - 3 * M_PI / 2) < 0.15;
        }
        QVERIFY(heck && !front);
        QCOMPARE(f2.blick, -1);
        QVERIFY(std::abs(f2.gier - M_PI) < 1e-9);
        QCOMPARE(wendeDauer(1.0), 1.8);
        QVERIFY(wendeDauer(0.5) >= 1.5 && wendeDauer(3.0) <= 2.5 && wendeDauer(3.0) > wendeDauer(1.0));
    }
    void wendenBrauchtPlatzUndRangiertSonstErst() {
        Welt w;
        w.fuellen("a", 16);
        w.buche("a");
        QVERIFY(w.bis([&] { return w.raeumt(); }, 120));
        // ein Fenster schiebt sich über die linke Hälfte: befahrbar bleibt nur 985 … 1300, die Mitte liegt jetzt am Rand
        w.fenster_.push_back(fenster("c", 600, 300, 385, 300, 5));
        w.sim.setFenster(w.fenster_);
        const Fahrzeug &f = w.einsatz()->fz;
        QVERIFY(w.bis([&] { return w.einsatz() && f.phase == FahrPhase::Wenden; }, 120));
        QVERIFY(!w.einsatz()->abbruch);
        const double radius = wendeRadius(Fahrzeugart::Raupe, 1.0);
        QCOMPARE(radius, 39.0);
        bool gedreht = false;
        double zeit = 0;
        while (f.phase == FahrPhase::Wenden && zeit < 20) {
            if (f.dreht) {
                gedreht = true;
                // während der Drehung ragt nichts weiter als das erlaubte Spiel über den befahrbaren Abschnitt
                QVERIFY2(f.x - radius >= w.einsatz()->segA - 4 - 1.01 && f.x + radius <= w.einsatz()->segB + 4 + 1.01,
                         qPrintable(QString("x=%1 Abschnitt %2 … %3").arg(f.x).arg(w.einsatz()->segA).arg(w.einsatz()->segB)));
            } else {
                QVERIFY(std::abs(std::sin(f.gier)) < 1e-9);   // beim Rangieren steht es noch gerade
            }
            w.laufen(1.0 / 30);
            zeit += 1.0 / 30;
        }
        QVERIFY(gedreht);
        QVERIFY(f.x >= 985 + radius - 4 - 1.01);             // ist dafür ein Stück von der Kante weggefahren
    }
    void trefferWaehrendDerDrehungWartenUndHubschrauberGiertGleitend() {
        Welt w;
        RaeumEinstellungen e;
        e.an = false;
        e.trefferBisStreik = 1;
        w.rd.setEinstellungen(e);
        w.fenster_[0].r = {300, 400, 1400, 300};   // breit genug für einen zweiten Durchgang nach links … rechts
        w.sim.setFenster(w.fenster_);
        w.fuellen("a", 16);
        w.buche("a");
        const auto dreht = [&] { return w.einsatz() && w.einsatz()->fz.phase == FahrPhase::Wenden && w.einsatz()->fz.dreht; };
        QVERIFY(w.bis(dreht, 120));
        const Fahrzeug &f = w.einsatz()->fz;
        w.laufen(0.5);
        QVERIFY(dreht());
        // Treffer mitten in der Drehung: er dreht zu Ende und beschwert sich dann
        w.triff();
        QCOMPARE(f.phase, FahrPhase::Wenden);
        QVERIFY(w.bis([&] { return f.phase != FahrPhase::Wenden; }, 5));
        QCOMPARE(f.phase, FahrPhase::Streik);
        QVERIFY(std::abs(f.gier - M_PI) < 1e-9);
        QVERIFY(f.spruch >= 0);
        // Hubschrauber: giert gleitend statt umzuklappen
        const Hubschrauber &h = w.einsatz()->heli;
        double letzter = h.gier, sprung = 0;
        for (int i = 0; i < 30 * 40 && w.einsatz(); ++i) {
            w.laufen(1.0 / 30);
            if (!w.einsatz()) break;
            double d = std::abs(h.gier - letzter);
            if (d > M_PI) d = 2 * M_PI - d;
            sprung = std::max(sprung, d);
            letzter = h.gier;
        }
        QVERIFY2(sprung < 0.1, qPrintable(QString::number(sprung)));
    }
    void hubschrauberGiertUeberFrontUndHeck() {
        Hubschrauber h;
        h.zielX = -1000;                         // Ziel weit links: dreht von rechts (0) über die Front (π/2) nach links (π)
        bool front = false;
        double zeit = 0;
        for (; zeit < 5 && std::abs(h.gier - M_PI) > 1e-9; zeit += 1.0 / 30) {
            h.schritt(1.0 / 30, 260, 220);
            front |= std::abs(h.gier - M_PI / 2) < 0.1;
            h.zielX = h.x - 1000;
        }
        QVERIFY(front);
        QVERIFY2(zeit > 1.0 && zeit < 2.0, qPrintable(QString::number(zeit)));
        // zurück nach rechts: weiter in derselben Drehrichtung, also über die Heckansicht (3π/2)
        bool heck = false;
        for (zeit = 0; zeit < 5 && h.gier > 1e-9; zeit += 1.0 / 30) {
            h.zielX = h.x + 1000;
            h.schritt(1.0 / 30, 260, 220);
            heck |= std::abs(h.gier - 3 * M_PI / 2) < 0.1;
        }
        QVERIFY(heck);
        QCOMPARE(h.gier, 0.0);
        // nah am Ziel bleibt er, wie er steht (kein Hin und Her beim Schweben)
        h.zielX = h.x - 50;
        for (int i = 0; i < 60; ++i) h.schritt(1.0 / 30, 260, 220);
        QCOMPARE(h.gier, 0.0);
    }
    void radpflugWendetInZuegenStattAufDerStelle() {
        Welt w;
        RaeumEinstellungen e;
        e.an = false;
        w.rd.setEinstellungen(e);
        w.fuellen("boden:0", 18);
        w.buche("boden:0");
        QVERIFY(w.bis([&] { return w.einsatz() && w.einsatz()->fz.phase == FahrPhase::Wenden && w.einsatz()->fz.dreht; }, 600));
        const Fahrzeug &f = w.einsatz()->fz;
        QCOMPARE(f.art, Fahrzeugart::Pflug);
        QCOMPARE(f.wendeZuege, 3);                     // genug Platz: vor, zurück, vor
        const double x0 = f.x, kette0 = f.kette;
        w.rd.klaengeAbholen();
        double zeit = 0, letzter = f.gier, weitesteAusfahrt = 0, tiefste = 0;
        bool vor = false, zurueck = false, linksEin = false, rechtsEin = false, monoton = true, warner = false;
        int richtungswechsel = 0;
        double letztesTempo = 0;
        while (f.phase == FahrPhase::Wenden && zeit < 20) {
            w.laufen(1.0 / 30);
            zeit += 1.0 / 30;
            if (f.phase != FahrPhase::Wenden) break;
            monoton &= f.gier >= letzter - 1e-9;
            letzter = f.gier;
            weitesteAusfahrt = std::max(weitesteAusfahrt, std::abs(f.x - x0));
            tiefste = std::max(tiefste, std::abs(f.tiefeZ));
            vor |= f.v > 8;
            zurueck |= f.v < -8;
            linksEin |= f.lenk > 0.4;
            rechtsEin |= f.lenk < -0.4;
            if (std::abs(f.v) > 8 && letztesTempo * f.v < 0) ++richtungswechsel;
            if (std::abs(f.v) > 8) letztesTempo = f.v;
            // gedreht wird nur im Fahren: steht er, bleibt der Winkel
            for (const Klangereignis &k : w.rd.klaengeAbholen()) warner |= k.typ == Klangereignis::Dauer && k.art == Geraeusch::Rueckfahrwarner;
        }
        QVERIFY(monoton);
        QVERIFY2(weitesteAusfahrt > 15, qPrintable(QString::number(weitesteAusfahrt)));   // fährt Bögen, dreht nicht auf der Stelle
        QVERIFY(tiefste > 6);                          // holt dabei zum Betrachter hin aus
        QVERIFY(vor && zurueck);
        QCOMPARE(richtungswechsel, 2);                 // vor → zurück → vor
        QVERIFY(linksEin && rechtsEin);                // Vorderräder schlagen in beide Richtungen ein
        QVERIFY(warner);                               // piept beim Zurücksetzen
        QVERIFY2(zeit > 2 && zeit < 9, qPrintable(QString::number(zeit)));
        QCOMPARE(f.blick, -1);
        QVERIFY(std::abs(f.gier - M_PI) < 1e-9);
        QVERIFY2(std::abs(f.x - x0) < 4, qPrintable(QString::number(f.x - x0)));          // steht wieder, wo er losfuhr
        QVERIFY(std::abs(f.kette - kette0) > 25);      // die Räder sind gerollt (zwei Bögen vor, einer zurück)
        // danach stellt er die Räder gerade und fährt in der Spur weiter
        w.laufen(1.5);
        QVERIFY(std::abs(f.lenk) < 0.02);
        QVERIFY(std::abs(f.tiefeZ) < 0.5);
    }
    void radpflugBrauchtBeiWenigPlatzMehrZuege() {
        Welt w;
        RaeumEinstellungen e;
        e.an = false;
        w.rd.setEinstellungen(e);
        Fenster leiste = fenster("leiste", 900, 1100, 100, 100, 9);   // schmale Taskleiste
        leiste.dock = true;
        w.fenster_.push_back(leiste);
        w.sim.setFenster(w.fenster_);
        w.fuellen("leiste", 16);
        w.buche("leiste");
        QVERIFY(w.einsatz());
        QVERIFY(w.bis([&] { return w.einsatz() && w.einsatz()->fz.phase == FahrPhase::Wenden && w.einsatz()->fz.dreht; }, 300));
        const Fahrzeug &f = w.einsatz()->fz;
        QCOMPARE(f.art, Fahrzeugart::Pflug);
        QVERIFY2(f.wendeZuege > 3, qPrintable(QString::number(f.wendeZuege)));
        const double x0 = f.x;
        double weiteste = 0, zeit = 0;
        while (f.phase == FahrPhase::Wenden && zeit < 30) { w.laufen(1.0 / 30); zeit += 1.0 / 30; weiteste = std::max(weiteste, std::abs(f.x - x0)); }
        QVERIFY(std::abs(f.gier - M_PI) < 1e-9);
        // kürzere Bögen: er holt weniger weit aus als bei der Wende in drei Zügen (dort knapp 28 px)
        QVERIFY2(weiteste < 22, qPrintable(QString::number(weiteste)));
    }
    void wulstformWaechstUndBleibtBegrenzt() {
        QCOMPARE(wulstHoehe(0, 1), 0.0);
        QVERIFY(wulstHoehe(200, 1) > wulstHoehe(50, 1));
        QVERIFY(wulstHoehe(1e6, 1) <= 26.0 + 1e-9);                // nie höher als das Schild erlaubt
        QVERIFY(wulstLaenge(4000, 1) > wulstLaenge(1000, 1));
        QVERIFY(wulstHoehe(1e6, 2) > wulstHoehe(1e6, 1));          // größeres Fahrzeug, größerer Wulst
        // Länge ↔ Fläche passen zusammen
        for (double a : {10.0, 300.0, 1352.0, 3000.0})
            QVERIFY(std::abs(wulstFlaecheFuerLaenge(wulstLaenge(a, 1), 1) - a) < 1e-6 * a + 1e-9);
        // Profil fällt nach vorn ab und endet am Wulstende
        QVERIFY(wulstProfil(1000, 1, 0) > wulstProfil(1000, 1, 20));
        QCOMPARE(wulstProfil(1000, 1, wulstLaenge(1000, 1) + 1), 0.0);
    }
};

QTEST_MAIN(TestRaeumdienst)
#include "test_raeumdienst.moc"
