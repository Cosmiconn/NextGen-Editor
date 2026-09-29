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
};

struct BrushSettings {
    float radius = 200.0f;        // Wirkradius in Weltraum-Einheiten
    float strength = 10.0f;       // Höhenänderung pro Anwendung (Raise/Lower); Blend-Faktor (Smooth/Flatten)
    float flattenTarget = 0.0f;   // Zielhöhe, nur relevant für BrushMode::Flatten
};

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

class UndoStack {
public:
    void Push(UndoPatch patch);
    bool Undo(Heightmap& heightmap);
    bool Redo(Heightmap& heightmap);
    void Clear();

    [[nodiscard]] bool CanUndo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool CanRedo() const noexcept { return !redo_.empty(); }
    [[nodiscard]] std::size_t UndoDepth() const noexcept { return undo_.size(); }

private:
    std::vector<UndoPatch> undo_;
    std::vector<UndoPatch> redo_;
};

} // namespace theseed::mapeditor::core
