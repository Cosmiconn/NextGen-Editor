#include "mapeditor/core/HeightmapExchange.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>

namespace theseed::mapeditor::core {

namespace {

// ---------------------------------------------------------------- CRC32 / Adler32
std::uint32_t Crc32(const std::uint8_t* data, std::size_t n, std::uint32_t crc = 0) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    crc = ~crc;
    for (std::size_t i = 0; i < n; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

std::uint32_t Adler32(const std::vector<std::uint8_t>& data) {
    std::uint32_t a = 1, b = 0;
    for (const auto byte : data) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

void PutBE32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 3; i >= 0; --i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
}

std::uint32_t GetBE32(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | p[3];
}

void PutChunk(std::vector<std::uint8_t>& out, const char type[4], const std::vector<std::uint8_t>& data) {
    PutBE32(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    PutBE32(out, Crc32(out.data() + start, out.size() - start));
}

// ---------------------------------------------------------------- Inflate (RFC 1951)
struct BitReader {
    std::span<const std::uint8_t> data;
    std::size_t pos = 0;
    std::uint32_t bitBuf = 0;
    int bitCount = 0;
    bool overrun = false;
    std::uint32_t Bits(int n) {
        while (bitCount < n) {
            if (pos >= data.size()) { overrun = true; return 0; }
            bitBuf |= std::uint32_t(data[pos++]) << bitCount;
            bitCount += 8;
        }
        const std::uint32_t v = bitBuf & ((1u << n) - 1u);
        bitBuf >>= n;
        bitCount -= n;
        return v;
    }
    void AlignByte() { bitBuf = 0; bitCount = 0; }
};

struct Huffman {
    std::array<std::uint16_t, 16> counts{};
    std::vector<std::uint16_t> symbols;
    bool Build(const std::uint8_t* lengths, int n) {
        counts.fill(0);
        for (int i = 0; i < n; ++i) ++counts[lengths[i]];
        counts[0] = 0;
        std::array<std::uint16_t, 16> offs{};
        for (int i = 1; i < 16; ++i) offs[i] = static_cast<std::uint16_t>(offs[i - 1] + counts[i - 1]);
        symbols.assign(static_cast<std::size_t>(n), 0);
        for (int i = 0; i < n; ++i)
            if (lengths[i]) symbols[offs[lengths[i]]++] = static_cast<std::uint16_t>(i);
        return true;
    }
    int Decode(BitReader& br) const {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len < 16; ++len) {
            code |= static_cast<int>(br.Bits(1));
            const int count = counts[len];
            if (code - count < first) return symbols[static_cast<std::size_t>(index + (code - first))];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
            if (br.overrun) return -1;
        }
        return -1;
    }
};

std::expected<std::vector<std::uint8_t>, std::string> Inflate(std::span<const std::uint8_t> in) {
    static const std::uint16_t lenBase[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                            35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const std::uint8_t lenExtra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const std::uint16_t distBase[] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
                                             1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const std::uint8_t distExtra[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    std::vector<std::uint8_t> out;
    BitReader br{in};
    bool last = false;
    while (!last) {
        last = br.Bits(1) != 0;
        const std::uint32_t type = br.Bits(2);
        if (br.overrun) return std::unexpected("PNG: Datenstrom endet vorzeitig");
        if (type == 0) {
            br.AlignByte();
            if (br.pos + 4 > in.size()) return std::unexpected("PNG: gespeicherter Block abgeschnitten");
            const std::uint16_t len = static_cast<std::uint16_t>(in[br.pos] | (in[br.pos + 1] << 8));
            br.pos += 4;
            if (br.pos + len > in.size()) return std::unexpected("PNG: gespeicherter Block abgeschnitten");
            out.insert(out.end(), in.begin() + static_cast<std::ptrdiff_t>(br.pos), in.begin() + static_cast<std::ptrdiff_t>(br.pos + len));
            br.pos += len;
            continue;
        }
        Huffman lit, dist;
        if (type == 1) {
            std::array<std::uint8_t, 288> l{};
            for (int i = 0; i < 144; ++i) l[static_cast<std::size_t>(i)] = 8;
            for (int i = 144; i < 256; ++i) l[static_cast<std::size_t>(i)] = 9;
            for (int i = 256; i < 280; ++i) l[static_cast<std::size_t>(i)] = 7;
            for (int i = 280; i < 288; ++i) l[static_cast<std::size_t>(i)] = 8;
            lit.Build(l.data(), 288);
            std::array<std::uint8_t, 30> d{};
            d.fill(5);
            dist.Build(d.data(), 30);
        } else if (type == 2) {
            const int hlit = static_cast<int>(br.Bits(5)) + 257, hdist = static_cast<int>(br.Bits(5)) + 1,
                      hclen = static_cast<int>(br.Bits(4)) + 4;
            static const std::uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            std::array<std::uint8_t, 19> cl{};
            for (int i = 0; i < hclen; ++i) cl[order[i]] = static_cast<std::uint8_t>(br.Bits(3));
            Huffman clh;
            clh.Build(cl.data(), 19);
            std::array<std::uint8_t, 320> lengths{};
            int n = 0;
            while (n < hlit + hdist) {
                const int sym = clh.Decode(br);
                if (sym < 0) return std::unexpected("PNG: ungültige Codelängen");
                if (sym < 16) { lengths[static_cast<std::size_t>(n++)] = static_cast<std::uint8_t>(sym); continue; }
                int repeat = 0;
                std::uint8_t value = 0;
                if (sym == 16) {
                    if (n == 0) return std::unexpected("PNG: ungültige Wiederholung");
                    value = lengths[static_cast<std::size_t>(n - 1)];
                    repeat = 3 + static_cast<int>(br.Bits(2));
                } else if (sym == 17) repeat = 3 + static_cast<int>(br.Bits(3));
                else repeat = 11 + static_cast<int>(br.Bits(7));
                if (n + repeat > hlit + hdist) return std::unexpected("PNG: zu viele Codelängen");
                while (repeat--) lengths[static_cast<std::size_t>(n++)] = value;
            }
            lit.Build(lengths.data(), hlit);
            dist.Build(lengths.data() + hlit, hdist);
        } else {
            return std::unexpected("PNG: ungültiger Deflate-Blocktyp");
        }
        for (;;) {
            const int sym = lit.Decode(br);
            if (sym < 0) return std::unexpected("PNG: ungültiger Huffman-Code");
            if (sym < 256) { out.push_back(static_cast<std::uint8_t>(sym)); continue; }
            if (sym == 256) break;
            const int li = sym - 257;
            if (li >= 29) return std::unexpected("PNG: ungültige Länge");
            const std::size_t len = lenBase[li] + br.Bits(lenExtra[li]);
            const int di = dist.Decode(br);
            if (di < 0 || di >= 30) return std::unexpected("PNG: ungültiger Abstand");
            const std::size_t d = distBase[di] + br.Bits(distExtra[di]);
            if (d > out.size()) return std::unexpected("PNG: Abstand vor Datenbeginn");
            const std::size_t from = out.size() - d;
            for (std::size_t i = 0; i < len; ++i) out.push_back(out[from + i]);
        }
    }
    return out;
}

int Paeth(int a, int b, int c) {
    const int p = a + b - c, pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

float Sample(const HeightImage& img, float u, float v) {
    // u,v in Pixelkoordinaten
    const float x = std::clamp(u, 0.0f, static_cast<float>(img.width - 1));
    const float y = std::clamp(v, 0.0f, static_cast<float>(img.height - 1));
    const auto x0 = static_cast<std::uint32_t>(x), y0 = static_cast<std::uint32_t>(y);
    const std::uint32_t x1 = std::min(x0 + 1, img.width - 1), y1 = std::min(y0 + 1, img.height - 1);
    const float fx = x - static_cast<float>(x0), fy = y - static_cast<float>(y0);
    const auto at = [&](std::uint32_t xx, std::uint32_t yy) { return static_cast<float>(img.values[static_cast<std::size_t>(yy) * img.width + xx]); };
    return (at(x0, y0) * (1 - fx) + at(x1, y0) * fx) * (1 - fy) + (at(x0, y1) * (1 - fx) + at(x1, y1) * fx) * fy;
}

} // namespace

HeightImage HeightmapToImage(const Heightmap& heightmap, float minHeight, float maxHeight) {
    HeightImage img;
    img.width = heightmap.Width();
    img.height = heightmap.Height();
    img.values.resize(static_cast<std::size_t>(img.width) * img.height);
    img.range = std::make_pair(minHeight, maxHeight);
    const float span = maxHeight > minHeight ? maxHeight - minHeight : 1.0f;
    for (std::uint32_t row = 0; row < img.height; ++row) {
        const std::uint32_t z = img.height - 1 - row; // Zeile 0 = Norden
        for (std::uint32_t x = 0; x < img.width; ++x) {
            const float t = std::clamp((heightmap.At(x, z) - minHeight) / span, 0.0f, 1.0f);
            img.values[static_cast<std::size_t>(row) * img.width + x] = static_cast<std::uint16_t>(std::lround(t * 65535.0f));
        }
    }
    return img;
}

std::vector<std::uint8_t> EncodePng16(const HeightImage& image) {
    std::vector<std::uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    std::vector<std::uint8_t> ihdr;
    PutBE32(ihdr, image.width);
    PutBE32(ihdr, image.height);
    ihdr.insert(ihdr.end(), {16, 0, 0, 0, 0}); // 16 Bit, Graustufen, Deflate, Filter 0, kein Interlace
    PutChunk(out, "IHDR", ihdr);
    if (image.range) {
        char buf[96];
        const int n = std::snprintf(buf, sizeof(buf), "NextGenHeightRange%c%.4f %.4f", '\0', image.range->first, image.range->second);
        PutChunk(out, "tEXt", std::vector<std::uint8_t>(buf, buf + n));
    }
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(image.height) * (1 + image.width * 2));
    for (std::uint32_t y = 0; y < image.height; ++y) {
        raw.push_back(0);
        for (std::uint32_t x = 0; x < image.width; ++x) {
            const std::uint16_t v = image.values[static_cast<std::size_t>(y) * image.width + x];
            raw.push_back(static_cast<std::uint8_t>(v >> 8));
            raw.push_back(static_cast<std::uint8_t>(v & 0xFF));
        }
    }
    std::vector<std::uint8_t> z = {0x78, 0x01};
    for (std::size_t pos = 0; pos < raw.size() || pos == 0;) {
        const std::size_t len = std::min<std::size_t>(65535, raw.size() - pos);
        const bool last = pos + len >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<std::uint8_t>(len & 0xFF));
        z.push_back(static_cast<std::uint8_t>(len >> 8));
        z.push_back(static_cast<std::uint8_t>(~len & 0xFF));
        z.push_back(static_cast<std::uint8_t>((~len >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos), raw.begin() + static_cast<std::ptrdiff_t>(pos + len));
        pos += len;
        if (last) break;
    }
    PutBE32(z, Adler32(raw));
    PutChunk(out, "IDAT", z);
    PutChunk(out, "IEND", {});
    return out;
}

std::expected<HeightImage, std::string> DecodePng(std::span<const std::uint8_t> bytes) {
    static const std::uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (bytes.size() < 8 || !std::equal(sig, sig + 8, bytes.begin())) return std::unexpected("Keine PNG-Datei");
    HeightImage img;
    int bitDepth = 0, colorType = 0, interlace = 0;
    std::vector<std::uint8_t> idat;
    std::vector<std::uint8_t> palette; // PLTE, RGB-Tripel
    for (std::size_t pos = 8; pos + 12 <= bytes.size();) {
        const std::uint32_t len = GetBE32(&bytes[pos]);
        if (pos + 12 + len > bytes.size()) return std::unexpected("PNG: Chunk abgeschnitten");
        const std::string type(reinterpret_cast<const char*>(&bytes[pos + 4]), 4);
        const std::uint8_t* data = &bytes[pos + 8];
        if (type == "IHDR" && len >= 13) {
            img.width = GetBE32(data);
            img.height = GetBE32(data + 4);
            bitDepth = data[8];
            colorType = data[9];
            interlace = data[12];
        } else if (type == "PLTE") {
            palette.assign(data, data + len);
        } else if (type == "IDAT") {
            idat.insert(idat.end(), data, data + len);
        } else if (type == "tEXt") {
            const std::string text(reinterpret_cast<const char*>(data), len);
            const auto zero = text.find('\0');
            if (zero != std::string::npos && text.substr(0, zero) == "NextGenHeightRange") {
                float a = 0, b = 0;
                if (std::sscanf(text.c_str() + zero + 1, "%f %f", &a, &b) == 2) img.range = std::make_pair(a, b);
            }
        } else if (type == "IEND") {
            break;
        }
        pos += 12 + len;
    }
    if (img.width == 0 || img.height == 0 || img.width > 16384 || img.height > 16384) return std::unexpected("PNG: ungültige Größe");
    if (interlace != 0) return std::unexpected("PNG: Interlacing wird nicht unterstützt");
    const int channels = colorType == 0 || colorType == 3 ? 1 : colorType == 4 ? 2 : colorType == 2 ? 3 : colorType == 6 ? 4 : 0;
    if (channels == 0) return std::unexpected("PNG: unbekannter Farbtyp");
    const bool lowBits = (colorType == 0 || colorType == 3) && (bitDepth == 1 || bitDepth == 2 || bitDepth == 4);
    if (!lowBits && bitDepth != 8 && !(bitDepth == 16 && colorType != 3)) return std::unexpected("PNG: nicht unterstützte Bittiefe");
    if (colorType == 3 && palette.size() < 3) return std::unexpected("PNG: Palette fehlt");
    if (idat.size() < 6) return std::unexpected("PNG: keine Bilddaten");
    auto raw = Inflate(std::span<const std::uint8_t>(idat).subspan(2, idat.size() - 6));
    if (!raw) return std::unexpected(raw.error());
    // Filter arbeiten auf ganzen Bytes (mindestens 1); Zeilen mit < 8 Bit je Pixel sind gepackt.
    const std::size_t bpp = std::max<std::size_t>(1, static_cast<std::size_t>(channels) * (bitDepth / 8));
    const std::size_t stride = lowBits ? (static_cast<std::size_t>(img.width) * bitDepth + 7) / 8 : bpp * img.width;
    if (raw->size() < (stride + 1) * img.height) return std::unexpected("PNG: zu wenig Bilddaten");
    std::vector<std::uint8_t> prev(stride, 0), cur(stride);
    img.values.resize(static_cast<std::size_t>(img.width) * img.height);
    for (std::uint32_t y = 0; y < img.height; ++y) {
        const std::uint8_t* line = raw->data() + y * (stride + 1);
        const int filter = line[0];
        for (std::size_t i = 0; i < stride; ++i) {
            const int a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
            int v = line[1 + i];
            switch (filter) {
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: v += Paeth(a, b, c); break;
                default: break;
            }
            cur[i] = static_cast<std::uint8_t>(v);
        }
        for (std::uint32_t x = 0; x < img.width; ++x) {
            std::uint32_t sample = 0; // Rohwert (Graustufe oder Palettenindex)
            if (lowBits) {
                const std::size_t bit = static_cast<std::size_t>(x) * bitDepth;
                sample = (cur[bit / 8] >> (8 - bitDepth - static_cast<int>(bit % 8))) & ((1u << bitDepth) - 1u);
            } else {
                const std::uint8_t* px = &cur[x * bpp];
                sample = bitDepth == 16 ? static_cast<std::uint32_t>((px[0] << 8) | px[1]) : px[0];
            }
            std::uint16_t value = 0;
            if (colorType == 3) {
                const std::size_t at = static_cast<std::size_t>(sample) * 3;
                value = static_cast<std::uint16_t>((at < palette.size() ? palette[at] : 0) * 257); // Rotkanal
            } else if (bitDepth == 16) {
                value = static_cast<std::uint16_t>(sample);
            } else {
                value = static_cast<std::uint16_t>(sample * 65535u / ((1u << bitDepth) - 1u));
            }
            img.values[static_cast<std::size_t>(y) * img.width + x] = value;
        }
        std::swap(prev, cur);
    }
    return img;
}

std::vector<std::uint8_t> EncodeR16(const HeightImage& image) {
    std::vector<std::uint8_t> out;
    out.reserve(image.values.size() * 2);
    for (const auto v : image.values) {
        out.push_back(static_cast<std::uint8_t>(v & 0xFF));
        out.push_back(static_cast<std::uint8_t>(v >> 8));
    }
    return out;
}

std::expected<HeightImage, std::string> DecodeR16(std::span<const std::uint8_t> bytes) {
    if (bytes.size() % 2 != 0) return std::unexpected("R16: ungerade Dateigröße");
    const std::size_t count = bytes.size() / 2;
    const auto side = static_cast<std::uint32_t>(std::lround(std::sqrt(static_cast<double>(count))));
    if (side == 0 || static_cast<std::size_t>(side) * side != count) return std::unexpected("R16: Datei ist nicht quadratisch");
    HeightImage img;
    img.width = img.height = side;
    img.values.resize(count);
    for (std::size_t i = 0; i < count; ++i) img.values[i] = static_cast<std::uint16_t>(bytes[2 * i] | (bytes[2 * i + 1] << 8));
    return img;
}

std::expected<void, std::string> ExportHeightmapImage(const Heightmap& heightmap, const std::filesystem::path& file) {
    const auto [lo, hi] = heightmap.MinMax();
    const HeightImage img = HeightmapToImage(heightmap, lo, hi);
    std::string ext = file.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::vector<std::uint8_t> bytes;
    if (ext == ".png") bytes = EncodePng16(img);
    else if (ext == ".r16" || ext == ".raw") bytes = EncodeR16(img);
    else return std::unexpected("Unbekannte Endung (erlaubt: .png, .r16, .raw): " + file.string());
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) return std::unexpected("Datei nicht schreibbar: " + file.string());
    return {};
}

std::expected<HeightImage, std::string> ImportHeightImage(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return std::unexpected("Datei nicht lesbar: " + file.string());
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string ext = file.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == ".png") return DecodePng(bytes);
    if (ext == ".r16" || ext == ".raw") return DecodeR16(bytes);
    return std::unexpected("Unbekannte Endung (erlaubt: .png, .r16, .raw): " + file.string());
}

UndoPatch ApplyHeightImage(Heightmap& heightmap, const HeightImage& image, float minHeight, float maxHeight) {
    UndoPatch patch;
    if (image.width == 0 || image.height == 0 || heightmap.Width() == 0 || heightmap.Height() == 0) return patch;
    const float sx = heightmap.Width() > 1 ? static_cast<float>(image.width - 1) / static_cast<float>(heightmap.Width() - 1) : 0.0f;
    const float sy = heightmap.Height() > 1 ? static_cast<float>(image.height - 1) / static_cast<float>(heightmap.Height() - 1) : 0.0f;
    for (std::uint32_t z = 0; z < heightmap.Height(); ++z) {
        const float row = static_cast<float>(heightmap.Height() - 1 - z) * sy; // Bildzeile 0 = Norden
        for (std::uint32_t x = 0; x < heightmap.Width(); ++x) {
            const float v = Sample(image, static_cast<float>(x) * sx, row) / 65535.0f;
            const float h = minHeight + v * (maxHeight - minHeight);
            const float old = heightmap.At(x, z);
            if (old == h) continue;
            patch.entries.push_back({x, z, old});
            heightmap.Set(x, z, h);
        }
    }
    return patch;
}

} // namespace theseed::mapeditor::core
