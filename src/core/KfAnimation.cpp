#include "mapeditor/core/KfAnimation.hpp"

#include <algorithm>
#include <bit>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <variant>

namespace theseed::mapeditor::core {
namespace {

constexpr std::size_t kMaxFileBytes = 128u * 1024u * 1024u;
constexpr std::size_t kMaxElements = 4u * 1024u * 1024u;
constexpr std::uint32_t kFiestaKfVersion = 0x14000004u; // 20.0.0.4
constexpr std::uint32_t kInvalidOffset = 0xFFFFFFFFu;

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::size_t Position() const noexcept { return pos_; }
    [[nodiscard]] std::size_t Remaining() const noexcept { return bytes_.size() - pos_; }

    [[noreturn]] void Fail(const std::string& message) const {
        throw std::runtime_error("KF byte " + std::to_string(pos_) + ": " + message);
    }

    void Require(std::size_t count) const {
        if (count > bytes_.size() - pos_) Fail("unexpected end of file");
    }

    std::uint8_t U8() {
        Require(1);
        return bytes_[pos_++];
    }

    std::uint16_t U16() {
        Require(2);
        const std::uint16_t value =
            static_cast<std::uint16_t>(bytes_[pos_]) |
            (static_cast<std::uint16_t>(bytes_[pos_ + 1]) << 8u);
        pos_ += 2;
        return value;
    }

    std::int16_t I16() { return std::bit_cast<std::int16_t>(U16()); }

    std::uint32_t U32() {
        Require(4);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= static_cast<std::uint32_t>(bytes_[pos_++]) << (8u * i);
        return value;
    }

    std::int32_t I32() { return std::bit_cast<std::int32_t>(U32()); }
    float F32() { return std::bit_cast<float>(U32()); }

    std::uint32_t Count(std::size_t minimumBytes = 0) {
        const std::uint32_t count = U32();
        if (count > kMaxElements) Fail("implausibly large element count");
        if (minimumBytes != 0 && static_cast<std::size_t>(count) > Remaining() / minimumBytes)
            Fail("element count exceeds remaining file");
        return count;
    }

    std::string String() {
        const std::uint32_t length = Count(1);
        Require(length);
        std::string result(reinterpret_cast<const char*>(bytes_.data() + pos_), length);
        pos_ += length;
        return result;
    }

    std::string HeaderLine() {
        const std::size_t begin = pos_;
        while (pos_ < bytes_.size() && bytes_[pos_] != '\n') ++pos_;
        if (pos_ >= bytes_.size()) Fail("missing header newline");
        std::string line(reinterpret_cast<const char*>(bytes_.data() + begin), pos_ - begin);
        ++pos_;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return line;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t pos_ = 0;
};

KfVec3 ReadVec3(Reader& r) {
    return {r.F32(), r.F32(), r.F32()};
}

KfQuat ReadQuat(Reader& r) {
    return {r.F32(), r.F32(), r.F32(), r.F32()};
}

KfTransform ReadTransform(Reader& r) {
    KfTransform result;
    result.translation = ReadVec3(r);
    result.rotation = ReadQuat(r);
    result.scale = r.F32();
    return result;
}

void ValidateInterpolation(Reader& r, std::uint32_t interpolation, bool allowXyz = false) {
    if (interpolation == 1 || interpolation == 2 || interpolation == 3 || interpolation == 5)
        return;
    if (allowXyz && interpolation == 4) return;
    r.Fail("unsupported key interpolation type " + std::to_string(interpolation));
}

KfKeyGroupFloat ReadFloatKeyGroup(Reader& r) {
    KfKeyGroupFloat group;
    const std::uint32_t count = r.Count(8);
    if (count == 0) return group;

    group.interpolation = r.U32();
    ValidateInterpolation(r, group.interpolation);
    group.keys.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        KfFloatKey key;
        key.time = r.F32();
        key.value = r.F32();
        if (group.interpolation == 2) {
            key.forward = r.F32();
            key.backward = r.F32();
        } else if (group.interpolation == 3) {
            key.tension = r.F32();
            key.bias = r.F32();
            key.continuity = r.F32();
        }
        group.keys.push_back(key);
    }
    return group;
}

KfKeyGroupVec3 ReadVec3KeyGroup(Reader& r) {
    KfKeyGroupVec3 group;
    const std::uint32_t count = r.Count(16);
    if (count == 0) return group;

    group.interpolation = r.U32();
    ValidateInterpolation(r, group.interpolation);
    group.keys.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        KfVec3Key key;
        key.time = r.F32();
        key.value = ReadVec3(r);
        if (group.interpolation == 2) {
            key.forward = ReadVec3(r);
            key.backward = ReadVec3(r);
        } else if (group.interpolation == 3) {
            key.tension = r.F32();
            key.bias = r.F32();
            key.continuity = r.F32();
        }
        group.keys.push_back(key);
    }
    return group;
}

struct RawControlledBlock {
    std::int32_t interpolator = -1;
    std::int32_t controller = -1;
    std::int32_t palette = -1;
    std::uint32_t nodeOffset = kInvalidOffset;
    std::uint32_t propertyOffset = kInvalidOffset;
    std::uint32_t controllerTypeOffset = kInvalidOffset;
    std::uint32_t controllerIdOffset = kInvalidOffset;
    std::uint32_t interpolatorIdOffset = kInvalidOffset;
};

struct RawSequence {
    std::string name;
    std::vector<RawControlledBlock> controlled;
    float weight = 1.0f;
    std::int32_t textKeys = -1;
    std::uint32_t cycleType = 0;
    float frequency = 1.0f;
    float startTime = 0.0f;
    float stopTime = 0.0f;
    std::int32_t manager = -1;
    std::string accumulationRoot;
    std::int32_t palette = -1;
};

struct RawPalette {
    std::string data;
};

struct RawTransformInterpolator {
    KfTransform pose;
    std::int32_t data = -1;
};

struct RawTransformData {
    KfTransformKeys keys;
};

struct RawSplineTransform {
    KfTransform pose;
    KfSplineTransform spline;
};

struct RawTextKeys {
    std::vector<KfTextKey> keys;
};

struct RawSplineData {
    KfSplineDataBlock data;
};

struct RawSplineBasis {
    KfSplineBasisBlock data;
};

using Block = std::variant<std::monostate,
                           RawSequence,
                           RawPalette,
                           RawTransformInterpolator,
                           RawTransformData,
                           RawSplineTransform,
                           RawTextKeys,
                           RawSplineData,
                           RawSplineBasis>;

RawSequence ReadControllerSequence(Reader& r) {
    RawSequence result;
    result.name = r.String();

    const std::uint32_t count = r.Count(32);
    (void)r.U32(); // Array Grow By
    result.controlled.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        RawControlledBlock block;
        block.interpolator = r.I32();
        block.controller = r.I32();
        block.palette = r.I32();
        block.nodeOffset = r.U32();
        block.propertyOffset = r.U32();
        block.controllerTypeOffset = r.U32();
        block.controllerIdOffset = r.U32();
        block.interpolatorIdOffset = r.U32();
        result.controlled.push_back(block);
    }

    result.weight = r.F32();
    result.textKeys = r.I32();
    result.cycleType = r.U32();
    if (result.cycleType > 2) r.Fail("unsupported cycle type");
    result.frequency = r.F32();
    result.startTime = r.F32();
    result.stopTime = r.F32();
    result.manager = r.I32();
    result.accumulationRoot = r.String();
    result.palette = r.I32();
    return result;
}

RawTransformData ReadTransformData(Reader& r) {
    RawTransformData result;
    auto& keys = result.keys;

    const std::uint32_t rotationCount = r.Count(20);
    if (rotationCount != 0) {
        keys.rotationType = r.U32();
        ValidateInterpolation(r, keys.rotationType, true);
        if (keys.rotationType == 4) {
            for (auto& axis : keys.xyzRotation) axis = ReadFloatKeyGroup(r);
        } else {
            keys.quaternionRotation.reserve(rotationCount);
            for (std::uint32_t i = 0; i < rotationCount; ++i) {
                KfQuatKey key;
                key.time = r.F32();
                key.value = ReadQuat(r);
                if (keys.rotationType == 3) {
                    key.tension = r.F32();
                    key.bias = r.F32();
                    key.continuity = r.F32();
                }
                keys.quaternionRotation.push_back(key);
            }
        }
    }

    keys.translation = ReadVec3KeyGroup(r);
    keys.scale = ReadFloatKeyGroup(r);
    return result;
}

RawSplineTransform ReadSplineTransform(Reader& r) {
    RawSplineTransform result;
    result.spline.startTime = r.F32();
    result.spline.stopTime = r.F32();
    result.spline.dataBlock = r.I32();
    result.spline.basisBlock = r.I32();
    result.pose = ReadTransform(r);
    result.spline.translationHandle = r.U32();
    result.spline.rotationHandle = r.U32();
    result.spline.scaleHandle = r.U32();
    result.spline.translationOffset = r.F32();
    result.spline.translationHalfRange = r.F32();
    result.spline.rotationOffset = r.F32();
    result.spline.rotationHalfRange = r.F32();
    result.spline.scaleOffset = r.F32();
    result.spline.scaleHalfRange = r.F32();
    return result;
}

void SkipFloatData(Reader& r) {
    (void)ReadFloatKeyGroup(r);
}

void SkipFloatInterpolator(Reader& r) {
    (void)r.F32();
    (void)r.I32();
}

void SkipBoolInterpolator(Reader& r) {
    (void)r.U8();
    (void)r.I32();
}

void SkipSplineFloatInterpolator(Reader& r) {
    (void)r.F32(); // start
    (void)r.F32(); // stop
    (void)r.I32(); // data
    (void)r.I32(); // basis
    (void)r.F32(); // value
    (void)r.U32(); // handle
    (void)r.F32(); // offset
    (void)r.F32(); // half range
}

RawTextKeys ReadTextKeys(Reader& r) {
    RawTextKeys result;
    (void)r.String(); // NiExtraData::Name
    const std::uint32_t count = r.Count(8);
    result.keys.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
        result.keys.push_back({r.F32(), r.String()});
    return result;
}

std::string PaletteString(Reader& r, const std::string& palette, std::uint32_t offset) {
    if (offset == kInvalidOffset) return {};
    if (offset >= palette.size()) r.Fail("string palette offset outside palette");
    std::size_t end = offset;
    while (end < palette.size() && palette[end] != '\0') ++end;
    if (end == palette.size()) r.Fail("unterminated string in palette");
    return palette.substr(offset, end - offset);
}

template <typename T>
const T* BlockAs(const std::vector<Block>& blocks, std::int32_t ref) {
    if (ref < 0 || static_cast<std::size_t>(ref) >= blocks.size()) return nullptr;
    return std::get_if<T>(&blocks[static_cast<std::size_t>(ref)]);
}

} // namespace

std::expected<KfAnimationFile, std::string> DecodeKfAnimation(std::span<const std::uint8_t> bytes) {
    try {
        if (bytes.empty()) return std::unexpected("KF is empty");
        if (bytes.size() > kMaxFileBytes) return std::unexpected("KF exceeds 128 MiB");

        Reader r(bytes);
        const std::string header = r.HeaderLine();
        if (header != "Gamebryo File Format, Version 20.0.0.4")
            return std::unexpected("Unsupported KF header/version: " + header);

        KfAnimationFile result;
        result.version = r.U32();
        if (result.version != kFiestaKfVersion)
            return std::unexpected("KF binary version is not 20.0.0.4");

        const std::uint8_t endian = r.U8();
        if (endian != 1) return std::unexpected("Big-endian KF files are not supported");

        (void)r.U32(); // user version
        const std::uint32_t numBlocks = r.Count(2);
        const std::uint16_t numTypes = r.U16();
        if (numTypes == 0 || numTypes > 2048)
            return std::unexpected("Invalid KF block type count");

        std::vector<std::string> typeTable;
        typeTable.reserve(numTypes);
        for (std::uint16_t i = 0; i < numTypes; ++i) typeTable.push_back(r.String());

        std::vector<std::uint16_t> typeIndices;
        typeIndices.reserve(numBlocks);
        for (std::uint32_t i = 0; i < numBlocks; ++i) {
            const std::uint16_t type = r.U16();
            if (type >= typeTable.size()) r.Fail("block type index outside type table");
            typeIndices.push_back(type);
            result.blockTypes.push_back(typeTable[type]);
        }

        const std::uint32_t numGroups = r.Count(4);
        for (std::uint32_t i = 0; i < numGroups; ++i) (void)r.U32();

        std::vector<Block> blocks(numBlocks);
        std::int32_t sequenceBlock = -1;

        for (std::uint32_t i = 0; i < numBlocks; ++i) {
            const std::string& type = result.blockTypes[i];
            if (type == "NiControllerSequence") {
                if (sequenceBlock >= 0) r.Fail("multiple NiControllerSequence roots are not supported yet");
                blocks[i] = ReadControllerSequence(r);
                sequenceBlock = static_cast<std::int32_t>(i);
            } else if (type == "NiStringPalette") {
                RawPalette palette;
                palette.data = r.String();
                const std::uint32_t repeatedLength = r.U32();
                if (repeatedLength != palette.data.size())
                    r.Fail("NiStringPalette repeated length does not match payload");
                blocks[i] = std::move(palette);
            } else if (type == "NiTransformInterpolator") {
                RawTransformInterpolator interp;
                interp.pose = ReadTransform(r);
                interp.data = r.I32();
                blocks[i] = interp;
            } else if (type == "NiTransformData") {
                blocks[i] = ReadTransformData(r);
            } else if (type == "NiBSplineCompTransformInterpolator") {
                blocks[i] = ReadSplineTransform(r);
            } else if (type == "NiBSplineData") {
                RawSplineData spline;
                spline.data.blockIndex = static_cast<std::int32_t>(i);
                const std::uint32_t floatCount = r.Count(4);
                spline.data.floatControlPoints.reserve(floatCount);
                for (std::uint32_t n = 0; n < floatCount; ++n)
                    spline.data.floatControlPoints.push_back(r.F32());
                const std::uint32_t compactCount = r.Count(2);
                spline.data.compactControlPoints.reserve(compactCount);
                for (std::uint32_t n = 0; n < compactCount; ++n)
                    spline.data.compactControlPoints.push_back(r.I16());
                blocks[i] = spline;
                result.splineData.push_back(spline.data);
            } else if (type == "NiBSplineBasisData") {
                RawSplineBasis basis;
                basis.data.blockIndex = static_cast<std::int32_t>(i);
                basis.data.controlPointCount = r.U32();
                if (basis.data.controlPointCount > kMaxElements)
                    r.Fail("implausibly large B-spline control point count");
                blocks[i] = basis;
                result.splineBases.push_back(basis.data);
            } else if (type == "NiBSplineCompFloatInterpolator") {
                SkipSplineFloatInterpolator(r);
            } else if (type == "NiFloatInterpolator") {
                SkipFloatInterpolator(r);
            } else if (type == "NiBoolInterpolator" || type == "NiBoolTimelineInterpolator") {
                SkipBoolInterpolator(r);
            } else if (type == "NiFloatData") {
                SkipFloatData(r);
            } else if (type == "NiTextKeyExtraData") {
                blocks[i] = ReadTextKeys(r);
            } else {
                return std::unexpected("Unsupported KF block type '" + type +
                                       "' at block " + std::to_string(i));
            }
        }

        // NIF footer: number of roots followed by block references.
        const std::uint32_t numRoots = r.Count(4);
        bool sequenceIsRoot = false;
        for (std::uint32_t i = 0; i < numRoots; ++i) {
            const std::int32_t root = r.I32();
            if (root == sequenceBlock) sequenceIsRoot = true;
            if (root < 0 || static_cast<std::uint32_t>(root) >= numBlocks)
                r.Fail("footer root reference outside block table");
        }
        if (r.Remaining() != 0) r.Fail("unexpected trailing bytes");
        if (sequenceBlock < 0) return std::unexpected("KF contains no NiControllerSequence");
        if (!sequenceIsRoot) return std::unexpected("NiControllerSequence is not a KF root");

        const auto* rawSequence = BlockAs<RawSequence>(blocks, sequenceBlock);
        if (!rawSequence) return std::unexpected("KF sequence block could not be decoded");

        result.sequence.name = rawSequence->name;
        result.sequence.accumulationRoot = rawSequence->accumulationRoot;
        result.sequence.weight = rawSequence->weight;
        result.sequence.cycleType = rawSequence->cycleType;
        result.sequence.frequency = rawSequence->frequency;
        result.sequence.startTime = rawSequence->startTime;
        result.sequence.stopTime = rawSequence->stopTime;
        result.sequence.controlledBlockCount = rawSequence->controlled.size();

        if (const auto* textKeys = BlockAs<RawTextKeys>(blocks, rawSequence->textKeys))
            result.sequence.textKeys = textKeys->keys;
        else if (rawSequence->textKeys >= 0)
            return std::unexpected("KF Text Keys reference does not point to NiTextKeyExtraData");

        for (const auto& controlled : rawSequence->controlled) {
            if (controlled.interpolator < 0 ||
                static_cast<std::size_t>(controlled.interpolator) >= blocks.size())
                return std::unexpected("Controlled KF block has invalid interpolator reference");

            const RawPalette* palette = BlockAs<RawPalette>(blocks, controlled.palette);
            if (!palette && controlled.palette >= 0)
                return std::unexpected("Controlled KF block palette reference is not NiStringPalette");

            auto readName = [&](std::uint32_t offset) {
                return palette ? PaletteString(r, palette->data, offset) : std::string{};
            };

            KfControlledTrack track;
            track.nodeName = readName(controlled.nodeOffset);
            track.propertyType = readName(controlled.propertyOffset);
            track.controllerType = readName(controlled.controllerTypeOffset);
            track.controllerId = readName(controlled.controllerIdOffset);
            track.interpolatorId = readName(controlled.interpolatorIdOffset);
            track.interpolatorBlock = controlled.interpolator;

            if (const auto* interp = BlockAs<RawTransformInterpolator>(blocks, controlled.interpolator)) {
                track.pose = interp->pose;
                if (interp->data >= 0) {
                    const auto* data = BlockAs<RawTransformData>(blocks, interp->data);
                    if (!data)
                        return std::unexpected("NiTransformInterpolator data reference is not NiTransformData");
                    track.keys = data->keys;
                }
                result.sequence.transformTracks.push_back(std::move(track));
            } else if (const auto* interp = BlockAs<RawSplineTransform>(blocks, controlled.interpolator)) {
                track.pose = interp->pose;
                track.compressedSpline = true;
                track.spline = interp->spline;
                if (!BlockAs<RawSplineData>(blocks, track.spline.dataBlock))
                    return std::unexpected("B-spline transform data reference is not NiBSplineData");
                if (!BlockAs<RawSplineBasis>(blocks, track.spline.basisBlock))
                    return std::unexpected("B-spline transform basis reference is not NiBSplineBasisData");
                result.sequence.transformTracks.push_back(std::move(track));
            }
            // Float/Bool controller blocks are intentionally parsed for byte-accurate traversal,
            // but are not skeletal transform tracks and therefore do not enter transformTracks.
        }

        return result;
    } catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<KfAnimationFile, std::string> LoadKfAnimation(const std::filesystem::path& path) {
    try {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in) return std::unexpected("Cannot open KF: " + path.string());

        const auto end = in.tellg();
        if (end < 0 || static_cast<std::uint64_t>(end) > kMaxFileBytes)
            return std::unexpected("Invalid KF size (limit 128 MiB)");

        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
        in.seekg(0);
        if (!bytes.empty() &&
            !in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
            return std::unexpected("Cannot read KF: " + path.string());

        return DecodeKfAnimation(bytes);
    } catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}


namespace {

float ClampTrackTime(const KfAnimationFile& file, float time) {
    if (!std::isfinite(time)) return file.sequence.startTime;
    const float lo = std::min(file.sequence.startTime, file.sequence.stopTime);
    const float hi = std::max(file.sequence.startTime, file.sequence.stopTime);
    return std::clamp(time, lo, hi);
}

template <class Key>
std::pair<const Key*, const Key*> BracketKeys(const std::vector<Key>& keys, float time) {
    if (keys.empty()) return {nullptr, nullptr};
    if (time <= keys.front().time) return {&keys.front(), &keys.front()};
    if (time >= keys.back().time) return {&keys.back(), &keys.back()};
    const auto it = std::upper_bound(keys.begin(), keys.end(), time,
        [](float t, const Key& key) { return t < key.time; });
    if (it == keys.begin()) return {&*it, &*it};
    return {&*(it - 1), &*it};
}

float SegmentAlpha(float time, float a, float b) {
    const float span = b - a;
    if (std::abs(span) < 1.0e-8f) return 0.0f;
    return std::clamp((time - a) / span, 0.0f, 1.0f);
}

std::expected<float, std::string>
SampleFloatGroup(const KfKeyGroupFloat& group, float time, const char* channel) {
    if (group.keys.empty()) return std::unexpected(std::string(channel) + ": no keys");
    if (group.interpolation != 1 && group.interpolation != 5) {
        return std::unexpected(std::string(channel) +
            ": interpolation type " + std::to_string(group.interpolation) +
            " is not yet verified for playback");
    }
    const auto [a,b] = BracketKeys(group.keys,time);
    if (!a || !b) return std::unexpected(std::string(channel) + ": invalid key range");
    if (a == b || group.interpolation == 5) return a->value;
    const float t=SegmentAlpha(time,a->time,b->time);
    return a->value + (b->value-a->value)*t;
}

std::expected<KfVec3, std::string>
SampleVec3Group(const KfKeyGroupVec3& group, float time, const char* channel) {
    if (group.keys.empty()) return std::unexpected(std::string(channel) + ": no keys");
    if (group.interpolation != 1 && group.interpolation != 5) {
        return std::unexpected(std::string(channel) +
            ": interpolation type " + std::to_string(group.interpolation) +
            " is not yet verified for playback");
    }
    const auto [a,b] = BracketKeys(group.keys,time);
    if (!a || !b) return std::unexpected(std::string(channel) + ": invalid key range");
    if (a == b || group.interpolation == 5) return a->value;
    const float t=SegmentAlpha(time,a->time,b->time);
    return KfVec3{
        a->value.x+(b->value.x-a->value.x)*t,
        a->value.y+(b->value.y-a->value.y)*t,
        a->value.z+(b->value.z-a->value.z)*t
    };
}

KfQuat NormalizeQuat(KfQuat q) {
    const float n=std::sqrt(q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z);
    if (n < 1.0e-8f) return {};
    q.w/=n; q.x/=n; q.y/=n; q.z/=n;
    return q;
}

KfQuat MulQuat(const KfQuat& a,const KfQuat& b) {
    return NormalizeQuat({
        a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,
        a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w
    });
}

KfQuat AxisQuat(float x,float y,float z,float angle) {
    const float half=angle*0.5f;
    const float sn=std::sin(half);
    return NormalizeQuat({std::cos(half),x*sn,y*sn,z*sn});
}

KfQuat SlerpQuat(KfQuat a,KfQuat b,float t) {
    a=NormalizeQuat(a); b=NormalizeQuat(b);
    float dot=a.w*b.w+a.x*b.x+a.y*b.y+a.z*b.z;
    if(dot<0.0f){dot=-dot;b.w=-b.w;b.x=-b.x;b.y=-b.y;b.z=-b.z;}
    if(dot>0.9995f) {
        return NormalizeQuat({
            a.w+(b.w-a.w)*t,
            a.x+(b.x-a.x)*t,
            a.y+(b.y-a.y)*t,
            a.z+(b.z-a.z)*t
        });
    }
    dot=std::clamp(dot,-1.0f,1.0f);
    const float theta=std::acos(dot);
    const float sn=std::sin(theta);
    if(std::abs(sn)<1.0e-8f) return a;
    const float wa=std::sin((1.0f-t)*theta)/sn;
    const float wb=std::sin(t*theta)/sn;
    return NormalizeQuat({
        a.w*wa+b.w*wb,
        a.x*wa+b.x*wb,
        a.y*wa+b.y*wb,
        a.z*wa+b.z*wb
    });
}

std::expected<KfQuat,std::string>
SampleRotation(const KfTransformKeys& keys,float time,const KfQuat& pose) {
    if(keys.rotationType==0) return pose;
    if(keys.rotationType==4) {
        std::array<float,3> angle{0.0f,0.0f,0.0f};
        for(std::size_t axis=0;axis<3;++axis) {
            if(keys.xyzRotation[axis].keys.empty()) continue;
            auto sampled=SampleFloatGroup(keys.xyzRotation[axis],time,"XYZ rotation");
            if(!sampled) return std::unexpected(sampled.error());
            angle[axis]=*sampled;
        }
        // Gamebryo XYZ rotation keys are applied X, then Y, then Z.
        return MulQuat(MulQuat(AxisQuat(0,0,1,angle[2]),AxisQuat(0,1,0,angle[1])),
                       AxisQuat(1,0,0,angle[0]));
    }
    if(keys.quaternionRotation.empty()) return pose;
    if(keys.rotationType!=1 && keys.rotationType!=5) {
        return std::unexpected("Quaternion rotation interpolation type " +
                               std::to_string(keys.rotationType) +
                               " is not yet verified for playback");
    }
    const auto [a,b]=BracketKeys(keys.quaternionRotation,time);
    if(!a||!b) return pose;
    if(a==b||keys.rotationType==5) return NormalizeQuat(a->value);
    return SlerpQuat(a->value,b->value,SegmentAlpha(time,a->time,b->time));
}

} // namespace

std::expected<KfTransform, std::string>
SampleKfTransformTrack(const KfAnimationFile& file, const KfControlledTrack& track,
                       float sequenceTime) {
    if(track.compressedSpline) {
        return std::unexpected("compressed B-spline sampling is not yet verified for playback");
    }

    const float time=ClampTrackTime(file,sequenceTime);
    KfTransform out=track.pose;

    if(!track.keys.translation.keys.empty()) {
        auto sampled=SampleVec3Group(track.keys.translation,time,"translation");
        if(!sampled) return std::unexpected(track.nodeName+": "+sampled.error());
        out.translation=*sampled;
    }
    if(!track.keys.scale.keys.empty()) {
        auto sampled=SampleFloatGroup(track.keys.scale,time,"scale");
        if(!sampled) return std::unexpected(track.nodeName+": "+sampled.error());
        out.scale=*sampled;
    }
    auto rotation=SampleRotation(track.keys,time,out.rotation);
    if(!rotation) return std::unexpected(track.nodeName+": "+rotation.error());
    out.rotation=*rotation;
    return out;
}

std::expected<std::vector<KfSampledTransform>, std::string>
SampleKfSequence(const KfAnimationFile& file, float sequenceTime) {
    std::vector<KfSampledTransform> out;
    out.reserve(file.sequence.transformTracks.size());
    for(const auto& track:file.sequence.transformTracks) {
        auto sampled=SampleKfTransformTrack(file,track,sequenceTime);
        if(!sampled) return std::unexpected(sampled.error());
        out.push_back({track.nodeName,*sampled});
    }
    return out;
}

} // namespace theseed::mapeditor::core
