#include <QtTest>
#include "schnee.h"
#include "einstellungen.h"

using namespace schnee;

namespace {
Einstellungen ruhig() {
    Einstellungen e;
    e.wind = 0;
    e.ablageJeFlocke = 1.0;
    e.maxHoehe = 20;
    e.spaltenBreite = 4;
    return e;
}
Fenster fenster(const char *id, double x, double y, double w, double h, int stapel) {
    Fenster f; f.id = id; f.r = {x, y, w, h}; f.stapel = stapel; return f;
}
Flocke flocke(double x, double y) { return Flocke{x, y, 0, 60, 3, 0}; }
// höchste Deckenhöhe in der Umgebung von x (Flocken schaukeln ein paar px seitlich)
double hoeheUm(const Simulation &s, const char *id, double x, double radius = 8) {
    double m = 0;
    for (double t = x - radius; t <= x + radius; t += 1) m = std::max(m, s.hoeheBei(id, t));
    return m;
}
// lässt die Simulation ohne neue Flocken laufen
void laufen(Simulation &s, double sekunden) {
    for (double t = 0; t < sekunden; t += 1.0 / 60) s.schritt(1.0 / 60, false);
}
}

class TestSchnee : public QObject {
    Q_OBJECT
private slots:
    void flockeBleibtAufFensterkanteLiegen();
    void verdeckteKanteNimmtKeinenSchneeAuf();
    void deckeIstBegrenztUndRutschtAmRandAb();
    void deckeZerfaelltBeiVeraenderung_data();
    void deckeZerfaelltBeiVeraenderung();
    void schmalerWerdenWirftUeberstandAb();
    void bodenSammeltUndSchmilzt();
    void flockenNurAufMonitorflaechen();
    void taskleisteTraegtSchneeStattBoden();
    void abschuettelnLaesstAlleDeckenFallen();
    void vollbildPausiertNurWennAktiv();
    void einstellungenLesen();
    void einstellungenGrenzenUndFehler();
    void einstellungenRundreise();
    void maximiertesFensterAmOberenMonitorrandTraegtSchnee();
    void vordereFlockenFallenVorFensternWeiter();
    void bodenWirdNurVonDocksVerdeckt();
    void henrysMonitoreUndFenster();
    void vorfuellenVerteiltFlockenSofortUeberAlleMonitore();
    void bodenSchmilztLangsamerAlsEsSchneit();
    void hinterFensternWerdenFlockenImFensterNichtGezeichnet();
    void flockendetailLesenUndSchreiben();
    void abfallenderSchneeIstPulverNeuerSchneeKristall();
    void groessereFlockenFallenSchneller();
    void deckenStandMeldetSprunghafteAenderungen();
    void abfallenderSchneeFaelltFastSenkrecht();
};

void TestSchnee::flockeBleibtAufFensterkanteLiegen() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    s.setFenster({fenster("a", 100, 300, 400, 300, 1)});
    s.flockeHinzufuegen(flocke(250, 250));
    laufen(s, 2.0);
    QCOMPARE(int(s.flocken().size()), 0);
    QVERIFY(hoeheUm(s, "a", 250) > 0.5);
    QCOMPARE(s.gesamtSchnee("a"), 1.0);
}

void TestSchnee::verdeckteKanteNimmtKeinenSchneeAuf() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    // "b" liegt oben und überdeckt die Kante von "a" zwischen x=200 und x=400
    s.setFenster({fenster("a", 100, 300, 400, 300, 1), fenster("b", 200, 250, 200, 200, 2)});
    s.flockeHinzufuegen(flocke(300, 270));   // unterhalb von b's Kante, über a's verdeckter Kante
    s.flockeHinzufuegen(flocke(450, 250));   // über a's sichtbarem Teil
    laufen(s, 6.0);
    QCOMPARE(hoeheUm(s, "a", 300, 80), 0.0);
    QVERIFY(hoeheUm(s, "a", 450) > 0.5);
    // die andere Flocke fiel durch, statt auf dem verdeckten Teil liegen zu bleiben
    QCOMPARE(s.gesamtSchnee("a"), 1.0);
    // wird b danach nach unten gestapelt, ist die Kante wieder frei
    s.setFenster({fenster("a", 100, 300, 400, 300, 2), fenster("b", 200, 250, 200, 200, 1)});
    s.flockeHinzufuegen(flocke(300, 200));
    laufen(s, 2.0);
    QVERIFY(hoeheUm(s, "b", 300) > 0.5);   // b's Kante bleibt sichtbar (liegt höher)
    s.flockeHinzufuegen(flocke(450, 280));
    laufen(s, 2.0);
    QVERIFY(hoeheUm(s, "a", 450) > 1.5);
}

void TestSchnee::deckeIstBegrenztUndRutschtAmRandAb() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    s.setFenster({fenster("a", 100, 300, 40, 300, 1)});   // 10 Spalten à 4 px
    bool fielHerunter = false;
    for (int i = 0; i < 400; ++i) {
        s.flockeHinzufuegen(flocke(118, 200));         // immer dieselbe Spalte
        s.schritt(0.05, false);
        for (const Flocke &f : s.flocken())
            if ((f.x < 100 || f.x >= 140) && f.y > 290) fielHerunter = true;
    }
    laufen(s, 10.0);
    const Decke *d = s.decke("a");
    QVERIFY(d);
    float maxH = *std::max_element(d->hoehe.begin(), d->hoehe.end());
    QVERIFY2(maxH <= 20.0f + 1.0f, qPrintable(QString::number(maxH)));
    // Schnee ist zu den Nachbarn gerutscht, auch bis an die Ränder
    QVERIFY(d->hoehe.front() > 0.5f);
    QVERIFY(d->hoehe.back() > 0.5f);
    // ein Teil ist über die Fensterkanten heruntergefallen
    QVERIFY(fielHerunter);
    QVERIFY(s.gesamtSchnee("a") < 400.0);
}

void TestSchnee::deckeZerfaelltBeiVeraenderung_data() {
    QTest::addColumn<QString>("art");
    QTest::newRow("verschoben") << "verschoben";
    QTest::newRow("geschlossen") << "geschlossen";
    QTest::newRow("minimiert") << "minimiert";
    QTest::newRow("verdeckt") << "verdeckt";
}

void TestSchnee::deckeZerfaelltBeiVeraenderung() {
    QFETCH(QString, art);
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    s.setFenster({fenster("a", 100, 300, 400, 300, 1)});
    for (int i = 0; i < 20; ++i) s.flockeHinzufuegen(flocke(120 + i * 15, 250));
    laufen(s, 2.0);
    QCOMPARE(s.gesamtSchnee("a"), 20.0);
    QCOMPARE(int(s.flocken().size()), 0);

    if (art == "verschoben") s.setFenster({fenster("a", 150, 400, 400, 300, 1)});
    if (art == "geschlossen") s.setFenster({});
    if (art == "minimiert") { Fenster f = fenster("a", 100, 300, 400, 300, 1); f.minimiert = true; s.setFenster({f}); }
    if (art == "verdeckt") s.setFenster({fenster("a", 100, 300, 400, 300, 1), fenster("b", 50, 200, 600, 400, 2)});

    QCOMPARE(s.gesamtSchnee("a"), 0.0);
    // der Schnee ist zu fallenden Flocken an der alten Stelle geworden
    QVERIFY(s.flocken().size() >= 10);
    for (const Flocke &f : s.flocken()) {
        QVERIFY(f.x >= 95 && f.x <= 505);
        QVERIFY(f.y <= 301 && f.y > 270);
        QVERIFY(f.vy > 0);
    }
    // und fällt nach unten
    double yVorher = s.flocken().front().y;
    s.schritt(0.1, false);
    QVERIFY(s.flocken().front().y > yVorher);
}

void TestSchnee::schmalerWerdenWirftUeberstandAb() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    s.setFenster({fenster("a", 100, 300, 400, 300, 1)});
    for (int i = 0; i < 20; ++i) s.flockeHinzufuegen(flocke(120 + i * 15, 250));
    laufen(s, 2.0);
    // gleiche Oberkante, nur schmaler: linker Teil bleibt liegen
    s.setFenster({fenster("a", 100, 300, 200, 300, 1)});
    QCOMPARE(s.gesamtSchnee("a"), 12.0);   // x = 120 … 285
    QVERIFY(s.flocken().size() >= 7);
    for (const Flocke &f : s.flocken()) QVERIFY(f.x >= 295);
}

void TestSchnee::bodenSammeltUndSchmilzt() {
    Einstellungen e = ruhig();
    e.schmelzen = 0.5;
    Simulation s(e);
    s.setBildschirme({{0, 0, 1000, 800}});
    s.flockeHinzufuegen(flocke(500, 700));
    s.flockeHinzufuegen(flocke(504, 700));
    laufen(s, 3.0);
    QCOMPARE(int(s.flocken().size()), 0);
    QVERIFY(hoeheUm(s, "boden:0", 502) > 0.5);
    laufen(s, 60.0);
    QCOMPARE(s.gesamtSchnee("boden:0"), 0.0);
}

void TestSchnee::flockenNurAufMonitorflaechen() {
    Einstellungen ohneSchmelzen = ruhig();
    ohneSchmelzen.schmelzen = 0;
    Simulation s(ohneSchmelzen);
    // zwei nebeneinander mit Lücke, und ein dritter direkt unter dem ersten
    s.setBildschirme({{0, 0, 500, 500}, {600, 0, 500, 800}, {0, 500, 500, 500}});
    s.flockeHinzufuegen(flocke(550, 100));   // in der Lücke
    s.schritt(1.0 / 60, false);
    QCOMPARE(int(s.flocken().size()), 0);
    // fällt vom oberen in den unteren Monitor und landet erst ganz unten
    s.flockeHinzufuegen(flocke(250, 450));
    laufen(s, 15.0);
    QCOMPARE(s.gesamtSchnee("boden:0"), 0.0);
    QVERIFY(hoeheUm(s, "boden:2", 250) > 0.5);
    // keine Flocken entstehen über einem Monitor, der unter einem anderen liegt
    Einstellungen e = ruhig();
    e.menge = 50;
    Simulation t(e);
    t.setBildschirme({{0, 0, 500, 500}, {0, 500, 500, 500}});
    t.schritt(0.2, true);
    QVERIFY(!t.flocken().empty());
    for (const Flocke &f : t.flocken()) QVERIFY(f.y < 30);
}

void TestSchnee::taskleisteTraegtSchneeStattBoden() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    Fenster leiste = fenster("leiste", 0, 760, 1000, 40, 5);
    leiste.dock = true;
    s.setFenster({leiste});
    s.flockeHinzufuegen(flocke(300, 700));
    laufen(s, 3.0);
    QVERIFY(hoeheUm(s, "leiste", 300) > 0.5);
    QCOMPARE(s.gesamtSchnee("boden:0"), 0.0);
}

void TestSchnee::abschuettelnLaesstAlleDeckenFallen() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    s.setFenster({fenster("a", 100, 300, 400, 300, 1)});
    for (int i = 0; i < 10; ++i) s.flockeHinzufuegen(flocke(120 + i * 30, 250));
    laufen(s, 2.0);
    QCOMPARE(s.gesamtSchnee("a"), 10.0);
    s.abschuetteln();
    QCOMPARE(s.gesamtSchnee("a"), 0.0);
    QVERIFY(s.flocken().size() >= 8);
    // Regression 18.11.: die Flocken blieben sofort wieder auf derselben Kante liegen
    laufen(s, 0.5);
    QCOMPARE(s.gesamtSchnee("a"), 0.0);
    QVERIFY(!s.flocken().empty());
    for (const Flocke &f : s.flocken()) QVERIFY(f.y > 300);   // unter die Kante gefallen
    // die Decke selbst bleibt bestehen und sammelt weiter
    s.flockeHinzufuegen(flocke(250, 200));
    s.alleFlockenEntfernen();
    s.flockeHinzufuegen(flocke(250, 200));
    laufen(s, 3.0);
    QVERIFY(s.gesamtSchnee("a") >= 1.0);
}

void TestSchnee::vollbildPausiertNurWennAktiv() {
    Fenster spiel = fenster("spiel", 0, 0, 1000, 800, 3);
    spiel.vollbild = true;
    Fenster editor = fenster("ed", 10, 10, 300, 300, 2);
    QVERIFY(!vollbildAktiv({spiel, editor}));     // Vollbild, aber nicht aktiv
    editor.aktiv = true;
    QVERIFY(!vollbildAktiv({spiel, editor}));
    editor.aktiv = false;
    spiel.aktiv = true;
    QVERIFY(vollbildAktiv({spiel, editor}));
    spiel.minimiert = true;
    QVERIFY(!vollbildAktiv({spiel, editor}));
}

void TestSchnee::einstellungenLesen() {
    auto p = einstellungenAusText(
        "# Kommentar\n"
        "menge = 2.5\n"
        "wind=-30\n"
        "flockengroesse=4\n"
        "max_deckenhoehe=25\n"
        "fallgeschwindigkeit=60\n"
        "fps=30\n"
        "an=false\n"
        "vollbild_pausieren=nein\n");
    QCOMPARE(p.sim.menge, 2.5);
    QCOMPARE(p.sim.wind, -30.0);
    QCOMPARE(p.sim.groesse, 4.0);
    QCOMPARE(p.sim.maxHoehe, 25.0);
    QCOMPARE(p.sim.fallGeschw, 60.0);
    QCOMPARE(p.fps, 30);
    QCOMPARE(p.an, false);
    QCOMPARE(p.vollbildPausieren, false);
}

void TestSchnee::einstellungenGrenzenUndFehler() {
    auto p = einstellungenAusText("menge=1000\nwind=abc\nflockengroesse=-5\nunbekannt=1\nfps=1000\nmax_deckenhoehe=0\n");
    Programmeinstellungen standard;
    QCOMPARE(p.sim.menge, 10.0);                 // nach oben begrenzt
    QCOMPARE(p.sim.wind, standard.sim.wind);     // kaputter Wert -> Standard
    QCOMPARE(p.sim.groesse, 1.0);                // nach unten begrenzt
    QCOMPARE(p.fps, 120);
    QCOMPARE(p.sim.maxHoehe, 2.0);
    QCOMPARE(p.an, true);
    QCOMPARE(p.vollbildPausieren, true);
    // leere Datei = Standardwerte
    auto leer = einstellungenAusText("");
    QCOMPARE(leer.sim.menge, standard.sim.menge);
    QCOMPARE(leer.fps, standard.fps);
}

void TestSchnee::einstellungenRundreise() {
    Programmeinstellungen p;
    p.sim.menge = 0.5;
    p.sim.wind = 12.25;
    p.sim.groesse = 5;
    p.sim.maxHoehe = 30;
    p.sim.fallGeschw = 70;
    p.fps = 45;
    p.an = false;
    p.vollbildPausieren = false;
    std::string text = einstellungenAlsText(p);
    QVERIFY(text.find("# ") != std::string::npos);   // mit Erklärungen für Menschen
    auto q = einstellungenAusText(text);
    QCOMPARE(q.sim.menge, 0.5);
    QCOMPARE(q.sim.wind, 12.25);
    QCOMPARE(q.sim.groesse, 5.0);
    QCOMPARE(q.sim.maxHoehe, 30.0);
    QCOMPARE(q.sim.fallGeschw, 70.0);
    QCOMPARE(q.fps, 45);
    QCOMPARE(q.an, false);
    QCOMPARE(q.vollbildPausieren, false);
}

// Fehler 2 (Henry 18.11.): maximierte Fenster liegen mit der Oberkante genau auf dem Monitorrand
void TestSchnee::maximiertesFensterAmOberenMonitorrandTraegtSchnee() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    s.setFenster({fenster("max", 0, 0, 1000, 760, 1)});
    s.flockeHinzufuegen(flocke(500, -2));   // kommt gerade oben herein
    laufen(s, 1.0);
    QVERIFY(hoeheUm(s, "max", 500) > 0.5);
}

// Fehler 1: Schnee fällt auch vor großen Fenstern weiter (nicht jede Flocke bleibt auf dem Dach)
void TestSchnee::vordereFlockenFallenVorFensternWeiter() {
    Einstellungen e;
    e.wind = 0;
    Simulation s(e, 3);
    s.setBildschirme({{0, 0, 1000, 800}});
    s.setFenster({fenster("gross", 50, 100, 900, 650, 1)});
    for (int i = 0; i < 60 * 40; ++i) s.schritt(1.0 / 60, true);
    int unten = 0;
    for (const Flocke &f : s.flocken())
        if (f.y > 450 && f.x > 100 && f.x < 900) ++unten;
    // ohne Fenster wären es rund menge/10000 * 800*350 = 28
    QVERIFY2(unten >= 8, qPrintable(QString::number(unten)));
    QVERIFY(s.gesamtSchnee("gross") > 20);
}

// Fehler 3: Boden unter normalen Fenstern sammelt trotzdem (Ebene liegt über allem), nur Docks tragen selbst
void TestSchnee::bodenWirdNurVonDocksVerdeckt() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    Fenster leiste = fenster("leiste", 600, 760, 400, 40, 5);
    leiste.dock = true;
    s.setFenster({fenster("unten", 0, 400, 500, 400, 1), leiste});
    Flocke vorne = flocke(200, 700);
    vorne.fang = 0;   // fällt an jedem Fenster vorbei
    s.flockeHinzufuegen(vorne);
    laufen(s, 3.0);
    QVERIFY(hoeheUm(s, "boden:0", 200) > 0.5);
    QCOMPARE(hoeheUm(s, "boden:0", 800, 150), 0.0);
}

// Nachbau von Henrys Aufbau (kscreen-doctor + KWin-Mitschnitt 18.11.)
void TestSchnee::henrysMonitoreUndFenster() {
    Simulation s(Einstellungen{}, 7);
    s.setBildschirme({{0, 1100, 1080, 1920}, {1080, 1440, 3413.33, 1440}, {1586, 0, 2560, 1440}, {4494, 1800, 1920, 1080}});
    Fenster leiste = fenster("leiste", 2218, 2824, 1137, 56, 11);
    leiste.dock = true;
    s.setFenster({fenster("sysmon", 0, 1100, 1080, 664, 4), fenster("ts", 0, 1764, 1080, 1256, 5),
                  fenster("steam", 1707.3, 1700, 1219.3, 678, 6), fenster("zen", 1155.6, 1534.8, 2152, 1284.7, 8),
                  fenster("zen2", 4494, 1800, 1920, 1080, 9), fenster("konsole", 3087.25, 1657.97, 1383.3, 1031.3, 10), leiste});
    s.vorfuellen();
    for (int i = 0; i < 60 * 120; ++i) s.schritt(1.0 / 60, true);
    int mitte = 0;
    for (const Flocke &f : s.flocken())
        if (f.y > 2000 && f.y < 2800 && f.x > 1700 && f.x < 4100) ++mitte;
    QVERIFY2(mitte >= 30, qPrintable(QString::number(mitte)));   // Mitte von DP-3 schneit es
    for (const char *id : {"sysmon", "ts", "zen", "zen2", "konsole", "leiste", "boden:1"})
        QVERIFY2(s.gesamtSchnee(id) > 10, id);
    QCOMPARE(s.gesamtSchnee("steam"), 0.0);   // liegt vollständig hinter Zen
}

void TestSchnee::vorfuellenVerteiltFlockenSofortUeberAlleMonitore() {
    Simulation s(Einstellungen{}, 5);
    s.setBildschirme({{0, 1100, 1080, 1920}, {1080, 1440, 3413.33, 1440}, {1586, 0, 2560, 1440}, {4494, 1800, 1920, 1080}});
    s.vorfuellen();
    QVERIFY(std::abs(int(s.flocken().size()) - s.zielAnzahl()) < s.zielAnzahl() / 10);
    int mitteDp3 = 0, luecke = 0;
    for (const Flocke &f : s.flocken()) {
        if (f.x > 2000 && f.x < 3500 && f.y > 2000 && f.y < 2800) ++mitteDp3;
        if (f.x > 1080 && f.x < 1586 && f.y < 1440) ++luecke;   // Lücke links über DP-3
    }
    QVERIFY2(mitteDp3 > 60, qPrintable(QString::number(mitteDp3)));
    QCOMPARE(luecke, 0);
}

void TestSchnee::bodenSchmilztLangsamerAlsEsSchneit() {
    Simulation s(Einstellungen{}, 9);
    s.setBildschirme({{0, 0, 1000, 600}});
    s.vorfuellen();
    for (int i = 0; i < 60 * 180; ++i) s.schritt(1.0 / 60, true);
    // nach 3 Minuten liegt am Boden sichtbar Schnee (im Mittel mehr als 1,5 px; ohne Schmelzen wären es gut 2)
    QVERIFY2(s.gesamtSchnee("boden:0") / (1000 / 3.0) > 1.5, qPrintable(QString::number(s.gesamtSchnee("boden:0"))));
}

void TestSchnee::hinterFensternWerdenFlockenImFensterNichtGezeichnet() {
    Einstellungen e = ruhig();
    e.vorFenstern = false;
    Simulation s(e);
    s.setBildschirme({{0, 0, 1000, 800}});
    Fenster leiste = fenster("leiste", 0, 760, 1000, 40, 5);
    leiste.dock = true;
    Fenster mini = fenster("mini", 600, 100, 300, 300, 3);
    mini.minimiert = true;
    s.setFenster({fenster("a", 100, 300, 400, 300, 1), leiste, mini});
    QVERIFY(!s.flockeZeichnen(flocke(250, 450)));   // mitten im Fenster
    QVERIFY(s.flockeZeichnen(flocke(250, 250)));    // über dem Fenster (Arbeitsfläche)
    QVERIFY(s.flockeZeichnen(flocke(250, 297)));    // auf der Decke über der Oberkante
    QVERIFY(s.flockeZeichnen(flocke(50, 450)));     // neben dem Fenster
    QVERIFY(s.flockeZeichnen(flocke(300, 780)));    // vor der Taskleiste
    QVERIFY(s.flockeZeichnen(flocke(700, 200)));    // minimiertes Fenster verdeckt nichts
    // Decken werden weiter gesammelt
    s.flockeHinzufuegen(flocke(250, 250));
    laufen(s, 2.0);
    QVERIFY(hoeheUm(s, "a", 250) > 0.5);
    // Standard: Schnee fällt vor den Fenstern
    Simulation v(ruhig());
    v.setBildschirme({{0, 0, 1000, 800}});
    v.setFenster({fenster("a", 100, 300, 400, 300, 1)});
    QVERIFY(v.flockeZeichnen(flocke(250, 450)));
    // Einstellung aus der Datei
    QCOMPARE(einstellungenAusText("vor_fenstern=false\n").sim.vorFenstern, false);
    QCOMPARE(einstellungenAusText("").sim.vorFenstern, true);
    Programmeinstellungen p;
    p.sim.vorFenstern = false;
    QCOMPARE(einstellungenAusText(einstellungenAlsText(p)).sim.vorFenstern, false);
}

// Aussehen: realistisch (Kristalle) ist Standard, „einfach“ gibt die alten weichen Punkte
void TestSchnee::flockendetailLesenUndSchreiben() {
    QCOMPARE(einstellungenAusText("").realistisch, true);
    QCOMPARE(einstellungenAusText("flockendetail=einfach\n").realistisch, false);
    QCOMPARE(einstellungenAusText("flockendetail = Realistisch\n").realistisch, true);
    QCOMPARE(einstellungenAusText("flockendetail=einfach\nflockendetail=quatsch\n").realistisch, false); // kaputter Wert ändert nichts
    Programmeinstellungen p;
    QVERIFY(einstellungenAlsText(p).find("flockendetail=realistisch\n") != std::string::npos);
    p.realistisch = false;
    QVERIFY(einstellungenAlsText(p).find("flockendetail=einfach\n") != std::string::npos);
    QCOMPARE(einstellungenAusText(einstellungenAlsText(p)).realistisch, false);
}

// Was von einer Decke herunterfällt, ist Pulver; was vom Himmel kommt, ein Kristall
void TestSchnee::abfallenderSchneeIstPulverNeuerSchneeKristall() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    s.vorfuellen();
    QVERIFY(!s.flocken().empty());
    for (const Flocke &f : s.flocken()) QCOMPARE(f.art, Flockenart::Kristall);
    s.alleFlockenEntfernen();
    s.setFenster({fenster("a", 100, 300, 400, 300, 1)});
    for (int i = 0; i < 10; ++i) s.flockeHinzufuegen(flocke(120 + i * 30, 250));
    laufen(s, 2.0);
    QVERIFY(s.flocken().empty());
    s.abschuetteln();
    QVERIFY(s.flocken().size() >= 8);
    for (const Flocke &f : s.flocken()) QCOMPARE(f.art, Flockenart::Pulver);
    // auch der Überhang, der am Fensterende abrutscht
    Einstellungen e = ruhig();
    Simulation t(e);
    t.setBildschirme({{0, 0, 1000, 800}});
    t.setFenster({fenster("a", 100, 300, 40, 300, 1)});
    for (int i = 0; i < 400; ++i) { t.flockeHinzufuegen(flocke(104, 290)); t.schritt(1.0 / 60, false); }
    QVERIFY(!t.flocken().empty());
    int pulver = 0;
    for (const Flocke &f : t.flocken()) if (f.art == Flockenart::Pulver) ++pulver;
    QVERIFY(pulver > 0);
}

// Tiefenwirkung: große (nahe) Flocken fallen deutlich schneller als kleine (ferne)
void TestSchnee::groessereFlockenFallenSchneller() {
    Einstellungen e;
    e.menge = 5;
    Simulation s(e);
    s.setBildschirme({{0, 0, 2000, 1000}});
    s.vorfuellen();
    double klein = 0, gross = 0;
    int nk = 0, ng = 0;
    for (const Flocke &f : s.flocken()) {
        if (f.groesse < e.groesse * 0.7) { klein += f.vy; ++nk; }
        if (f.groesse > e.groesse * 1.3) { gross += f.vy; ++ng; }
    }
    QVERIFY(nk > 20 && ng > 20);
    QVERIFY2(gross / ng > 1.6 * klein / nk, qPrintable(QString("%1 gegen %2").arg(gross / ng).arg(klein / nk)));
}

// Die Ebene zeichnet Decken aus einem Zwischenspeicher; bei Sprüngen muss sie es sofort erfahren
void TestSchnee::deckenStandMeldetSprunghafteAenderungen() {
    Simulation s(ruhig());
    s.setBildschirme({{0, 0, 1000, 800}});
    uint64_t stand = s.deckenStand();
    s.setFenster({fenster("a", 100, 300, 400, 300, 1)});
    QVERIFY(s.deckenStand() != stand);
    stand = s.deckenStand();
    for (int i = 0; i < 10; ++i) s.flockeHinzufuegen(flocke(120 + i * 30, 250));
    laufen(s, 2.0);
    QCOMPARE(s.deckenStand(), stand); // langsames Wachsen zählt nicht
    s.abschuetteln();
    QVERIFY(s.deckenStand() != stand);
    stand = s.deckenStand();
    s.setFenster({fenster("a", 150, 300, 400, 300, 1)}); // verschoben
    QVERIFY(s.deckenStand() != stand);
}

// Henry 18.11.: abrutschender Schnee soll fast senkrecht fallen, nicht schräg vom Fenster weg
void TestSchnee::abfallenderSchneeFaelltFastSenkrecht() {
    Einstellungen e; // Standardwerte, kein Wind
    e.groesse = 4;
    e.fallGeschw = 23;
    Simulation s(e);
    s.setBildschirme({{0, 0, 1000, 800}});
    s.setFenster({fenster("a", 300, 300, 60, 300, 1)});
    // Decke überfüllen, bis Schnee an den Enden abrutscht
    int geprueft = 0;
    for (int i = 0; i < 3000; ++i) {
        s.flockeHinzufuegen(Flocke{303 + (i % 18) * 3.0, 296, 0, 60, 4, 0});
        s.schritt(1.0 / 60, false);
        for (const Flocke &f : s.flocken()) {
            if (f.art != Flockenart::Pulver) continue;
            ++geprueft;
            // seitliche Bewegung (Drift + Schaukeln) höchstens ein Viertel der Fallgeschwindigkeit: unter 15 Grad
            QVERIFY2(std::abs(f.vx) + 0.75 * f.groesse <= 0.27 * f.vy, qPrintable(QString("vx %1 vy %2").arg(f.vx).arg(f.vy)));
        }
    }
    QVERIFY(geprueft > 50);
    // und der Weg selbst: nach dem Abschütteln bleibt jede Flocke nah an ihrer Senkrechten
    // (frische Simulation: nur die Fensterdecke trägt Schnee, keine Flocke verlässt den Bildschirm)
    Simulation t(e);
    t.setBildschirme({{0, 0, 1000, 800}});
    t.setFenster({fenster("a", 300, 300, 60, 300, 1)});
    for (int i = 0; i < 40; ++i) t.flockeHinzufuegen(Flocke{303 + (i % 18) * 3.0, 296, 0, 60, 4, 0});
    laufen(t, 1.0);
    QVERIFY(t.flocken().empty());
    t.abschuetteln();
    std::vector<Flocke> start = t.flocken();
    QVERIFY(start.size() > 20);
    for (int i = 0; i < 60; ++i) t.schritt(1.0 / 60, false);
    QCOMPARE(t.flocken().size(), start.size());
    for (size_t i = 0; i < start.size(); ++i) {
        const double dx = std::abs(t.flocken()[i].x - start[i].x), dy = t.flocken()[i].y - start[i].y;
        QVERIFY2(dy > 15 && dx < 0.3 * dy, qPrintable(QString("dx %1 dy %2").arg(dx).arg(dy)));
    }
}

QTEST_GUILESS_MAIN(TestSchnee)
#include "test_schnee.moc"
