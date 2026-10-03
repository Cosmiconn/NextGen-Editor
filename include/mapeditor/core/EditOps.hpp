#pragma once
// EditOps.hpp
// Bearbeitungswerkzeuge für das Heightmap-Modul: Pinsel-Operationen (Raise/Lower/Smooth/
// Flatten) und ein Undo/Redo-Stack auf Basis minimaler Diff-Patches (nur betroffene Vertizes,
// nicht das gesamte Gitter).

#include "mapeditor/core/Heightmap.hpp"

#include <cstdint>
#include <vector>

namespace theseed::mapeditor::core {

enum class BrushMode {
    Raise,
    Lower,
    Smooth,
    Flatten,
    Noise,    // kohärentes Value-Noise, strength = Amplitude pro Anwendung
    Terrace,  // zieht Höhen zur nächsten Stufe (terraceStep)
    Erode,    // thermische Erosion: Material rutscht von zu steilen Stellen bergab
    Sharpen,  // Gegenteil von Smooth (Unreals "Detail")
    Ramp,     // ApplyRamp: ebene Rampe zwischen zwei Punkten (kein Stempel)
};

// Falloff vom Zentrum (1) zum Rand (0), wie Unreals Landscape-Falloff.
enum class BrushFalloff {
    Smooth,    // kubischer Smoothstep (Standard)
    Linear,
    Spherical, // Kugelkappe: breit oben, steil am Rand
    Tip,       // spitz: quadratisch zum Zentrum hin
    Constant,  // volle Stärke im ganzen Radius
};

// Flatten nur anheben, nur absenken oder beides (Unreal: Flatten Mode).
enum class FlattenSide { Both, RaiseOnly, LowerOnly };

struct BrushSettings {
    float radius = 200.0f;        // Wirkradius in Weltraum-Einheiten
    float strength = 10.0f;       // Höhenänderung pro Anwendung (Raise/Lower/Noise); Blend-Faktor in % (Smooth/Flatten/Terrace/Erode/Sharpen)
    float flattenTarget = 0.0f;   // Zielhöhe, nur relevant für BrushMode::Flatten
    BrushFalloff falloff = BrushFalloff::Smooth;
    FlattenSide flattenSide = FlattenSide::Both;
    float noiseScale = 400.0f;    // Wellenlänge des Noise in Weltraum-Einheiten
    std::uint32_t noiseSeed = 1;
    float terraceStep = 100.0f;   // Stufenhöhe
    float erodeTalus = 20.0f;     // erlaubter Höhenunterschied je Block, darüber rutscht Material
};

// Falloff-Gewicht für normierte Distanz 0 (Zentrum) .. 1 (Rand).
[[nodiscard]] float BrushFalloffWeight(BrushFalloff falloff, float normalizedDist);

// Ein einzelner rückgängig machbarer Bearbeitungsschritt: betroffene Vertizes + ihre Werte
// VOR der Anwendung. Wird sowohl für Undo (Werte zurückschreiben) als auch als Blaupause
// für den korrespondierenden Redo-Patch verwendet (siehe UndoStack).
struct UndoPatch {
    struct Entry {
        std::uint32_t x;
        std::uint32_t z;
        float oldValue;
    };
    std::vector<Entry> entries;
};

// Wendet einen einzelnen Pinselstempel an Weltposition (worldX, worldZ) an.
// Ein "Strich" (mehrere Frames mit gehaltener Maustaste) besteht aus mehreren Aufrufen;
// jeder Aufruf erzeugt sein eigenes UndoPatch (bewusst einfach gehalten für v1 -
// spätere Optimierung: Patches innerhalb eines Strichs zusammenfassen).
UndoPatch ApplyBrush(Heightmap& heightmap, BrushMode mode, const BrushSettings& settings,
                      float worldX, float worldZ);

// Schreibt die in patch gespeicherten Werte auf das Gitter zurück (für Undo UND Redo nutzbar,
// da beide letztlich "schreibe diese Werte an diese Positionen" sind).
void RevertPatch(Heightmap& heightmap, const UndoPatch& patch);

// Rampe zwischen zwei Weltpunkten (x, z, Höhe): Vertizes im Abstand <= width/2 von der Strecke
// werden auf die interpolierte Höhe gesetzt; ein Randstreifen (falloffWidth) blendet weich aus.
UndoPatch ApplyRamp(Heightmap& heightmap, float startX, float startZ, float startHeight,
                    float endX, float endZ, float endHeight, float width, float falloffWidth);

class UndoStack {
public:
    void Push(UndoPatch patch);
    bool Undo(Heightmap& heightmap);
    bool Redo(Heightmap& heightmap);
    void Clear();
    void ClearRedo() noexcept { redo_.clear(); }

    [[nodiscard]] bool CanUndo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool CanRedo() const noexcept { return !redo_.empty(); }
    [[nodiscard]] std::size_t UndoDepth() const noexcept { return undo_.size(); }

private:
    std::vector<UndoPatch> undo_;
    std::vector<UndoPatch> redo_;
};

} // namespace theseed::mapeditor::core
