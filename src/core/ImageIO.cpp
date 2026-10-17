#include "mapeditor/core/ImageIO.hpp"

#include "mapeditor/core/DdsImage.hpp"

#include "stb_image.h"
#include "stb_image_resize2.h"
#include "stb_image_write.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

namespace theseed::mapeditor::core {

namespace {
std::uint16_t Le16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
std::uint32_t Le32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
void PutLe16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}

std::expected<std::vector<std::uint8_t>, std::string> ReadAll(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::unexpected("Datei nicht lesbar: " + file.string());
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool IsPng(std::span<const std::uint8_t> b) {
    static constexpr std::uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    return b.size() >= 8 && std::memcmp(b.data(), sig, 8) == 0;
}
bool IsDds(std::span<const std::uint8_t> b) { return b.size() >= 128 && std::memcmp(b.data(), "DDS ", 4) == 0; }
bool IsJpg(std::span<const std::uint8_t> b) { return b.size() >= 3 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF; }
bool IsBmp(std::span<const std::uint8_t> b) { return b.size() >= 26 && b[0] == 'B' && b[1] == 'M'; }
// TGA hat keine Kennung: Kopf auf Plausibilität prüfen (Echtfarbe/Graustufe, roh oder RLE).
bool LooksLikeTga(std::span<const std::uint8_t> b) {
    if (b.size() < 18) return false;
    const std::uint8_t type = b[2], bpp = b[16];
    const bool typeOk = type == 2 || type == 3 || type == 10 || type == 11;
    const bool bppOk = bpp == 8 || bpp == 16 || bpp == 24 || bpp == 32;
    return typeOk && bppOk && b[1] == 0 && Le16(&b[12]) > 0 && Le16(&b[14]) > 0;
}

// ---- TGA (eigener Codec: 16-Bit-Dateien tragen 1 Alpha-Bit, das stb verwirft)
std::expected<RgbaImage, std::string> DecodeTga(std::span<const std::uint8_t> b) {
    if (!LooksLikeTga(b)) return std::unexpected("TGA: unbekannter Kopf");
    const std::uint8_t idLength = b[0], type = b[2], bpp = b[16], desc = b[17];
    const std::uint32_t w = Le16(&b[12]), h = Le16(&b[14]);
    const std::size_t bytesPerPixel = bpp / 8;
    std::size_t pos = 18 + idLength;
    if (b[1] != 0) return std::unexpected("TGA mit Farbpalette wird nicht unterstützt");
    RgbaImage img;
    img.width = w;
    img.height = h;
    img.rgba.resize(std::size_t{w} * h * 4);
    const std::size_t pixels = std::size_t{w} * h;
    std::vector<std::uint8_t> raw(pixels * bytesPerPixel);
    if (type == 2 || type == 3) {
        if (pos + raw.size() > b.size()) return std::unexpected("TGA: Datei zu kurz");
        std::memcpy(raw.data(), b.data() + pos, raw.size());
    } else {
        std::size_t out = 0;
        while (out < raw.size()) {
            if (pos >= b.size()) return std::unexpected("TGA: RLE-Daten zu kurz");
            const std::uint8_t header = b[pos++];
            const std::size_t count = (header & 0x7F) + 1u;
            if (header & 0x80) {
                if (pos + bytesPerPixel > b.size()) return std::unexpected("TGA: RLE-Daten zu kurz");
                for (std::size_t i = 0; i < count && out < raw.size(); ++i, out += bytesPerPixel)
                    std::memcpy(raw.data() + out, b.data() + pos, bytesPerPixel);
                pos += bytesPerPixel;
            } else {
                const std::size_t n = std::min(count * bytesPerPixel, raw.size() - out);
                if (pos + n > b.size()) return std::unexpected("TGA: RLE-Daten zu kurz");
                std::memcpy(raw.data() + out, b.data() + pos, n);
                pos += n;
                out += n;
            }
        }
    }
    const bool topDown = (desc & 0x20) != 0;
    const bool rightToLeft = (desc & 0x10) != 0;
    int alphaBits = desc & 0x0F;
    // Alle 84 16-Bit-TGAs in NA2016 resmenu (Ladebilder) melden 1 Alpha-Bit, setzen es aber in
    // keinem Pixel: Ist es überall 0, trägt es keine Information und das Bild gilt als deckend.
    if (bpp == 16 && alphaBits >= 1) {
        bool anyAlpha = false;
        for (std::size_t i = 0; i < pixels && !anyAlpha; ++i) anyAlpha = (raw[i * 2 + 1] & 0x80) != 0;
        if (!anyAlpha) alphaBits = 0;
    }
    for (std::uint32_t y = 0; y < h; ++y) {
        const std::uint32_t srcY = topDown ? y : h - 1 - y;
        for (std::uint32_t x = 0; x < w; ++x) {
            const std::uint32_t srcX = rightToLeft ? w - 1 - x : x;
            const std::uint8_t* s = raw.data() + (std::size_t{srcY} * w + srcX) * bytesPerPixel;
            std::uint8_t* d = img.rgba.data() + (std::size_t{y} * w + x) * 4;
            if (bpp == 8) { d[0] = d[1] = d[2] = s[0]; d[3] = 255; }
            else if (bpp == 16) {
                const std::uint16_t v = Le16(s);
                d[0] = static_cast<std::uint8_t>(((v >> 10) & 0x1F) * 255 / 31);
                d[1] = static_cast<std::uint8_t>(((v >> 5) & 0x1F) * 255 / 31);
                d[2] = static_cast<std::uint8_t>((v & 0x1F) * 255 / 31);
                d[3] = alphaBits >= 1 ? ((v & 0x8000) ? 255 : 0) : 255;
            } else {
                d[0] = s[2]; d[1] = s[1]; d[2] = s[0];
                d[3] = bpp == 32 ? s[3] : 255;
            }
        }
    }
    return img;
}

std::vector<std::uint8_t> EncodeTga(const RgbaImage& img, int bits, bool topDown) {
    std::vector<std::uint8_t> out(18, 0);
    out[2] = 2; // unkomprimiert, Echtfarbe
    out[12] = static_cast<std::uint8_t>(img.width & 0xFF);
    out[13] = static_cast<std::uint8_t>(img.width >> 8);
    out[14] = static_cast<std::uint8_t>(img.height & 0xFF);
    out[15] = static_cast<std::uint8_t>(img.height >> 8);
    out[16] = static_cast<std::uint8_t>(bits);
    out[17] = static_cast<std::uint8_t>((bits == 32 ? 8 : bits == 16 ? 1 : 0) | (topDown ? 0x20 : 0));
    out.reserve(18 + std::size_t{img.width} * img.height * static_cast<std::size_t>(bits / 8));
    // 16 Bit: ein deckendes Bild wird wie die Originale mit Alpha-Bit 0 geschrieben (siehe
    // DecodeTga); nur echte Transparenz setzt das Bit für die deckenden Pixel.
    bool transparent = false;
    if (bits == 16)
        for (std::size_t i = 3; i < img.rgba.size() && !transparent; i += 4) transparent = img.rgba[i] < 128;
    for (std::uint32_t row = 0; row < img.height; ++row) {
        const std::uint32_t y = topDown ? row : img.height - 1 - row;
        for (std::uint32_t x = 0; x < img.width; ++x) {
            const std::uint8_t* p = img.rgba.data() + (std::size_t{y} * img.width + x) * 4;
            if (bits == 16) {
                const std::uint16_t v = static_cast<std::uint16_t>((transparent && p[3] >= 128 ? 0x8000 : 0) | ((p[0] * 31 + 127) / 255) << 10 |
                                                                   ((p[1] * 31 + 127) / 255) << 5 | ((p[2] * 31 + 127) / 255));
                PutLe16(out, v);
            } else {
                out.push_back(p[2]);
                out.push_back(p[1]);
                out.push_back(p[0]);
                if (bits == 32) out.push_back(p[3]);
            }
        }
    }
    return out;
}

// ---- DDS aus Bytes (oberste Mip-Stufe)
std::expected<RgbaImage, std::string> DecodeDds(std::span<const std::uint8_t> b) {
    if (!IsDds(b)) return std::unexpected("DDS: unbekannter Kopf");
    const std::uint32_t h = Le32(&b[12]), w = Le32(&b[16]), pfFlags = Le32(&b[80]);
    const auto data = b.subspan(128);
    std::expected<DdsImage, std::string> decoded = std::unexpected("DDS: Pixelformat nicht unterstützt");
    if (pfFlags & 0x4) {
        const char* four = reinterpret_cast<const char*>(&b[84]);
        const std::uint32_t pf = std::memcmp(four, "DXT1", 4) == 0 ? 4u : std::memcmp(four, "DXT3", 4) == 0 ? 5u
                               : std::memcmp(four, "DXT5", 4) == 0 ? 6u : 0u;
        if (pf == 0) return std::unexpected("DDS: FourCC nicht unterstützt");
        decoded = DecodeBcImage(w, h, pf, data);
    } else {
        const std::array<std::uint32_t, 4> masks = {Le32(&b[92]), Le32(&b[96]), Le32(&b[100]), (pfFlags & 0x1) ? Le32(&b[104]) : 0u};
        decoded = DecodePackedImage(w, h, Le32(&b[88]), masks, data);
    }
    if (!decoded) return std::unexpected(decoded.error());
    RgbaImage img;
    img.width = decoded->width;
    img.height = decoded->height;
    img.rgba = std::move(decoded->rgba);
    return img;
}

void StbWrite(void* context, void* data, int size) {
    auto* out = static_cast<std::vector<std::uint8_t>*>(context);
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + size);
}

std::uint32_t RoundUp4(std::uint32_t v) { return std::max<std::uint32_t>(4, (v + 3) / 4 * 4); }
} // namespace

const char* ImageContainerName(ImageContainer container) {
    switch (container) {
    case ImageContainer::Png: return "PNG";
    case ImageContainer::Jpg: return "JPG";
    case ImageContainer::Tga: return "TGA";
    case ImageContainer::Dds: return "DDS";
    case ImageContainer::Bmp: return "BMP";
    }
    return "?";
}

std::string DescribeImageFormat(const ImageFormat& f) {
    std::string out = ImageContainerName(f.container);
    if (f.container == ImageContainer::Dds) {
        out += f.dds == DdsFormat::Dxt1 ? " DXT1" : f.dds == DdsFormat::Dxt3 ? " DXT3" : f.dds == DdsFormat::Dxt5 ? " DXT5" : " A8R8G8B8";
        if (f.mipmaps) out += " +Mipmaps";
    } else if (f.container == ImageContainer::Tga) {
        out += " " + std::to_string(f.tgaBits) + " Bit";
    } else if (f.container == ImageContainer::Png) {
        out += f.pngAlpha ? " RGBA" : " RGB";
    }
    return out + ", " + std::to_string(f.width) + "x" + std::to_string(f.height);
}

std::expected<ImageFormat, std::string> InspectImageBytes(std::span<const std::uint8_t> b) {
    ImageFormat f;
    if (IsDds(b)) {
        f.container = ImageContainer::Dds;
        f.height = Le32(&b[12]);
        f.width = Le32(&b[16]);
        f.mipmaps = Le32(&b[28]) > 1;
        const std::uint32_t pfFlags = Le32(&b[80]);
        if (pfFlags & 0x4) {
            const char* four = reinterpret_cast<const char*>(&b[84]);
            if (std::memcmp(four, "DXT1", 4) == 0) f.dds = DdsFormat::Dxt1;
            else if (std::memcmp(four, "DXT3", 4) == 0) f.dds = DdsFormat::Dxt3;
            else if (std::memcmp(four, "DXT5", 4) == 0) f.dds = DdsFormat::Dxt5;
            else return std::unexpected("DDS: FourCC nicht unterstützt");
        } else {
            f.dds = DdsFormat::Bgra32;
        }
        return f;
    }
    if (IsPng(b) && b.size() >= 26) {
        f.container = ImageContainer::Png;
        f.width = (std::uint32_t{b[16]} << 24) | (std::uint32_t{b[17]} << 16) | (std::uint32_t{b[18]} << 8) | b[19];
        f.height = (std::uint32_t{b[20]} << 24) | (std::uint32_t{b[21]} << 16) | (std::uint32_t{b[22]} << 8) | b[23];
        const std::uint8_t colorType = b[25];
        f.pngAlpha = colorType == 4 || colorType == 6 || colorType == 3; // Palette kann Transparenz tragen
        return f;
    }
    if (IsJpg(b) || IsBmp(b)) {
        int w = 0, h = 0, comp = 0;
        if (!stbi_info_from_memory(b.data(), static_cast<int>(b.size()), &w, &h, &comp))
            return std::unexpected(std::string("Bildkopf nicht lesbar: ") + stbi_failure_reason());
        f.container = IsJpg(b) ? ImageContainer::Jpg : ImageContainer::Bmp;
        f.width = static_cast<std::uint32_t>(w);
        f.height = static_cast<std::uint32_t>(h);
        return f;
    }
    if (LooksLikeTga(b)) {
        f.container = ImageContainer::Tga;
        f.width = Le16(&b[12]);
        f.height = Le16(&b[14]);
        f.tgaBits = b[16] == 8 ? 24 : b[16];
        f.tgaTopDown = (b[17] & 0x20) != 0;
        return f;
    }
    return std::unexpected("Unbekanntes Bildformat");
}

std::expected<ImageFormat, std::string> InspectImageFile(const std::filesystem::path& file) {
    auto bytes = ReadAll(file);
    if (!bytes) return std::unexpected(bytes.error());
    return InspectImageBytes(*bytes);
}

std::expected<RgbaImage, std::string> DecodeImage(std::span<const std::uint8_t> b) {
    if (IsDds(b)) return DecodeDds(b);
    if (!IsPng(b) && !IsJpg(b) && !IsBmp(b) && LooksLikeTga(b)) return DecodeTga(b);
    int w = 0, h = 0, comp = 0;
    stbi_uc* pixels = stbi_load_from_memory(b.data(), static_cast<int>(b.size()), &w, &h, &comp, 4);
    if (!pixels) return std::unexpected(std::string("Bild nicht dekodierbar: ") + stbi_failure_reason());
    RgbaImage img;
    img.width = static_cast<std::uint32_t>(w);
    img.height = static_cast<std::uint32_t>(h);
    img.rgba.assign(pixels, pixels + std::size_t{img.width} * img.height * 4);
    stbi_image_free(pixels);
    return img;
}

std::expected<RgbaImage, std::string> LoadImageFile(const std::filesystem::path& file) {
    auto bytes = ReadAll(file);
    if (!bytes) return std::unexpected(bytes.error());
    return DecodeImage(*bytes);
}

std::expected<std::vector<std::uint8_t>, std::string> EncodeImage(const RgbaImage& image, const ImageFormat& format) {
    if (!image.Valid()) return std::unexpected("Bild leer oder ungültig");
    std::vector<std::uint8_t> out;
    const int w = static_cast<int>(image.width), h = static_cast<int>(image.height);
    switch (format.container) {
    case ImageContainer::Dds:
        return BuildDdsFile(image.rgba, image.width, image.height, format.dds, format.mipmaps);
    case ImageContainer::Tga:
        return EncodeTga(image, format.tgaBits == 16 || format.tgaBits == 24 ? format.tgaBits : 32, format.tgaTopDown);
    case ImageContainer::Png: {
        if (format.pngAlpha) {
            if (!stbi_write_png_to_func(StbWrite, &out, w, h, 4, image.rgba.data(), w * 4)) return std::unexpected("PNG nicht kodierbar");
        } else {
            std::vector<std::uint8_t> rgb(std::size_t{image.width} * image.height * 3);
            for (std::size_t i = 0, j = 0; i < image.rgba.size(); i += 4, j += 3) std::memcpy(&rgb[j], &image.rgba[i], 3);
            if (!stbi_write_png_to_func(StbWrite, &out, w, h, 3, rgb.data(), w * 3)) return std::unexpected("PNG nicht kodierbar");
        }
        return out;
    }
    case ImageContainer::Jpg:
        if (!stbi_write_jpg_to_func(StbWrite, &out, w, h, 4, image.rgba.data(), std::clamp(format.jpgQuality, 1, 100)))
            return std::unexpected("JPG nicht kodierbar");
        return out;
    case ImageContainer::Bmp:
        if (!stbi_write_bmp_to_func(StbWrite, &out, w, h, 4, image.rgba.data())) return std::unexpected("BMP nicht kodierbar");
        return out;
    }
    return std::unexpected("Format nicht unterstützt");
}

const char* FitModeName(FitMode mode, bool german) {
    switch (mode) {
    case FitMode::Stretch: return german ? "Strecken" : "Stretch";
    case FitMode::Fit: return german ? "Einpassen (Ränder transparent)" : "Fit (transparent borders)";
    case FitMode::Fill: return german ? "Füllen (mittig beschneiden)" : "Fill (crop centre)";
    }
    return "?";
}

namespace {
RgbaImage ResizeExact(const RgbaImage& image, std::uint32_t width, std::uint32_t height) {
    RgbaImage out;
    out.width = std::max<std::uint32_t>(1, width);
    out.height = std::max<std::uint32_t>(1, height);
    out.rgba.resize(std::size_t{out.width} * out.height * 4);
    if (out.width == image.width && out.height == image.height) { out.rgba = image.rgba; return out; }
    stbir_resize_uint8_srgb(image.rgba.data(), static_cast<int>(image.width), static_cast<int>(image.height), 0,
                            out.rgba.data(), static_cast<int>(out.width), static_cast<int>(out.height), 0, STBIR_RGBA);
    return out;
}
} // namespace

RgbaImage ResizeImage(const RgbaImage& image, std::uint32_t width, std::uint32_t height, FitMode mode) {
    if (!image.Valid() || width == 0 || height == 0) return {};
    if (mode == FitMode::Stretch) return ResizeExact(image, width, height);
    const double sx = static_cast<double>(width) / image.width, sy = static_cast<double>(height) / image.height;
    const double s = mode == FitMode::Fit ? std::min(sx, sy) : std::max(sx, sy);
    const auto scaledW = static_cast<std::uint32_t>(std::max(1.0, std::round(image.width * s)));
    const auto scaledH = static_cast<std::uint32_t>(std::max(1.0, std::round(image.height * s)));
    const RgbaImage scaled = ResizeExact(image, scaledW, scaledH);
    if (mode == FitMode::Fill) {
        const std::uint32_t x = scaledW > width ? (scaledW - width) / 2 : 0;
        const std::uint32_t y = scaledH > height ? (scaledH - height) / 2 : 0;
        RgbaImage out = CropImage(scaled, x, y, width, height);
        if (out.width != width || out.height != height) return ResizeExact(out, width, height); // Rundungsrest
        return out;
    }
    RgbaImage out;
    out.width = width;
    out.height = height;
    out.rgba.assign(std::size_t{width} * height * 4, 0);
    PasteImage(out, scaled, (width - std::min(width, scaledW)) / 2, (height - std::min(height, scaledH)) / 2);
    return out;
}

std::expected<ReplaceResult, std::string> BuildReplacement(const RgbaImage& source, const ImageFormat& original,
                                                           const ReplaceOptions& options) {
    if (!source.Valid()) return std::unexpected("Quellbild leer");
    ReplaceResult result;
    result.format = original;
    std::uint32_t w = options.keepOriginalSize && original.width > 0 ? original.width : source.width;
    std::uint32_t h = options.keepOriginalSize && original.height > 0 ? original.height : source.height;
    if (original.container == ImageContainer::Dds && original.dds != DdsFormat::Bgra32) { w = RoundUp4(w); h = RoundUp4(h); }
    if (original.container == ImageContainer::Tga && (w > 65535 || h > 65535)) return std::unexpected("TGA: höchstens 65535 Pixel je Seite");
    result.format.width = w;
    result.format.height = h;
    result.image = ResizeImage(source, w, h, options.fit);
    auto bytes = EncodeImage(result.image, result.format);
    if (!bytes) return std::unexpected(bytes.error());
    result.bytes = std::move(*bytes);
    return result;
}

RgbaImage CropImage(const RgbaImage& image, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height) {
    RgbaImage out;
    if (!image.Valid() || x >= image.width || y >= image.height) return out;
    out.width = std::min(width, image.width - x);
    out.height = std::min(height, image.height - y);
    out.rgba.resize(std::size_t{out.width} * out.height * 4);
    for (std::uint32_t row = 0; row < out.height; ++row)
        std::memcpy(out.rgba.data() + std::size_t{row} * out.width * 4,
                    image.rgba.data() + (std::size_t{y + row} * image.width + x) * 4, std::size_t{out.width} * 4);
    return out;
}

void PasteImage(RgbaImage& target, const RgbaImage& piece, std::uint32_t x, std::uint32_t y) {
    if (!target.Valid() || !piece.Valid() || x >= target.width || y >= target.height) return;
    const std::uint32_t w = std::min(piece.width, target.width - x), h = std::min(piece.height, target.height - y);
    for (std::uint32_t row = 0; row < h; ++row)
        std::memcpy(target.rgba.data() + (std::size_t{y + row} * target.width + x) * 4,
                    piece.rgba.data() + std::size_t{row} * piece.width * 4, std::size_t{w} * 4);
}

std::expected<std::vector<std::uint8_t>, std::string> SpliceDdsRegion(std::span<const std::uint8_t> original,
                                                                     std::span<const std::uint8_t> updated, std::uint32_t x,
                                                                     std::uint32_t y, std::uint32_t width, std::uint32_t height) {
    if (!IsDds(original) || !IsDds(updated)) return std::unexpected("DDS erwartet");
    const auto o = InspectImageBytes(original), u = InspectImageBytes(updated);
    if (!o || !u) return std::unexpected(!o ? o.error() : u.error());
    const std::uint32_t mips = std::max<std::uint32_t>(1, Le32(&original[28]));
    if (o->width != u->width || o->height != u->height || o->dds != u->dds || mips != std::max<std::uint32_t>(1, Le32(&updated[28])) ||
        original.size() != updated.size())
        return std::unexpected("DDS: Format, Größe oder Mipmaps weichen ab");
    const bool block = o->dds != DdsFormat::Bgra32;
    const std::size_t unitBytes = o->dds == DdsFormat::Dxt1 ? 8 : block ? 16 : std::max<std::uint32_t>(1, Le32(&original[88]) / 8);
    std::vector<std::uint8_t> out(original.begin(), original.end());
    std::size_t offset = 128;
    for (std::uint32_t level = 0; level < mips; ++level) {
        const std::uint32_t lw = std::max<std::uint32_t>(1, o->width >> level), lh = std::max<std::uint32_t>(1, o->height >> level);
        const std::uint32_t unitsX = block ? (lw + 3) / 4 : lw, unitsY = block ? (lh + 3) / 4 : lh;
        // Rechteck dieser Stufe, nach außen auf ganze Blöcke gerundet.
        const std::uint32_t scale = 1u << level, unit = block ? 4u : 1u;
        const std::uint32_t x0 = (x / scale) / unit, y0 = (y / scale) / unit;
        const std::uint32_t x1 = std::min(unitsX, ((x + width + scale - 1) / scale + unit - 1) / unit);
        const std::uint32_t y1 = std::min(unitsY, ((y + height + scale - 1) / scale + unit - 1) / unit);
        const std::size_t levelBytes = std::size_t{unitsX} * unitsY * unitBytes;
        if (offset + levelBytes > out.size()) return std::unexpected("DDS: Datei zu kurz");
        for (std::uint32_t row = y0; row < y1; ++row) {
            const std::size_t at = offset + (std::size_t{row} * unitsX + x0) * unitBytes;
            if (x1 > x0) std::memcpy(out.data() + at, updated.data() + at, std::size_t{x1 - x0} * unitBytes);
        }
        offset += levelBytes;
    }
    return out;
}

} // namespace theseed::mapeditor::core
