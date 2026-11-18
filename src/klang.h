// Prozedural synthetisierte Geräusche (keine Audiodateien): Hubschrauber, Dieselmotor, Rückfahrwarner,
// Schnee-Rutsch, Plumps, Schneeball-Wurf/-Treffer, Murren. Ausgabe über QAudioSink (Pull-Modus),
// 48 kHz Stereo. Ist der Ton aus oder lange nichts zu hören, wird das Audiogerät geschlossen (keine CPU).
#pragma once
#include <QObject>
#include <memory>

enum class KlangArt { Rotor, Motor, Rueckfahrwarner, Rutsch, Plumps, Wurf, Treffer, Murren };

class Klang : public QObject {
public:
    static constexpr int Abtastrate = 48000;
    static constexpr int MaxEinmal = 12; // gleichzeitige Einmalklänge
    static constexpr int MaxDauer = 6;   // gleichzeitige Dauerklänge

    // mitGeraet = false: nur Synthese (Tests, WAV-Ausgabe), nie ein Audiogerät
    explicit Klang(bool mitGeraet = true, QObject *parent = nullptr);
    ~Klang() override;

    // Einmalklang an Bildschirmposition (x, y); staerke 0..2 (bei Rutsch: Menge/Länge)
    void ereignis(KlangArt art, double x, double y, double staerke = 1.0);
    // startet oder aktualisiert einen Dauerklang (Rotor, Motor, Rueckfahrwarner); last 0..1
    void dauerklang(int id, KlangArt art, double x, double y, double last);
    void dauerklangStopp(int id);

    void setzeAn(bool an);
    void setzeLautstaerke(double l); // 0..1
    void setzeWeltbreite(double links, double rechts);
    bool istAn() const;

    // Mischt `frames` Stereo-Frames (interleaved L,R) in `aus`; Werte immer in [-1, 1].
    // Wird vom Audiogerät aufgerufen, ist aber auch ohne Gerät nutzbar (Tests, WAV).
    void render(float *aus, int frames);

    int aktiveStimmen() const;           // Einmal- plus Dauerstimmen (auch ausklingende)
    double dauerFrequenz(int id) const;  // aktuelle Grundfrequenz in Hz, 0 wenn es id nicht gibt
    bool geraetOffen() const;

private:
    void geraetSicherstellen();
    struct Impl;
    std::unique_ptr<Impl> d;
};
