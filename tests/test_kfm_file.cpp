#include "mapeditor/core/KfmFile.hpp"
#include <cassert>
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>

using namespace theseed::mapeditor::core;
namespace fs=std::filesystem;
namespace {
// Independent on-disk fixtures: non-default index/footer, signed zero, NaN payload,
// direct transition, text-key mapping and intermediate animation in the SAME file.
constexpr const char* modern=
"3b47616d656272796f204b464d2046696c652056657273696f6e20322e302e302e30620a01090000006d6f64656c2e6e696604000000526f6f740100000002000000000000804523c17f020000002a0000000700000069646c652e6b6603000000020000004d000000050000004d00000002000000cdcccc3e0100000003000000656e64050000007374617274010000002a000000000080bf4d0000000600000072756e2e6b6600000000000000007b000000";
constexpr const char* legacy=
"3b47616d656272796f204b464d2046696c652056657273696f6e20312e322e34620d0a090000006d6f64656c2e6e696604000000526f6f740100000002000000000000804523c17f020000002a0000000400000049646c650700000069646c652e6b6603000000020000004d000000050000004d00000002000000cdcccc3e0100000003000000656e64050000007374617274010000002a000000000080bf4d0000000300000052756e0600000072756e2e6b6600000000000000007b000000";
std::vector<std::uint8_t> hex(std::string_view text) {
    const auto nibble=[](char c){return c<='9'?c-'0':c-'a'+10;};
    std::vector<std::uint8_t> b;
    for(std::size_t i=0;i<text.size();i+=2)b.push_back(static_cast<std::uint8_t>(nibble(text[i])*16+nibble(text[i+1])));
    return b;
}
}
int main() {
    for(const auto text:{modern,legacy}) {
        const auto bytes=hex(text);auto f=DecodeKfm(bytes);assert(f);
        assert(f->animations.size()==2 && f->animations[0].index==3 && f->unknownInt3==123);
        assert(std::bit_cast<std::uint32_t>(f->unknownFloat1)==0x80000000);
        assert(std::bit_cast<std::uint32_t>(f->unknownFloat2)==0x7fc12345);
        const auto& t=f->animations[0].transitions[1];
        assert(t.type==2 && t.eventCode==77 && t.duration==0.4f);
        assert(t.textKeys.size()==1 && t.textKeys[0].source=="end" && t.textKeys[0].destination=="start");
        assert(t.intermediateAnimations.size()==1 && t.intermediateAnimations[0].eventCode==42 && t.intermediateAnimations[0].value==-1);
        if(f->version==KfmVersion::V1_2_4b)assert(f->crlf && f->animations[0].name=="Idle" && f->animations[1].name=="Run");
        auto encoded=EncodeKfm(*f);assert(encoded && *encoded==bytes);
        for(std::size_t i=0;i<bytes.size();++i)assert(!DecodeKfm(std::span(bytes).first(i)));
        auto trailing=bytes;trailing.push_back(0);assert(!DecodeKfm(trailing));
        f->animations[0].kfFileName="changed path\\idle.kf";
        f->animations[0].transitions[1].textKeys.push_back({"left","right"});
        f->animations[0].transitions[1].intermediateAnimations.push_back({77,0.7f});
        auto changed=EncodeKfm(*f);assert(changed && *changed!=bytes);
        auto reloaded=DecodeKfm(*changed);assert(reloaded && reloaded->animations[0].kfFileName=="changed path\\idle.kf");
        assert(reloaded->animations[0].transitions[1].textKeys[1].destination=="right");
        assert(reloaded->animations[0].transitions[1].intermediateAnimations[1].value==0.7f);
        assert(std::bit_cast<std::uint32_t>(reloaded->unknownFloat2)==0x7fc12345);
        f->animations[0].transitions[0].duration=0.5f;assert(!EncodeKfm(*f));
    }
    const auto bytes=hex(modern);
    // Every count/string-length location in this independent golden fixture.
    for(const auto offset:{37u,50u,74u,82u,97u,121u,125u,132u,141u,157u,171u}) {
        for(const auto high:{0x7fu,0xffu}) {
            auto bad=bytes;for(unsigned i=0;i<4;++i)bad[offset+i]=0xff;
            bad[offset+3]=static_cast<std::uint8_t>(high);assert(!DecodeKfm(bad));
        }
    }
    auto unsupported=bytes;unsupported[25]='3';assert(!DecodeKfm(unsupported));
    auto f=DecodeKfm(bytes);assert(f);f->animations[0].name="would be discarded";assert(!EncodeKfm(*f));
    f=DecodeKfm(bytes);assert(f);
    const auto dir=fs::temp_directory_path()/("nextgen-kfm-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir/"sub");
    struct Cleanup { fs::path path;~Cleanup(){std::error_code ec;fs::remove_all(path,ec);} } cleanup{dir};
    std::ofstream(dir/"model.nif").put('x');std::ofstream(dir/"IDLE.KF").put('x');std::ofstream(dir/"run .kf").put('x');
    const auto out=dir/"catalog.kfm";assert(SaveKfmFile(*f,out));
    assert(!SaveKfmFile(*f,out)); // exclusive copy must preserve an existing file
    auto saved=LoadKfmFile(out);assert(saved && EncodeKfm(*saved)==EncodeKfm(*f));
    auto refs=InspectKfmReferences(*f,out);
    assert(refs.nif && refs.animations[0] && !refs.animations[1] && refs.missingKfFiles==1);
    assert(refs.duplicateEventCodes==0 && refs.missingTransitionTargets==0 && refs.missingIntermediateTargets==0);
    f->animations.push_back(f->animations.front());f->animations[0].transitions[1].eventCode=999;
    f->animations[0].transitions[1].intermediateAnimations[0].eventCode=888;
    f->nifFileName="..\\model.nif";
    refs=InspectKfmReferences(*f,dir/"sub"/"catalog.kfm");
    assert(refs.nif && refs.duplicateEventCodes==1 && refs.missingTransitionTargets==1 && refs.missingIntermediateTargets==1);
    f->nifFileName="C:\\model.nif";assert(!InspectKfmReferences(*f,out).nif);
    std::cout<<"KFM: golden layouts, full truncation sweep, malformed fields, edits, bit preservation, exclusive copy and references passed\n";
}
