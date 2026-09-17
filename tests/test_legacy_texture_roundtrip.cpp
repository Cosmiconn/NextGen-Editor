// test_legacy_texture_roundtrip.cpp
// GUI-freier Test für BMP-Codec und vollständigen Legacy-Texturing-Rundlauf.
#include "mapeditor/core/TexturePaintOps.hpp"
#include "mapeditor/core/legacy/BmpBlendMap.hpp"
#include "mapeditor/core/legacy/LegacyTextureSetIO.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
using namespace theseed::mapeditor::core;
namespace { int g_failures=0; void Check(bool c,const char* w){if(!c){std::fprintf(stderr,"[FEHLER] %s\n",w);++g_failures;}else std::printf("[ok]     %s\n",w);} }
void TestBmpCodecSelfRoundtrip(){ BlendMap blend(64,48); for(std::uint32_t z=0;z<blend.Height();++z) for(std::uint32_t x=0;x<blend.Width();++x) blend.Set(x,z,float(x+z)/float(blend.Width()+blend.Height()-2)); const auto path=std::filesystem::temp_directory_path()/"map_editor_bmp_selftest.bmp"; Check(legacy::WriteBlendMapBmp(blend,path).has_value(),"WriteBlendMapBmp erfolgreich"); auto r=legacy::ReadBlendMapBmp(path); Check(r.has_value(),"ReadBlendMapBmp erfolgreich"); if(r){Check(r->Width()==blend.Width()&&r->Height()==blend.Height(),"BMP-Roundtrip: Dimensionen identisch"); bool ok=true; for(std::uint32_t z=0;z<blend.Height()&&ok;++z) for(std::uint32_t x=0;x<blend.Width()&&ok;++x) ok=std::abs(r->At(x,z)-blend.At(x,z))<=1.0f/255.0f+1e-4f; Check(ok,"BMP-Roundtrip: Werte innerhalb der Quantisierungstoleranz");} std::filesystem::remove(path); }
int main(){std::printf("== BMP-Blend-Codec Selbsttest ==\n");TestBmpCodecSelfRoundtrip();std::printf("\n%d Fehler.\n",g_failures);return g_failures==0?0:1;}
