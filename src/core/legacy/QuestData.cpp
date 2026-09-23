#include "mapeditor/core/legacy/QuestData.hpp"

#include <fstream>
#include <cstring>
#include <algorithm>

namespace theseed::mapeditor::core::legacy {

namespace {

class Reader {
public:
    explicit Reader(const std::vector<std::uint8_t>& data) : data_(data) {}

    bool Ok() const { return ok_; }
    std::size_t Pos() const { return pos_; }

    std::uint8_t U8() { return ReadRaw<std::uint8_t>(); }
    std::uint16_t U16() { return ReadRaw<std::uint16_t>(); }
    std::uint32_t U32() { return ReadRaw<std::uint32_t>(); }

    std::vector<std::uint8_t> Bytes(std::size_t n) {
        if (!ok_ || pos_ + n > data_.size()) { ok_ = false; return {}; }
        std::vector<std::uint8_t> out(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
                                       data_.begin() + static_cast<std::ptrdiff_t>(pos_ + n));
        pos_ += n;
        return out;
    }

    template <std::size_t N>
    std::array<std::uint8_t, N> BytesArray() {
        std::array<std::uint8_t, N> out{};
        auto v = Bytes(N);
        if (v.size() == N) std::copy(v.begin(), v.end(), out.begin());
        return out;
    }

    // Liest bis zum nächsten NUL-Byte (exklusiv) und überspringt das NUL selbst - entspricht
    // read_nul() im Referenzparser.
    std::string ReadNulString() {
        if (!ok_) return {};
        auto it = std::find(data_.begin() + static_cast<std::ptrdiff_t>(pos_), data_.end(), std::uint8_t{0});
        if (it == data_.end()) { ok_ = false; return {}; }
        std::string s(reinterpret_cast<const char*>(&data_[pos_]), static_cast<std::size_t>(it - (data_.begin() + static_cast<std::ptrdiff_t>(pos_))));
        pos_ = static_cast<std::size_t>(it - data_.begin()) + 1;
        return s;
    }

private:
    template <typename T>
    T ReadRaw() {
        if (!ok_ || pos_ + sizeof(T) > data_.size()) { ok_ = false; return T{}; }
        T v;
        std::memcpy(&v, &data_[pos_], sizeof(T));
        pos_ += sizeof(T);
        return v;
    }

    const std::vector<std::uint8_t>& data_;
    std::size_t pos_ = 0;
    bool ok_ = true;
};

} // namespace

std::expected<QuestDataFile, std::string> LoadQuestData(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::unexpected("Datei konnte nicht geöffnet werden: " + path.string());
    std::vector<std::uint8_t> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (buf.size() < 4) return std::unexpected("Datei ist zu klein.");

    Reader r(buf);
    QuestDataFile file;
    file.header = r.U16();
    if (file.header != 0x0006) {
        return std::unexpected("Unerwarteter QuestData-Kopf (0x" +
                                std::to_string(file.header) + "), erwartet 0x0006.");
    }
    const std::uint16_t count = r.U16();
    file.records.reserve(count);

    for (std::uint16_t index = 0; index < count; ++index) {
        const std::size_t recordStart = r.Pos();
        QuestRecord rec;
        rec.dataLen = r.U32();

        rec.id = r.U16(); rec.title = r.U16(); rec.description = r.U16();
        rec.unk1 = r.U8();
        rec.questGrade = r.U8(); rec.multiQuest = r.U8(); rec.dailyQuest = r.U8();
        rec.unk2 = r.U16();
        rec.enableQuest = r.U8(); rec.instAcc = r.U8(); rec.needLevel = r.U8();
        rec.minLevel = r.U8(); rec.maxLevel = r.U8(); rec.needNpc = r.U8();
        rec.startingNpc = r.U16();
        rec.needItem = r.U8();
        rec.unk3 = r.U8();
        rec.itemId = r.U16();
        rec.itemVanish = r.U8();
        rec.unk4 = r.BytesArray<19>();
        rec.needPred = r.U8();
        rec.unk5 = r.U8();
        rec.predecessor = r.U16();
        rec.unk6 = r.U16();
        rec.needClass = r.U8(); rec.classType = r.U8();
        rec.unk7 = r.BytesArray<24>();
        rec.instHand = r.U8();
        rec.unk8 = r.BytesArray<3>();

        for (auto& m : rec.mobs) {
            m.active = r.U8(); m.isMob = r.U8();
            m.id = r.U16();
            m.hasToBeKilled = r.U8(); m.amount = r.U8(); m.unk1 = r.U8(); m.unk2 = r.U8();
        }
        for (auto& it : rec.items) {
            it.active = r.U8(); it.type = r.U8();
            it.id = r.U16(); it.amount = r.U16();
        }

        const std::uint32_t dropCount = r.U32();
        rec.drops.reserve(dropCount);
        for (std::uint32_t i = 0; i < dropCount; ++i) {
            QuestDrop d;
            d.active = r.U32(); d.mobId = r.U32(); d.amount = r.U32(); d.itemId = r.U32();
            d.rate = r.U32(); d.unk1 = r.U32(); d.unk2 = r.U32();
            rec.drops.push_back(d);
        }
        const std::size_t paddingLen = 28u * (11u - dropCount) + 12u;
        rec.itemDropPadding = r.Bytes(paddingLen);

        rec.rewardsRaw = r.Bytes(144);
        rec.extra8 = r.BytesArray<8>();

        const std::uint16_t startLen = r.U16();
        const std::uint16_t finishLen = r.U16();
        const std::uint16_t actionLen = r.U16();
        rec.rewardData = r.BytesArray<14>();

        // Physische Reihenfolge Start/Action/Finish (siehe Header-Kommentar in QuestData.hpp) -
        // die gespeicherten Längen (Start/Finish/Action-Reihenfolge) dienen hier nur als
        // Cross-Check, nicht zum Springen (read_nul liest ohnehin bis zum echten NUL-Byte).
        rec.start.text = r.ReadNulString(); rec.start.storedLength = startLen;
        rec.action.text = r.ReadNulString(); rec.action.storedLength = actionLen;
        rec.finish.text = r.ReadNulString(); rec.finish.storedLength = finishLen;

        if (!r.Ok()) {
            return std::unexpected("Unerwartetes Dateiende beim Parsen von Quest-Index " + std::to_string(index));
        }
        const std::size_t expectedEnd = recordStart + rec.dataLen;
        if (r.Pos() != expectedEnd) {
            return std::unexpected("Quest " + std::to_string(index) + " (ID " + std::to_string(rec.id) +
                                    ") nicht ausgerichtet: 0x" + std::to_string(r.Pos()) +
                                    " != 0x" + std::to_string(expectedEnd));
        }
        file.records.push_back(std::move(rec));
    }

    if (r.Pos() != buf.size()) {
        return std::unexpected("Restdaten übrig: Parser endete bei 0x" + std::to_string(r.Pos()) +
                                ", Datei ist 0x" + std::to_string(buf.size()) + " groß.");
    }
    return file;
}

namespace {
void AppendU8(std::vector<std::uint8_t>& out, std::uint8_t v) { out.push_back(v); }
void AppendU16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
void AppendU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}
template <std::size_t N>
void AppendArray(std::vector<std::uint8_t>& out, const std::array<std::uint8_t, N>& a) {
    out.insert(out.end(), a.begin(), a.end());
}
void AppendBytes(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& b) {
    out.insert(out.end(), b.begin(), b.end());
}
void AppendNulString(std::vector<std::uint8_t>& out, const std::string& s) {
    out.insert(out.end(), s.begin(), s.end());
    out.push_back(0);
}
} // namespace

std::expected<void, std::string> SaveQuestData(const QuestDataFile& file, const std::filesystem::path& path) {
    std::vector<std::uint8_t> out;
    AppendU16(out, file.header);
    AppendU16(out, static_cast<std::uint16_t>(file.records.size()));

    for (const auto& rec : file.records) {
        std::vector<std::uint8_t> body; // alles NACH dataLen selbst

        AppendU16(body, rec.id); AppendU16(body, rec.title); AppendU16(body, rec.description);
        AppendU8(body, rec.unk1);
        AppendU8(body, rec.questGrade); AppendU8(body, rec.multiQuest); AppendU8(body, rec.dailyQuest);
        AppendU16(body, rec.unk2);
        AppendU8(body, rec.enableQuest); AppendU8(body, rec.instAcc); AppendU8(body, rec.needLevel);
        AppendU8(body, rec.minLevel); AppendU8(body, rec.maxLevel); AppendU8(body, rec.needNpc);
        AppendU16(body, rec.startingNpc);
        AppendU8(body, rec.needItem);
        AppendU8(body, rec.unk3);
        AppendU16(body, rec.itemId);
        AppendU8(body, rec.itemVanish);
        AppendArray(body, rec.unk4);
        AppendU8(body, rec.needPred);
        AppendU8(body, rec.unk5);
        AppendU16(body, rec.predecessor);
        AppendU16(body, rec.unk6);
        AppendU8(body, rec.needClass); AppendU8(body, rec.classType);
        AppendArray(body, rec.unk7);
        AppendU8(body, rec.instHand);
        AppendArray(body, rec.unk8);

        for (const auto& m : rec.mobs) {
            AppendU8(body, m.active); AppendU8(body, m.isMob);
            AppendU16(body, m.id);
            AppendU8(body, m.hasToBeKilled); AppendU8(body, m.amount); AppendU8(body, m.unk1); AppendU8(body, m.unk2);
        }
        for (const auto& it : rec.items) {
            AppendU8(body, it.active); AppendU8(body, it.type);
            AppendU16(body, it.id); AppendU16(body, it.amount);
        }

        AppendU32(body, static_cast<std::uint32_t>(rec.drops.size()));
        for (const auto& d : rec.drops) {
            AppendU32(body, d.active); AppendU32(body, d.mobId); AppendU32(body, d.amount);
            AppendU32(body, d.itemId); AppendU32(body, d.rate); AppendU32(body, d.unk1); AppendU32(body, d.unk2);
        }
        // Padding-Länge MUSS zur (evtl. geänderten) Drop-Anzahl passen - siehe LoadQuestData.
        // Vorhandene Bytes so weit wie möglich beibehalten (unbekannter Inhalt, kein reines
        // Füllmaterial), Rest mit 0 auffüllen bzw. abschneiden.
        const std::size_t neededPadding = 28u * (11u - rec.drops.size()) + 12u;
        std::vector<std::uint8_t> padding = rec.itemDropPadding;
        padding.resize(neededPadding, 0);
        AppendBytes(body, padding);

        std::vector<std::uint8_t> rewards = rec.rewardsRaw;
        rewards.resize(144, 0);
        AppendBytes(body, rewards);
        AppendArray(body, rec.extra8);

        // Skript-Längen inkl. NUL-Terminator neu berechnen (nicht die evtl. veralteten
        // gespeicherten storedLength-Werte übernehmen) - Reihenfolge im Kopf ist Start/Finish/
        // Action, siehe LoadQuestData.
        AppendU16(body, static_cast<std::uint16_t>(rec.start.text.size() + 1));
        AppendU16(body, static_cast<std::uint16_t>(rec.finish.text.size() + 1));
        AppendU16(body, static_cast<std::uint16_t>(rec.action.text.size() + 1));
        AppendArray(body, rec.rewardData);

        AppendNulString(body, rec.start.text);
        AppendNulString(body, rec.action.text);
        AppendNulString(body, rec.finish.text);

        const std::uint32_t dataLen = static_cast<std::uint32_t>(body.size() + 4); // +4 für dataLen selbst
        AppendU32(out, dataLen);
        AppendBytes(out, body);
    }

    std::ofstream f(path, std::ios::binary);
    if (!f) return std::unexpected("Datei konnte nicht zum Schreiben geöffnet werden: " + path.string());
    f.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return {};
}

} // namespace theseed::mapeditor::core::legacy
