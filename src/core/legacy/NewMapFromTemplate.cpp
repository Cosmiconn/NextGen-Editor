#include "mapeditor/core/legacy/NewMapFromTemplate.hpp"

#include "mapeditor/core/legacy/LegacyPathResolve.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <vector>

namespace theseed::mapeditor::core::legacy {

namespace {
// 24-Bit-BMP (BITMAPINFOHEADER, unten-oben) in einer Farbe - Format wie Rouvertexcolor2.bmp.
std::vector<std::uint8_t> SolidBmp24(std::uint32_t width, std::uint32_t height, std::uint8_t value) {
    const std::uint32_t stride = (width * 3 + 3) & ~3u;
    const std::uint32_t imageSize = stride * height;
    std::vector<std::uint8_t> out(54 + imageSize, 0);
    const auto put16 = [&](std::size_t at, std::uint16_t v) { out[at] = v & 0xFF; out[at + 1] = v >> 8; };
    const auto put32 = [&](std::size_t at, std::uint32_t v) { for (int i = 0; i < 4; ++i) out[at + i] = (v >> (8 * i)) & 0xFF; };
    out[0] = 'B'; out[1] = 'M';
    put32(2, static_cast<std::uint32_t>(out.size()));
    put32(10, 54);
    put32(14, 40);
    put32(18, width);
    put32(22, height);
    put16(26, 1);
    put16(28, 24);
    put32(34, imageSize);
    put32(38, 2834);
    put32(42, 2834);
    for (std::uint32_t y = 0; y < height; ++y)
        std::fill_n(out.begin() + 54 + static_cast<std::ptrdiff_t>(y) * stride, width * 3, value);
    return out;
}
} // namespace

bool IsValidNewMapName(const std::string& name) {
    // Server- und Client-Tabellen speichern Kartennamen als STRING[20] o.ä.; Ordner und Dateinamen
    // im Client sind einfache ASCII-Namen.
    if (name.empty() || name.size() > 15) return false;
    if (!std::isalpha(static_cast<unsigned char>(name.front()))) return false;
    return std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; });
}

std::expected<LegacyMapProject, std::string> CreateMapFromTemplate(const LegacyMapProject& tmpl, const NewMapParams& params) {
    if (!IsValidNewMapName(params.name))
        return std::unexpected("Ungültiger Kartenname (Buchstabe am Anfang, dann Buchstaben/Ziffern/_, höchstens 15 Zeichen)");
    if (params.heightmapPoints < 33 || params.heightmapPoints > 2049)
        return std::unexpected("Kartengröße muss zwischen 33 und 2049 Punkten liegen");
    if (!tmpl.hasHeightmap || tmpl.textureStack.LayerCount() == 0)
        return std::unexpected("Die Vorlage braucht Terrain und mindestens einen Textur-Layer");

    const std::uint32_t points = params.heightmapPoints;
    const std::uint32_t blocks = points - 1;
    const float blockW = tmpl.heightmap.BlockWidth(), blockH = tmpl.heightmap.BlockHeight();
    const std::string dir = ".\\resmap\\field\\" + params.name + "\\";

    LegacyMapProject map;
    map.sourceStem = params.name;

    map.heightmap = Heightmap(points, points, blockW, blockH);
    for (auto& h : map.heightmap.MutableData()) h = params.baseHeight;
    map.hasHeightmap = true;
    map.htdHeader = tmpl.htdHeader;

    // INI: neue Kanonik (kein Originaltext), Pfade relativ zum Client-Ordner wie im Original.
    map.ini = LegacyMapIni{};
    map.ini.heightFileName = dir + params.name + ".HTD";
    // Alle NA2016-Terrainkarten haben eine Vertex-Color-Bitmap in Heightmap-Größe (24 Bit, Werte nahe
    // Weiß = Licht unverändert); die neue Karte bekommt eine weiße.
    map.ini.vertexColorTexture = dir + params.name + "_VertexColor.bmp";
    map.preservedFiles.push_back({params.name + "_VertexColor.bmp", SolidBmp24(points, points, 255)});
    map.ini.heightmapWidth = points;
    map.ini.heightmapHeight = points;
    map.ini.oneBlockWidth = blockW;
    map.ini.oneBlockHeight = blockH;
    // QuadsWide/-High im Verhältnis der Vorlage (Rou: 256 Blöcke -> 64).
    const std::uint32_t tmplBlocks = std::max<std::uint32_t>(1, tmpl.heightmap.Width() - 1);
    const auto scaleQuads = [&](std::uint32_t q) {
        const double ratio = q > 0 ? static_cast<double>(tmplBlocks) / q : 4.0;
        return static_cast<std::uint32_t>(std::max(1.0, std::round(static_cast<double>(blocks) / ratio)));
    };
    map.ini.quadsWide = scaleQuads(tmpl.ini.quadsWide);
    map.ini.quadsHigh = scaleQuads(tmpl.ini.quadsHigh);

    // Textur-Layer: gleiche Auflösung der Masken wie die Vorlage (mindestens 256).
    const std::uint32_t maskRes = std::max<std::uint32_t>(256, tmpl.textureStack.Width());
    map.textureStack = TextureLayerStack(maskRes, maskRes);
    for (std::size_t i = 0; i < tmpl.textureStack.LayerCount(); ++i) {
        const auto& src = tmpl.textureStack.Layer(i);
        map.textureStack.AddLayer(src.name, src.diffuseFileName, src.uvScaleDiffuse);
        auto& layer = map.textureStack.Layer(i);
        layer.blend = BlendMap(maskRes, maskRes, i == 0 ? 1.0f : 0.0f);
        LegacyLayerDef def;
        def.name = src.name;
        def.diffuseFileName = src.diffuseFileName;
        def.blendFileName = dir + params.name + "_" + std::to_string(i + 1) + ".bmp";
        def.width = static_cast<float>(points);
        def.height = static_cast<float>(points);
        def.uvScaleDiffuse = src.uvScaleDiffuse;
        def.uvScaleBlend = i < tmpl.ini.layers.size() ? tmpl.ini.layers[i].uvScaleBlend : 1.0f;
        map.ini.layers.push_back(def);
    }

    // Block&Walk: Zeilen = Blöcke * 8 (Zelle 6.25 bei Blockgröße 50), quadratisch, alles frei.
    const std::uint32_t rows = blocks * 8;
    map.walkGrid = WalkGrid((rows + 15) / 16, rows, 0);
    map.hasWalkGrid = true;
    const std::uint32_t headerFields[2] = {blocks, rows};
    std::memcpy(map.shbdHeader.raw, headerFields, sizeof(headerFields));

    // Objekte: Szenenlicht und Himmel der Vorlage, sonst leer.
    map.objects.environment = tmpl.objects.environment;
    map.objects.hasLightingFooter = tmpl.objects.hasLightingFooter;
    for (const auto& category : tmpl.objects.categories) {
        ObjectCategoryList copy;
        copy.name = category.name;
        if (EqualsCaseInsensitive(category.name, "Sky")) copy.modelPaths = category.modelPaths;
        map.objects.categories.push_back(std::move(copy));
    }
    map.hasObjects = true;

    // .aid wie in der Vorlage (Bedeutung der Werte nicht gesichert, daher unverändert).
    map.zone = tmpl.zone;
    map.hasZone = tmpl.hasZone;

    // Begleitdateien: nur die Licht-/Glow-Datei <Karte>.conf, unter neuem Namen.
    for (const auto& preserved : tmpl.preservedFiles) {
        const auto dot = preserved.fileName.find_last_of('.');
        if (dot == std::string::npos || !EqualsCaseInsensitive(preserved.fileName.substr(dot), ".conf")) continue;
        PreservedMapFile conf = preserved;
        conf.fileName = params.name + ".conf";
        map.preservedFiles.push_back(std::move(conf));
    }
    return map;
}

} // namespace theseed::mapeditor::core::legacy
