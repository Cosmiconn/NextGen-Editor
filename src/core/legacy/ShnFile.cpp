#include "mapeditor/core/legacy/ShnFile.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <type_traits>

namespace theseed::mapeditor::core::legacy {
namespace {

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& b) : bytes(b) {}
    std::size_t pos = 0;
    bool CanRead(std::size_t n) const { return pos + n <= bytes.size(); }
    template<class T> std::expected<T, std::string> Read() {
        if (!CanRead(sizeof(T))) return std::unexpected("SHN: unerwartetes Dateiende.");
        T value{};
        std::memcpy(&value, bytes.data() + pos, sizeof(T));
        pos += sizeof(T);
        return value;
    }
    std::expected<std::vector<std::uint8_t>, std::string> ReadBytes(std::size_t n) {
        if (!CanRead(n)) return std::unexpected("SHN: unerwartetes Dateiende bei Rohdaten.");
        std::vector<std::uint8_t> out(bytes.begin() + static_cast<std::ptrdiff_t>(pos),
                                      bytes.begin() + static_cast<std::ptrdiff_t>(pos + n));
        pos += n;
        return out;
    }
    std::expected<std::string, std::string> ReadPaddedString(std::size_t n) {
        auto raw = ReadBytes(n);
        if (!raw) return std::unexpected(raw.error());
        auto end = std::find(raw->begin(), raw->end(), static_cast<std::uint8_t>(0));
        return std::string(reinterpret_cast<const char*>(raw->data()),
                           static_cast<std::size_t>(end - raw->begin()));
    }
private:
    const std::vector<std::uint8_t>& bytes;
};

class Writer {
public:
    std::vector<std::uint8_t> bytes;
    template<class T> void Write(T value) {
        const auto* p = reinterpret_cast<const std::uint8_t*>(&value);
        bytes.insert(bytes.end(), p, p + sizeof(T));
    }
    void WriteBytes(const std::vector<std::uint8_t>& v) { bytes.insert(bytes.end(), v.begin(), v.end()); }
    void WritePaddedString(const std::string& value, std::size_t length) {
        const std::size_t n = std::min(value.size(), length);
        bytes.insert(bytes.end(), value.begin(), value.begin() + static_cast<std::ptrdiff_t>(n));
        bytes.insert(bytes.end(), length - n, 0);
    }
};

void Crypt(std::vector<std::uint8_t>& data) {
    std::uint8_t num = static_cast<std::uint8_t>(data.size());
    for (std::size_t k = data.size(); k-- > 0;) {
        const std::uint8_t i = static_cast<std::uint8_t>(k);
        data[k] = static_cast<std::uint8_t>(data[k] ^ num);
        std::uint8_t n = static_cast<std::uint8_t>(i & 15u);
        n = static_cast<std::uint8_t>(n + 0x55u);
        n = static_cast<std::uint8_t>(n ^ static_cast<std::uint8_t>(i * 11u));
        n = static_cast<std::uint8_t>(n ^ num);
        n = static_cast<std::uint8_t>(n ^ 0xAAu);
        num = n;
    }
}

ShnValueKind KindFor(std::uint32_t type) {
    switch (type) {
        case 1: case 12: case 16: return ShnValueKind::UInt8;
        case 2: return ShnValueKind::UInt16;
        case 3: case 11: case 18: case 27: return ShnValueKind::UInt32;
        case 5: return ShnValueKind::Float;
        case 9: case 24: case 26: return ShnValueKind::String;
        case 13: case 21: return ShnValueKind::Int16;
        case 20: return ShnValueKind::Int8;
        case 22: return ShnValueKind::Int32;
        case 29: return ShnValueKind::PairUInt32;
        default: return ShnValueKind::Raw;
    }
}

std::string Hex(const std::vector<std::uint8_t>& bytes) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i) out << ' ';
        out << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    return out.str();
}

std::expected<std::uint64_t, std::string> ParseUnsigned(const std::string& text) {
    try {
        std::size_t used = 0;
        const int base = text.starts_with("0x") || text.starts_with("0X") ? 16 : 10;
        const auto value = std::stoull(text, &used, base);
        if (used != text.size()) return std::unexpected("Ungültige Ganzzahl: " + text);
        return value;
    } catch (...) { return std::unexpected("Ungültige Ganzzahl: " + text); }
}

std::expected<std::int64_t, std::string> ParseSigned(const std::string& text) {
    try {
        std::size_t used = 0;
        const auto value = std::stoll(text, &used, 10);
        if (used != text.size()) return std::unexpected("Ungültige Ganzzahl: " + text);
        return value;
    } catch (...) { return std::unexpected("Ungültige Ganzzahl: " + text); }
}

} // namespace

std::string ShnFile::FileName() const { return path.filename().string(); }

std::string ShnFile::TypeName(const ShnColumn& column) const {
    switch (column.kind) {
        case ShnValueKind::UInt8: return "UInt8 (type " + std::to_string(column.type) + ")";
        case ShnValueKind::UInt16: return "UInt16 (type " + std::to_string(column.type) + ")";
        case ShnValueKind::UInt32: return "UInt32 (type " + std::to_string(column.type) + ")";
        case ShnValueKind::Int8: return "Int8 (type 20)";
        case ShnValueKind::Int16: return "Int16 (type " + std::to_string(column.type) + ")";
        case ShnValueKind::Int32: return "Int32 (type 22)";
        case ShnValueKind::Float: return "Float32 (type 5)";
        case ShnValueKind::String: return "String (type " + std::to_string(column.type) + ")";
        case ShnValueKind::PairUInt32: return "UInt32Pair (type 29)";
        case ShnValueKind::Raw: return "Raw/Unknown (type " + std::to_string(column.type) + ")";
    }
    return "Unknown";
}

std::expected<ShnFile, std::string> LoadShnFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::unexpected("SHN konnte nicht geöffnet werden: " + path.string());
    std::vector<std::uint8_t> file((std::istreambuf_iterator<char>(in)), {});
    if (file.size() < 16) return std::unexpected("SHN ist zu klein.");

    ShnFile out;
    out.path = path;
    std::vector<std::uint8_t> content;
    if (file.size() >= 36) {
        std::uint32_t storedLength = 0;
        std::memcpy(&storedLength, file.data() + 32, 4);
        if (storedLength == file.size() && storedLength >= 36) {
            out.encrypted = true;
            out.cryptoHeader.assign(file.begin(), file.begin() + 32);
            content.assign(file.begin() + 36, file.end());
            Crypt(content);
        } else {
            out.encrypted = false;
            content = std::move(file);
        }
    } else {
        out.encrypted = false;
        content = std::move(file);
    }

    Reader r(content);
    auto version = r.Read<std::uint32_t>(); if (!version) return std::unexpected(version.error());
    auto rowCount = r.Read<std::uint32_t>(); if (!rowCount) return std::unexpected(rowCount.error());
    auto defaultLen = r.Read<std::uint32_t>(); if (!defaultLen) return std::unexpected(defaultLen.error());
    auto colCount = r.Read<std::uint32_t>(); if (!colCount) return std::unexpected(colCount.error());
    out.version = *version;
    out.defaultRecordLength = *defaultLen;
    if (*colCount > 4096 || *rowCount > 10000000) return std::unexpected("SHN Header enthält unrealistische Zeilen-/Spaltenzahlen.");

    out.columns.reserve(*colCount);
    std::uint32_t calculatedDefault = 2;
    for (std::uint32_t i = 0; i < *colCount; ++i) {
        auto name = r.ReadPaddedString(48); if (!name) return std::unexpected(name.error());
        auto type = r.Read<std::uint32_t>(); if (!type) return std::unexpected(type.error());
        auto len = r.Read<std::uint32_t>(); if (!len) return std::unexpected(len.error());
        ShnColumn c{*name, *type, *len, KindFor(*type)};
        if (c.name.size() < 2) c.name = "Undefined " + std::to_string(i);
        out.columns.push_back(std::move(c));
        calculatedDefault += *len;
    }
    if (calculatedDefault != out.defaultRecordLength) {
        return std::unexpected("SHN: DefaultRecordLength passt nicht zu den Spaltendefinitionen.");
    }

    out.rows.reserve(*rowCount);
    for (std::uint32_t ri = 0; ri < *rowCount; ++ri) {
        auto rowLen = r.Read<std::uint16_t>(); if (!rowLen) return std::unexpected(rowLen.error());
        ShnRow row; row.recordLength = *rowLen; row.values.reserve(out.columns.size());
        for (const auto& c : out.columns) {
            switch (c.kind) {
                case ShnValueKind::UInt8: { auto v=r.Read<std::uint8_t>(); if(!v)return std::unexpected(v.error()); row.values.emplace_back(*v); break; }
                case ShnValueKind::UInt16:{ auto v=r.Read<std::uint16_t>();if(!v)return std::unexpected(v.error());row.values.emplace_back(*v);break; }
                case ShnValueKind::UInt32:{ auto v=r.Read<std::uint32_t>();if(!v)return std::unexpected(v.error());row.values.emplace_back(*v);break; }
                case ShnValueKind::Int8:{ auto v=r.Read<std::int8_t>();if(!v)return std::unexpected(v.error());row.values.emplace_back(*v);break; }
                case ShnValueKind::Int16:{ auto v=r.Read<std::int16_t>();if(!v)return std::unexpected(v.error());row.values.emplace_back(*v);break; }
                case ShnValueKind::Int32:{ auto v=r.Read<std::int32_t>();if(!v)return std::unexpected(v.error());row.values.emplace_back(*v);break; }
                case ShnValueKind::Float:{ auto v=r.Read<float>();if(!v)return std::unexpected(v.error());row.values.emplace_back(*v);break; }
                case ShnValueKind::String: {
                    std::size_t n = c.type == 26 ? 0 : c.length;
                    if (c.type == 26) {
                        // Type 26 is a variable-length, NUL-terminated string. Its extra bytes
                        // are represented by RowLength - DefaultRecordLength + 1.
                        n = *rowLen >= out.defaultRecordLength ? *rowLen - out.defaultRecordLength + 1u : 1u;
                    }
                    auto v=r.ReadPaddedString(n); if(!v)return std::unexpected(v.error()); row.values.emplace_back(*v); break;
                }
                case ShnValueKind::PairUInt32:{ auto a=r.Read<std::uint32_t>();if(!a)return std::unexpected(a.error()); auto b=r.Read<std::uint32_t>();if(!b)return std::unexpected(b.error()); row.values.emplace_back(std::make_pair(*a,*b));break; }
                case ShnValueKind::Raw:{ auto v=r.ReadBytes(c.length);if(!v)return std::unexpected(v.error());row.values.emplace_back(std::move(*v));break; }
            }
        }
        out.rows.push_back(std::move(row));
    }
    return out;
}

std::expected<void, std::string> SaveShnFile(const ShnFile& file, const std::filesystem::path& path) {
    if (file.columns.size() > std::numeric_limits<std::uint32_t>::max() || file.rows.size() > std::numeric_limits<std::uint32_t>::max())
        return std::unexpected("Zu viele SHN-Zeilen oder Spalten.");
    Writer w;
    w.Write(file.version);
    w.Write(static_cast<std::uint32_t>(file.rows.size()));
    std::uint32_t defaultLen = 2;
    for (const auto& c : file.columns) defaultLen += c.length;
    w.Write(defaultLen);
    w.Write(static_cast<std::uint32_t>(file.columns.size()));
    for (const auto& c : file.columns) {
        w.WritePaddedString(c.name, 48);
        w.Write(c.type);
        w.Write(c.length);
    }

    for (const auto& row : file.rows) {
        if (row.values.size() != file.columns.size()) return std::unexpected("SHN: Zeile hat falsche Spaltenzahl.");
        Writer rowData;
        std::uint32_t variableExtra = 0;
        for (std::size_t i=0; i<file.columns.size(); ++i) {
            const auto& c=file.columns[i]; const auto& v=row.values[i];
            switch (c.kind) {
                case ShnValueKind::UInt8: rowData.Write(std::get<std::uint8_t>(v)); break;
                case ShnValueKind::UInt16: rowData.Write(std::get<std::uint16_t>(v)); break;
                case ShnValueKind::UInt32: rowData.Write(std::get<std::uint32_t>(v)); break;
                case ShnValueKind::Int8: rowData.Write(std::get<std::int8_t>(v)); break;
                case ShnValueKind::Int16: rowData.Write(std::get<std::int16_t>(v)); break;
                case ShnValueKind::Int32: rowData.Write(std::get<std::int32_t>(v)); break;
                case ShnValueKind::Float: rowData.Write(std::get<float>(v)); break;
                case ShnValueKind::String: {
                    const auto& s=std::get<std::string>(v);
                    if (c.type==26) { rowData.WritePaddedString(s,s.size()+1); variableExtra += static_cast<std::uint32_t>(s.size()); }
                    else rowData.WritePaddedString(s,c.length);
                    break;
                }
                case ShnValueKind::PairUInt32: { const auto& p=std::get<std::pair<std::uint32_t,std::uint32_t>>(v); rowData.Write(p.first); rowData.Write(p.second); break; }
                case ShnValueKind::Raw: { const auto& raw=std::get<std::vector<std::uint8_t>>(v); if(raw.size()!=c.length)return std::unexpected("SHN: Rohdatenlänge stimmt nicht."); rowData.WriteBytes(raw); break; }
            }
        }
        const std::uint32_t recordLength = defaultLen + variableExtra;
        if (recordLength > std::numeric_limits<std::uint16_t>::max()) return std::unexpected("SHN: Zeile ist länger als 65535 Bytes.");
        w.Write(static_cast<std::uint16_t>(recordLength));
        w.WriteBytes(rowData.bytes);
    }

    std::vector<std::uint8_t> content = std::move(w.bytes);
    std::vector<std::uint8_t> finalBytes;
    if (file.encrypted) {
        if (file.cryptoHeader.size() != 32) return std::unexpected("SHN: 32-Byte Crypto-Header fehlt.");
        auto encrypted = content; Crypt(encrypted);
        finalBytes = file.cryptoHeader;
        const std::uint32_t totalLength = static_cast<std::uint32_t>(encrypted.size() + 36u);
        const auto* p = reinterpret_cast<const std::uint8_t*>(&totalLength);
        finalBytes.insert(finalBytes.end(), p, p+4);
        finalBytes.insert(finalBytes.end(), encrypted.begin(), encrypted.end());
    } else {
        finalBytes = std::move(content);
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return std::unexpected("SHN konnte nicht geschrieben werden: " + path.string());
    out.write(reinterpret_cast<const char*>(finalBytes.data()), static_cast<std::streamsize>(finalBytes.size()));
    if (!out) return std::unexpected("Fehler beim Schreiben der SHN-Datei.");
    return {};
}

std::string ShnValueToString(const ShnValue& value) {
    return std::visit([](const auto& v) -> std::string {
        using T=std::decay_t<decltype(v)>;
        std::ostringstream s;
        if constexpr (std::is_same_v<T,std::string>) return v;
        else if constexpr (std::is_same_v<T,std::vector<std::uint8_t>>) return Hex(v);
        else if constexpr (std::is_same_v<T,std::pair<std::uint32_t,std::uint32_t>>) { s<<v.first<<":"<<v.second; return s.str(); }
        else if constexpr (std::is_same_v<T,float>) { s<<std::setprecision(9)<<v; return s.str(); }
        else { s<<+v; return s.str(); }
    }, value);
}

std::expected<ShnValue, std::string> ParseShnValue(const ShnColumn& c, const std::string& text) {
    try {
        switch(c.kind) {
            case ShnValueKind::UInt8:{auto v=ParseUnsigned(text);if(!v||*v>255)return std::unexpected("UInt8 außerhalb des Bereichs");return ShnValue(static_cast<std::uint8_t>(*v));}
            case ShnValueKind::UInt16:{auto v=ParseUnsigned(text);if(!v||*v>65535)return std::unexpected("UInt16 außerhalb des Bereichs");return ShnValue(static_cast<std::uint16_t>(*v));}
            case ShnValueKind::UInt32:{auto v=ParseUnsigned(text);if(!v||*v>0xffffffffULL)return std::unexpected("UInt32 außerhalb des Bereichs");return ShnValue(static_cast<std::uint32_t>(*v));}
            case ShnValueKind::Int8:{auto v=ParseSigned(text);if(!v||*v<-128||*v>127)return std::unexpected("Int8 außerhalb des Bereichs");return ShnValue(static_cast<std::int8_t>(*v));}
            case ShnValueKind::Int16:{auto v=ParseSigned(text);if(!v||*v<-32768||*v>32767)return std::unexpected("Int16 außerhalb des Bereichs");return ShnValue(static_cast<std::int16_t>(*v));}
            case ShnValueKind::Int32:{auto v=ParseSigned(text);if(!v||*v<std::numeric_limits<std::int32_t>::min()||*v>std::numeric_limits<std::int32_t>::max())return std::unexpected("Int32 außerhalb des Bereichs");return ShnValue(static_cast<std::int32_t>(*v));}
            case ShnValueKind::Float:{std::size_t u=0;float v=std::stof(text,&u);if(u!=text.size())return std::unexpected("Ungültiger Float");return ShnValue(v);}
            case ShnValueKind::String:return ShnValue(text);
            case ShnValueKind::PairUInt32:{auto p=text.find(':');if(p==std::string::npos)return std::unexpected("Pair erwartet Format A:B");auto a=ParseUnsigned(text.substr(0,p));auto b=ParseUnsigned(text.substr(p+1));if(!a||!b||*a>0xffffffffULL||*b>0xffffffffULL)return std::unexpected("Ungültiges Pair");return ShnValue(std::make_pair(static_cast<std::uint32_t>(*a),static_cast<std::uint32_t>(*b)));}
            case ShnValueKind::Raw:{std::vector<std::uint8_t> raw;std::istringstream in(text);std::string token;while(in>>token){auto v=std::stoul(token,nullptr,16);if(v>255)return std::unexpected("Hex-Byte außerhalb des Bereichs");raw.push_back(static_cast<std::uint8_t>(v));}if(raw.size()!=c.length)return std::unexpected("Rohdaten müssen exakt "+std::to_string(c.length)+" Bytes enthalten.");return ShnValue(std::move(raw));}
        }
    } catch (...) { return std::unexpected("Ungültiger Wert."); }
    return std::unexpected("Unbekannter SHN-Typ.");
}

} // namespace theseed::mapeditor::core::legacy
