#pragma once
// DdsWrite.hpp
// DDS-Schreiber für Client-Bilder (Minimap, Stadtportal): BC1/BC2/BC3 (DXT1/3/5) mit optionalen
// Mipmaps. Header wie in den NA2016-Originalen: ohne Mipmaps Flags 0x81007 / Caps 0x1000,
// mit Mipmaps 0xA1007 / Caps 0x401008 und einer Kette bis 1x1.
// Eingabe: RGBA8, Zeile 0 = obere Bildkante (Dateireihenfolge, wie DecodeBcImage liefert).

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

// Bgra32: unkomprimiert A8R8G8B8 (Kopf wie Icon/MHEmotion00.dds: Flags 0x81007, PixelFormat 0x41).
enum class DdsFormat : std::uint8_t { Dxt1, Dxt3, Dxt5, Bgra32 };

// Komprimiert ein Bild (Breite/Höhe beliebig, Ränder werden aufgefüllt) in BC-Blöcke.
[[nodiscard]] std::vector<std::uint8_t> EncodeBcImage(std::span<const std::uint8_t> rgba, std::uint32_t width,
                                                     std::uint32_t height, DdsFormat format);

// Halbiert ein RGBA-Bild (Box-Filter, ungerade Kanten werden geklemmt); Mindestgröße 1.
[[nodiscard]] std::vector<std::uint8_t> DownsampleRgba(std::span<const std::uint8_t> rgba, std::uint32_t width,
                                                      std::uint32_t height, std::uint32_t& outWidth,
                                                      std::uint32_t& outHeight);

// Komplette DDS-Datei als Bytes.
[[nodiscard]] std::vector<std::uint8_t> BuildDdsFile(std::span<const std::uint8_t> rgba, std::uint32_t width,
                                                    std::uint32_t height, DdsFormat format, bool mipmaps);

std::expected<void, std::string> WriteDdsFile(const std::filesystem::path& file, std::span<const std::uint8_t> rgba,
                                              std::uint32_t width, std::uint32_t height, DdsFormat format,
                                              bool mipmaps);

} // namespace theseed::mapeditor::core
