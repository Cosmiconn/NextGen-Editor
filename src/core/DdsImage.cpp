#include "mapeditor/core/DdsImage.hpp"

#include <cstring>
#include <fstream>

namespace theseed::mapeditor::core {

namespace {

struct Rgba { std::uint8_t r, g, b, a; };

void Unpack565(std::uint16_t c, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
    r = static_cast<std::uint8_t>(((c >> 11) & 0x1F) * 255 / 31);
    g = static_cast<std::uint8_t>(((c >> 5) & 0x3F) * 255 / 63);
    b = static_cast<std::uint8_t>((c & 0x1F) * 255 / 31);
}

// Dekodiert den 8-Byte-Farbblock, gemeinsam für BC1/BC2/BC3 (BC2/BC3 immer im "4-Farben"-Modus,
// da deren Alpha separat kodiert ist - kein Platz für den BC1-Transparenz-Sondermodus nötig).
void DecodeColorBlock(const std::uint8_t* data, Rgba outColors[4], bool forceFourColor) {
    const auto c0 = static_cast<std::uint16_t>(data[0] | (data[1] << 8));
    const auto c1 = static_cast<std::uint16_t>(data[2] | (data[3] << 8));

    std::uint8_t r0, g0, b0, r1, g1, b1;
    Unpack565(c0, r0, g0, b0);
    Unpack565(c1, r1, g1, b1);

    outColors[0] = {r0, g0, b0, 255};
    outColors[1] = {r1, g1, b1, 255};
    if (forceFourColor || c0 > c1) {
        outColors[2] = {static_cast<std::uint8_t>((2 * r0 + r1) / 3), static_cast<std::uint8_t>((2 * g0 + g1) / 3),
                         static_cast<std::uint8_t>((2 * b0 + b1) / 3), 255};
        outColors[3] = {static_cast<std::uint8_t>((r0 + 2 * r1) / 3), static_cast<std::uint8_t>((g0 + 2 * g1) / 3),
                         static_cast<std::uint8_t>((b0 + 2 * b1) / 3), 255};
    } else {
        outColors[2] = {static_cast<std::uint8_t>((r0 + r1) / 2), static_cast<std::uint8_t>((g0 + g1) / 2),
                         static_cast<std::uint8_t>((b0 + b1) / 2), 255};
        outColors[3] = {0, 0, 0, 0};
    }
}

void DecodeBC1Block(const std::uint8_t* data, Rgba out[16]) {
    Rgba colors[4];
    DecodeColorBlock(data, colors, false);
    const std::uint32_t indices = static_cast<std::uint32_t>(data[4]) | (static_cast<std::uint32_t>(data[5]) << 8) |
                                   (static_cast<std::uint32_t>(data[6]) << 16) | (static_cast<std::uint32_t>(data[7]) << 24);
    for (int i = 0; i < 16; ++i) {
        out[i] = colors[(indices >> (i * 2)) & 0x3];
    }
}

// BC2/DXT3: 8 Byte expliziter 4-bit-Alpha (16 Texel), gefolgt vom 8-Byte-Farbblock.
void DecodeBC2Block(const std::uint8_t* alphaData, const std::uint8_t* colorData, Rgba out[16]) {
    Rgba colors[4];
    DecodeColorBlock(colorData, colors, true);
    const std::uint32_t indices = static_cast<std::uint32_t>(colorData[4]) | (static_cast<std::uint32_t>(colorData[5]) << 8) |
                                   (static_cast<std::uint32_t>(colorData[6]) << 16) | (static_cast<std::uint32_t>(colorData[7]) << 24);
    for (int i = 0; i < 16; ++i) {
        const std::uint8_t byteVal = alphaData[i / 2];
        const std::uint8_t nibble = (i % 2 == 0) ? (byteVal & 0x0F) : (byteVal >> 4);
        out[i] = colors[(indices >> (i * 2)) & 0x3];
        out[i].a = static_cast<std::uint8_t>(nibble * 17); // 0..15 -> 0..255
    }
}

// BC3/DXT5: 8 Byte interpolierter Alpha (2 Referenzwerte + 3-bit-Indizes), gefolgt vom
// 8-Byte-Farbblock.
void DecodeBC3Block(const std::uint8_t* alphaData, const std::uint8_t* colorData, Rgba out[16]) {
    Rgba colors[4];
    DecodeColorBlock(colorData, colors, true);
    const std::uint32_t indices = static_cast<std::uint32_t>(colorData[4]) | (static_cast<std::uint32_t>(colorData[5]) << 8) |
                                   (static_cast<std::uint32_t>(colorData[6]) << 16) | (static_cast<std::uint32_t>(colorData[7]) << 24);

    const std::uint8_t a0 = alphaData[0];
    const std::uint8_t a1 = alphaData[1];
    std::uint64_t alphaIndices = 0;
    for (int k = 0; k < 6; ++k) {
        alphaIndices |= static_cast<std::uint64_t>(alphaData[2 + k]) << (8 * k);
    }

    std::uint8_t alphaTable[8];
    alphaTable[0] = a0;
    alphaTable[1] = a1;
    if (a0 > a1) {
        for (int k = 1; k <= 6; ++k) {
            alphaTable[k + 1] = static_cast<std::uint8_t>(((7 - k) * a0 + k * a1) / 7);
        }
    } else {
        for (int k = 1; k <= 4; ++k) {
            alphaTable[k + 1] = static_cast<std::uint8_t>(((5 - k) * a0 + k * a1) / 5);
        }
        alphaTable[6] = 0;
        alphaTable[7] = 255;
    }

    for (int i = 0; i < 16; ++i) {
        out[i] = colors[(indices >> (i * 2)) & 0x3];
        out[i].a = alphaTable[(alphaIndices >> (i * 3)) & 0x7];
    }
}

enum class BcFormat { BC1, BC2, BC3, Unsupported };

BcFormat DetectFormat(const char fourCC[4]) {
    if (std::memcmp(fourCC, "DXT1", 4) == 0) return BcFormat::BC1;
    if (std::memcmp(fourCC, "DXT3", 4) == 0) return BcFormat::BC2;
    if (std::memcmp(fourCC, "DXT5", 4) == 0) return BcFormat::BC3;
    return BcFormat::Unsupported;
}

} // namespace

std::expected<DdsImage, std::string> DecodeBcImage(std::uint32_t width, std::uint32_t height,
                                                     std::uint32_t pixelFormat,
                                                     const std::vector<std::uint8_t>& data) {
    // pixelFormat ist ein Gamebryo-internes Enum (kleine Werte 0..N), NICHT der rohe D3DFORMAT-
    // Code der Engine (bestaetigt: David, 22.09.2026 - Fiesta rendert direkt mit DirectX 9, aber
    // dieses Feld bildet trotzdem ueber eine eigene Tabelle ab statt den D3DFORMAT-Wert 1:1 zu
    // uebernehmen - an allen bisher geprueften Dateien ausschliesslich 4/5/6 beobachtet).
    BcFormat format = BcFormat::Unsupported;
    switch (pixelFormat) {
        case 4: format = BcFormat::BC1; break; // PX_FMT_DXT1
        case 5: format = BcFormat::BC2; break; // PX_FMT_DXT3
        case 6: format = BcFormat::BC3; break; // PX_FMT_DXT5
        default: break;
    }
    if (format == BcFormat::Unsupported) {
        if (pixelFormat == 3) {
            return std::unexpected("NiPixelData-Pixelformat 3 ist palettiert/unkomprimiert und darf nicht als BC1 interpretiert werden");
        }
        return std::unexpected("Nicht unterstuetztes NiPixelData-Pixelformat: " + std::to_string(pixelFormat));
    }
    if (width == 0 || height == 0) {
        return std::unexpected("Ungueltige NiPixelData-Dimensionen");
    }

    const std::uint32_t blocksWide = (width + 3) / 4;
    const std::uint32_t blocksHigh = (height + 3) / 4;
    std::size_t blockSize = (format == BcFormat::BC1) ? 8 : 16;
    std::size_t required = static_cast<std::size_t>(blocksWide) * blocksHigh * blockSize;
    // Einige Fiesta-NIFs kennzeichnen die NiPixelData als DXT5, speichern den Top-Mip
    // tatsächlich aber in der 8-Byte-Variante (DXT1-kompatible Farbdaten). Die Mipmap-Offsets
    // sind hier die zuverlässigere Aussage über die tatsächlich gespeicherte Blockgröße.
    if (data.size() < required && format == BcFormat::BC3) {
        const std::size_t bc1Required = static_cast<std::size_t>(blocksWide) * blocksHigh * 8;
        if (data.size() >= bc1Required) {
            format = BcFormat::BC1;
            blockSize = 8;
            required = bc1Required;
        }
    }
    if (data.size() < required) {
        return std::unexpected("NiPixelData ist kuerzer als das angegebene Top-Mip");
    }

    DdsImage image;
    image.width = width;
    image.height = height;
    image.rgba.assign(static_cast<std::size_t>(width) * height * 4, 0);

    for (std::uint32_t by = 0; by < blocksHigh; ++by) {
        for (std::uint32_t bx = 0; bx < blocksWide; ++bx) {
            const std::size_t blockOffset = (static_cast<std::size_t>(by) * blocksWide + bx) * blockSize;
            const std::uint8_t* block = data.data() + blockOffset;
            Rgba pixels[16];
            switch (format) {
                case BcFormat::BC1: DecodeBC1Block(block, pixels); break;
                case BcFormat::BC2: DecodeBC2Block(block, block + 8, pixels); break;
                case BcFormat::BC3: DecodeBC3Block(block, block + 8, pixels); break;
                default: break;
            }
            for (std::uint32_t py = 0; py < 4; ++py) {
                const std::uint32_t y = by * 4 + py;
                if (y >= height) continue;
                for (std::uint32_t px = 0; px < 4; ++px) {
                    const std::uint32_t x = bx * 4 + px;
                    if (x >= width) continue;
                    const Rgba& p = pixels[py * 4 + px];
                    const std::size_t idx = (static_cast<std::size_t>(y) * width + x) * 4;
                    image.rgba[idx + 0] = p.r;
                    image.rgba[idx + 1] = p.g;
                    image.rgba[idx + 2] = p.b;
                    image.rgba[idx + 3] = p.a;
                }
            }
        }
    }
    return image;
}

void FlipVertical(DdsImage& image) {
    const std::size_t rowBytes = static_cast<std::size_t>(image.width) * 4;
    std::vector<std::uint8_t> row(rowBytes);
    for (std::uint32_t y = 0; y < image.height / 2; ++y) {
        auto* a = image.rgba.data() + static_cast<std::size_t>(y) * rowBytes;
        auto* b = image.rgba.data() + static_cast<std::size_t>(image.height - 1 - y) * rowBytes;
        std::memcpy(row.data(), a, rowBytes);
        std::memcpy(a, b, rowBytes);
        std::memcpy(b, row.data(), rowBytes);
    }
}

std::expected<DdsImage, std::string> LoadDdsImage(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return std::unexpected("Konnte DDS nicht \u00f6ffnen: " + file.string());
    }

    char header[128]{};
    in.read(header, sizeof(header));
    if (!in || std::memcmp(header, "DDS ", 4) != 0) {
        return std::unexpected("Keine g\u00fcltige DDS-Datei (Signatur fehlt): " + file.string());
    }

    std::uint32_t height = 0, width = 0;
    std::memcpy(&height, header + 12, 4);
    std::memcpy(&width, header + 16, 4);

    char fourCC[4]{};
    std::memcpy(fourCC, header + 84, 4);
    const BcFormat format = DetectFormat(fourCC);
    if (format == BcFormat::Unsupported) {
        return std::unexpected("Nicht unterst\u00fctztes DDS-Format (FourCC '" + std::string(fourCC, 4) +
                                "') - nur DXT1/DXT3/DXT5 werden unterst\u00fctzt: " + file.string());
    }
    if (width == 0 || height == 0) {
        return std::unexpected("Ung\u00fcltige DDS-Dimensionen: " + file.string());
    }

    const std::size_t blockSize = (format == BcFormat::BC1) ? 8 : 16;
    const std::uint32_t blocksWide = (width + 3) / 4;
    const std::uint32_t blocksHigh = (height + 3) / 4;
    const std::size_t bytes = static_cast<std::size_t>(blocksWide) * blocksHigh * blockSize;
    std::vector<std::uint8_t> blockData(bytes);
    in.read(reinterpret_cast<char*>(blockData.data()), static_cast<std::streamsize>(blockData.size()));
    if (!in) {
        return std::unexpected("Unerwartetes Dateiende in den DDS-Blockdaten (Top-Mip): " + file.string());
    }

    const std::uint32_t pf = format == BcFormat::BC1 ? 4u : (format == BcFormat::BC2 ? 5u : 6u);
    auto image = DecodeBcImage(width, height, pf, blockData);
    if (!image) return std::unexpected(image.error());

    // DDS speichert die Bildzeilen in normaler Top-Down-Reihenfolge, OpenGL interpretiert
    // beim Upload jedoch Texel-Zeile 0 als V=0 (unten). Für die Kartenkoordinaten des Editors
    // muss deshalb die Zeilenreihenfolge genau einmal gedreht werden. Der bisherige Versuch,
    // das nur im Terrain-Shader zu korrigieren, ließ die Diffuse-Textur relativ zu Blend/Block/Walk
    // weiterhin gespiegelt erscheinen. Die Korrektur gehört an die gemeinsame DDS-Decoding-Grenze.
    FlipVertical(*image);
    return image;
}


std::expected<DdsImage, std::string> LoadTgaImage(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::unexpected("Konnte TGA nicht oeffnen: " + file.string());
    std::uint8_t h[18]{};
    in.read(reinterpret_cast<char*>(h), sizeof(h));
    if (!in) return std::unexpected("Unerwartetes TGA-Dateiende im Header: " + file.string());
    const std::uint8_t idLen = h[0];
    const std::uint8_t colorMapType = h[1];
    const std::uint8_t imageType = h[2];
    if (colorMapType != 0) return std::unexpected("Palettierte TGA wird nicht unterstuetzt: " + file.string());
    if (imageType != 2 && imageType != 10 && imageType != 3) {
        return std::unexpected("Nicht unterstuetzter TGA-Typ " + std::to_string(imageType) + ": " + file.string());
    }
    const std::uint16_t width = static_cast<std::uint16_t>(h[12] | (h[13] << 8));
    const std::uint16_t height = static_cast<std::uint16_t>(h[14] | (h[15] << 8));
    const std::uint8_t depth = h[16];
    if (width == 0 || height == 0) return std::unexpected("Ungueltige TGA-Dimensionen: " + file.string());
    const bool grayscale = imageType == 3;
    if (grayscale ? depth != 8 : (depth != 24 && depth != 32)) {
        return std::unexpected("Nicht unterstuetzte TGA-Bittiefe: " + std::to_string(depth) + ": " + file.string());
    }
    in.seekg(idLen, std::ios::cur);
    if (!in) return std::unexpected("Unerwartetes TGA-Dateiende nach Image-ID: " + file.string());
    const std::size_t pixelBytes = grayscale ? 1u : static_cast<std::size_t>(depth / 8u);
    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    DdsImage image;
    image.width = width; image.height = height;
    image.rgba.resize(pixelCount * 4);
    std::size_t outPixel = 0;
    auto putPixel = [&](const std::uint8_t* src) {
        auto* dst = image.rgba.data() + outPixel * 4;
        if (grayscale) { dst[0] = dst[1] = dst[2] = src[0]; dst[3] = 255; }
        else { dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = pixelBytes == 4 ? src[3] : 255; }
        ++outPixel;
    };
    std::vector<std::uint8_t> px(pixelBytes);
    while (outPixel < pixelCount) {
        std::size_t run = 1;
        if (imageType == 10) {
            const int packet = in.get();
            if (packet == EOF) return std::unexpected("Unerwartetes TGA-Dateiende im RLE-Stream: " + file.string());
            run = static_cast<std::size_t>((packet & 0x7F) + 1);
            if (run > pixelCount - outPixel) return std::unexpected("TGA-RLE-Paket ueberschreitet Bildgroesse: " + file.string());
            in.read(reinterpret_cast<char*>(px.data()), static_cast<std::streamsize>(pixelBytes));
            if (!in) return std::unexpected("Unerwartetes TGA-Dateiende im RLE-Pixel: " + file.string());
            if (packet & 0x80) { for (std::size_t i = 0; i < run; ++i) putPixel(px.data()); continue; }
            putPixel(px.data()); --run;
        }
        for (std::size_t i = 0; i < run; ++i) {
            in.read(reinterpret_cast<char*>(px.data()), static_cast<std::streamsize>(pixelBytes));
            if (!in) return std::unexpected("Unerwartetes TGA-Dateiende in den Pixeln: " + file.string());
            putPixel(px.data());
        }
    }
    // TGA Origin-Bit: bit 5 gesetzt = obere Zeile zuerst, sonst untere Zeile zuerst.
    // OpenGL erwartet bei unserem gemeinsamen Texturpfad V=0 unten; daher nur bei Top-Origin flippen.
    if ((h[17] & 0x20u) != 0) FlipVertical(image);
    return image;
}


} // namespace theseed::mapeditor::core
