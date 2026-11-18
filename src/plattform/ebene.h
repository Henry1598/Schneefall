// Ebene: Plattform-Teil einer Overlay-Ebene (ein durchsichtiges Fenster je Monitor).
//
// Die Overlay-Klasse (overlay.cpp) zeichnet plattformneutral mit OpenGL in ein QOpenGLWindow. Was das Fenster
// zum Overlay macht – Fensterart, über allen Fenstern, nie Fokus, Durchklicken, Eingabeflächen, Monitortakt –
// steckt hinter dieser Schnittstelle (Linux: LayerShellQt, Windows: WS_EX_LAYERED|TRANSPARENT|TOPMOST …).
// Alle Aufrufe kommen aus dem GUI-Thread.
#pragma once
#include <QRegion>
#include <memory>

class QScreen;
class QWindow;

namespace plattform {

class Ebene {
public:
    virtual ~Ebene() = default;
    // einmal, bevor das Fenster erzeugt/gezeigt wird: Fensterart, Monitor, Titel, Stapelplatz, Durchklicken an
    virtual void einrichten(QWindow &fenster, QScreen *bildschirm) = 0;
    // Größe/Lage an den Monitor anpassen (beim Einrichten und wenn sich der Monitor ändert)
    virtual void anBildschirmAnpassen(QWindow &fenster, QScreen *bildschirm) = 0;
    // Eingabefläche (lokale logische Koordinaten); leer = alle Klicks gehen durch. Wird nur bei Änderung gerufen.
    // Nie darf die ganze Fläche kurzzeitig Klicks annehmen.
    virtual void eingabeSetzen(QWindow &fenster, const QRegion &lokal) = 0;
    // Sichtbarkeit (Pause, Vollbild); nach dem Zeigen kommt das erste Bild mit dem Freilegen (expose)
    virtual void zeigen(QWindow &fenster, bool sichtbar) = 0;
    // nächstes Bild im Takt des Monitors/Compositors anfordern; das Bild kommt als paintGL, danach frameSwapped
    virtual void bildAnfordern(QWindow &fenster) = 0;
};

// die Ebene der Plattform, für die gebaut wurde
std::unique_ptr<Ebene> ebeneErzeugen();

} // namespace plattform
