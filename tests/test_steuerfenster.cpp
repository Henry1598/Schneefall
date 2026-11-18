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
        const auto slider = f->findChildren<QSlider *>();
        QCOMPARE(slider.size(), 6); // 5 Regler + Lautstärke
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
        QVERIFY(haken("Schnee an") && haken("Schnee fällt vor Fenstern") && haken("Bei Vollbild pausieren") && haken("Mit dem System starten"));
        haken("Schnee fällt vor Fenstern")->click();
        QCOMPARE(app.einstellungen().sim.vorFenstern, false);
        // Flockendetail: Standard realistisch, umstellbar auf einfach
        auto *detail = f->findChild<QComboBox *>();
        QVERIFY(detail);
        QCOMPARE(app.einstellungen().realistisch, true);
        QCOMPARE(detail->currentText(), QString("Realistisch (Schneekristalle)"));
        detail->setCurrentIndex(detail->findText("Einfach (weiche Punkte)"));
        QCOMPARE(app.einstellungen().realistisch, false);
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
            return t.contains("menge=2\n") && t.contains("wind=-30\n") && t.contains("vor_fenstern=false\n") && t.contains("flockendetail=einfach\n");
        }(), 3000);
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
        QTRY_COMPARE_WITH_TIMEOUT(findChild()->findChild<QComboBox *>()->currentIndex(), 0, 3000);
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
