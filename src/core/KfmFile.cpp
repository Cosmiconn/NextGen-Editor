#include "mapeditor/core/KfmFile.hpp"
#include "mapeditor/core/legacy/LegacyPathResolve.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace theseed::mapeditor::core {
namespace {
constexpr std::size_t maxFileBytes = 64 * 1024 * 1024;
constexpr std::size_t maxDecodedBytes = 256 * 1024 * 1024;
constexpr std::string_view prefix = ";Gamebryo KFM File Version ";
struct Reader {
    std::span<const std::uint8_t> bytes;
    std::size_t pos = 0, allocation = 0;
    [[noreturn]] void fail(const std::string& reason) const {
        throw std::runtime_error("KFM byte " + std::to_string(pos) + ": " + reason);
    }
    void require(std::size_t n) const { if (n > bytes.size() - pos) fail("truncated data"); }
    void charge(std::size_t n) {
        if (n > maxDecodedBytes - allocation) fail("decoded memory limit exceeded");
        allocation += n;
    }
    std::uint8_t u8() { require(1); return bytes[pos++]; }
    std::uint32_t u32() {
        require(4); std::uint32_t v = 0;
        for (unsigned i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(bytes[pos++]) << (8*i);
        return v;
    }
    std::int32_t i32() { return std::bit_cast<std::int32_t>(u32()); }
    float f32() { return std::bit_cast<float>(u32()); }
    std::size_t count(std::size_t minimumBytes, std::size_t objectBytes) {
        const auto n = i32();
        if (n < 0 || static_cast<std::size_t>(n) > (bytes.size()-pos)/minimumBytes) fail("invalid count");
        if (static_cast<std::size_t>(n) > (maxDecodedBytes-allocation)/objectBytes) fail("decoded memory limit exceeded");
        charge(static_cast<std::size_t>(n)*objectBytes);
        return static_cast<std::size_t>(n);
    }
    std::string string() {
        const auto n = u32(); require(n); charge(n);
        std::string result(reinterpret_cast<const char*>(bytes.data()+pos), n); pos += n; return result;
    }
};
struct Writer {
    std::vector<std::uint8_t> bytes;
    void room(std::size_t n) const { if (n > maxFileBytes-bytes.size()) throw std::runtime_error("KFM exceeds 64 MiB"); }
    void u8(std::uint8_t v) { room(1); bytes.push_back(v); }
    void u32(std::uint32_t v) { room(4); for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(v>>(8*i))); }
    void i32(std::int32_t v) { u32(std::bit_cast<std::uint32_t>(v)); }
    void f32(float v) { u32(std::bit_cast<std::uint32_t>(v)); }
    void count(std::size_t n) {
        if(n>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))throw std::runtime_error("KFM count too large");
        i32(static_cast<std::int32_t>(n));
    }
    void raw(std::string_view s) { room(s.size()); bytes.insert(bytes.end(),s.begin(),s.end()); }
    void string(const std::string& s) { room(s.size()); count(s.size()); raw(s); }
};
}
const char* KfmVersionName(KfmVersion version) {
    switch(version) { case KfmVersion::V1_2_4b:return "1.2.4b"; case KfmVersion::V2_0_0_0b:return "2.0.0.0b"; }
    return "unsupported";
}
std::expected<KfmFile,std::string> DecodeKfm(std::span<const std::uint8_t> bytes) {
    try {
        if(bytes.size()>maxFileBytes)return std::unexpected("KFM exceeds 64 MiB");
        Reader r{bytes}; KfmFile f;
        std::string header;
        while(true) { const auto c=r.u8(); if(c=='\n')break; if(header.size()>=80)r.fail("invalid header"); header+=static_cast<char>(c); }
        if(!header.empty() && header.back()=='\r') { f.crlf=true; header.pop_back(); }
        if(header==std::string(prefix)+"1.2.4b")f.version=KfmVersion::V1_2_4b;
        else if(header==std::string(prefix)+"2.0.0.0b")f.version=KfmVersion::V2_0_0_0b;
        else r.fail("unsupported header/version");
        const bool old=f.version==KfmVersion::V1_2_4b;
        if(!old)f.unknownByte=r.u8();
        f.nifFileName=r.string(); f.master=r.string();
        f.unknownInt1=r.i32(); f.unknownInt2=r.i32();
        f.unknownFloat1=r.f32(); f.unknownFloat2=r.f32();
        const auto n=r.count(old?20:16,sizeof(KfmAnimation)); f.animations.reserve(n);
        for(std::size_t a=0;a<n;++a) {
            KfmAnimation animation; animation.eventCode=r.i32();
            if(old)animation.name=r.string();
            animation.kfFileName=r.string(); animation.index=r.i32();
            const auto nt=r.count(8,sizeof(KfmTransition)); animation.transitions.reserve(nt);
            for(std::size_t t=0;t<nt;++t) {
                KfmTransition transition; transition.eventCode=r.i32(); transition.type=r.i32();
                if(transition.type!=5) {
                    transition.duration=r.f32();
                    // Corpus-verified layout. Historical kfmxml labels/order are
                    // incorrect here; see docs/KFM_FORMAT.md and real fixtures.
                    const auto nk=r.count(8,sizeof(KfmTextKeyPair)); transition.textKeys.reserve(nk);
                    for(std::size_t k=0;k<nk;++k) { KfmTextKeyPair pair; pair.source=r.string();pair.destination=r.string();transition.textKeys.push_back(std::move(pair)); }
                    const auto ni=r.count(8,sizeof(KfmIntermediateAnimation)); transition.intermediateAnimations.reserve(ni);
                    for(std::size_t i=0;i<ni;++i) { KfmIntermediateAnimation mid;mid.eventCode=r.i32();mid.value=r.f32();transition.intermediateAnimations.push_back(mid); }
                }
                animation.transitions.push_back(std::move(transition));
            }
            f.animations.push_back(std::move(animation));
        }
        f.unknownInt3=r.i32();
        if(r.pos!=bytes.size())r.fail("unexpected trailing bytes");
        return f;
    } catch(const std::exception& e) { return std::unexpected(e.what()); }
}
std::expected<std::vector<std::uint8_t>,std::string> EncodeKfm(const KfmFile& f) {
    try {
        if(f.version!=KfmVersion::V1_2_4b && f.version!=KfmVersion::V2_0_0_0b)return std::unexpected("Unsupported KFM version");
        Writer w; const bool old=f.version==KfmVersion::V1_2_4b;
        w.raw(prefix);w.raw(KfmVersionName(f.version));w.raw(f.crlf?"\r\n":"\n");
        if(!old)w.u8(f.unknownByte);
        w.string(f.nifFileName);w.string(f.master);w.i32(f.unknownInt1);w.i32(f.unknownInt2);
        w.f32(f.unknownFloat1);w.f32(f.unknownFloat2);w.count(f.animations.size());
        for(const auto& a:f.animations) {
            w.i32(a.eventCode);
            if(old)w.string(a.name);else if(!a.name.empty())return std::unexpected("KFM 2.0.0.0b cannot store legacy animation names");
            w.string(a.kfFileName);w.i32(a.index);w.count(a.transitions.size());
            for(const auto& t:a.transitions) {
                w.i32(t.eventCode);w.i32(t.type);
                if(t.type==5) {
                    if(std::bit_cast<std::uint32_t>(t.duration)!=0 || !t.textKeys.empty() || !t.intermediateAnimations.empty())
                        return std::unexpected("KFM type 5 cannot store duration, text keys or intermediate animations");
                } else {
                    w.f32(t.duration);w.count(t.textKeys.size());
                    for(const auto& k:t.textKeys){w.string(k.source);w.string(k.destination);}
                    w.count(t.intermediateAnimations.size());
                    for(const auto& i:t.intermediateAnimations){w.i32(i.eventCode);w.f32(i.value);}
                }
            }
        }
        w.i32(f.unknownInt3);return std::move(w.bytes);
    } catch(const std::exception& e) { return std::unexpected(e.what()); }
}
std::expected<KfmFile,std::string> LoadKfmFile(const std::filesystem::path& path) {
    try {
        std::ifstream in(path,std::ios::binary|std::ios::ate);
        if(!in)return std::unexpected("Cannot open KFM: "+path.string());
        const auto size=in.tellg();
        if(size<0 || static_cast<std::uint64_t>(size)>maxFileBytes)return std::unexpected("Invalid KFM size (limit 64 MiB)");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));in.seekg(0);
        if(!in.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))return std::unexpected("Cannot read KFM");
        return DecodeKfm(bytes);
    } catch(const std::exception& e) { return std::unexpected(e.what()); }
}
std::expected<void,std::string> SaveKfmFile(const KfmFile& file,const std::filesystem::path& path) {
    auto bytes=EncodeKfm(file);if(!bytes)return std::unexpected(bytes.error());
    std::ofstream out(path,std::ios::binary|std::ios::noreplace);
    if(!out)return std::unexpected("Cannot create KFM copy (destination must not exist): "+path.string());
    out.write(reinterpret_cast<const char*>(bytes->data()),static_cast<std::streamsize>(bytes->size()));out.close();
    if(!out)return std::unexpected("Cannot finish writing KFM copy: "+path.string());
    return {};
}
KfmReferences InspectKfmReferences(const KfmFile& file,const std::filesystem::path& source) {
    KfmReferences result;
    std::unordered_map<std::string,std::optional<std::filesystem::path>> paths;
    const auto resolve=[&](const std::string& name) {
        if(const auto it=paths.find(name);it!=paths.end())return it->second;
        std::optional<std::filesystem::path> path;
        if(!name.empty() && name.find('\0')==std::string::npos) {
            const auto relative=legacy::LegacyPathToNative(name);
            std::error_code ec;
            // Do not follow absolute paths embedded by an asset author.
            if(!relative.is_absolute() && !relative.has_root_name() && name.find(':')==std::string::npos) {
                try {
                    auto current=source.parent_path();bool found=true;
                    for(const auto& component:relative) {
                        if(std::filesystem::exists(current/component,ec)){current/=component;continue;}
                        std::optional<std::filesystem::path> match;
                        for(const auto& entry:std::filesystem::directory_iterator(current,ec)) {
                            if(legacy::EqualsCaseInsensitive(entry.path().filename().string(),component.string())) {
                                if(match){found=false;break;}match=entry.path();
                            }
                        }
                        if(!found || !match){found=false;break;}current=*match;
                    }
                    if(found)path=current.lexically_normal();
                }
                catch(const std::filesystem::filesystem_error&) {}
                if(path && !std::filesystem::is_regular_file(*path,ec))path.reset();
            }
        }
        paths.emplace(name,path);return path;
    };
    result.nif=resolve(file.nifFileName);
    std::unordered_set<std::int32_t> events;
    result.animations.reserve(file.animations.size());
    for(const auto& a:file.animations) {
        if(!events.insert(a.eventCode).second)++result.duplicateEventCodes;
        auto path=resolve(a.kfFileName);if(!path)++result.missingKfFiles;
        result.animations.push_back(std::move(path));
    }
    for(const auto& a:file.animations)for(const auto& t:a.transitions) {
        if(!events.contains(t.eventCode))++result.missingTransitionTargets;
        for(const auto& i:t.intermediateAnimations)if(!events.contains(i.eventCode))++result.missingIntermediateTargets;
    }
    return result;
}
} // namespace theseed::mapeditor::core
