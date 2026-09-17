// test_heightmap_core.cpp
// Eigenständiger Smoke-/Regressionstest ohne GUI-Abhängigkeiten (kein GLFW/ImGui nötig).
// Baubar direkt mit g++, siehe README.md. Prüft:
//   1) Heightmap-Grundfunktionen (At/Set/SampleWorld/MinMax)
//   2) .tshm Save/Load-Roundtrip
//   3) Legacy-Import gegen die echten, vom Nutzer bereitgestellten Rou.HTD/Rou.HTDG-Dateien
//      (Pfad wird als Kommandozeilenargument übergeben, Test wird sonst übersprungen).

#include "mapeditor/core/EditOps.hpp"
#include "mapeditor/core/Heightmap.hpp"
#include "mapeditor/core/HeightmapIO.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace theseed::mapeditor::core;

namespace {
int g_failures = 0;
void Check(bool condition, const char* what) {
    if (!condition) { std::fprintf(stderr, "[FEHLER] %s\n", what); ++g_failures; }
    else std::printf("[ok]     %s\n", what);
}
void TestBasicHeightmap() {
    Heightmap hm(4, 4, 50.0f, 50.0f); hm.Set(1,1,10.0f); hm.Set(2,1,20.0f); hm.Set(1,2,30.0f); hm.Set(2,2,40.0f);
    Check(hm.At(1,1)==10.0f,"At() liefert gesetzten Wert zurück");
    Check(hm.SampleWorld(50.0f,50.0f)==10.0f,"SampleWorld exakt auf Vertex");
    Check(std::abs(hm.SampleWorld(75.0f,75.0f)-25.0f)<0.001f,"SampleWorld bilinear im Quad-Zentrum");
    const auto [lo,hi]=hm.MinMax(); Check(lo==0.0f&&hi==40.0f,"MinMax über gemischtes Gitter");
}
void TestUndoRedo() {
    Heightmap hm(9,9,50.0f,50.0f); UndoStack undo; BrushSettings settings; settings.radius=120.0f; settings.strength=10.0f;
    const float before=hm.At(4,4); auto patch=ApplyBrush(hm,BrushMode::Raise,settings,200.0f,200.0f); undo.Push(patch); const float afterRaise=hm.At(4,4);
    Check(afterRaise>before,"Raise-Pinsel erhöht Höhe im Zentrum"); Check(undo.Undo(hm),"Undo erfolgreich"); Check(hm.At(4,4)==before,"Undo stellt Ausgangswert wieder her");
    Check(undo.Redo(hm),"Redo erfolgreich"); Check(hm.At(4,4)==afterRaise,"Redo stellt bearbeiteten Wert wieder her");
}
void TestFlattenConverges() {
    Heightmap hm(9,9,50.0f,50.0f); for(auto& v:hm.MutableData()) v=100.0f; BrushSettings s; s.radius=300.0f; s.strength=100.0f; s.flattenTarget=0.0f;
    for(int i=0;i<20;++i) ApplyBrush(hm,BrushMode::Flatten,s,200.0f,200.0f); Check(std::abs(hm.At(4,4))<1.0f,"Flatten konvergiert gegen Zielhöhe");
}
void TestTshmRoundtrip() {
    Heightmap hm(5,3,25.0f,30.0f); for(std::uint32_t z=0;z<hm.Height();++z) for(std::uint32_t x=0;x<hm.Width();++x) hm.Set(x,z,float(x)*1.5f-float(z));
    auto p=std::filesystem::temp_directory_path()/"map_editor_roundtrip_test.tshm"; auto save=SaveTshm(hm,p); Check(save.has_value(),"SaveTshm erfolgreich"); auto load=LoadTshm(p); Check(load.has_value(),"LoadTshm erfolgreich");
    if(load){ const auto& l=*load; Check(l.Width()==hm.Width()&&l.Height()==hm.Height(),"Dimensionen nach Roundtrip identisch"); bool same=true; for(std::uint32_t z=0;z<hm.Height()&&same;++z) for(std::uint32_t x=0;x<hm.Width()&&same;++x) same=l.At(x,z)==hm.At(x,z); Check(same,"Höhenwerte nach Roundtrip identisch"); } std::filesystem::remove(p);
}
void TestLegacyImport(const std::filesystem::path& htd,const std::filesystem::path& htdg){ constexpr std::uint32_t w=257,h=257; LegacyHtdHeader header{}; auto a=ImportLegacyHtd(htd,w,h,50.0f,50.0f,&header); Check(a.has_value(),"ImportLegacyHtd(Rou.HTD) erfolgreich"); if(a){const auto [lo,hi]=a->MinMax(); Check(hi>lo,"Rou.HTD enthält nicht-triviale Höhendaten");} auto b=ImportLegacyHtd(htdg,w,h,50.0f,50.0f); Check(b.has_value(),"ImportLegacyHtd(Rou.HTDG) erfolgreich");}
void TestLegacyExportRoundtrip(const std::filesystem::path& htd){ constexpr std::uint32_t w=257,h=257; LegacyHtdHeader header{}; auto a=ImportLegacyHtd(htd,w,h,50.0f,50.0f,&header); Check(a.has_value(),"Import für Export-Roundtrip-Test erfolgreich"); if(!a)return; auto p=std::filesystem::temp_directory_path()/"map_editor_legacy_export_test.HTD"; auto e=ExportLegacyHtd(*a,p,header); Check(e.has_value(),"ExportLegacyHtd erfolgreich"); std::ifstream o(htd,std::ios::binary),x(p,std::ios::binary); std::vector<char> ob((std::istreambuf_iterator<char>(o)),{}), xb((std::istreambuf_iterator<char>(x)),{}); Check(ob.size()==xb.size(),"Exportierte Datei hat identische Größe wie Original"); Check(ob==xb,"Exportierte Datei ist BYTE-FÜR-BYTE IDENTISCH zum Original"); std::filesystem::remove(p); }
}
int main(int argc,char** argv){ std::printf("== Heightmap Core Tests ==\n"); TestBasicHeightmap(); TestUndoRedo(); TestFlattenConverges(); TestTshmRoundtrip(); if(argc>=3){TestLegacyImport(argv[1],argv[2]);TestLegacyExportRoundtrip(argv[1]);} else std::printf("\n(Legacy-Tests übersprungen)\n"); std::printf("\n%d Fehler.\n",g_failures); return g_failures==0?0:1; }
