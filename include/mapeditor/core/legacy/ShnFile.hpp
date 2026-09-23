#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>
#include <vector>
#include <utility>
#include <expected>

namespace theseed::mapeditor::core::legacy {

enum class ShnValueKind {
    UInt8, UInt16, UInt32, Int8, Int16, Int32, Float, String, PairUInt32, Raw
};

using ShnValue = std::variant<std::uint8_t, std::uint16_t, std::uint32_t,
                              std::int8_t, std::int16_t, std::int32_t, float,
                              std::string, std::pair<std::uint32_t, std::uint32_t>,
                              std::vector<std::uint8_t>>;

struct ShnColumn {
    std::string name;
    std::uint32_t type = 0;
    std::uint32_t length = 0;
    ShnValueKind kind = ShnValueKind::Raw;
    // true, wenn die Datei fuer diese Spalte gar keinen Namen enthaelt und `name` nur ein
    // Anzeige-Platzhalter ("Undefined N") ist - beim Speichern wird dann wieder ein leerer
    // Name geschrieben (Round-Trip), siehe CHANGELOG [0.44.25].
    bool synthesizedName = false;
};

struct ShnRow {
    std::uint16_t recordLength = 0;
    std::vector<ShnValue> values;
};

struct ShnFile {
    std::uint32_t version = 0;
    std::uint32_t defaultRecordLength = 0;
    std::vector<std::uint8_t> cryptoHeader; // 32 bytes for encrypted files
    std::vector<ShnColumn> columns;
    std::vector<ShnRow> rows;
    bool encrypted = true;
    std::filesystem::path path;

    std::string FileName() const;
    std::string TypeName(const ShnColumn& column) const;
};

std::expected<ShnFile, std::string> LoadShnFile(const std::filesystem::path& path);
std::expected<void, std::string> SaveShnFile(const ShnFile& file, const std::filesystem::path& path);

std::string ShnValueToString(const ShnValue& value);
std::expected<ShnValue, std::string> ParseShnValue(const ShnColumn& column, const std::string& text);

} // namespace theseed::mapeditor::core::legacy
