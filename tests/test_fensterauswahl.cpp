// Windows-Fensterquelle: Filter, Taskleiste, Vollbild, Stapel und DPI-Umrechnung mit nachgebildeten Fenstern.
// Plattformneutral (läuft unter Linux und Windows).
#include <QtTest>
#include "plattform/windows/fensterauswahl.h"

using namespace plattform::win;

namespace {
RohFenster fenster(std::uint64_t hwnd, QRect r, QString klasse = QStringLiteral("Notepad")) {
    RohFenster f;
    f.hwnd = hwnd;
    f.rahmen = r;
    f.klasse = std::move(klasse);
    return f;
}
const schnee::Fenster *finde(const std::vector<schnee::Fenster> &l, std::uint64_t hwnd) {
    const std::string id = "0x" + QString::number(hwnd, 16).toStdString();
    for (const auto &f : l)
        if (f.id == id) return &f;
    return nullptr;
}
// ein Monitor 1920x1080 bei 100 %
std::vector<Monitor> einMonitor() { return {{QRect(0, 0, 1920, 1080), 1.0}}; }
} // namespace

class TestFensterauswahl : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void normalesFensterWirdGemeldet() {
        Lagen lagen;
        const auto l = fensterAuswaehlen({fenster(0x10, QRect(100, 200, 800, 600))}, einMonitor(), 0x10, lagen);
        QCOMPARE(l.size(), size_t(1));
        QCOMPARE(l[0].id, std::string("0x10"));
        QCOMPARE(l[0].r.x, 100.0);
        QCOMPARE(l[0].r.y, 200.0);
        QCOMPARE(l[0].r.w, 800.0);
        QCOMPARE(l[0].r.h, 600.0);
        QVERIFY(l[0].aktiv);
        QVERIFY(!l[0].minimiert && !l[0].vollbild && !l[0].dock);
    }

    void unsichtbareVerhuellteUndWerkzeugfensterFallenWeg() {
        auto unsichtbar = fenster(1, QRect(0, 0, 300, 300));
        unsichtbar.sichtbar = false;
        auto verhuellt = fenster(2, QRect(0, 0, 300, 300));
        verhuellt.verhuellt = true;
        auto werkzeug = fenster(3, QRect(0, 0, 300, 300));
        werkzeug.werkzeug = true;
        auto werkzeugMitApp = fenster(4, QRect(0, 0, 300, 300));
        werkzeugMitApp.werkzeug = true;
        werkzeugMitApp.appfenster = true;
        auto hinweis = fenster(5, QRect(0, 0, 300, 300));
        hinweis.keinAktivieren = true;
        auto ebene = fenster(6, QRect(0, 0, 1920, 1080), QStringLiteral("Qt6QWindowIcon"));
        ebene.eigeneEbene = true;
        Lagen lagen;
        const auto l = fensterAuswaehlen({unsichtbar, verhuellt, werkzeug, werkzeugMitApp, hinweis, ebene}, einMonitor(), 0, lagen);
        QCOMPARE(l.size(), size_t(1));
        QVERIFY(finde(l, 4)); // Werkzeugfenster mit WS_EX_APPWINDOW zählt wie ein normales
    }

    void shellFensterFallenWeg() {
        Lagen lagen;
        std::vector<RohFenster> roh;
        std::uint64_t h = 1;
        for (const char *k : {"Progman", "WorkerW", "Windows.UI.Core.CoreWindow", "NotifyIconOverflowWindow", "#32768", "tooltips_class32",
                              "Xaml_WindowedPopupClass", "TopLevelWindowForOverflowXamlIsland", "XamlExplorerHostIslandWindow"})
            roh.push_back(fenster(h++, QRect(0, 0, 400, 400), QString::fromLatin1(k)));
        QVERIFY(fensterAuswaehlen(roh, einMonitor(), 0, lagen).empty());
        QVERIFY(istShellFenster(QStringLiteral("Progman")));
        QVERIFY(!istShellFenster(QStringLiteral("Notepad")));
    }

    void winzigeUndAusserhalbLiegendeFensterFallenWeg() {
        Lagen lagen;
        const auto l = fensterAuswaehlen({fenster(1, QRect(10, 10, 1, 300)), fenster(2, QRect(10, 10, 300, 0)),
                                          fenster(3, QRect(5000, 5000, 300, 300)), fenster(4, QRect(-400, 10, 300, 300))},
                                         einMonitor(), 0, lagen);
        QVERIFY(l.empty());
    }

    void stapelOberstesHatDenHoechstenWert() {
        Lagen lagen;
        // EnumWindows-Reihenfolge: oberstes zuerst
        const auto l = fensterAuswaehlen({fenster(0xa, QRect(0, 0, 300, 300)), fenster(0xb, QRect(100, 100, 300, 300)),
                                          fenster(0xc, QRect(200, 200, 300, 300))},
                                         einMonitor(), 0xb, lagen);
        QCOMPARE(l.size(), size_t(3));
        QVERIFY(finde(l, 0xa)->stapel > finde(l, 0xb)->stapel);
        QVERIFY(finde(l, 0xb)->stapel > finde(l, 0xc)->stapel);
        QVERIFY(finde(l, 0xb)->aktiv);
        QVERIFY(!finde(l, 0xa)->aktiv);
    }

    void taskleisteIstDock() {
        Lagen lagen;
        const auto l = fensterAuswaehlen({fenster(1, QRect(0, 1032, 1920, 48), QStringLiteral("Shell_TrayWnd")),
                                          fenster(2, QRect(1920, 1032, 1920, 48), QStringLiteral("Shell_SecondaryTrayWnd"))},
                                         {{QRect(0, 0, 1920, 1080), 1.0}, {QRect(1920, 0, 1920, 1080), 1.0}}, 0, lagen);
        QCOMPARE(l.size(), size_t(2));
        QVERIFY(finde(l, 1)->dock && finde(l, 2)->dock);
        QVERIFY(!finde(l, 1)->vollbild);
    }

    void taskleisteZaehltTrotzWerkzeugstil() {
        // Shell_TrayWnd hat WS_EX_TOOLWINDOW (und je nach Version WS_EX_NOACTIVATE)
        auto t = fenster(1, QRect(0, 1032, 1920, 48), QStringLiteral("Shell_TrayWnd"));
        t.werkzeug = true;
        t.keinAktivieren = true;
        Lagen lagen;
        const auto l = fensterAuswaehlen({t}, einMonitor(), 0, lagen);
        QCOMPARE(l.size(), size_t(1));
        QVERIFY(l[0].dock);
    }

    void ausgeblendeteTaskleisteFaelltWeg() {
        // automatisch ausgeblendet: bis auf 2 px unter den Bildschirmrand geschoben
        Lagen lagen;
        const auto l = fensterAuswaehlen({fenster(1, QRect(0, 1078, 1920, 48), QStringLiteral("Shell_TrayWnd"))}, einMonitor(), 0, lagen);
        QVERIFY(l.empty());
    }

    void vollbildWennDasFensterDenMonitorGanzDeckt() {
        Lagen lagen;
        auto voll = fenster(1, QRect(0, 0, 1920, 1080));
        auto maximiert = fenster(2, QRect(0, 0, 1920, 1032)); // Arbeitsfläche ohne Taskleiste
        auto groesser = fenster(3, QRect(-8, -8, 1936, 1096)); // altes Spiel mit Rand außerhalb
        const auto l = fensterAuswaehlen({voll, maximiert, groesser}, einMonitor(), 1, lagen);
        QVERIFY(finde(l, 1)->vollbild);
        QVERIFY(!finde(l, 2)->vollbild);
        QVERIFY(finde(l, 3)->vollbild);
    }

    void vollbildAufDemZweitenMonitor() {
        Lagen lagen;
        const auto l = fensterAuswaehlen({fenster(1, QRect(1920, 0, 2560, 1440))},
                                         {{QRect(0, 0, 1920, 1080), 1.0}, {QRect(1920, 0, 2560, 1440), 1.5}}, 1, lagen);
        QVERIFY(finde(l, 1)->vollbild);
        // logisch: Ursprung bleibt, Größe durch 1,5
        QCOMPARE(finde(l, 1)->r.x, 1920.0);
        QCOMPARE(finde(l, 1)->r.w, 2560.0 / 1.5);
        QCOMPARE(finde(l, 1)->r.h, 1440.0 / 1.5);
    }

    void minimiertBehaeltDieLetzteLage() {
        Lagen lagen;
        auto f = fenster(7, QRect(100, 100, 400, 300));
        fensterAuswaehlen({f}, einMonitor(), 0, lagen);
        f.minimiert = true;
        f.rahmen = QRect(-32000, -32000, 160, 28);
        const auto l = fensterAuswaehlen({f}, einMonitor(), 0, lagen);
        QCOMPARE(l.size(), size_t(1));
        QVERIFY(l[0].minimiert);
        QCOMPARE(l[0].r.x, 100.0);
        QCOMPARE(l[0].r.w, 400.0);
    }

    void minimiertOhneBekannteLageFaelltWeg() {
        Lagen lagen;
        auto f = fenster(7, QRect(-32000, -32000, 160, 28));
        f.minimiert = true;
        QVERIFY(fensterAuswaehlen({f}, einMonitor(), 0, lagen).empty());
    }

    void verschwundeneFensterVergisstDasGedaechtnis() {
        Lagen lagen;
        fensterAuswaehlen({fenster(1, QRect(0, 0, 300, 300)), fenster(2, QRect(0, 0, 300, 300))}, einMonitor(), 0, lagen);
        QCOMPARE(lagen.size(), size_t(2));
        fensterAuswaehlen({fenster(2, QRect(0, 0, 300, 300))}, einMonitor(), 0, lagen);
        QCOMPARE(lagen.size(), size_t(1));
        QVERIFY(lagen.count(2));
    }

    void gemischteSkalierungWieQt() {
        // links 150 % (2880x1620 physisch → 1920x1080 logisch), rechts 100 % ab x=2880 (Qt: Ursprung bleibt)
        const std::vector<Monitor> m{{QRect(0, 0, 2880, 1620), 1.5}, {QRect(2880, 0, 1920, 1080), 1.0}};
        const auto a = physZuLogisch(QRect(300, 150, 1500, 900), m);
        QCOMPARE(a.x, 200.0);
        QCOMPARE(a.y, 100.0);
        QCOMPARE(a.w, 1000.0);
        QCOMPARE(a.h, 600.0);
        const auto b = physZuLogisch(QRect(3000, 100, 800, 600), m);
        QCOMPARE(b.x, 3000.0);
        QCOMPARE(b.w, 800.0);
        // Fenster über beiden Monitoren: zählt der Monitor mit der größten Überdeckung
        QCOMPARE(monitorZu(QRect(2000, 0, 1500, 500), m), 0);
        QCOMPARE(monitorZu(QRect(2700, 0, 1500, 500), m), 1);
        // außerhalb: nächster Monitor
        QCOMPARE(monitorZu(QRect(9000, 0, 100, 100), m), 1);
        QCOMPARE(monitorZu(QRect(0, 0, 10, 10), {}), -1);
    }

    void monitorUrsprungNegativ() {
        // Zweitmonitor links vom Hauptmonitor, 125 %
        const std::vector<Monitor> m{{QRect(0, 0, 1920, 1080), 1.0}, {QRect(-2400, 0, 2400, 1350), 1.25}};
        const auto r = physZuLogisch(QRect(-2400 + 250, 125, 500, 250), m);
        QCOMPARE(r.x, -2400.0 + 200.0);
        QCOMPARE(r.y, 100.0);
        QCOMPARE(r.w, 400.0);
        QCOMPARE(r.h, 200.0);
    }

    void listenVergleich() {
        Lagen lagen;
        const auto a = fensterAuswaehlen({fenster(1, QRect(0, 0, 300, 300))}, einMonitor(), 0, lagen);
        auto b = a;
        QVERIFY(gleich(a, b));
        b[0].r.x += 1;
        QVERIFY(!gleich(a, b));
        b = a;
        b[0].aktiv = true;
        QVERIFY(!gleich(a, b));
        QVERIFY(!gleich(a, {}));
    }

    void hoechstens1000Fenster() {
        Lagen lagen;
        std::vector<RohFenster> roh;
        for (std::uint64_t i = 1; i <= 1500; ++i) roh.push_back(fenster(i, QRect(int(i % 100), 10, 300, 300)));
        QCOMPARE(fensterAuswaehlen(roh, einMonitor(), 0, lagen).size(), size_t(1000));
    }
};

QTEST_GUILESS_MAIN(TestFensterauswahl)
#include "test_fensterauswahl.moc"
