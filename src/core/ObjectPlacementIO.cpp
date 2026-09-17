#include "mapeditor/core/ObjectPlacementIO.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace theseed::mapeditor::core {

std::expected<ObjectPlacementSet, std::string> LoadTsObj(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) return std::unexpected("Konnte .tsobj nicht öffnen: " + file.string());
    std::string magic; int version = 0; in >> magic >> version;
    if (magic != "TSOBJ" || version != 1) return std::unexpected("Ungültige .tsobj-Datei: " + file.string());
    ObjectPlacementSet set; std::string keyword; in >> keyword;
    std::size_t categoryCount = 0; in >> categoryCount;
    for (std::size_t i=0;i<categoryCount;++i) { in >> keyword; ObjectCategoryList category; in >> std::quoted(category.name); std::size_t pathCount=0; in >> pathCount; for(std::size_t p=0;p<pathCount;++p){std::string path; in>>std::quoted(path); category.modelPaths.push_back(std::move(path));} set.categories.push_back(std::move(category)); }
    in >> keyword; in >> keyword >> set.environment.globalLight[0] >> set.environment.globalLight[1] >> set.environment.globalLight[2];
    in >> keyword >> set.environment.fog[0] >> set.environment.fog[1] >> set.environment.fog[2] >> set.environment.fog[3];
    in >> keyword >> set.environment.backgroundColor[0] >> set.environment.backgroundColor[1] >> set.environment.backgroundColor[2];
    in >> keyword >> set.environment.frustumFar;
    in >> keyword >> set.environment.directionLightAmbient[0] >> set.environment.directionLightAmbient[1] >> set.environment.directionLightAmbient[2];
    in >> keyword >> set.environment.directionLightDiffuse[0] >> set.environment.directionLightDiffuse[1] >> set.environment.directionLightDiffuse[2];
    in >> keyword; std::size_t objectCount=0; in>>objectCount;
    for(std::size_t i=0;i<objectCount;++i){in>>keyword; PlacedObject obj; in>>std::quoted(obj.modelPath)>>obj.posX>>obj.posY>>obj.posZ>>obj.rotX>>obj.rotY>>obj.rotZ>>obj.rotW>>obj.scale; set.AddObject(std::move(obj));}
    if(!in) return std::unexpected("Parse-Fehler in .tsobj: "+file.string()); return set;
}

std::expected<void, std::string> SaveTsObj(const ObjectPlacementSet& set, const std::filesystem::path& file) {
    std::ofstream out(file,std::ios::trunc); if(!out) return std::unexpected("Konnte .tsobj nicht schreiben: "+file.string());
    out<<"TSOBJ 1\nCATEGORIES "<<set.categories.size()<<"\n";
    for(const auto& category:set.categories){out<<"CATEGORY "<<std::quoted(category.name)<<" "<<category.modelPaths.size()<<"\n";for(const auto& path:category.modelPaths)out<<std::quoted(path)<<"\n";}
    const auto& env=set.environment; out<<"ENVIRONMENT\nGlobalLight "<<env.globalLight[0]<<" "<<env.globalLight[1]<<" "<<env.globalLight[2]<<"\nFog "<<env.fog[0]<<" "<<env.fog[1]<<" "<<env.fog[2]<<" "<<env.fog[3]<<"\nBackgroundColor "<<env.backgroundColor[0]<<" "<<env.backgroundColor[1]<<" "<<env.backgroundColor[2]<<"\nFrustum "<<env.frustumFar<<"\nDirectionLightAmbient "<<env.directionLightAmbient[0]<<" "<<env.directionLightAmbient[1]<<" "<<env.directionLightAmbient[2]<<"\nDirectionLightDiffuse "<<env.directionLightDiffuse[0]<<" "<<env.directionLightDiffuse[1]<<" "<<env.directionLightDiffuse[2]<<"\nOBJECTS "<<set.Count()<<"\n";
    for(const auto& obj:set.Objects()) out<<"OBJ "<<std::quoted(obj.modelPath)<<" "<<obj.posX<<" "<<obj.posY<<" "<<obj.posZ<<" "<<obj.rotX<<" "<<obj.rotY<<" "<<obj.rotZ<<" "<<obj.rotW<<" "<<obj.scale<<"\n";
    if(!out)return std::unexpected("Fehler beim Schreiben der .tsobj: "+file.string()); return {};
}

} // namespace theseed::mapeditor::core

namespace theseed::mapeditor::core::legacy {

std::expected<ObjectPlacementSet, std::string> ParseLegacyShmd(const std::filesystem::path& file) {
    std::ifstream in(file,std::ios::binary); if(!in)return std::unexpected("Konnte .shmd nicht öffnen: "+file.string()); ObjectPlacementSet set; std::string formatVersion; if(!(in>>formatVersion))return std::unexpected("Leere .shmd-Datei: "+file.string());
    std::string token; while(in>>token){if(token=="GlobalLight")break; ObjectCategoryList category; category.name=token; std::size_t count=0; if(!(in>>count))return std::unexpected("Fehler Kategorie: "+file.string()); for(std::size_t i=0;i<count;++i){std::string path;if(!(in>>path))return std::unexpected("Unerwartetes Dateiende: "+file.string());category.modelPaths.push_back(std::move(path));}set.categories.push_back(std::move(category));} if(!in)return std::unexpected("GlobalLight fehlt: "+file.string());
    auto& env=set.environment; if(!(in>>env.globalLight[0]>>env.globalLight[1]>>env.globalLight[2]))return std::unexpected("GlobalLight-Fehler"); std::string kw;
    if(!(in>>kw)||kw!="Fog"||!(in>>env.fog[0]>>env.fog[1]>>env.fog[2]>>env.fog[3]))return std::unexpected("Fog-Fehler");
    if(!(in>>kw)||kw!="BackGroundColor"||!(in>>env.backgroundColor[0]>>env.backgroundColor[1]>>env.backgroundColor[2]))return std::unexpected("BackGroundColor-Fehler");
    if(!(in>>kw)||kw!="Frustum"||!(in>>env.frustumFar))return std::unexpected("Frustum-Fehler");
    while(in>>token){if(token=="DataObjectLoadingEnd")break;std::string modelPath=token;std::size_t count=0;if(!(in>>count))return std::unexpected("Instanzanzahl-Fehler");for(std::size_t i=0;i<count;++i){float x,y,z,rx,ry,rz,rw=1,scale=1;if(!(in>>x>>y>>z>>rx>>ry>>rz>>rw>>scale))return std::unexpected("Instanzdaten-Fehler");PlacedObject obj;obj.modelPath=modelPath;obj.posX=x;obj.posY=z;obj.posZ=y;obj.rotX=rx;obj.rotY=rz;obj.rotZ=ry;obj.rotW=rw;obj.scale=scale;set.AddObject(std::move(obj));}} if(!in)return std::unexpected("DataObjectLoadingEnd fehlt");
    if(!(in>>kw)||kw!="DirectionLightAmbient"||!(in>>env.directionLightAmbient[0]>>env.directionLightAmbient[1]>>env.directionLightAmbient[2]))return std::unexpected("Ambient-Fehler"); if(!(in>>kw)||kw!="DirectionLightDiffuse"||!(in>>env.directionLightDiffuse[0]>>env.directionLightDiffuse[1]>>env.directionLightDiffuse[2]))return std::unexpected("Diffuse-Fehler"); return set;
}

namespace { std::string FormatFloat6(float value){double d=value;bool neg=std::signbit(d);double a=neg?-d:d;long long scaled=std::llround(a*1000000.0);char buf[64];std::snprintf(buf,sizeof(buf),"%s%lld.%06lld",neg?"-":"",scaled/1000000,scaled%1000000);return buf;} void WriteRecordLine(std::ofstream& out,const std::vector<std::string>& tokens){for(const auto& t:tokens)out<<t<<" ";out<<"\r\n";} }

std::expected<void, std::string> SerializeLegacyShmd(const ObjectPlacementSet& set,const std::filesystem::path& file){std::ofstream out(file,std::ios::binary|std::ios::trunc);if(!out)return std::unexpected("Konnte .shmd nicht schreiben: "+file.string());WriteRecordLine(out,{"shmd0_5"});for(const auto& c:set.categories){WriteRecordLine(out,{c.name,std::to_string(c.modelPaths.size())});for(const auto& p:c.modelPaths)WriteRecordLine(out,{p});}const auto& e=set.environment;WriteRecordLine(out,{"GlobalLight",FormatFloat6(e.globalLight[0]),FormatFloat6(e.globalLight[1]),FormatFloat6(e.globalLight[2])});WriteRecordLine(out,{"Fog",FormatFloat6(e.fog[0]),FormatFloat6(e.fog[1]),FormatFloat6(e.fog[2]),FormatFloat6(e.fog[3])});WriteRecordLine(out,{"BackGroundColor",FormatFloat6(e.backgroundColor[0]),FormatFloat6(e.backgroundColor[1]),FormatFloat6(e.backgroundColor[2])});WriteRecordLine(out,{"Frustum",FormatFloat6(e.frustumFar)});
    std::vector<std::string> order;std::vector<std::vector<const PlacedObject*>> groups;for(const auto& obj:set.Objects()){std::size_t i=0;for(;i<order.size()&&order[i]!=obj.modelPath;++i){}if(i==order.size()){order.push_back(obj.modelPath);groups.push_back({});}groups[i].push_back(&obj);}for(std::size_t i=0;i<order.size();++i){WriteRecordLine(out,{order[i],std::to_string(groups[i].size())});for(const auto* o:groups[i])WriteRecordLine(out,{FormatFloat6(o->posX),FormatFloat6(o->posZ),FormatFloat6(o->posY),FormatFloat6(o->rotX),FormatFloat6(o->rotZ),FormatFloat6(o->rotY),FormatFloat6(o->rotW),FormatFloat6(o->scale)});}out<<"DataObjectLoadingEnd\r\n";WriteRecordLine(out,{"DirectionLightAmbient",FormatFloat6(e.directionLightAmbient[0]),FormatFloat6(e.directionLightAmbient[1]),FormatFloat6(e.directionLightAmbient[2])});WriteRecordLine(out,{"DirectionLightDiffuse",FormatFloat6(e.directionLightDiffuse[0]),FormatFloat6(e.directionLightDiffuse[1]),FormatFloat6(e.directionLightDiffuse[2])});if(!out)return std::unexpected("Fehler beim Schreiben der .shmd: "+file.string());return {};}

} // namespace theseed::mapeditor::core::legacy
