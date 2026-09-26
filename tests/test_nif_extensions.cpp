#include "../src/core/NifModel.cpp"
#include <bit>
#include <iostream>

using namespace theseed::mapeditor::core;
namespace {
int failures = 0;
void check(bool value, const char* message) { if (!value) { std::cerr << message << '\n'; ++failures; } }
struct Bytes {
    std::vector<std::uint8_t> data;
    void u8(std::uint8_t n) { data.push_back(n); }
    void u16(std::uint16_t n) { for(int b=0;b<2;++b) u8(static_cast<std::uint8_t>(n>>(b*8))); }
    void u32(std::uint32_t n) { for(int b=0;b<4;++b) u8(static_cast<std::uint8_t>(n>>(b*8))); }
    void f32(float n) { u32(std::bit_cast<std::uint32_t>(n)); }
    void string(const std::string& s) { u32(static_cast<std::uint32_t>(s.size())); data.insert(data.end(),s.begin(),s.end()); }
};
void boundary(Bytes bytes, void (*parse)(ByteReader&)) {
    const auto end=bytes.data.size(); bytes.u32(0x12345678);
    ByteReader r(bytes.data); parse(r);
    check(r.Ok() && r.Pos()==end && r.U32()==0x12345678,"exact extension boundary");
    for(std::size_t length=0; length<end; ++length) {
        std::vector<std::uint8_t> shortData(bytes.data.begin(),bytes.data.begin()+length);
        ByteReader shortReader(shortData); parse(shortReader);
        check(!shortReader.Ok(),"truncated extension rejected");
    }
}
void parseZBufferBoundary(ByteReader& r) { (void)ParseNiZBufferProperty(r); }
}
int main(int argc, char** argv) {
    if(argc!=2) return 2;
    Bytes zbuffer;
    zbuffer.string("");      // NiObjectNET name
    zbuffer.u32(0);          // no extra-data refs
    zbuffer.u32(0xFFFFFFFF); // no controller
    zbuffer.u16(1);          // z-test on, z-buffer read-only
    zbuffer.u32(3);          // ZCOMP_LESS_EQUAL
    boundary(zbuffer,parseZBufferBoundary);
    {
        ByteReader zr(zbuffer.data);
        const auto z=ParseNiZBufferProperty(zr);
        check(zr.Ok(),"NiZBufferProperty parses");
        check(z.test && !z.write,"NiZBufferProperty flags preserve test/read-only semantics");
        check(z.function==3,"NiZBufferProperty preserves comparison function");
    }

    Bytes accum; accum.u8(1); accum.u8(2); accum.f32(0);
    for(int i=0;i<58;++i) accum.f32(static_cast<float>(i)+0.25f);
    boundary(accum,ParseFiestaAccumulationState);
    Bytes shader; shader.string("NPTR_IS"); shader.string("PTSEV2"); shader.u8(1);
    boundary(shader,ParseFiestaShaderReference);
    shader.data[4]='X'; ByteReader invalid(shader.data); ParseFiestaShaderReference(invalid);
    check(!invalid.Ok(),"foreign shader signature rejected");
    Bytes toon; toon.string(std::string(1,'\0'));
    for(int i=0;i<4;++i) toon.f32(static_cast<float>(i));
    toon.u32(0x00ca4008); toon.f32(800); toon.u8(1); toon.u8(1); toon.f32(40); toon.f32(800);
    boundary(toon,ParseFiestaToonExtraData);
    std::size_t skinnedPartsSeen = 0;
    for(const auto* name : {"EglackMad.nif","Helga.nif","M_MajesticLion.nif","KingdomC00.nif",
        "Female_Hat_Antler00.nif","Male_Hat_Antler00.nif","LegelFairy.nif","LegelFeatherDemon.nif","BirthdayConf.nif"}) {
        const auto model=LoadNifMesh(std::filesystem::path(argv[1])/name,false);
        check(model && !model->parts.empty() && !model->recovered && !model->partial,name);
        if(!model) std::cerr << model.error() << '\n';
        if(model) {
            for (const auto& part : model->parts) {
                if (!part.skinned) continue;
                ++skinnedPartsSeen;
                check(part.skinBinding.has_value(),"skinned part preserves playback binding");
                if (!part.skinBinding) continue;
                const auto& skin=*part.skinBinding;
                check(skin.sourcePositions.size()==part.positions.size(),
                      "skin source vertex count matches rendered part");
                check(skin.vertexInfluences.size()==skin.sourcePositions.size(),
                      "skin influence array matches source vertices");
                check(!skin.bones.empty(),"skinned part exports bone bindings");
                check(std::any_of(skin.bones.begin(),skin.bones.end(),[](const auto& bone) {
                    return bone.nodeIndex>=0;
                }),"skin bone references resolve into exported NIF hierarchy");
            }
        }
        if(model && (std::string(name).starts_with("Legel") || std::string(name)=="BirthdayConf.nif")) {
            check(model->decodedEmbeddedTextures > 0 && model->undecodedEmbeddedTextures == 0,
                "all real embedded NIF textures decoded, including particle-only textures");
            if (std::string(name)!="BirthdayConf.nif") check(std::any_of(model->parts.begin(),model->parts.end(),[](const auto& part) {
                return part.embeddedDiffuseTexture && !part.embeddedDiffuseTexture->rgba.empty();
            }),"real 16-bit NIF texture decoded and linked");
        }
    }
    check(skinnedPartsSeen>0,"real NIF extension corpus contains preserved skinned parts");

    for (const auto* name : {"Arc-f_Bip01_Emotion_ChargdDance69.kf", "BallCrush_Base_Stand.kf"}) {
        const auto model = LoadNifMesh(std::filesystem::path(argv[1]) / name, false);
        check(model && !model->recovered && !model->partial, name);
        if (!model) std::cerr << model.error() << '\n';
    }
    return failures ? 1 : 0;
}
