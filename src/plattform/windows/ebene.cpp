// Windows-Ebene: je Monitor ein randloses OpenGL-Fenster mit
//   WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW
// (über allen Fenstern, durchklickbar, nie aktiv, nicht in Taskleiste/Alt-Tab). Durchsichtig: Alpha-Kanal des
// GL-Puffers über DWM (Layered mit LWA_ALPHA 255 plus DwmEnableBlurBehindWindow mit leerer Region – im Spike in der
// VM mit Mesa/llvmpipe geprüft; so machen es auch GLFW/andere Overlays auf echten GPUs).
//
// Eingabeflächen: Ein Layered-Fenster kann nur ganz oder gar nicht klickbar sein. Die Mausposition wird daher mit
// 60 Hz geprüft (und bei jeder Mausbewegung über der Ebene): nur über der Eingabefläche wird WS_EX_TRANSPARENT
// entfernt, sonst gleich wieder gesetzt.
//
// Takt: je Monitor ein Thread wartet auf das Monitorbild (IDXGIOutput::WaitForVBlank, sonst DwmFlush, sonst Schlaf
// nach der Bildrate) und löst dann die angeforderten Bilder aus. Swap-Intervall bleibt 0 (siehe overlay.cpp).
#include "plattform/ebene.h"
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QWindow>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <memory>
#include <thread>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <dxgi.h>

namespace plattform {

namespace {

constexpr LONG_PTR EBENE_STIL = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
constexpr int MAUS_MS = 16;    // Mausprüfung (60 Hz) solange es Eingabeflächen gibt
constexpr int OBEN_MS = 250;   // Stapelplatz prüfen: bleibt über anderen Topmost-Fenstern

HWND hwndVon(QWindow &w) { return reinterpret_cast<HWND>(w.winId()); }

// alle Ebenen-Fenster (für „liegt etwas Fremdes über uns?“)
std::set<HWND> &ebenenFenster() {
    static std::set<HWND> s;
    return s;
}

// ---- Monitortakt ----
// Ein Thread je Monitor. Er wartet nur, solange ein Bild angefordert ist (in Pause schläft er ganz).
class Monitortakt {
public:
    explicit Monitortakt(HMONITOR monitor, double hz) : m_hz(hz > 1 ? hz : 60.0) {
        m_ausgabe = ausgabeSuchen(monitor);
        m_thread = std::thread([this] { laufen(); });
    }
    ~Monitortakt() {
        stoppen();
        if (m_ausgabe) m_ausgabe->Release();
    }
    // Thread beenden (GUI-Thread; vor dem Ende der Anwendung, weil er an qApp postet). Das Objekt bleibt gültig,
    // bis die letzte Ebene es freigibt – anfordern()/vergessen() sind danach wirkungslos.
    void stoppen() {
        {
            std::lock_guard l(m_mutex);
            m_ende = true;
        }
        m_cv.notify_all();
        if (m_thread.joinable()) m_thread.join();
    }
    // GUI-Thread: Fenster soll zeichnen, teiler Monitorbilder nach seinem letzten Bild (mindestens beim nächsten)
    void anfordern(QWindow *w, int teiler) {
        {
            std::lock_guard l(m_mutex);
            const std::uint64_t letzte = m_letzte.count(w) ? m_letzte[w] : 0;
            m_wartend[w] = std::max(m_zaehler + 1, letzte + std::uint64_t(std::max(1, teiler)));
        }
        m_cv.notify_all();
    }
    void vergessen(QWindow *w) {
        std::lock_guard l(m_mutex);
        m_wartend.erase(w);
        m_letzte.erase(w);
    }
    void setzeHz(double hz) { m_hz = hz > 1 ? hz : 60.0; }
    const char *art() const { return m_art.load(); }

private:
    static IDXGIOutput *ausgabeSuchen(HMONITOR monitor) {
        IDXGIFactory1 *fabrik = nullptr;
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&fabrik)))) return nullptr;
        IDXGIOutput *gefunden = nullptr;
        IDXGIAdapter1 *adapter = nullptr;
        for (UINT a = 0; !gefunden && fabrik->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; ++a) {
            IDXGIOutput *ausgabe = nullptr;
            for (UINT o = 0; !gefunden && adapter->EnumOutputs(o, &ausgabe) != DXGI_ERROR_NOT_FOUND; ++o) {
                DXGI_OUTPUT_DESC d{};
                if (SUCCEEDED(ausgabe->GetDesc(&d)) && d.Monitor == monitor) gefunden = ausgabe;
                else ausgabe->Release();
            }
            adapter->Release();
        }
        fabrik->Release();
        return gefunden;
    }

    void laufen() {
        LARGE_INTEGER frequenz, start;
        QueryPerformanceFrequency(&frequenz);
        bool vblankGeht = m_ausgabe != nullptr, dwmGeht = true;
        for (;;) {
            {
                std::unique_lock l(m_mutex);
                m_cv.wait(l, [this] { return m_ende || !m_wartend.empty(); });
                if (m_ende) return;
            }
            // auf das nächste Monitorbild warten
            QueryPerformanceCounter(&start);
            bool gewartet = false;
            if (vblankGeht) {
                gewartet = SUCCEEDED(m_ausgabe->WaitForVBlank());
                vblankGeht = gewartet;
                if (gewartet) m_art = "vblank";
            }
            if (!gewartet && dwmGeht) {
                gewartet = SUCCEEDED(DwmFlush());
                LARGE_INTEGER jetzt;
                QueryPerformanceCounter(&jetzt);
                // DwmFlush kehrt ohne ausstehende Zusammensetzung sofort zurück: dann taugt es nicht als Takt
                if (gewartet && double(jetzt.QuadPart - start.QuadPart) / double(frequenz.QuadPart) < 0.25 / m_hz) gewartet = false;
                if (gewartet) m_art = "dwm";
            }
            if (!gewartet) {
                m_art = "schlaf";
                Sleep(DWORD(std::max(1.0, 1000.0 / m_hz)));
            }
            std::vector<QWindow *> faellig;
            {
                std::lock_guard l(m_mutex);
                ++m_zaehler; // Monitorbilder seit dem Start (nur während angefordert wird)
                for (auto it = m_wartend.begin(); it != m_wartend.end();) {
                    if (it->second <= m_zaehler) {
                        faellig.push_back(it->first);
                        m_letzte[it->first] = m_zaehler;
                        it = m_wartend.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            for (QWindow *w : faellig) {
                QPointer<QWindow> p(w);
                // im GUI-Thread: Bild sofort zeichnen (wie Qts requestUpdate, nur im Monitortakt)
                QMetaObject::invokeMethod(qApp, [p] {
                    if (p) QCoreApplication::postEvent(p.data(), new QEvent(QEvent::UpdateRequest), Qt::HighEventPriority);
                }, Qt::QueuedConnection);
            }
        }
    }

    std::atomic<double> m_hz;
    std::atomic<const char *> m_art{"-"};
    IDXGIOutput *m_ausgabe = nullptr;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::map<QWindow *, std::uint64_t> m_wartend; // Fenster → Monitorbild, mit dem es zeichnen soll
    std::map<QWindow *, std::uint64_t> m_letzte;  // Fenster → Monitorbild seines letzten Bildes
    std::uint64_t m_zaehler = 0;
    bool m_ende = false;
    std::thread m_thread;
};

// geteilt: die Ebenen halten ihren Takt selbst, damit er beim Aufräumen nach aboutToQuit noch gültig ist
std::map<HMONITOR, std::shared_ptr<Monitortakt>> &takte() {
    static std::map<HMONITOR, std::shared_ptr<Monitortakt>> t;
    return t;
}

std::shared_ptr<Monitortakt> taktFuer(QWindow &fenster) {
    HMONITOR m = MonitorFromWindow(hwndVon(fenster), MONITOR_DEFAULTTONEAREST);
    static const bool aufraeumen = [] {
        // Takt-Threads vor dem Ende der Anwendung anhalten (sie posten an qApp)
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, [] {
            for (auto &[m, t] : takte()) t->stoppen();
            takte().clear();
        });
        return true;
    }();
    Q_UNUSED(aufraeumen)
    auto &t = takte()[m];
    const double hz = fenster.screen() ? fenster.screen()->refreshRate() : 60.0;
    if (!t) t = std::make_shared<Monitortakt>(m, hz);
    else t->setzeHz(hz);
    return t;
}

// ---- Ebene ----
class WindowsEbene : public Ebene, public QAbstractNativeEventFilter {
public:
    ~WindowsEbene() override {
        if (m_hwnd) ebenenFenster().erase(m_hwnd);
        if (m_fenster && m_takt) m_takt->vergessen(m_fenster);
        if (qApp) qApp->removeNativeEventFilter(this);
    }

    void einrichten(QWindow &fenster, QScreen *bildschirm) override {
        m_fenster = &fenster;
        // Qt::Tool → WS_EX_TOOLWINDOW (nicht in Taskleiste/Alt-Tab), StaysOnTop → WS_EX_TOPMOST,
        // DoesNotAcceptFocus → WS_EX_NOACTIVATE. Durchklicken setzen wir selbst (s. stilSetzen), nicht über
        // Qt::WindowTransparentForInput: jedes setFlags würde das Fenster neu aufbauen.
        fenster.setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus | Qt::WindowStaysOnTopHint | Qt::Tool
                         | Qt::NoDropShadowWindowHint);
        fenster.setScreen(bildschirm);
        fenster.setTitle(QStringLiteral("Schneefall-Ebene"));
        m_hwnd = hwndVon(fenster); // legt das native Fenster an
        ebenenFenster().insert(m_hwnd);
        stilSetzen();
        qApp->installNativeEventFilter(this);

        m_maus.setTimerType(Qt::PreciseTimer);
        QObject::connect(&m_maus, &QTimer::timeout, [this] { mausPruefen(); });
        QObject::connect(&m_oben, &QTimer::timeout, [this] { obenHalten(); });
        m_oben.start(OBEN_MS);
    }

    void anBildschirmAnpassen(QWindow &fenster, QScreen *bildschirm) override {
        fenster.setScreen(bildschirm);
        fenster.setGeometry(bildschirm->geometry());
        if (m_takt) m_takt->vergessen(&fenster);
        m_takt = nullptr; // Monitor kann gewechselt haben
    }

    void eingabeSetzen(QWindow &, const QRegion &lokal) override {
        m_region = lokal;
        if (lokal.isEmpty()) {
            m_maus.stop();
            durchlaessig(true); // sofort: nirgends mehr Klicks annehmen
        } else {
            if (!m_maus.isActive()) m_maus.start(MAUS_MS);
            mausPruefen();
        }
    }

    void zeigen(QWindow &fenster, bool sichtbar) override {
        if (sichtbar) {
            fenster.show();
            stilSetzen();
            obenHalten(true);
        } else {
            fenster.hide();
        }
    }

    void bildAnfordern(QWindow &fenster) override {
        if (!m_takt) m_takt = taktFuer(fenster);
        m_takt->anfordern(&fenster, m_teiler);
    }
    bool teiltSelbst() const override { return true; }
    void setzeTeiler(int n) override { m_teiler = std::max(1, n); }

    QString diagnose() const override {
        return QStringLiteral("takt=%1 klickbar=%2").arg(QLatin1String(m_takt ? m_takt->art() : "-")).arg(m_klickbar);
    }

    // Mausbewegung über der (gerade klickbaren) Ebene: sofort prüfen – verlässt die Maus die Eingabefläche,
    // wird die Ebene durchlässig, bevor ein Klick ankommt
    bool nativeEventFilter(const QByteArray &, void *nachricht, qintptr *) override {
        const MSG *m = static_cast<const MSG *>(nachricht);
        if (m->hwnd == m_hwnd && m_hwnd && (m->message == WM_MOUSEMOVE || m->message == WM_NCMOUSEMOVE)) mausPruefen();
        return false;
    }

private:
    void stilSetzen() {
        if (!m_hwnd) return;
        LONG_PTR ex = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
        ex |= EBENE_STIL;
        if (!m_klickbar) ex |= WS_EX_TRANSPARENT;
        else ex &= ~LONG_PTR(WS_EX_TRANSPARENT);
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, ex);
        SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);
        // Alpha des GL-Puffers an DWM durchreichen (leere Unschärfe-Region = keine Unschärfe, nur Transparenz)
        DWM_BLURBEHIND bb{};
        bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.fEnable = TRUE;
        bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
        DwmEnableBlurBehindWindow(m_hwnd, &bb);
        if (bb.hRgnBlur) DeleteObject(bb.hRgnBlur);
        // keine Ein-/Ausblend-Animation (sonst blitzt es bei Pause/Vollbild), nicht in „Desktop anzeigen“-Vorschau
        const BOOL an = TRUE;
        DwmSetWindowAttribute(m_hwnd, DWMWA_TRANSITIONS_FORCEDISABLED, &an, sizeof an);
        DwmSetWindowAttribute(m_hwnd, DWMWA_EXCLUDED_FROM_PEEK, &an, sizeof an);
    }

    void durchlaessig(bool ja) {
        if (!m_hwnd || m_klickbar == !ja) return;
        m_klickbar = !ja;
        LONG_PTR ex = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
        if (ja) ex |= WS_EX_TRANSPARENT;
        else ex &= ~LONG_PTR(WS_EX_TRANSPARENT);
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, ex);
    }

    void mausPruefen() {
        if (!m_fenster || m_region.isEmpty()) {
            durchlaessig(true);
            return;
        }
        // QCursor::pos() ist global logisch wie die Ebene (Qt rechnet je Monitor um)
        const QPoint lokal = m_fenster->mapFromGlobal(QCursor::pos());
        durchlaessig(!m_region.contains(lokal));
    }

    // liegt ein fremdes sichtbares Fenster über der Ebene (z. B. die Taskleiste nach einem Klick), wieder nach oben
    void obenHalten(bool immer = false) {
        if (!m_hwnd || !m_fenster || !m_fenster->isVisible()) return;
        bool nachOben = immer;
        for (HWND h = GetWindow(m_hwnd, GW_HWNDPREV); h && !nachOben; h = GetWindow(h, GW_HWNDPREV))
            if (IsWindowVisible(h) && !ebenenFenster().count(h)) nachOben = true;
        if (nachOben)
            SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING);
    }

    QWindow *m_fenster = nullptr;
    HWND m_hwnd = nullptr;
    std::shared_ptr<Monitortakt> m_takt;
    QRegion m_region;
    int m_teiler = 1;
    bool m_klickbar = false;
    QTimer m_maus, m_oben;
};
} // namespace

std::unique_ptr<Ebene> ebeneErzeugen() { return std::make_unique<WindowsEbene>(); }

} // namespace plattform
