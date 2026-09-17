#include "mapeditor/core/legacy/BmpBlendMap.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace theseed::mapeditor::core::legacy {

namespace {

template <typename T>
void WriteRaw(std::ofstream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool ReadRaw(std::ifstream& in, T& value) {
    in.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(in);
}

std::uint32_t RowSizeBytes(std::uint32_t width, std::uint32_t bytesPerPixel) {
    return ((width * bytesPerPixel + 3u) / 4u) * 4u;
}

} // namespace

std::expected<void, std::string> WriteBlendMapBmp(const BlendMap& blend, const std::filesystem::path& file) {
    const std::uint32_t width = blend.Width();
    const std::uint32_t height = blend.Height();
    if (width == 0 || height == 0) {
        return std::unexpected("WriteBlendMapBmp: leeres Gitter (0x0)");
    }

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        return std::unexpected("Konnte BMP nicht zum Schreiben öffnen: " + file.string());
    }

    constexpr std::uint32_t kHeaderSize = 14 + 40;
    const std::uint32_t rowSize = RowSizeBytes(width, 3);
    const std::uint32_t imageSize = rowSize * height;
    const std::uint32_t fileSize = kHeaderSize + imageSize;

    out.put('B');
    out.put('M');
    WriteRaw(out, fileSize);
    WriteRaw<std::uint16_t>(out, 0);
    WriteRaw<std::uint16_t>(out, 0);
    WriteRaw(out, kHeaderSize);

    WriteRaw<std::uint32_t>(out, 40);
    WriteRaw<std::int32_t>(out, static_cast<std::int32_t>(width));
    WriteRaw<std::int32_t>(out, static_cast<std::int32_t>(height));
    WriteRaw<std::uint16_t>(out, 1);
    WriteRaw<std::uint16_t>(out, 24);
    WriteRaw<std::uint32_t>(out, 0);
    WriteRaw<std::uint32_t>(out, imageSize);
    WriteRaw<std::int32_t>(out, 2835);
    WriteRaw<std::int32_t>(out, 2835);
    WriteRaw<std::uint32_t>(out, 0);
    WriteRaw<std::uint32_t>(out, 0);

    std::vector<std::uint8_t> row(rowSize, 0);
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::uint32_t z = height - 1 - y;
        for (std::uint32_t x = 0; x < width; ++x) {
            const float w = std::clamp(blend.At(x, z), 0.0f, 1.0f);
            const auto gray = static_cast<std::uint8_t>(w * 255.0f + 0.5f);
            row[x * 3 + 0] = gray;
            row[x * 3 + 1] = gray;
            row[x * 3 + 2] = gray;
        }
        std::fill(row.begin() + width * 3, row.end(), std::uint8_t{0});
        out.write(reinterpret_cast<const char*>(row.data()), static_cast<std::streamsize>(row.size()));
    }

    if (!out) {
        return std::unexpected("Fehler beim Schreiben der BMP-Pixeldaten: " + file.string());
    }
    return {};
}

std::expected<BlendMap, std::string> ReadBlendMapBmp(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::unexpected("Konnte BMP nicht öffnen: " + file.string());
    }

    char sig[2]{};
    in.read(sig, 2);
    if (!in || sig[0] != 'B' || sig[1] != 'M') {
        return std::unexpected("Keine gültige BMP-Datei (Signatur fehlt): " + file.string());
    }

    std::uint32_t fileSize = 0;
    std::uint16_t reserved1 = 0, reserved2 = 0;
    std::uint32_t dataOffset = 0;
    if (!ReadRaw(in, fileSize) || !ReadRaw(in, reserved1) || !ReadRaw(in, reserved2) || !ReadRaw(in, dataOffset)) {
        return std::unexpected("Unerwartetes Dateiende im BMP-File-Header: " + file.string());
    }
    (void)fileSize; (void)reserved1; (void)reserved2;

    std::uint32_t infoHeaderSize = 0;
    std::int32_t width = 0, height = 0;
    std::uint16_t planes = 0, bitCount = 0;
    std::uint32_t compression = 0, imageSize = 0, colorsUsed = 0;
    std::int32_t xPpm = 0, yPpm = 0;
    std::uint32_t colorsImportant = 0;

    if (!ReadRaw(in, infoHeaderSize) || !ReadRaw(in, width) || !ReadRaw(in, height) ||
        !ReadRaw(in, planes) || !ReadRaw(in, bitCount) || !ReadRaw(in, compression) ||
        !ReadRaw(in, imageSize) || !ReadRaw(in, xPpm) || !ReadRaw(in, yPpm) ||
        !ReadRaw(in, colorsUsed) || !ReadRaw(in, colorsImportant)) {
        return std::unexpected("Unerwartetes Dateiende im BMP-Info-Header: " + file.string());
    }
    (void)xPpm; (void)yPpm; (void)planes; (void)imageSize; (void)colorsImportant;

    if (infoHeaderSize < 40 || compression != 0) {
        return std::unexpected("Nicht unterstütztes BMP-Headerformat oder Kompression: " + file.string());
    }
    if (bitCount != 24 && bitCount != 8) {
        return std::unexpected("Nur 24-bit oder 8-bit BMPs werden unterstützt: " + file.string());
    }
    if (width <= 0 || height == 0) {
        return std::unexpected("Ungültige BMP-Dimensionen: " + file.string());
    }

    const bool topDown = height < 0;
    const auto w = static_cast<std::uint32_t>(width);
    const auto h = static_cast<std::uint32_t>(topDown ? -height : height);
    const std::uint32_t bytesPerPixel = bitCount / 8;

    std::vector<std::uint8_t> paletteGray;
    if (bitCount == 8) {
        const std::uint32_t paletteEntries = colorsUsed != 0 ? colorsUsed : 256u;
        in.seekg(14 + static_cast<std::streamoff>(infoHeaderSize), std::ios::beg);
        paletteGray.resize(paletteEntries, 0);
        for (std::uint32_t i = 0; i < paletteEntries; ++i) {
            std::uint8_t b = 0, g = 0, r = 0, reservedByte = 0;
            if (!ReadRaw(in, b) || !ReadRaw(in, g) || !ReadRaw(in, r) || !ReadRaw(in, reservedByte)) {
                return std::unexpected("Unerwartetes Dateiende in der BMP-Palette: " + file.string());
            }
            paletteGray[i] = b;
        }
    }

    in.seekg(dataOffset, std::ios::beg);
    if (!in) {
        return std::unexpected("Ungültiger Pixeldaten-Offset in BMP: " + file.string());
    }

    const std::uint32_t rowSize = RowSizeBytes(w, bytesPerPixel);
    std::vector<std::uint8_t> row(rowSize);
    BlendMap blend(w, h);
    for (std::uint32_t y = 0; y < h; ++y) {
        in.read(reinterpret_cast<char*>(row.data()), static_cast<std::streamsize>(rowSize));
        if (!in) {
            return std::unexpected("Unerwartetes Dateiende in BMP-Pixeldaten: " + file.string());
        }
        const std::uint32_t z = topDown ? y : (h - 1 - y);
        for (std::uint32_t x = 0; x < w; ++x) {
            const std::uint8_t gray = bitCount == 24
                ? row[x * 3]
                : (row[x] < paletteGray.size() ? paletteGray[row[x]] : row[x]);
            blend.Set(x, z, static_cast<float>(gray) / 255.0f);
        }
    }
    return blend;
}

} // namespace theseed::mapeditor::core::legacy
