#include "plattform/windows/fensterauswahl.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace plattform::win {

namespace {
constexpr std::size_t MAX_FENSTER = 1000; // wie bei der KWin-Liste
constexpr int MIN_SEITE = 2;              // physische Pixel: schmalere Fenster tragen keinen Schnee
constexpr int MIN_TASKLEISTE = 4;         // sichtbarer Rest einer ausgeblendeten Taskleiste ist kein dock

qint64 flaeche(const QRect &r) { return r.isEmpty() ? 0 : qint64(r.width()) * r.height(); }

// Abstand zweier Rechtecke (0, wenn sie sich berühren/überdecken)
qint64 abstand2(const QRect &a, const QRect &b) {
    const qint64 dx = std::max<qint64>({0, qint64(b.left()) - a.right(), qint64(a.left()) - b.right()});
    const qint64 dy = std::max<qint64>({0, qint64(b.top()) - a.bottom(), qint64(a.top()) - b.bottom()});
    return dx * dx + dy * dy;
}

bool aufEinemMonitor(const QRect &r, const std::vector<Monitor> &monitore) {
    return std::any_of(monitore.begin(), monitore.end(), [&](const Monitor &m) { return m.phys.intersects(r); });
}
} // namespace

int monitorZu(const QRect &phys, const std::vector<Monitor> &monitore) {
    int best = -1;
    qint64 bestFlaeche = 0;
    for (int i = 0; i < int(monitore.size()); ++i) {
        const qint64 f = flaeche(monitore[size_t(i)].phys.intersected(phys));
        if (f > bestFlaeche) {
            bestFlaeche = f;
            best = i;
        }
    }
    if (best >= 0) return best;
    qint64 bestAbstand = std::numeric_limits<qint64>::max();
    for (int i = 0; i < int(monitore.size()); ++i) {
        const qint64 d = abstand2(monitore[size_t(i)].phys, phys);
        if (d < bestAbstand) {
            bestAbstand = d;
            best = i;
        }
    }
    return best;
}

schnee::Rechteck physZuLogisch(const QRect &phys, const std::vector<Monitor> &monitore) {
    const int i = monitorZu(phys, monitore);
    if (i < 0) return {double(phys.x()), double(phys.y()), double(phys.width()), double(phys.height())};
    const Monitor &m = monitore[size_t(i)];
    const double s = m.skala > 0 ? m.skala : 1.0;
    const double ox = m.phys.x(), oy = m.phys.y();
    return {ox + (phys.x() - ox) / s, oy + (phys.y() - oy) / s, phys.width() / s, phys.height() / s};
}

bool istTaskleiste(const QString &klasse) {
    return klasse == QLatin1String("Shell_TrayWnd") || klasse == QLatin1String("Shell_SecondaryTrayWnd");
}

bool istShellFenster(const QString &klasse) {
    static const char *const SHELL[] = {
        "Progman", "WorkerW",                                  // Desktop
        "Windows.UI.Core.CoreWindow",                          // Startmenü, Suche, Info-Center, Benachrichtigungen
        "Shell_InputSwitchTopLevelWindow", "EdgeUiInputTopWndClass", "EdgeUiInputWndClass",
        "NotifyIconOverflowWindow", "TopLevelWindowForOverflowXamlIsland", // Überlauf des Infobereichs
        "Xaml_WindowedPopupClass", "XamlExplorerHostIslandWindow", "Windows.Internal.Shell.TabProxyWindow",
        "ForegroundStaging", "MultitaskingViewFrame", "TaskSwitcherWnd", "TaskSwitcherOverlayWnd",
        "Shell_LightDismissOverlay", "ApplicationManager_DesktopShellWindow", "ApplicationManager_ImmersiveShellWindow",
        "#32768",                                              // Menüs
        "tooltips_class32", "SysShadow", "IME", "MSCTFIME UI", "Internet Explorer_Hidden",
    };
    return std::any_of(std::begin(SHELL), std::end(SHELL), [&](const char *k) { return klasse == QLatin1String(k); });
}

std::vector<schnee::Fenster> fensterAuswaehlen(const std::vector<RohFenster> &roh, const std::vector<Monitor> &monitore,
                                               std::uint64_t vordergrund, Lagen &lagen) {
    std::vector<schnee::Fenster> liste;
    Lagen neu;
    const int n = int(std::min(roh.size(), std::size_t(100000)));
    for (int i = 0; i < n && liste.size() < MAX_FENSTER; ++i) {
        const RohFenster &w = roh[size_t(i)];
        if (!w.sichtbar || w.verhuellt || w.eigeneEbene) continue;
        const bool dock = istTaskleiste(w.klasse); // die Taskleiste ist selbst ein Werkzeugfenster
        if (!dock && w.werkzeug && !w.appfenster) continue;
        if (!dock && w.keinAktivieren && !w.appfenster) continue;
        if (istShellFenster(w.klasse)) continue;

        schnee::Fenster f;
        f.id = "0x" + QString::number(w.hwnd, 16).toStdString();
        f.stapel = n - i; // oberstes zuerst → höchster Wert
        f.aktiv = w.hwnd == vordergrund;
        f.dock = dock;
        if (w.minimiert) {
            // minimiert liegt das Fenster bei -32000: die letzte sichtbare Lage weitermelden (Rutsch/Wolke an der richtigen Stelle)
            const auto alt = lagen.find(w.hwnd);
            if (alt == lagen.end() || dock) continue;
            f.minimiert = true;
            f.r = alt->second;
            neu[w.hwnd] = f.r;
            liste.push_back(std::move(f));
            continue;
        }
        if (w.rahmen.width() < MIN_SEITE || w.rahmen.height() < MIN_SEITE) continue;
        if (!aufEinemMonitor(w.rahmen, monitore)) continue;
        const int mi = monitorZu(w.rahmen, monitore);
        const QRect mon = monitore[size_t(mi)].phys;
        QRect rahmen = w.rahmen;
        if (dock) {
            // Taskleiste: nur der sichtbare Teil; automatisch ausgeblendet ragt nur ein Streifen herein
            rahmen = rahmen.intersected(mon);
            if (rahmen.width() < MIN_TASKLEISTE || rahmen.height() < MIN_TASKLEISTE) continue;
        } else {
            f.vollbild = rahmen.contains(mon);
        }
        f.r = physZuLogisch(rahmen, monitore);
        neu[w.hwnd] = f.r;
        liste.push_back(std::move(f));
    }
    lagen = std::move(neu);
    return liste;
}

bool gleich(const std::vector<schnee::Fenster> &a, const std::vector<schnee::Fenster> &b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const schnee::Fenster &x = a[i], &y = b[i];
        if (x.id != y.id || x.stapel != y.stapel || x.minimiert != y.minimiert || x.vollbild != y.vollbild || x.aktiv != y.aktiv
            || x.dock != y.dock || x.r.x != y.r.x || x.r.y != y.r.y || x.r.w != y.r.w || x.r.h != y.r.h)
            return false;
    }
    return true;
}

} // namespace plattform::win
