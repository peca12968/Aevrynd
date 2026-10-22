#include "MistwatchEntranceWorldAuthor.h"
#include "MistwatchOriginalForestSource.h"
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
using namespace Mistwatch;using namespace Aevrynd;
unsigned checks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void Reject(F action,std::string_view expected){
    try{action();}catch(const std::exception& e){Need(std::string_view(e.what()).find(expected)!=std::string_view::npos,"WRONG_REJECTION");return;}
    throw std::runtime_error("INVALID_AUTHOR_INPUT_ACCEPTED");
}
std::array<FernColonyAsset,4> LoadFerns(const std::filesystem::path& directory){
    constexpr std::array<std::size_t,4> sizes{75104,70896,24656,25628};
    constexpr std::array<const char*,4> hashes{
        "83BEE63C50758AF9833AAF8CB0ABC06000DF21C412F4971BD582D64ADD4EAE81",
        "07DAAD96517DA59466BE5551A6460F0943E5142B55CF2F7F0254938D92BA0306",
        "78702057068C309D6C559F223FB282C29BAC32B8C7B40987C14338F5F65E7E5A",
        "243A1BA9863EE362F8BEBCDDA7BA0DC44D847FF8684CAC327B68A422C26663F8"};
    std::array<FernColonyAsset,4> assets;
    for(unsigned i=0;i<4;++i){
        const auto path=directory/FernMeshName(i);
        Need(std::filesystem::file_size(path)==sizes[i],"FERN_SIZE");
        std::ifstream stream(path,std::ios::binary);std::vector<std::byte> raw(sizes[i]);
        stream.read(reinterpret_cast<char*>(raw.data()),std::streamsize(raw.size()));
        Need(stream.gcount()==std::streamsize(raw.size()),"FERN_READ");
        std::array<std::byte,32> digest{};Need(WorldDeltaSha2563D(raw,digest),"FERN_SHA");
        std::ostringstream text;text<<std::uppercase<<std::hex<<std::setfill('0');
        for(auto b:digest)text<<std::setw(2)<<std::to_integer<unsigned>(b);
        Need(text.str()==hashes[i],"FERN_PIN");
        auto loaded=LoadAevryndMeshFile(path);Need(bool(loaded),"FERN_NATIVE_LOAD");
        static_assert(sizeof(Vertex3D)==28&&std::endian::native==std::endian::little);
        const auto vb=loaded.mesh.vertices.size()*sizeof(Vertex3D),ib=loaded.mesh.indices.size()*sizeof(std::uint32_t);
        Need(raw.size()==16+vb+ib&&std::memcmp(raw.data()+16,loaded.mesh.vertices.data(),vb)==0&&
            std::memcmp(raw.data()+16+vb,loaded.mesh.indices.data(),ib)==0,"FERN_RAW_NATIVE_BITS");
        NormalizeFernAsset(loaded.mesh);assets[i]=FernColonyAssetFromMesh(loaded.mesh);
        Need(assets[i].vertices==FernVertexCounts[i]&&assets[i].triangles==FernTriangleCounts[i],"FERN_GEOMETRY");
    }
    return assets;
}
struct Words {
    std::vector<std::uint32_t> data;
    void Word(std::uint32_t x){data.push_back(x);}
    void Wide(std::uint64_t x){Word(std::uint32_t(x));Word(std::uint32_t(x>>32));}
    void Count(std::size_t n){Need(n<65536,"BOUNDED_EXPORT");Word(std::uint32_t(n));}
    void Float(float x){Need(std::isfinite(x),"FINITE_EXPORT");Word(std::bit_cast<std::uint32_t>(x));}
    void Vec(Vec3 v){Float(v.x);Float(v.y);Float(v.z);}
    void Pose(const Transform3D& p){Vec(p.position);Vec(p.rotation);Vec(p.scale);}
    void Box(const BoxCollider3D& b){Vec(b.center);Vec(b.extents);}
    void Boxes(std::span<const BoxCollider3D> boxes){Count(boxes.size());for(const auto& b:boxes)Box(b);}
};
std::vector<std::uint32_t> Encode(const EntranceWorldAuthor& a){
    Words w;w.Word(a.applied?1u:0u);w.Word(unsigned(a.fallenProfile));
    w.Wide(a.sourceFirFingerprint);w.Wide(a.sourceMiddleFingerprint);w.Wide(a.fingerprint);
    w.Wide(a.firs.fingerprint);w.Word(a.firs.reachedBudget?1u:0u);
    for(auto n:{a.firs.centreAttempts,a.firs.plantAttempts,a.firs.rejectedClearance,a.firs.rejectedSpacing})w.Word(n);
    w.Count(a.firs.patchCentres.size());for(auto p:a.firs.patchCentres)w.Vec(p);
    w.Count(a.firs.roots.size());for(auto p:a.firs.roots)w.Vec(p);
    w.Count(a.firs.firs.size());for(const auto& f:a.firs.firs){w.Vec(f.root);w.Pose(f.render);w.Box(f.collider);w.Word(f.patch);}
    w.Count(a.props.size());for(const auto& p:a.props){w.Word(p.mesh);w.Pose(p.transform);w.Box(p.collider);}
    for(const auto* list:{&a.understory.ferns,&a.understory.saplings}){w.Count(list->size());for(const auto& p:*list)w.Pose(p);}
    w.Count(a.broadleaf.size());for(const auto& p:a.broadleaf){w.Pose(p.transform);w.Box(p.trunk);}
    w.Word(a.middle.reachedBudget?1u:0u);
    for(auto n:{a.middle.attempted,a.middle.rejectedClearance,a.middle.rejectedTerrain,a.middle.rejectedSpacing})w.Word(n);
    w.Count(a.middle.trees.size());for(const auto& p:a.middle.trees){w.Pose(p.transform);w.Box(p.trunk);}
    for(const auto* list:{&a.firSourceIds,&a.propSourceIds,&a.saplingSourceIds,&a.middleSourceIds}){
        w.Count(list->size());for(auto id:*list)w.Word(id);
    }
    w.Boxes(a.mission);
    for(bool repaired:{false,true}){w.Boxes(a.WorldSolids(repaired));w.Boxes(a.VegetationSolids(repaired));}
    return w.data;
}
void SameBoxes(std::span<const BoxCollider3D> a,std::span<const BoxCollider3D> b){
    Need(a.size()==b.size(),"SOLID_COUNT");
    for(std::size_t i=0;i<a.size();++i)Need(FallenLogDetail::SameBox(a[i],b[i]),"SOLID_RAW_BITS");
}
void Run(const std::filesystem::path& fernDirectory,const std::filesystem::path& output){
    const auto assets=LoadFerns(fernDirectory);const auto terrain=CreateTerrain();
    const auto source=OriginalForestSource::Create(terrain);
    Words exported;
    for(auto profile:{FallenLogProfile::LegacyCylinder,FallenLogProfile::ScannedStraightV1}){
#if defined(AEVRYND_ENTRANCE_SOURCE_BEFORE)
        static_assert(OriginalEntryLayoutActive);
        const auto author=CreateEntranceWorldAuthor(terrain,source.Firs(),source.Props(),source.Understory(),
            source.Broadleaf(),source.Middle(),assets,profile);
#else
        const auto author=CreateOriginalEntranceWorldAuthor(source,terrain,assets,profile);
        const auto old=[&]{return CreateEntranceWorldAuthor(terrain,source.Firs(),source.Props(),source.Understory(),
            source.Broadleaf(),source.Middle(),assets,profile);};
        if constexpr(OriginalEntryLayoutActive){
            Need(Encode(old())==Encode(author),"OLD_PUBLIC_AND_EXPLICIT_ORIGINAL_OUTPUT_DRIFT");
            for(bool repaired:{false,true})
                SameBoxes(source.MappedSolids(repaired,profile),WorldSolids(repaired,FirLayoutSelector::ForestWood,profile));
            auto bad=source.Understory();bad.ferns[0].position.x+=.01f;
            Reject([&]{(void)CreateEntranceWorldAuthor(terrain,source.Firs(),source.Props(),bad,source.Broadleaf(),
                source.Middle(),assets,profile);},"original-fern-full-bit-identity");
        }else{
            Reject([&]{(void)old();},"entry-layout-requires-new-typed-world-author");
            Reject([&]{(void)CreateEntranceWorldAuthor(terrain,{},{},{},{},{},{},FallenLogProfile(255));},
                "entry-layout-requires-new-typed-world-author");
        }
        auto wrong=terrain;Need(wrong.SetHeight(0,0,std::nextafter(wrong.Height(0,0),1.f)),"TERRAIN_FIXTURE");
        Reject([&]{(void)CreateOriginalEntranceWorldAuthor(source,wrong,assets,profile);},"FALLEN_TERRAIN_IDENTITY");
        const auto noBridge=source.MappedSolids(false,profile),bridge=source.MappedSolids(true,profile);
        Need(noBridge.size()==223&&bridge.size()==224,"MAPPED_BOTH_BRIDGE_STATES");
        SameBoxes(std::span(bridge).first(33),std::span(noBridge).first(33));
        SameBoxes(std::span(bridge).subspan(34),std::span(noBridge).subspan(33));
        Need(FallenLogDetail::SameBox(bridge[33],{{0,-.2f,36},{2,.2f,4}}),"BRIDGE_INSERTION_ROLE");
#endif
        Need(author.applied&&author.firs.firs.size()==95&&author.firs.roots.size()==95&&author.props.size()==33&&
            author.understory.ferns.size()==420&&author.understory.saplings.size()==40&&author.broadleaf.size()==12&&
            author.middle.trees.size()==23&&author.middle.reachedBudget,"AUTHOR_POPULATION");
        Need(author.WorldSolids(false).size()==213&&author.WorldSolids(true).size()==214&&
            author.VegetationSolids(false).size()==236&&author.VegetationSolids(true).size()==237,"AUTHOR_BRIDGE_WORLD");
        const auto words=Encode(author);exported.Count(words.size());
        exported.data.insert(exported.data.end(),words.begin(),words.end());
    }
    if(!output.empty()){
        Need(output.is_absolute()&&!std::filesystem::exists(output),"FRESH_EXPORT");
        std::ofstream file(output,std::ios::binary);
        for(auto word:exported.data)for(unsigned i=0;i<4;++i)file.put(char((word>>(i*8))&255u));
        file.close();Need(bool(file),"EXPORT_WRITE");
    }
    std::cout<<"ORIGINAL_ENTRANCE_SOURCE_CPU_PASS checks="<<checks<<" profile="<<unsigned(ActiveEntryLayout.id)
        <<" exports_words="<<exported.data.size()<<" source=original profiles=legacy,scanned full_world=0 native=0\n";
}
}
int main(int argc,char** argv){try{
    if(argc!=3&&argc!=5)throw std::invalid_argument("ARGS");
    if(std::string_view(argv[1])!="--fern-dir"||(argc==5&&std::string_view(argv[3])!="--out"))throw std::invalid_argument("ARGS");
    Run(std::filesystem::path(argv[2]),argc==5?std::filesystem::path(argv[4]):std::filesystem::path{});return 0;
}catch(const std::exception& e){std::cerr<<"ORIGINAL_ENTRANCE_SOURCE_FIRST_DIVERGENCE "<<e.what()<<" checks="<<checks<<'\n';return 1;}}
