#include "ProcessRunner.hpp"
#include "mapeditor/core/DdsImage.hpp"
#include "mapeditor/core/NifModel.hpp"
#include "mapeditor/core/KfmFile.hpp"
#include "mapeditor/core/HeightmapIO.hpp"
#include "mapeditor/core/ObjectPlacementIO.hpp"
#include "mapeditor/core/WalkGridIO.hpp"
#include "mapeditor/core/legacy/LegacyMapProject.hpp"
#include "mapeditor/core/legacy/LegacyIdmAid.hpp"
#include "mapeditor/core/legacy/ShnFile.hpp"
#include "mapeditor/core/legacy/ShineText.hpp"
#include "mapeditor/core/legacy/QuestData.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>

namespace core=theseed::mapeditor::core;
namespace legacy=core::legacy;
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
namespace {
std::string lower(std::string s) { for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
std::string clean(std::string s) { for(auto& c:s)if(c=='\t'||c=='\r'||c=='\n')c=' '; return s; }
std::string read(const fs::path& path) { std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}}; }
bool equalFiles(const fs::path& a,const fs::path& b) {
    if(fs::file_size(a)!=fs::file_size(b))return false;
    std::ifstream x(a,std::ios::binary),y(b,std::ios::binary);
    std::array<char,65536> xb{},yb{};
    while(x){x.read(xb.data(),xb.size());y.read(yb.data(),yb.size());if(x.gcount()!=y.gcount()||!std::equal(xb.begin(),xb.begin()+x.gcount(),yb.begin()))return false;}
    return !x.bad()&&!y.bad();
}
struct Result { std::string status, detail; };
template<class T,class Save>
Result roundtrip(std::expected<T,std::string> value,const fs::path& source,const fs::path& target,Save save) {
    if(!value)return {"ERROR",value.error()};
    auto result=save(*value,target);
    if(!result)return {"ERROR",result.error()};
    return {equalFiles(source,target)?"ROUNDTRIP_EXACT":"ROUNDTRIP_CHANGED",{}};
}
Result probe(const fs::path& path,const fs::path& scratch,bool recovery=false) {
    const auto ext=lower(path.extension().string());
    if(ext==".nif"||ext==".kf") {
        auto m=core::LoadNifMesh(path,recovery);
        if(!m)return {"ERROR",m.error()};
        std::size_t parts=0,vertices=0,triangles=0,textures=0;
        for(const auto& p:m->parts){if(!p.positions.empty()&&!p.triangleIndices.empty())++parts;vertices+=p.positions.size();triangles+=p.triangleIndices.size()/3;if(p.embeddedDiffuseTexture)++textures;}
        return {m->partial?"RECOVERY_PARTIAL":m->recovered?"RECOVERY_OK":parts?"OK_GEOMETRY":"OK_NO_GEOMETRY",
            "parts="+std::to_string(parts)+", vertices="+std::to_string(vertices)+", triangles="+std::to_string(triangles)+", textures="+std::to_string(textures)+
            ", decoded="+std::to_string(m->decodedEmbeddedTextures)+", undecoded="+std::to_string(m->undecodedEmbeddedTextures)};
    }
    if(ext==".kfm") {
        auto f=core::LoadKfmFile(path);if(!f)return {"ERROR",f.error()};
        const auto refs=core::InspectKfmReferences(*f,path);
        std::size_t transitions=0,keys=0,intermediate=0;
        for(const auto& a:f->animations)for(const auto& t:a.transitions){++transitions;keys+=t.textKeys.size();intermediate+=t.intermediateAnimations.size();}
        auto saved=core::SaveKfmFile(*f,scratch);if(!saved)return {"ERROR",saved.error()};
        Result result{equalFiles(path,scratch)?"ROUNDTRIP_EXACT":"ROUNDTRIP_CHANGED",{}};
        result.detail="version="+std::string(core::KfmVersionName(f->version))+", animations="+std::to_string(f->animations.size())+
            ", transitions="+std::to_string(transitions)+", textkeys="+std::to_string(keys)+", intermediate="+std::to_string(intermediate)+
            ", missingNif="+std::to_string(!refs.nif)+", missingKf="+std::to_string(refs.missingKfFiles)+
            ", duplicateIds="+std::to_string(refs.duplicateEventCodes)+", missingTargets="+std::to_string(refs.missingTransitionTargets)+
            ", missingIntermediateTargets="+std::to_string(refs.missingIntermediateTargets)+"; "+result.detail;
        return result;
    }
    if(ext==".dds"||ext==".tga") {
        auto image=ext==".dds"?core::LoadDdsImage(path):core::LoadTgaImage(path);
        if(!image)return {"ERROR",image.error()};
        return {"DECODED",std::to_string(image->width)+"x"+std::to_string(image->height)};
    }
    if(ext==".shn") {
        if(lower(path.filename().string())=="questdata.shn")return roundtrip(legacy::LoadQuestData(path),path,scratch,legacy::SaveQuestData);
        return roundtrip(legacy::LoadShnFile(path),path,scratch,legacy::SaveShnFile);
    }
    if(ext==".txt") {
        auto text=legacy::LoadShineTextFile(path);
        if(!text)return {"ERROR",text.error()};
        const auto tables=text->tables.size();
        auto result=roundtrip(std::move(text),path,scratch,legacy::SaveShineTextFile);
        result.detail="tables="+std::to_string(tables)+(tables?"":"; preserved text, no table semantics"); return result;
    }
    if(ext==".idm")return roundtrip(legacy::ParseLegacyIdm(path),path,scratch,legacy::SerializeLegacyIdm);
    if(ext==".aid")return roundtrip(legacy::ParseLegacyAid(path),path,scratch,legacy::SerializeLegacyAid);
    if(ext==".shmd")return roundtrip(legacy::ParseLegacyShmd(path),path,scratch,legacy::SerializeLegacyShmd);
    if(ext==".shbd") {
        const auto header=core::PeekLegacyShbdHeader(path);
        const auto bytes=fs::file_size(path);
        if(!header||!header->height||bytes<8||(bytes-8)%2||(bytes-8)/2%header->height)return {"ERROR","Invalid SHBD size/header"};
        const auto width=(bytes-8)/2/header->height;
        if(width>65536||header->height>65536)return {"ERROR","SHBD dimensions exceed supported range"};
        core::LegacyShbdHeader raw;
        auto grid=core::ImportLegacyShbd(path,static_cast<std::uint32_t>(width),header->height,&raw);
        return roundtrip(std::move(grid),path,scratch,[&](const auto& g,const auto& out){return core::ExportLegacyShbd(g,out,raw);});
    }
    if(ext==".htd"||ext==".htdg") {
        for(const auto& entry:fs::directory_iterator(path.parent_path())) {
            if(lower(entry.path().extension().string())!=".ini")continue;
            const auto ini=legacy::ParseLegacyMapIni(entry.path()); if(!ini)continue;
            auto named=ini->heightFileName;std::replace(named.begin(),named.end(),'\\','/');
            // An INI may reference another map's heightmap (e.g. EventF -> DarkVally).
            // Its dimensions must not be applied to a same-named, unreferenced sibling.
            const auto expectedStem = named.empty() ? entry.path().stem().string() : fs::path(named).stem().string();
            if(lower(expectedStem)!=lower(path.stem().string()))continue;
            core::LegacyHtdHeader header;std::vector<std::uint8_t> tail;
            auto height=core::ImportLegacyHtd(path,ini->heightmapWidth,ini->heightmapHeight,ini->oneBlockWidth,ini->oneBlockHeight,&header,&tail);
            return roundtrip(std::move(height),path,scratch,[&](const auto& h,const auto& out){return core::ExportLegacyHtd(h,out,header,tail);});
        }
        return {"NEEDS_MAP_INI","Dimensions require the matching map INI"};
    }
    if(ext==".ini") {
        auto ini=legacy::ParseLegacyMapIni(path);
        if(!ini)return {"NOT_MAP_INI",ini.error()};
        auto result=roundtrip(std::move(ini),path,scratch,legacy::SerializeLegacyMapIni);
        if(result.status=="ROUNDTRIP_CHANGED") {
            const auto again=legacy::ParseLegacyMapIni(scratch);
            if(!again)return {"ERROR",again.error()};
            result.status="REPARSED";result.detail="Canonical rewrite; byte preservation not verified";
        }
        return result;
    }
    return {"UNRESEARCHED","No complete codec for this format"};
}
struct Item { fs::path path; std::string ext; };
}

int main(int argc,char** argv) {
    try {
        if(argc==4&&(std::string(argv[1])=="--probe"||std::string(argv[1])=="--recover")) {
            Result r;
            try {r=probe(argv[2],argv[3],std::string(argv[1])=="--recover");}
            catch(const std::exception& e){r={"ERROR",e.what()};}
            std::cout << "RESULT\t" << r.status << '\t' << clean(r.detail) << '\n';return r.status=="ERROR"?1:0;
        }
        fs::path out;std::vector<fs::path> roots;bool nifOnly=false;std::set<std::string> onlyExtensions;unsigned jobs=4;int timeout=5000;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if(arg=="--nif-only")nifOnly=true;
            else if(i+1<argc&&arg=="--root")roots.emplace_back(argv[++i]);
            else if(i+1<argc&&arg=="--extension")onlyExtensions.insert(lower(argv[++i]));
            else if(i+1<argc&&arg=="--out")out=argv[++i];
            else if(i+1<argc&&arg=="--jobs")jobs=static_cast<unsigned>(std::stoul(argv[++i]));
            else if(i+1<argc&&arg=="--timeout-ms")timeout=std::stoi(argv[++i]);
            else throw std::runtime_error("Unknown/incomplete argument: "+arg);
        }
        if(out.empty()||roots.empty()||!jobs||jobs>32||timeout<=0||timeout>3600000)
            throw std::runtime_error("Usage: fiesta_audit --root directory [--root directory] --out directory [--nif-only] [--jobs 4] [--timeout-ms 5000]");
        out=fs::absolute(out);fs::create_directories(out/"scratch");
        std::vector<Item> inputs;std::map<std::string,std::size_t> extensions;
        const std::set<std::string> codecs{".nif",".kf",".kfm",".dds",".tga",".shn",".txt",".idm",".aid",".shmd",".shbd",".htd",".htdg",".ini"};
        std::ofstream inventory(out/"inventory.tsv");inventory<<"path\textension\tbytes\tcodec\n";
        std::set<fs::path> seen;
        for(const auto& root:roots) {
            if(!fs::is_directory(root))throw std::runtime_error("Root must be an existing extracted directory: "+root.string());
            for(const auto& entry:fs::recursive_directory_iterator(root,fs::directory_options::skip_permission_denied)) {
                if(!entry.is_regular_file())continue;
                const auto path=fs::absolute(entry.path()).lexically_normal();
                if(!seen.insert(path).second)continue;
                const auto ext=lower(path.extension().string());if(nifOnly&&ext!=".nif")continue;
                ++extensions[ext];const bool supported=codecs.contains(ext);
                inventory<<clean(path.string())<<'\t'<<ext<<'\t'<<entry.file_size()<<'\t'<<(supported?"AVAILABLE":"UNRESEARCHED")<<'\n';
                if(supported && (onlyExtensions.empty() || onlyExtensions.contains(ext)))inputs.push_back({path,ext});
            }
        }
        inventory.close();
        std::ofstream kinds(out/"extensions.tsv");kinds<<"extension\tfiles\tcodec\n";
        for(const auto& [ext,n]:extensions)kinds<<ext<<'\t'<<n<<'\t'<<(codecs.contains(ext)?"AVAILABLE":"UNRESEARCHED")<<'\n';
        kinds.close();
        std::sort(inputs.begin(),inputs.end(),[](const auto& a,const auto& b){return a.path<b.path;});
        const auto executable=fs::absolute(argv[0]);
        std::vector<Item> recoveryInputs;std::map<std::string,std::size_t> counts;
        std::mutex mutex;
        std::atomic<std::size_t> infrastructureFailures = 0;
        const auto runPhase=[&](const std::vector<Item>& list,bool recovery) {
            std::ofstream report(out/(recovery?"recovery.tsv":"standard.tsv"));
            report<<"path\textension\tstatus\tmilliseconds\tdetail\twarnings\n";report.flush();
            std::atomic<std::size_t> next=0,done=0;
            std::vector<std::jthread> workers;
            for(unsigned worker=0;worker<jobs;++worker)workers.emplace_back([&,worker] {
                for(auto index=next.fetch_add(1);index<list.size();index=next.fetch_add(1)) {
                    const auto& item=list[index];
                    const auto log=out/"scratch"/(std::to_string(worker)+".log");
                    const auto scratch=out/"scratch"/(std::to_string(worker)+item.ext);
                    const auto begin=Clock::now();
                    const auto child=nextgen::tools::RunProcess(executable,{recovery?"--recover":"--probe",item.path,scratch},log,std::chrono::milliseconds(timeout));
                    const auto ms=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();
                    Result result{"ERROR",child.error};std::string warnings;
                    if(!child.error.empty()) {result={"INFRA_ERROR",child.error}; ++infrastructureFailures;}
                    else if(child.timedOut)result={"TIMEOUT","Hard process timeout"};
                    else {
                        std::istringstream lines(read(log));std::string line;
                        while(std::getline(lines,line)) {
                            if(line.starts_with("RESULT\t")) {const auto tab=line.find('\t',7);result={line.substr(7,tab-7),tab==std::string::npos?"":line.substr(tab+1)};}
                            else if(!line.empty())warnings+=line+" ";
                        }
                        if(child.exitCode!=0&&result.status!="ERROR")result={"ERROR","Child failed: "+std::to_string(child.exitCode)};
                    }
                    {std::scoped_lock lock(mutex);
                        report<<clean(item.path.string())<<'\t'<<item.ext<<'\t'<<result.status<<'\t'<<ms<<'\t'<<clean(result.detail)<<'\t'<<clean(warnings)<<'\n';report.flush();
                        ++counts[(recovery?"recovery:":"standard:")+item.ext+":"+result.status];
                        if(!recovery&&item.ext==".nif"&&result.status=="ERROR")recoveryInputs.push_back(item);
                        const auto finished=++done;if(finished%500==0||finished==list.size())std::cout<<(recovery?"recovery ":"standard ")<<finished<<'/'<<list.size()<<std::endl;
                    }
                    std::error_code ec;fs::remove(scratch,ec);fs::remove(log,ec);
                }
            });
        };
        runPhase(inputs,false);
        runPhase(recoveryInputs,true);
        std::ofstream summary(out/"summary.tsv");summary<<"phase:extension:status\tcount\n";
        for(const auto& [key,n]:counts)summary<<key<<'\t'<<n<<'\n';
        std::cout<<"Audit complete. Files with no codec remain explicitly UNRESEARCHED in the inventory.\n";
        return infrastructureFailures ? 2 : 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
