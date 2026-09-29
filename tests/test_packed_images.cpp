#include "mapeditor/core/DdsImage.hpp"
#include <array>
#include <algorithm>
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

    // Legacy DDS cube map, 2x2 RGBA32 with a second 1x1 mip after every face.
    // Distinct face colors verify that the loader skips each face's full mip chain.
    std::vector<std::uint8_t> cube(128 + 6 * 20, 0);
    auto putCube = [&](int at, std::uint32_t n) {
        for (int b = 0; b < 4; ++b) cube[at + b] = static_cast<std::uint8_t>(n >> (b * 8));
    };
    putCube(0,0x20534444); putCube(4,124); putCube(8,0x2100F); putCube(12,2); putCube(16,2);
    putCube(20,8); putCube(28,2); putCube(76,32); putCube(80,0x41); putCube(88,32);
    putCube(92,0x000000ff); putCube(96,0x0000ff00); putCube(100,0x00ff0000); putCube(104,0xff000000);
    putCube(108,0x401008); // texture + mipmap + complex
    putCube(112,0x0000fe00); // cubemap + all six faces
    const std::array<std::array<std::uint8_t,4>,6> faceColors{{
        {{255,0,0,255}}, {{0,255,0,255}}, {{0,0,255,255}},
        {{255,255,0,255}}, {{255,0,255,255}}, {{0,255,255,255}}
    }};
    for (std::size_t face = 0; face < faceColors.size(); ++face) {
        const std::size_t base = 128 + face * 20;
        for (std::size_t px = 0; px < 4; ++px)
            for (std::size_t ch = 0; ch < 4; ++ch)
                cube[base + px*4 + ch] = faceColors[face][ch];
        // Cube faces intentionally stay in DDS top-down row order. Make the bottom row
        // of +X different so an accidental ordinary-2D vertical flip is observable.
        if (face == 0) {
            cube[base + 8] = 64;  cube[base + 9] = 0; cube[base + 10] = 0; cube[base + 11] = 255;
            cube[base + 12] = 64; cube[base + 13] = 0; cube[base + 14] = 0; cube[base + 15] = 255;
        }
        // lower mip deliberately differs; next face must start after these four bytes.
        cube[base+16]=static_cast<std::uint8_t>(face+1);
        cube[base+17]=cube[base+18]=0;
        cube[base+19]=255;
    }
    const auto cubeFile = std::filesystem::temp_directory_path() / "nextgen-cubemap.dds";
    { std::ofstream out(cubeFile,std::ios::binary);
      out.write(reinterpret_cast<const char*>(cube.data()),static_cast<std::streamsize>(cube.size())); }
    const auto cubeImage = LoadDdsCubeImage(cubeFile);
    check(cubeImage && cubeImage->width == 2 && cubeImage->height == 2, "DDS cube dimensions");
    if (cubeImage) {
        for (std::size_t face = 0; face < faceColors.size(); ++face) {
            check(cubeImage->faces[face].rgba.size() == 16, "DDS cube face pixel count");
            if (cubeImage->faces[face].rgba.size() >= 4)
                check(std::equal(faceColors[face].begin(), faceColors[face].end(),
                                 cubeImage->faces[face].rgba.begin()),
                      "DDS cube face order and mip stride");
            if (face == 0 && cubeImage->faces[face].rgba.size() >= 12) {
                check(cubeImage->faces[face].rgba[0] == 255 &&
                      cubeImage->faces[face].rgba[8] == 64,
                      "DDS cube preserves top-down face row order");
            }
        }
    }
    cube.resize(cube.size()-1);
    { std::ofstream out(cubeFile,std::ios::binary);
      out.write(reinterpret_cast<const char*>(cube.data()),static_cast<std::streamsize>(cube.size())); }
    check(!LoadDdsCubeImage(cubeFile), "Truncated DDS cube rejected");
    std::filesystem::remove(cubeFile);

    // 24-bit BMP: 2x2, positive height (bottom-up), each 6-byte row padded to 8 bytes.
    // DdsImage convention keeps the bottom row first for OpenGL V=0.
    std::vector<std::uint8_t> bmp(54 + 16, 0);
    auto putBmp16 = [&](int at, std::uint16_t n) {
        bmp[at] = static_cast<std::uint8_t>(n);
        bmp[at + 1] = static_cast<std::uint8_t>(n >> 8);
    };
    auto putBmp32 = [&](int at, std::uint32_t n) {
        for (int b = 0; b < 4; ++b) bmp[at + b] = static_cast<std::uint8_t>(n >> (b * 8));
    };
    bmp[0] = 'B'; bmp[1] = 'M';
    putBmp32(2, static_cast<std::uint32_t>(bmp.size()));
    putBmp32(10, 54); putBmp32(14, 40); putBmp32(18, 2); putBmp32(22, 2);
    putBmp16(26, 1); putBmp16(28, 24);
    // bottom: red, green; top: blue, white (BGR byte order)
    bmp[54] = 0; bmp[55] = 0; bmp[56] = 255;
    bmp[57] = 0; bmp[58] = 255; bmp[59] = 0;
    bmp[62] = 255; bmp[63] = 0; bmp[64] = 0;
    bmp[65] = 255; bmp[66] = 255; bmp[67] = 255;
    const auto bmpFile = std::filesystem::temp_directory_path() / "nextgen-packed-images.bmp";
    { std::ofstream out(bmpFile, std::ios::binary);
      out.write(reinterpret_cast<const char*>(bmp.data()), static_cast<std::streamsize>(bmp.size())); }
    const auto bm = LoadBmpImage(bmpFile);
    check(bm && bm->rgba == std::vector<std::uint8_t>{
        255,0,0,255, 0,255,0,255,
        0,0,255,255, 255,255,255,255},
        "BMP24 row padding, BGR channels and bottom-up origin");
    // Truncation must be a hard decode error for the strict NIF asset gate.
    bmp.resize(bmp.size() - 1);
    { std::ofstream out(bmpFile, std::ios::binary);
      out.write(reinterpret_cast<const char*>(bmp.data()), static_cast<std::streamsize>(bmp.size())); }
    check(!LoadBmpImage(bmpFile), "Truncated BMP rejected");
    std::filesystem::remove(bmpFile);

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
