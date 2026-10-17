#pragma once
// NifPatch.hpp
// Fenster-Layouts (Interface-NIFs unter resmenu) verschieben und skalieren, ohne die Datei neu zu
// schreiben: es werden nur die Vertex-Floats des NiTriShapeData-Blocks eines Elements und dessen
// Bounding-Sphere überschrieben. Gamebryo 10.x/20.0.0.4 kennt keine Blockgrößen- oder
// Offset-Tabellen; Dateigröße und alle übrigen Bytes bleiben gleich.
//
// Jede Änderung wird vor der Rückgabe geprüft: die gepatchten Bytes müssen im strikten
// Standardpfad wieder laden, dieselben Elemente liefern, das Element muss genau im Zielrechteck
// liegen, alle anderen Elemente unverändert sein, und nur die erwarteten Bytebereiche dürfen
// sich unterscheiden. Sonst wird nichts geschrieben.

#include "mapeditor/core/NifModel.hpp"

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

// Rechteck eines Elements in der 2D-Ansicht: x = Editor-X, y = Editor-Z (Datei-Y), Bildschirmpixel.
struct NifRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

[[nodiscard]] NifRect NifPartRect(const NifMeshPart& part);

// Ob (und warum nicht) ein Element verschoben/skaliert werden kann.
struct NifPartPatchCheck {
    bool patchable = false;
    std::string reason;      // bei patchable == false: Grund (deutsch)
    std::string reasonEn;    // dasselbe auf Englisch
    int sharedUsers = 1;     // Elemente, die denselben Datenblock nutzen
};
[[nodiscard]] NifPartPatchCheck CheckNifPartPatchable(const NifByteLayout& layout, std::size_t partIndex);

// Verschiebt/skaliert Element `partIndex` so, dass es genau `target` einnimmt (Breite/Höhe 0 eines
// flachen Elements bleiben 0). Liefert die neuen Dateibytes oder einen Fehler.
[[nodiscard]] std::expected<std::vector<std::uint8_t>, std::string> PatchNifPartRect(const std::vector<std::uint8_t>& bytes,
                                                                                   std::size_t partIndex, const NifRect& target);

} // namespace theseed::mapeditor::core
