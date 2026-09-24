#include "mapeditor/core/DdsImage.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

using namespace theseed::mapeditor::core;
namespace {
int failures = 0;
void check(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; ++failures; }
}
}
int main() {
    // RGB565 primaries, unlike channel-swapped BGR565; no alpha channel => opaque.
    const std::array<std::uint8_t, 6> rgb565{0x00,0xf8, 0xe0,0x07, 0x1f,0x00};
    auto rgb = DecodePackedImage(3,1,16,{0xf800,0x07e0,0x001f,0},rgb565);
    check(rgb && rgb->rgba == std::vector<std::uint8_t>{255,0,0,255,0,255,0,255,0,0,255,255}, "RGB565 channels");
    // Fiesta uses least-significant R, then G/B, then the one-bit alpha.
    const std::array<std::uint8_t,4> rgba5551{0x1f,0x80,0x00,0x7c};
    auto rgba = DecodePackedImage(2,1,16,{0x1f,0x3e0,0x7c00,0x8000},rgba5551);
    check(rgba && rgba->rgba == std::vector<std::uint8_t>{255,0,0,255,0,0,255,0}, "RGB5A1 channels and transparency");
    const std::array<std::uint8_t,4> bgra{3,2,1,4};
    auto swapped = DecodePackedImage(1,1,32,{0xff0000,0xff00,0xff,0xff000000},bgra);
    check(swapped && swapped->rgba == std::vector<std::uint8_t>{1,2,3,4}, "BGRA masks");
    check(!DecodePackedImage(1,1,16,{0xff,0xff,0,0},bgra), "overlapping masks rejected");
    check(!DecodePackedImage(1,1,16,{5,0,0,0},bgra), "noncontiguous masks rejected");
    check(!DecodePackedImage(1,1,16,{0xff0000,0,0,0},bgra), "out-of-range masks rejected");
    check(!DecodePackedImage(3,1,16,{31,992,31744,32768},rgba5551), "short data rejected");
    check(!DecodePackedImage(0xffffffffu,0xffffffffu,32,{255,65280,16711680,4278190080u},bgra), "overflow dimensions rejected");
    check(!DecodeBcImage(0xffffffffu,4,4,bgra), "BC dimension overflow rejected");

    // A 24-bit DDS with padded rows, and visibly different top/bottom pixels.
    std::vector<std::uint8_t> dds(136);
    auto put = [&](int at, std::uint32_t n) { for(int b=0;b<4;++b) dds[at+b]=static_cast<std::uint8_t>(n>>(b*8)); };
    put(0,0x20534444); put(4,124); put(8,8); put(12,2); put(16,1); put(20,4);
    put(76,32); put(80,0x40); put(88,24); put(92,0xff0000); put(96,0xff00); put(100,0xff);
    dds[130]=255; dds[132]=255; // red top, blue bottom; each row has one padding byte
    const auto file = std::filesystem::temp_directory_path()/"nextgen-packed-images.dds";
    { std::ofstream out(file,std::ios::binary); out.write(reinterpret_cast<const char*>(dds.data()),dds.size()); }
    const auto image = LoadDdsImage(file);
    check(image && image->rgba == std::vector<std::uint8_t>{0,0,255,255,255,0,0,255}, "DDS row pitch, masks and vertical origin");
    std::filesystem::remove(file);
    // 16-bit TGA, bottom/right origin, one-bit alpha. Source starts at the right pixel.
    std::vector<std::uint8_t> tga(22);
    tga[2] = 2; tga[12] = 2; tga[14] = 1; tga[16] = 16; tga[17] = 0x11;
    tga[18] = 31; tga[19] = 0; tga[20] = 0; tga[21] = 0xfc;
    const auto tgaFile = std::filesystem::temp_directory_path() / "nextgen-packed-images.tga";
    const auto writeTga = [&] { std::ofstream out(tgaFile, std::ios::binary); out.write(reinterpret_cast<const char*>(tga.data()), tga.size()); };
    writeTga(); const auto tg = LoadTgaImage(tgaFile);
    check(tg && tg->rgba == std::vector<std::uint8_t>{255,0,0,255,0,0,255,0}, "TGA16 BGR5A1, alpha and horizontal origin");
    tga.resize(20); writeTga(); check(!LoadTgaImage(tgaFile), "Truncated uncompressed TGA rejected");
    tga.resize(21); tga[2] = 10; tga[18] = 0x81; tga[19] = 0; tga[20] = 0xfc;
    writeTga(); const auto rle = LoadTgaImage(tgaFile);
    check(rle && rle->rgba == std::vector<std::uint8_t>{255,0,0,255,255,0,0,255}, "TGA16 RLE packet");
    tga[18] = 0x82; writeTga(); check(!LoadTgaImage(tgaFile), "TGA RLE overrun rejected");
    std::filesystem::remove(tgaFile);
    return failures ? 1 : 0;
}
