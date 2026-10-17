#include "mapeditor/core/DdsWrite.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>

namespace theseed::mapeditor::core {

namespace {

struct Rgb {
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

std::uint16_t Pack565(const Rgb& c) {
    const auto q = [](float v, int maxv) {
        return static_cast<std::uint16_t>(std::clamp(static_cast<int>(std::lround(v * maxv / 255.0f)), 0, maxv));
    };
    return static_cast<std::uint16_t>((q(c.r, 31) << 11) | (q(c.g, 63) << 5) | q(c.b, 31));
}

Rgb Unpack565(std::uint16_t v) {
    const int r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
    return {static_cast<float>((r << 3) | (r >> 2)), static_cast<float>((g << 2) | (g >> 4)),
            static_cast<float>((b << 3) | (b >> 2))};
}

float Dist2(const Rgb& a, const Rgb& b) {
    const float dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}

// Palette und Indizes für zwei 565-Endpunkte (4-Farben-Modus); liefert den Fehler.
float AssignIndices(const std::array<Rgb, 16>& px, std::uint16_t c0, std::uint16_t c1, std::array<int, 16>& idx) {
    const Rgb p0 = Unpack565(c0), p1 = Unpack565(c1);
    const std::array<Rgb, 4> pal = {p0, p1, Rgb{(2 * p0.r + p1.r) / 3, (2 * p0.g + p1.g) / 3, (2 * p0.b + p1.b) / 3},
                                    Rgb{(p0.r + 2 * p1.r) / 3, (p0.g + 2 * p1.g) / 3, (p0.b + 2 * p1.b) / 3}};
    float err = 0.0f;
    for (int i = 0; i < 16; ++i) {
        int best = 0;
        float bd = Dist2(px[i], pal[0]);
        for (int k = 1; k < 4; ++k) {
            const float d = Dist2(px[i], pal[k]);
            if (d < bd) { bd = d; best = k; }
        }
        idx[i] = best;
        err += bd;
    }
    return err;
}

void EncodeColorBlock(const std::array<Rgb, 16>& px, std::uint8_t* out) {
    Rgb mean;
    for (const auto& p : px) { mean.r += p.r; mean.g += p.g; mean.b += p.b; }
    mean.r /= 16; mean.g /= 16; mean.b /= 16;
    // Hauptachse der Farbverteilung (Potenzmethode auf der Kovarianz).
    float cov[6] = {};
    for (const auto& p : px) {
        const float r = p.r - mean.r, g = p.g - mean.g, b = p.b - mean.b;
        cov[0] += r * r; cov[1] += r * g; cov[2] += r * b; cov[3] += g * g; cov[4] += g * b; cov[5] += b * b;
    }
    Rgb axis{1.0f, 1.0f, 1.0f};
    for (int it = 0; it < 8; ++it) {
        const Rgb n{cov[0] * axis.r + cov[1] * axis.g + cov[2] * axis.b, cov[1] * axis.r + cov[3] * axis.g + cov[4] * axis.b,
                    cov[2] * axis.r + cov[4] * axis.g + cov[5] * axis.b};
        const float len = std::sqrt(n.r * n.r + n.g * n.g + n.b * n.b);
        if (len < 1e-6f) break;
        axis = {n.r / len, n.g / len, n.b / len};
    }
    float tmin = 0.0f, tmax = 0.0f;
    for (const auto& p : px) {
        const float t = (p.r - mean.r) * axis.r + (p.g - mean.g) * axis.g + (p.b - mean.b) * axis.b;
        tmin = std::min(tmin, t);
        tmax = std::max(tmax, t);
    }
    const auto clampRgb = [](Rgb c) { return Rgb{std::clamp(c.r, 0.0f, 255.0f), std::clamp(c.g, 0.0f, 255.0f), std::clamp(c.b, 0.0f, 255.0f)}; };
    Rgb e0 = clampRgb({mean.r + axis.r * tmax, mean.g + axis.g * tmax, mean.b + axis.b * tmax});
    Rgb e1 = clampRgb({mean.r + axis.r * tmin, mean.g + axis.g * tmin, mean.b + axis.b * tmin});

    std::uint16_t c0 = Pack565(e0), c1 = Pack565(e1);
    std::array<int, 16> idx{};
    float err = AssignIndices(px, std::max(c0, c1), std::min(c0, c1), idx);
    std::uint16_t best0 = std::max(c0, c1), best1 = std::min(c0, c1);

    // Eine Verfeinerung: Endpunkte per kleinster Quadrate aus den Indizes neu bestimmen.
    {
        static constexpr float w[4] = {1.0f, 0.0f, 2.0f / 3.0f, 1.0f / 3.0f}; // Gewicht von Endpunkt 0
        float aa = 0, bb = 0, ab = 0;
        Rgb ax, bx;
        for (int i = 0; i < 16; ++i) {
            const float a = w[idx[i]], b = 1.0f - a;
            aa += a * a; bb += b * b; ab += a * b;
            ax.r += a * px[i].r; ax.g += a * px[i].g; ax.b += a * px[i].b;
            bx.r += b * px[i].r; bx.g += b * px[i].g; bx.b += b * px[i].b;
        }
        const float det = aa * bb - ab * ab;
        if (std::abs(det) > 1e-6f) {
            const Rgb n0 = clampRgb({(ax.r * bb - bx.r * ab) / det, (ax.g * bb - bx.g * ab) / det, (ax.b * bb - bx.b * ab) / det});
            const Rgb n1 = clampRgb({(bx.r * aa - ax.r * ab) / det, (bx.g * aa - ax.g * ab) / det, (bx.b * aa - ax.b * ab) / det});
            std::uint16_t r0 = Pack565(n0), r1 = Pack565(n1);
            if (r0 < r1) std::swap(r0, r1);
            std::array<int, 16> idx2{};
            const float err2 = AssignIndices(px, r0, r1, idx2);
            if (err2 < err && r0 != r1) { err = err2; idx = idx2; best0 = r0; best1 = r1; }
        }
    }
    if (best0 == best1) idx.fill(0); // einfarbig: Index 0 ist in beiden Modi die Farbe c0
    out[0] = static_cast<std::uint8_t>(best0 & 0xFF);
    out[1] = static_cast<std::uint8_t>(best0 >> 8);
    out[2] = static_cast<std::uint8_t>(best1 & 0xFF);
    out[3] = static_cast<std::uint8_t>(best1 >> 8);
    for (int row = 0; row < 4; ++row) {
        std::uint8_t bits = 0;
        for (int col = 0; col < 4; ++col) bits |= static_cast<std::uint8_t>(idx[row * 4 + col] << (col * 2));
        out[4 + row] = bits;
    }
}

void EncodeAlphaBc3(const std::array<std::uint8_t, 16>& a, std::uint8_t* out) {
    const int amax = *std::max_element(a.begin(), a.end());
    const int amin = *std::min_element(a.begin(), a.end());
    out[0] = static_cast<std::uint8_t>(amax);
    out[1] = static_cast<std::uint8_t>(amin);
    std::uint64_t bits = 0;
    if (amax != amin) {
        int pal[8];
        pal[0] = amax;
        pal[1] = amin;
        for (int k = 1; k < 7; ++k) pal[k + 1] = ((7 - k) * amax + k * amin) / 7;
        for (int i = 0; i < 16; ++i) {
            int best = 0, bd = 1 << 30;
            for (int k = 0; k < 8; ++k) {
                const int d = std::abs(a[i] - pal[k]);
                if (d < bd) { bd = d; best = k; }
            }
            bits |= static_cast<std::uint64_t>(best) << (3 * i);
        }
    }
    for (int k = 0; k < 6; ++k) out[2 + k] = static_cast<std::uint8_t>((bits >> (8 * k)) & 0xFF);
}

void EncodeAlphaBc2(const std::array<std::uint8_t, 16>& a, std::uint8_t* out) {
    for (int i = 0; i < 8; ++i) {
        const int lo = (a[2 * i] * 15 + 127) / 255, hi = (a[2 * i + 1] * 15 + 127) / 255;
        out[i] = static_cast<std::uint8_t>(lo | (hi << 4));
    }
}

void Put32(std::vector<std::uint8_t>& v, std::size_t at, std::uint32_t x) {
    for (int k = 0; k < 4; ++k) v[at + k] = static_cast<std::uint8_t>((x >> (8 * k)) & 0xFF);
}

} // namespace

std::vector<std::uint8_t> EncodeBcImage(std::span<const std::uint8_t> rgba, std::uint32_t width, std::uint32_t height,
                                        DdsFormat format) {
    const std::uint32_t bw = (width + 3) / 4, bh = (height + 3) / 4;
    const std::size_t blockSize = format == DdsFormat::Dxt1 ? 8 : 16;
    std::vector<std::uint8_t> out(static_cast<std::size_t>(bw) * bh * blockSize);
    if (width == 0 || height == 0 || rgba.size() < static_cast<std::size_t>(width) * height * 4) return {};
    for (std::uint32_t by = 0; by < bh; ++by) {
        for (std::uint32_t bx = 0; bx < bw; ++bx) {
            std::array<Rgb, 16> px{};
            std::array<std::uint8_t, 16> alpha{};
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x) {
                    const std::uint32_t sx = std::min(bx * 4 + x, width - 1), sy = std::min(by * 4 + y, height - 1);
                    const std::uint8_t* p = &rgba[(static_cast<std::size_t>(sy) * width + sx) * 4];
                    px[y * 4 + x] = {static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])};
                    alpha[y * 4 + x] = p[3];
                }
            }
            std::uint8_t* block = &out[(static_cast<std::size_t>(by) * bw + bx) * blockSize];
            if (format == DdsFormat::Dxt1) {
                EncodeColorBlock(px, block);
            } else {
                if (format == DdsFormat::Dxt3) EncodeAlphaBc2(alpha, block);
                else EncodeAlphaBc3(alpha, block);
                EncodeColorBlock(px, block + 8);
            }
        }
    }
    return out;
}

std::vector<std::uint8_t> DownsampleRgba(std::span<const std::uint8_t> rgba, std::uint32_t width, std::uint32_t height,
                                         std::uint32_t& outWidth, std::uint32_t& outHeight) {
    outWidth = std::max<std::uint32_t>(1, width / 2);
    outHeight = std::max<std::uint32_t>(1, height / 2);
    std::vector<std::uint8_t> out(static_cast<std::size_t>(outWidth) * outHeight * 4);
    for (std::uint32_t y = 0; y < outHeight; ++y) {
        for (std::uint32_t x = 0; x < outWidth; ++x) {
            for (int c = 0; c < 4; ++c) {
                int sum = 0;
                for (int dy = 0; dy < 2; ++dy)
                    for (int dx = 0; dx < 2; ++dx) {
                        const std::uint32_t sx = std::min(x * 2 + dx, width - 1), sy = std::min(y * 2 + dy, height - 1);
                        sum += rgba[(static_cast<std::size_t>(sy) * width + sx) * 4 + c];
                    }
                out[(static_cast<std::size_t>(y) * outWidth + x) * 4 + c] = static_cast<std::uint8_t>((sum + 2) / 4);
            }
        }
    }
    return out;
}

std::vector<std::uint8_t> BuildDdsFile(std::span<const std::uint8_t> rgba, std::uint32_t width, std::uint32_t height,
                                       DdsFormat format, bool mipmaps) {
    std::vector<std::uint8_t> file(128, 0);
    std::memcpy(file.data(), "DDS ", 4);
    if (format == DdsFormat::Bgra32) {
        // Unkomprimiert: Bytes B,G,R,A je Pixel, Masken R 0xFF0000, G 0xFF00, B 0xFF, A 0xFF000000.
        std::uint32_t levels = 1;
        if (mipmaps)
            for (std::uint32_t w = width, h = height; w > 1 || h > 1; w = std::max<std::uint32_t>(1, w / 2), h = std::max<std::uint32_t>(1, h / 2)) ++levels;
        Put32(file, 4, 124);
        Put32(file, 8, mipmaps ? 0xA1007u : 0x81007u);
        Put32(file, 12, height);
        Put32(file, 16, width);
        Put32(file, 20, width * height * 4);
        Put32(file, 28, mipmaps ? levels : 0);
        Put32(file, 76, 32);
        Put32(file, 80, 0x41); // DDPF_RGB | DDPF_ALPHAPIXELS
        Put32(file, 88, 32);
        Put32(file, 92, 0x00FF0000u);
        Put32(file, 96, 0x0000FF00u);
        Put32(file, 100, 0x000000FFu);
        Put32(file, 104, 0xFF000000u);
        Put32(file, 108, mipmaps ? 0x401008u : 0x1000u);
        std::vector<std::uint8_t> level(rgba.begin(), rgba.end());
        std::uint32_t w = width, h = height;
        for (std::uint32_t l = 0; l < levels; ++l) {
            for (std::size_t i = 0; i + 3 < level.size(); i += 4) {
                file.push_back(level[i + 2]);
                file.push_back(level[i + 1]);
                file.push_back(level[i]);
                file.push_back(level[i + 3]);
            }
            if (l + 1 < levels) {
                std::uint32_t nw = 0, nh = 0;
                level = DownsampleRgba(level, w, h, nw, nh);
                w = nw;
                h = nh;
            }
        }
        return file;
    }
    const std::size_t blockSize = format == DdsFormat::Dxt1 ? 8 : 16;
    const std::uint32_t topSize = static_cast<std::uint32_t>(((width + 3) / 4) * ((height + 3) / 4) * blockSize);
    std::uint32_t levels = 1;
    if (mipmaps)
        for (std::uint32_t w = width, h = height; w > 1 || h > 1; w = std::max<std::uint32_t>(1, w / 2), h = std::max<std::uint32_t>(1, h / 2)) ++levels;
    Put32(file, 4, 124);
    Put32(file, 8, mipmaps ? 0xA1007u : 0x81007u);
    Put32(file, 12, height);
    Put32(file, 16, width);
    Put32(file, 20, topSize);
    Put32(file, 28, mipmaps ? levels : 0);
    Put32(file, 76, 32);
    Put32(file, 80, 0x4); // DDPF_FOURCC
    std::memcpy(&file[84], format == DdsFormat::Dxt1 ? "DXT1" : format == DdsFormat::Dxt3 ? "DXT3" : "DXT5", 4);
    Put32(file, 108, mipmaps ? 0x401008u : 0x1000u);

    std::vector<std::uint8_t> level(rgba.begin(), rgba.end());
    std::uint32_t w = width, h = height;
    for (std::uint32_t l = 0; l < levels; ++l) {
        const auto blocks = EncodeBcImage(level, w, h, format);
        file.insert(file.end(), blocks.begin(), blocks.end());
        if (l + 1 < levels) {
            std::uint32_t nw = 0, nh = 0;
            level = DownsampleRgba(level, w, h, nw, nh);
            w = nw;
            h = nh;
        }
    }
    return file;
}

std::expected<void, std::string> WriteDdsFile(const std::filesystem::path& file, std::span<const std::uint8_t> rgba,
                                              std::uint32_t width, std::uint32_t height, DdsFormat format, bool mipmaps) {
    if (width == 0 || height == 0 || rgba.size() < static_cast<std::size_t>(width) * height * 4)
        return std::unexpected("DDS: ungültige Bildgröße");
    const auto bytes = BuildDdsFile(rgba, width, height, format, mipmaps);
    std::error_code ec;
    if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) return std::unexpected("DDS nicht schreibbar: " + file.string());
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) return std::unexpected("DDS nicht schreibbar: " + file.string());
    return {};
}

} // namespace theseed::mapeditor::core
