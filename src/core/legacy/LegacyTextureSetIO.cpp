#include "mapeditor/core/legacy/LegacyTextureSetIO.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include "mapeditor/core/legacy/BmpBlendMap.hpp"
#include "mapeditor/core/legacy/LegacyPathResolve.hpp"

namespace theseed::mapeditor::core::legacy {

namespace {

// Fallback-Auflösung, falls KEINE einzige Blend-BMP gefunden/lesbar ist (reiner
// Metadaten-Import ohne Bilddaten) - 512x512 ist die in allen bisher gesichteten echten
// Kartensets (Adl/Bera/RouVal01) beobachtete Blend-Auflösung, siehe docs/MAP_FORMAT.md.
// Reine Verlegenheitslösung, KEIN aus dem Format hergeleiteter Wert.
constexpr std::uint32_t kFallbackResolution = 512;

std::vector<std::uint8_t> ReadAllBytes(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// Zielpfad einer Blend-BMP. Ini-Pfade mit "resmap\"-Präfix sind relativ zum Client-Ordner
// (".\resmap\fieldtexture\L1_A.BMP" -> <Client>/resmap/fieldtexture/L1_A.BMP). Liegt das
// Ausgabeverzeichnis unter einem resmap-Ordner, wird dort hinein geschrieben; sonst (Export in
// einen beliebigen Ordner) wie bisher relativ zum Kartenordner ohne das Präfix.
std::expected<std::filesystem::path, std::string> BlendOutputPath(
    const std::filesystem::path& outDir, const std::string& legacyPath) {
    const auto native = LegacyPathToNative(legacyPath);
    for (const auto& part : native)
        if (part == "..") return std::unexpected("Unsicherer Blend-Pfad: " + legacyPath);
    if (native.is_absolute() || native.has_root_name())
        return std::unexpected("Absoluter Blend-Pfad: " + legacyPath);
    const auto stripped = StripResmapPrefix(native);
    if (stripped != native) {
        if (const auto resmap = FindResmapAncestor(outDir)) return (*resmap / stripped).lexically_normal();
    }
    return (outDir / stripped).lexically_normal();
}

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
        stack.Layer(idx).regionStartX = layerDef.startX;
        stack.Layer(idx).regionStartY = layerDef.startY;
        stack.Layer(idx).regionWidth = layerDef.width;
        stack.Layer(idx).regionHeight = layerDef.height;

        auto resolvedPath = ResolveLegacyAssetPath(mapDir, layerDef.blendFileName);
        if (!resolvedPath) {
            if (report != nullptr) {
                report->missingBlendFiles.push_back(layerDef.name + " (nicht gefunden: " + layerDef.blendFileName + ")");
            }
            stack.Layer(idx).blendMissingAtImport = true;
            continue;
        }

        auto blendResult = ReadBlendMapBmp(*resolvedPath);
        if (!blendResult) {
            if (report != nullptr) {
                report->missingBlendFiles.push_back(layerDef.name + " (Lesefehler: " + blendResult.error() + ")");
            }
            stack.Layer(idx).blendMissingAtImport = true;
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
            stack.Layer(idx).sourceBlend = std::move(*blendResult);
            stack.Layer(idx).sourceBmpBytes = ReadAllBytes(*resolvedPath);
            continue;
        }

        stack.Layer(idx).sourceBlend = *blendResult;
        stack.Layer(idx).blend = std::move(*blendResult);
        stack.Layer(idx).sourceBmpBytes = ReadAllBytes(*resolvedPath);
    }

    // Anfangszustand fehlender Masken merken (nach allen AddLayer-Aufrufen, die die Gewichte
    // der übrigen Layer noch verändern können), damit der Export "unbemalt" erkennt.
    for (std::size_t i = 0; i < stack.LayerCount(); ++i)
        if (stack.Layer(i).blendMissingAtImport) stack.Layer(i).sourceBlend = stack.Layer(i).blend;

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
        const auto blendTarget = BlendOutputPath(outDir, outIni.layers[i].blendFileName);
        if (!blendTarget) return std::unexpected(blendTarget.error());
        const std::filesystem::path& blendPath = *blendTarget;
        std::filesystem::create_directories(blendPath.parent_path(), ec);
        if (ec) {
            return std::unexpected("Konnte Verzeichnis f\u00fcr Blend-BMP nicht anlegen: " + blendPath.parent_path().string());
        }
        // Unverändert -> Originalbytes. Bearbeitet -> Werte in Originalauflösung (bei abweichender
        // Auflösung zurückresampelt) und, wenn möglich, nur die geänderten Pixel im Original patchen.
        const auto& layer = stack.Layer(i);
        // Beim Import nicht gefundene Maske, nie bemalt: nicht anlegen. Eine leere Datei würde
        // im Spiel die echte (für den Editor nur nicht auffindbare) Maske überdecken - bei
        // gemeinsamen Masken unter resmap/fieldtexture sogar für alle Karten.
        if (layer.blendMissingAtImport && layer.sourceBlend) {
            const auto a = layer.sourceBlend->Data();
            const auto b = layer.blend.Data();
            if (a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin())) continue;
        }
        const BlendMap* values = &layer.blend;
        BlendMap restored;
        bool unchanged = false;
        if (layer.sourceBlend && layer.sourceBlend->Width() > 0 && layer.sourceBlend->Height() > 0) {
            const bool sameSize = layer.sourceBlend->Width() == layer.blend.Width() &&
                                  layer.sourceBlend->Height() == layer.blend.Height();
            const BlendMap reference = sameSize ? *layer.sourceBlend
                : ResampleBlendMap(*layer.sourceBlend, layer.blend.Width(), layer.blend.Height());
            const auto a = reference.Data();
            const auto b = layer.blend.Data();
            unchanged = a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
            if (unchanged) {
                values = &*layer.sourceBlend;
            } else if (!sameSize) {
                // Zurück auf die Originalauflösung. Nur Pixel, die sich gegenüber dem ebenso
                // zurückgerechneten Ausgangsstand ändern, bekommen den neuen Wert; alle anderen
                // behalten exakt den Originalwert (Hin-und-zurück-Resampling ist nicht verlustfrei).
                const auto sw = layer.sourceBlend->Width(), sh = layer.sourceBlend->Height();
                restored = ResampleBlendMap(layer.blend, sw, sh);
                const BlendMap baseline = ResampleBlendMap(reference, sw, sh);
                auto q = [](float v) { return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
                for (std::uint32_t z = 0; z < sh; ++z)
                    for (std::uint32_t x = 0; x < sw; ++x)
                        if (q(restored.At(x, z)) == q(baseline.At(x, z))) restored.Set(x, z, layer.sourceBlend->At(x, z));
                values = &restored;
            }
        }
        if (!layer.sourceBmpBytes.empty()) {
            std::vector<std::uint8_t> bytes;
            if (unchanged) bytes = layer.sourceBmpBytes;
            else if (auto patched = PatchBlendMapBmp(layer.sourceBmpBytes, *values)) bytes = std::move(*patched);
            if (!bytes.empty()) {
                std::ofstream out(blendPath, std::ios::binary | std::ios::trunc);
                out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                if (!out) return std::unexpected("Fehler beim Schreiben der Blend-BMP: " + blendPath.string());
                continue;
            }
        }
        auto writeResult = WriteBlendMapBmp(*values, blendPath);
        if (!writeResult) {
            return std::unexpected(writeResult.error());
        }
    }

    return {};
}

} // namespace theseed::mapeditor::core::legacy
