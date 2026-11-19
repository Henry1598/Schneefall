// Windows-Fensterquelle: EnumWindows + SetWinEventHook.
// Rohdaten je oberstem Fenster (Rahmen über DWMWA_EXTENDED_FRAME_BOUNDS, verhüllt über DWMWA_CLOAKED, Stile, minimiert),
// Monitore über EnumDisplayMonitors + GetDpiForMonitor; Auswahl, Taskleiste, Vollbild und DPI-Umrechnung in
// fensterauswahl.cpp. Gemeldet wird nur bei Änderung, gebündelt nach höchstens 15 ms (wie das KWin-Skript),
// dazu jede Sekunde ein Abgleich, falls ein Ereignis fehlt.
#include "plattform/fensterquelle.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include "plattform/windows/fensterauswahl.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <shellscalingapi.h>

#ifndef EVENT_OBJECT_CLOAKED
#define EVENT_OBJECT_CLOAKED 0x8017
#define EVENT_OBJECT_UNCLOAKED 0x8018
#endif

namespace plattform {

namespace {
using win::Monitor;
using win::RohFenster;

constexpr int BUENDELN_MS = 15;  // Ereignisse sammeln, höchstens so lange bis zur Meldung
constexpr int ABGLEICH_MS = 1000; // Sicherheitsabgleich ohne Ereignis

QString klasseVon(HWND h) {
    wchar_t puffer[128];
    const int n = GetClassNameW(h, puffer, int(std::size(puffer)));
    return n > 0 ? QString::fromWCharArray(puffer, n) : QString();
}

QRect rahmenVon(HWND h) {
    RECT r{};
    if (FAILED(DwmGetWindowAttribute(h, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof r)) && !GetWindowRect(h, &r)) return {};
    return QRect(QPoint(r.left, r.top), QPoint(r.right - 1, r.bottom - 1));
}

BOOL CALLBACK monitorSammeln(HMONITOR m, HDC, LPRECT, LPARAM daten) {
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfoW(m, &mi)) return TRUE;
    UINT dx = 96, dy = 96;
    if (FAILED(GetDpiForMonitor(m, MDT_EFFECTIVE_DPI, &dx, &dy))) dx = 96;
    const RECT &r = mi.rcMonitor;
    reinterpret_cast<std::vector<Monitor> *>(daten)->push_back({QRect(QPoint(r.left, r.top), QPoint(r.right - 1, r.bottom - 1)), dx / 96.0});
    return TRUE;
}

class WindowsFensterquelle;
WindowsFensterquelle *g_quelle = nullptr; // WinEvent-Rückrufe haben keinen Kontextzeiger

class WindowsFensterquelle : public Fensterquelle {
public:
    WindowsFensterquelle() {
        m_buendeln.setSingleShot(true);
        m_buendeln.setTimerType(Qt::PreciseTimer);
        QObject::connect(&m_buendeln, &QTimer::timeout, [this] { abfragen(); });
        QObject::connect(&m_abgleich, &QTimer::timeout, [this] { abfragen(); });
    }
    ~WindowsFensterquelle() override {
        for (HWINEVENTHOOK h : m_haken)
            if (h) UnhookWinEvent(h);
        if (g_quelle == this) g_quelle = nullptr;
    }

    void starten(Empfaenger empfaenger) override {
        m_empfaenger = std::move(empfaenger);
        g_quelle = this;
        // außerhalb des Prozesses, Rückruf im GUI-Thread über dessen Nachrichtenschleife; eigener Prozess zählt mit
        // (das Steuerfenster trägt Schnee), die Ebenen filtert die Auswahl
        const DWORD art = WINEVENT_OUTOFCONTEXT;
        m_haken.push_back(SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, ereignis, 0, 0, art));
        m_haken.push_back(SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MINIMIZEEND, nullptr, ereignis, 0, 0, art));
        m_haken.push_back(SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_REORDER, nullptr, ereignis, 0, 0, art));
        m_haken.push_back(SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, ereignis, 0, 0, art));
        m_haken.push_back(SetWinEventHook(EVENT_OBJECT_CLOAKED, EVENT_OBJECT_UNCLOAKED, nullptr, ereignis, 0, 0, art));
        m_abgleich.start(ABGLEICH_MS);
        abfragen();
    }

    QString diagnose() const override { return m_diagnose; }

private:
    static void CALLBACK ereignis(HWINEVENTHOOK, DWORD ereignis, HWND h, LONG objekt, LONG kind, DWORD, DWORD) {
        if (!g_quelle) return;
        // nur ganze Fenster (nicht Mauszeiger, Textmarke, Bedienelemente)
        if (ereignis != EVENT_SYSTEM_FOREGROUND && (objekt != OBJID_WINDOW || kind != CHILDID_SELF)) return;
        if (h && GetAncestor(h, GA_ROOT) != h) return;
        g_quelle->vormerken();
    }

    void vormerken() {
        if (!m_buendeln.isActive()) m_buendeln.start(BUENDELN_MS);
    }

    static BOOL CALLBACK fensterSammeln(HWND h, LPARAM daten) {
        auto *self = reinterpret_cast<WindowsFensterquelle *>(daten);
        RohFenster f;
        f.hwnd = std::uint64_t(reinterpret_cast<quintptr>(h));
        f.sichtbar = IsWindowVisible(h);
        if (!f.sichtbar) {
            self->m_roh.push_back(std::move(f)); // zählt für die Stapelzählung nicht, wird gefiltert
            return TRUE;
        }
        f.klasse = klasseVon(h);
        DWORD verhuellt = 0;
        f.verhuellt = SUCCEEDED(DwmGetWindowAttribute(h, DWMWA_CLOAKED, &verhuellt, sizeof verhuellt)) && verhuellt != 0;
        const LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
        f.werkzeug = ex & WS_EX_TOOLWINDOW;
        f.appfenster = ex & WS_EX_APPWINDOW;
        f.keinAktivieren = ex & WS_EX_NOACTIVATE;
        f.minimiert = IsIconic(h);
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        // eigene Ebenen: durchklickbar (WS_EX_TRANSPARENT/LAYERED) im eigenen Prozess; das Steuerfenster bleibt
        f.eigeneEbene = pid == GetCurrentProcessId() && (ex & WS_EX_LAYERED) && (ex & WS_EX_NOACTIVATE);
        if (!f.minimiert) f.rahmen = rahmenVon(h);
        self->m_roh.push_back(std::move(f));
        return TRUE;
    }

    void abfragen() {
        if (!m_empfaenger) return;
        m_roh.clear();
        EnumWindows(fensterSammeln, reinterpret_cast<LPARAM>(this));
        std::vector<Monitor> monitore;
        EnumDisplayMonitors(nullptr, nullptr, monitorSammeln, reinterpret_cast<LPARAM>(&monitore));
        const HWND vorn = GetForegroundWindow();
        std::vector<schnee::Fenster> liste =
            win::fensterAuswaehlen(m_roh, monitore, std::uint64_t(reinterpret_cast<quintptr>(vorn)), m_lagen);
        if (m_gemeldet && win::gleich(liste, m_letzte)) return;
        m_gemeldet = true;
        m_letzte = liste;
        m_diagnose = diagnoseText(liste, monitore);
        m_empfaenger(std::move(liste));
    }

    static QString diagnoseText(const std::vector<schnee::Fenster> &f, const std::vector<Monitor> &monitore) {
        QJsonArray a;
        for (const schnee::Fenster &w : f) {
            QJsonObject o;
            o.insert(QStringLiteral("id"), QString::fromStdString(w.id));
            o.insert(QStringLiteral("x"), w.r.x);
            o.insert(QStringLiteral("y"), w.r.y);
            o.insert(QStringLiteral("w"), w.r.w);
            o.insert(QStringLiteral("h"), w.r.h);
            o.insert(QStringLiteral("s"), w.stapel);
            o.insert(QStringLiteral("min"), w.minimiert);
            o.insert(QStringLiteral("fs"), w.vollbild);
            o.insert(QStringLiteral("akt"), w.aktiv);
            o.insert(QStringLiteral("dock"), w.dock);
            const HWND h = reinterpret_cast<HWND>(quintptr(std::stoull(w.id.substr(2), nullptr, 16)));
            o.insert(QStringLiteral("klasse"), klasseVon(h));
            a.append(o);
        }
        QJsonArray m;
        for (const Monitor &x : monitore)
            m.append(QJsonObject{{QStringLiteral("x"), x.phys.x()}, {QStringLiteral("y"), x.phys.y()}, {QStringLiteral("w"), x.phys.width()},
                                 {QStringLiteral("h"), x.phys.height()}, {QStringLiteral("skala"), x.skala}});
        return QString::fromUtf8(QJsonDocument(QJsonObject{{QStringLiteral("monitore"), m}, {QStringLiteral("fenster"), a}})
                                     .toJson(QJsonDocument::Compact));
    }

    Empfaenger m_empfaenger;
    std::vector<HWINEVENTHOOK> m_haken;
    QTimer m_buendeln, m_abgleich;
    std::vector<RohFenster> m_roh;
    win::Lagen m_lagen;
    std::vector<schnee::Fenster> m_letzte;
    bool m_gemeldet = false;
    QString m_diagnose;
};
} // namespace

std::unique_ptr<Fensterquelle> fensterquelleErzeugen() { return std::make_unique<WindowsFensterquelle>(); }

} // namespace plattform
