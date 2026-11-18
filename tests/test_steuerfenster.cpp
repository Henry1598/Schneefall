// Steuerfenster bedienen wie ein Mensch: Regler ziehen, Häkchen setzen, Datei prüfen.
// Läuft offscreen mit eigenem XDG_CONFIG_HOME (berührt nie die echte Konfiguration).
#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QTemporaryDir>
#include "app.h"
#include "steuerfenster.h"
#include "overlay.h"

class TestSteuerfenster : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        qputenv("XDG_CONFIG_HOME", m_dir.path().toUtf8());
    }
    void reglerUndHaekchenWirkenUndWerdenGespeichert() {
        App app;
        app.starten();   // wie im echten Programm: Datei laden, beobachten, Ebenen, Systemabschnitt
        app.Steuerfenster();
        auto *f = findChild();
        QVERIFY(f);
        if (qEnvironmentVariableIsSet("STEUERFENSTER_BILD")) f->grab().save(qEnvironmentVariable("STEUERFENSTER_BILD")); // zum Ansehen des Aufbaus
        const auto slider = f->findChildren<QSlider *>();
        QCOMPARE(slider.size(), 16);   // 5 oben + Lautstärke, 6 in der Gruppe „Fenster“, 4 im „Räumdienst“
        QCOMPARE(slider[3]->maximum(), 80);   // max. Deckenhöhe bis 80 px
        // jeden Regler einmal ganz durchziehen
        for (QSlider *s : slider)
            for (int v = s->minimum(); v <= s->maximum(); v += std::max(1, (s->maximum() - s->minimum()) / 7))
                s->setValue(v);
        slider[0]->setValue(40);   // Menge 2
        slider[1]->setValue(-30);  // Wind nach links
        slider[2]->setValue(8);    // Größe 4
        slider[3]->setValue(25);   // Deckenhöhe
        slider[4]->setValue(80);   // Fall
        QCOMPARE(app.einstellungen().sim.menge, 2.0);
        QCOMPARE(app.einstellungen().sim.wind, -30.0);
        QCOMPARE(app.einstellungen().sim.groesse, 4.0);
        QCOMPARE(app.einstellungen().sim.maxHoehe, 25.0);
        QCOMPARE(app.einstellungen().sim.fallGeschw, 80.0);
        auto haken = [&](const QString &text) {
            for (QCheckBox *c : f->findChildren<QCheckBox *>())
                if (c->text() == text) return c;
            return static_cast<QCheckBox *>(nullptr);
        };
        QVERIFY(haken("Schnee an") && haken("Bei Vollbild pausieren") && haken("Mit dem System starten"));
        QVERIFY(!haken("Schnee fällt vor Fenstern"));   // ersetzt durch den Fenstermodus
        // Gruppe „Fenster“
        auto *modus = f->findChild<QComboBox *>("fenstermodus");
        QVERIFY(modus);
        QCOMPARE(app.einstellungen().sim.modus, schnee::Fenstermodus::Fest);
        modus->setCurrentIndex(2);
        QCOMPARE(app.einstellungen().sim.modus, schnee::Fenstermodus::Davor);
        modus->setCurrentIndex(0);
        QCOMPARE(app.einstellungen().sim.modus, schnee::Fenstermodus::Fest);
        slider[5]->setValue(70);   // Lautstärke
        QCOMPARE(app.einstellungen().lautstaerke, 70);
        slider[6]->setValue(30);   // Schüttel-Schwelle 30 000 px/s²
        slider[7]->setValue(3);    // Rutsch ab 3 min
        slider[8]->setValue(15);   // Rutschtempo 1,5
        slider[9]->setValue(12);   // Wolkendauer 12 s
        slider[10]->setValue(25);   // 25 % bleiben liegen
        slider[11]->setValue(40);  // Boden max 40 px
        QCOMPARE(app.einstellungen().sim.schuettelnSchwelle, 30000.0);
        QCOMPARE(app.einstellungen().sim.rutschAbMinuten, 3.0);
        QCOMPARE(app.einstellungen().sim.rutschTempo, 1.5);
        QCOMPARE(app.einstellungen().sim.wolkeDauer, 12.0);
        QCOMPARE(app.einstellungen().sim.wolkeLiegen, 0.25);
        QCOMPARE(app.einstellungen().sim.bodenMax, 40.0);
        // Gruppe „Räumdienst“
        slider[12]->setValue(60);  // Räumen ab 60 %
        slider[13]->setValue(15);  // Fahrzeuggröße 1,5
        slider[14]->setValue(2);   // zwei Einsätze zugleich
        slider[15]->setValue(5);   // Treffer bis Streik
        QCOMPARE(app.einstellungen().raeum.raeumenAb, 0.6);
        QCOMPARE(app.einstellungen().raeum.fahrzeugGroesse, 1.5);
        QCOMPARE(app.einstellungen().raeum.gleichzeitig, 2);
        QCOMPARE(app.einstellungen().raeum.trefferBisStreik, 5);
        // Ausdrucksweise der Sprüche: Standard „frech“ (Stufe 3), umstellbar bis „richtig vulgär“
        auto *ausdruck = f->findChild<QComboBox *>("sprueche_max_stufe");
        QVERIFY(ausdruck);
        QCOMPARE(app.einstellungen().spruecheMaxStufe, 3);
        QCOMPARE(ausdruck->currentText(), QString("frech"));
        ausdruck->setCurrentIndex(4);
        QCOMPARE(app.einstellungen().spruecheMaxStufe, 5);
        QCOMPARE(ausdruck->itemText(4), QString("richtig vulgär"));
        for (const char *name : {"raeumdienst", "taskleiste_raeumen", "schnee_anfassbar"}) {
            auto *c = f->findChild<QCheckBox *>(name);
            QVERIFY2(c && c->isChecked(), name);   // Standard: an
            c->click();
        }
        QCOMPARE(app.einstellungen().raeum.an, false);
        QCOMPARE(app.einstellungen().raeum.taskleiste, false);
        QCOMPARE(app.einstellungen().raeum.anfassbar, false);
        // „Jetzt räumen“ ohne Schnee: nichts zu tun, aber auch kein Absturz
        QVERIFY(f->findChild<QPushButton *>("jetzt_raeumen"));
        f->findChild<QPushButton *>("jetzt_raeumen")->click();
        QCOMPARE(app.Raeumen(), QString());
        // Flockendetail: Standard realistisch, umstellbar auf einfach
        auto *detail = f->findChild<QComboBox *>("flockendetail");
        QVERIFY(detail);
        QCOMPARE(app.einstellungen().realistisch, true);
        QCOMPARE(detail->currentText(), QString("Realistisch (Schneekristalle)"));
        detail->setCurrentIndex(detail->findText("Einfach (weiche Punkte)"));
        QCOMPARE(app.einstellungen().realistisch, false);
        // Bildrate: Standard 60, wählbar Auto (so oft wie der Monitor) und Obergrenzen 30/60/120/144
        auto *rate = f->findChild<QComboBox *>("bildrate");
        QVERIFY(rate);
        QCOMPARE(app.einstellungen().bildrate, 60);
        QCOMPARE(rate->currentText(), QString("60 Bilder/s"));
        QCOMPARE(rate->count(), 5);
        rate->setCurrentIndex(0);
        QCOMPARE(app.einstellungen().bildrate, 0);
        rate->setCurrentIndex(rate->findText("30 Bilder/s"));
        QCOMPARE(app.einstellungen().bildrate, 30);
        haken("Bei Vollbild pausieren")->click();
        QCOMPARE(app.einstellungen().vollbildPausieren, false);
        haken("Schnee an")->click();
        QCOMPARE(app.einstellungen().an, false);
        haken("Schnee an")->click();
        // Autostart legt genau eine Datei an und entfernt sie wieder
        const QString auto_ = m_dir.path() + "/autostart/schneefall.desktop";
        haken("Mit dem System starten")->click();
        QVERIFY(QFile::exists(auto_));
        haken("Mit dem System starten")->click();
        QVERIFY(!QFile::exists(auto_));
        // Abschütteln-Knopf
        for (QPushButton *b : f->findChildren<QPushButton *>())
            if (b->text() == "Schnee abschütteln") b->click();
        // gespeichert (verzögert)
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(App::konfigPfad()), 3000);
        QTRY_VERIFY_WITH_TIMEOUT([] {
            QFile d(App::konfigPfad());
            d.open(QIODevice::ReadOnly);
            QByteArray t = d.readAll();
            return t.contains("menge=2\n") && t.contains("wind=-30\n") && t.contains("fenstermodus=fest\n") && !t.contains("vor_fenstern")
                && t.contains("rutsch_ab_minuten=3\n") && t.contains("wolke_liegen=25\n") && t.contains("boden_max=40\n")
                && t.contains("flockendetail=einfach\n") && t.contains("raeumen_ab=60\n") && t.contains("fahrzeug_groesse=1.5\n")
                && t.contains("einsaetze_gleichzeitig=2\n") && t.contains("treffer_bis_streik=5\n") && t.contains("raeumdienst=false\n")
                && t.contains("taskleiste_raeumen=false\n") && t.contains("schnee_anfassbar=false\n") && t.contains("lautstaerke=70\n") && t.contains("sprueche_max_stufe=5\n")
                && t.contains("bildrate=30\n");
        }(), 3000);
    }
    // Flockenfreie Flächen für den Shader: je Monitor zugeschnitten, in Gerätepixeln, Ursprung unten links
    void flockenfreieFlaechenInGeraetepixeln() {
        using schnee::Rechteck;
        float f[8][4];
        // Henrys 5K-Monitor: 3413×1440 logisch bei (1080, 1440), Skalierung 1,5
        const float bx = 1080, by = 1440, bw = 3413, bh = 1440;
        // maximiertes Fenster über der Taskleiste (56 px hoch)
        QCOMPARE(flockenfreiFuer({{1080, 1440, 3413, 1384}}, bx, by, bw, bh, 1.5, f), 1);
        QCOMPARE(f[0][0], 0.f);
        QCOMPARE(f[0][2], 5120.f);          // 3413 × 1,5 = 5119,5 → nach außen gerundet
        QCOMPARE(f[0][1], 84.f);            // unten bleiben 56 × 1,5 Pixel frei
        QCOMPARE(f[0][3], 2160.f);          // bis zur Oberkante des Bildes (1440 × 1,5)
        // Fenster auf einem anderen Monitor zählt nicht, ein überstehendes wird abgeschnitten
        QCOMPARE(flockenfreiFuer({{0, 1100, 1080, 664}}, bx, by, bw, bh, 1.5, f), 0);
        QCOMPARE(flockenfreiFuer({{900, 1300, 400, 300}}, bx, by, bw, bh, 1.0, f), 1);
        QCOMPARE(f[0][0], 0.f);
        QCOMPARE(f[0][2], 220.f);
        QCOMPARE(f[0][3], 1440.f);
        QCOMPARE(f[0][1], 1280.f);
        // mehr als acht: die größten zuerst, der Rest entfällt
        std::vector<Rechteck> viele;
        for (int i = 0; i < 12; ++i) viele.push_back({1100.0 + i * 50, 1500, 40.0 + i * 10, 100});
        QCOMPARE(flockenfreiFuer(viele, bx, by, bw, bh, 1.0, f), 8);
        QCOMPARE(f[0][2] - f[0][0], 150.f);
    }
    void dateiAenderungKommtImFensterAn() {
        App app;
        app.starten();   // wie im echten Programm: Datei laden, beobachten, Ebenen, Systemabschnitt
        app.Steuerfenster();
        QFile d(App::konfigPfad());
        QVERIFY(d.open(QIODevice::WriteOnly | QIODevice::Truncate));
        d.write("menge=0.5\nmax_deckenhoehe=40\nflockendetail=einfach\n");
        d.close();
        app.Fenster("[]"); // irgendein Ereignis; das Nachladen kommt über den Dateibeobachter
        QTRY_COMPARE_WITH_TIMEOUT(app.einstellungen().sim.maxHoehe, 40.0, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(findChild()->findChildren<QSlider *>()[3]->value(), 40, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(findChild()->findChild<QComboBox *>("flockendetail")->currentIndex(), 0, 3000);
    }
    // Henrys alte Datei: vor_fenstern=false wird zu fenstermodus=fest, alle anderen Werte bleiben, neue Schlüssel kommen dazu
    void alteDateiWirdErgaenztOhneWerteZuVerstellen() {
        {
            QFile d(App::konfigPfad());
            QVERIFY(d.open(QIODevice::WriteOnly | QIODevice::Truncate));
            d.write("menge=2\nwind=0\nflockengroesse=4\nmax_deckenhoehe=18\nfallgeschwindigkeit=23\nfps=30\nan=true\n"
                    "vor_fenstern=false\nvollbild_pausieren=true\nflockendetail=realistisch\n");
        }
        App app;
        app.starten();
        QFile d(App::konfigPfad());
        QVERIFY(d.open(QIODevice::ReadOnly));
        const QByteArray t = d.readAll();
        for (const char *z : {"menge=2\n", "wind=0\n", "flockengroesse=4\n", "max_deckenhoehe=18\n", "fallgeschwindigkeit=23\n", "fps=30\n",
                              "an=true\n", "vollbild_pausieren=true\n", "flockendetail=realistisch\n", "fenstermodus=fest\n",
                              "schuetteln_schwelle=15000\n", "rutsch_ab_minuten=10\n", "boden_max=18\n", "ton=false\n", "raeumdienst=true\n",
                              "raeumen_ab=80\n", "fahrzeug_groesse=1\n", "einsaetze_gleichzeitig=1\n", "taskleiste_raeumen=true\n",
                              "treffer_bis_streik=3\n", "schnee_anfassbar=true\n"})
            QVERIFY2(t.contains(z), z);
        QVERIFY(!t.contains("vor_fenstern"));
        QCOMPARE(app.einstellungen().sim.modus, schnee::Fenstermodus::Fest);
    }

private:
    Steuerfenster *findChild() {
        for (QWidget *w : QApplication::topLevelWidgets())
            if (auto *s = dynamic_cast<Steuerfenster *>(w)) return s;
        return nullptr;
    }
    QTemporaryDir m_dir;
};

QTEST_MAIN(TestSteuerfenster)
#include "test_steuerfenster.moc"
