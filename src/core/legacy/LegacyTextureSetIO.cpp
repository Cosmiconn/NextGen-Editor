#include "mapeditor/core/legacy/LegacyTextureSetIO.hpp"
#include "mapeditor/core/legacy/BmpBlendMap.hpp"
#include "mapeditor/core/legacy/LegacyPathResolve.hpp"

namespace theseed::mapeditor::core::legacy {

namespace {

// Fallback-Auflösung, falls KEINE einzige Blend-BMP gefunden/lesbar ist (reiner
// Metadaten-Import ohne Bilddaten) - 512x512 ist die in allen bisher gesichteten echten
// Kartensets (Adl/Bera/RouVal01) beobachtete Blend-Auflösung, siehe docs/MAP_FORMAT.md.
// Reine Verlegenheitslösung, KEIN aus dem Format hergeleiteter Wert.
constexpr std::uint32_t kFallbackResolution = 512;

} // namespace

std::expected<TextureLayerStack, std::string> ImportLegacyTextureSet(
    const std::filesystem::path& iniFile,
    TextureSetImportReport* report) {

    auto iniResult = ParseLegacyMapIni(iniFile);
    if (!iniResult) {
        return std::unexpected(iniResult.error());
    }
    const LegacyMapIni& ini = *iniResult;
    const std::filesystem::path mapDir = iniFile.parent_path();

    // Erster Durchlauf: Blend-Auflösung aus der ERSTEN tatsächlich ladbaren BMP bestimmen.
    // WICHTIG: Diese Auflösung ist NICHT an die Heightmap gekoppelt - echte Referenzkarten
    // zeigen Blend-Bitmaps in fester, von der Heightmap-Auflösung unabhängiger Größe (z.B.
    // Bera: 257x257-Heightmap, aber 512x512-Blend-BMPs), siehe docs/MAP_FORMAT.md.
    std::uint32_t resolvedWidth = 0;
    std::uint32_t resolvedHeight = 0;
    for (const auto& layerDef : ini.layers) {
        auto resolvedPath = ResolveLegacyAssetPath(mapDir, layerDef.blendFileName);
        if (!resolvedPath) continue;
        auto probe = ReadBlendMapBmp(*resolvedPath);
        if (probe) {
            resolvedWidth = probe->Width();
            resolvedHeight = probe->Height();
            break;
        }
    }
    bool usedFallback = false;
    if (resolvedWidth == 0 || resolvedHeight == 0) {
        resolvedWidth = kFallbackResolution;
        resolvedHeight = kFallbackResolution;
        usedFallback = true;
    }

    TextureLayerStack stack(resolvedWidth, resolvedHeight);
    for (const auto& layerDef : ini.layers) {
        const std::size_t idx = stack.AddLayer(layerDef.name, layerDef.diffuseFileName, layerDef.uvScaleDiffuse);

        auto resolvedPath = ResolveLegacyAssetPath(mapDir, layerDef.blendFileName);
        if (!resolvedPath) {
            if (report != nullptr) {
                report->missingBlendFiles.push_back(layerDef.name + " (nicht gefunden: " + layerDef.blendFileName + ")");
            }
            continue;
        }

        auto blendResult = ReadBlendMapBmp(*resolvedPath);
        if (!blendResult) {
            if (report != nullptr) {
                report->missingBlendFiles.push_back(layerDef.name + " (Lesefehler: " + blendResult.error() + ")");
            }
            continue;
        }
        if (blendResult->Width() != stack.Width() || blendResult->Height() != stack.Height()) {
            // Beobachtet bei der echten Adl-Karte: 8 von 10 Layern nutzen eine andere Auflösung
            // als der Rest (476x476 statt 512x512). Statt den Layer ohne Blend-Daten zu lassen,
            // wird er auf die Stack-Auflösung resampelt (bilinear) - nicht verlustfrei, aber
            // deutlich besser als leere Gewichte, siehe docs/MAP_FORMAT.md.
            if (report != nullptr) {
                report->missingBlendFiles.push_back(
                    layerDef.name + " (auf Stack-Aufl\u00f6sung resampelt: " +
                    std::to_string(blendResult->Width()) + "x" + std::to_string(blendResult->Height()) + " -> " +
                    std::to_string(stack.Width()) + "x" + std::to_string(stack.Height()) + ")");
            }
            stack.Layer(idx).blend = ResampleBlendMap(*blendResult, stack.Width(), stack.Height());
            continue;
        }

        stack.Layer(idx).blend = std::move(*blendResult);
    }

    if (usedFallback && report != nullptr) {
        report->missingBlendFiles.push_back(
            "(Hinweis: keine lesbare Blend-BMP gefunden - Aufl\u00f6sung auf " +
            std::to_string(kFallbackResolution) + "x" + std::to_string(kFallbackResolution) + " geraten)");
    }

    return stack;
}

std::expected<void, std::string> ExportLegacyTextureSet(
    const TextureLayerStack& stack,
    const LegacyMapIni& iniMeta,
    const std::filesystem::path& outDir,
    const std::string& iniFileName) {

    if (iniMeta.layers.size() != stack.LayerCount()) {
        return std::unexpected(
            "ExportLegacyTextureSet: Layer-Anzahl in TextureLayerStack (" + std::to_string(stack.LayerCount()) +
            ") und iniMeta (" + std::to_string(iniMeta.layers.size()) + ") stimmt nicht \u00fcberein");
    }

    LegacyMapIni outIni = iniMeta;
    // WICHTIG: outIni.heightmapWidth/Height NICHT anfassen - das sind Heightmap-Felder, die
    // Textur-Layer-Auflösung (stack.Width()/Height()) ist davon unabhängig und hat im
    // Legacy-.ini-Format gar kein eigenes Feld (steckt implizit in der BMP selbst).
    for (std::size_t i = 0; i < stack.LayerCount(); ++i) {
        outIni.layers[i].name = stack.Layer(i).name;
        outIni.layers[i].diffuseFileName = stack.Layer(i).diffuseFileName;
        outIni.layers[i].uvScaleDiffuse = stack.Layer(i).uvScaleDiffuse;
        // blendFileName / startX / startY / width / height / uvScaleBlend bleiben aus iniMeta
        // erhalten - der TextureLayerStack selbst kennt diese Legacy-spezifischen Felder nicht.
    }

    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    if (ec) {
        return std::unexpected("Konnte Ausgabeverzeichnis nicht anlegen: " + outDir.string() + " (" + ec.message() + ")");
    }

    auto serializeResult = SerializeLegacyMapIni(outIni, outDir / iniFileName);
    if (!serializeResult) {
        return std::unexpected(serializeResult.error());
    }

    for (std::size_t i = 0; i < stack.LayerCount(); ++i) {
        // Export schreibt NEUE Dateien - "resmap"-Präfix entfernen (symmetrisch zum Import,
        // sonst findet ein nachfolgender Import der eigenen Export-Ausgabe die Dateien nicht).
        const std::filesystem::path blendPath =
            outDir / StripResmapPrefix(LegacyPathToNative(outIni.layers[i].blendFileName));
        std::filesystem::create_directories(blendPath.parent_path(), ec);
        if (ec) {
            return std::unexpected("Konnte Verzeichnis f\u00fcr Blend-BMP nicht anlegen: " + blendPath.parent_path().string());
        }
        auto writeResult = WriteBlendMapBmp(stack.Layer(i).blend, blendPath);
        if (!writeResult) {
            return std::unexpected(writeResult.error());
        }
    }

    return {};
}

} // namespace theseed::mapeditor::core::legacy
