// Linux-Ebene: Wayland-Layer-Shell (LayerShellQt) auf der Overlay-Schicht, an allen Rändern verankert,
// ohne Tastatur, ohne Platzbedarf. Takt: Frame-Callbacks des Compositors (requestUpdate nach jedem Bild).
#include "plattform/ebene.h"
#include <LayerShellQt/Window>
#include <QScreen>
#include <QWindow>

namespace plattform {

namespace {
class LayerEbene : public Ebene {
public:
    void einrichten(QWindow &fenster, QScreen *bildschirm) override {
        fenster.setFlags(Qt::FramelessWindowHint | Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus);
        fenster.setScreen(bildschirm);
        fenster.setTitle(QStringLiteral("Schneefall-Ebene"));
        auto *ls = LayerShellQt::Window::get(&fenster);
        ls->setLayer(LayerShellQt::Window::LayerOverlay);
        ls->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                                                      | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
        ls->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        ls->setExclusiveZone(-1);
        ls->setScope(QStringLiteral("schneefall"));
        ls->setWantsToBeOnActiveScreen(false);
        ls->setScreen(bildschirm);
        ls->setActivateOnShow(false);
    }
    void anBildschirmAnpassen(QWindow &fenster, QScreen *bildschirm) override { fenster.resize(bildschirm->geometry().size()); }
    // Reihenfolge wichtig: erst die Maske setzen, dann die Durchlässigkeit aufheben – nie ist die ganze Fläche „dicht“.
    void eingabeSetzen(QWindow &fenster, const QRegion &lokal) override {
        if (lokal.isEmpty()) {
            fenster.setFlag(Qt::WindowTransparentForInput, true);
        } else {
            fenster.setMask(lokal);
            fenster.setFlag(Qt::WindowTransparentForInput, false);
        }
    }
    void zeigen(QWindow &fenster, bool sichtbar) override {
        if (sichtbar) fenster.show();
        else fenster.hide();
    }
    void bildAnfordern(QWindow &fenster) override { fenster.requestUpdate(); }
};
} // namespace

std::unique_ptr<Ebene> ebeneErzeugen() { return std::make_unique<LayerEbene>(); }

} // namespace plattform
