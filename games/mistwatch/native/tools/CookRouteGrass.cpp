#include "MistwatchRouteGrassCook.h"
#include "MistwatchWestCliffRelief.h"
#include <Aevrynd/MeshLoader.h>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#ifndef AEVRYND_ROUTE_GRASS_COOK_BUILD_SHA256
#error "A pinned source and runtime build identity is required"
#endif
using namespace Aevrynd;
namespace C=Mistwatch::CoreHerbClusters;
namespace W=Mistwatch::WoodlandBridgeWorld;
namespace G=Mistwatch::GroupedCanopyVolume;
namespace R=Mistwatch::UnderstoryRetention;
namespace K=Mistwatch::RouteGrassCook;
namespace {
void Need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T Number(const std::string& text){T result{};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
    Need(parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size(),"COOK_CSV_NUMBER");return result;}
template<unsigned N>std::array<std::string,N> Fields(const std::string& line){
    std::istringstream row(line);std::array<std::string,N> f;
    for(auto& v:f)Need(bool(std::getline(row,v,',')),"COOK_CSV_INCOMPLETE");std::string extra;
    Need(!std::getline(row,extra,','),"COOK_CSV_EXTRA");return f;
}
}
int main(int argc,char** argv){try{
    Need(argc==6,"assets, actual occupancy, complete soft, actual ground prefix and fresh output required");
    const std::filesystem::path assets(argv[1]),hard(argv[2]),soft(argv[3]),prefix(argv[4]),output(argv[5]);
    for(const auto& path:{assets,hard,soft,prefix,output})Need(path.is_absolute(),"COOK_ABSOLUTE_PATHS");
    Need(!std::filesystem::exists(output),"COOK_REFUSES_OUTPUT_OVERWRITE");
    std::ifstream input(hard);std::string line;
    Need(bool(std::getline(input,line))&&line=="kind,id,x,y,z,ex,ey,ez","COOK_HARD_HEADER");
    std::vector<BoxCollider3D> retained,before,after;std::array<std::vector<BoxCollider3D>,12> wood;unsigned rows{};
    while(std::getline(input,line)){Need(++rows<=20000&&line.size()<=2048,"COOK_HARD_BUDGET");const auto f=Fields<8>(line);
        std::vector<BoxCollider3D>* target{};
        if(f[0]=="retained-vegetation")target=&retained;else if(f[0]=="world-before")target=&before;
        else if(f[0]=="world-after")target=&after;else if(f[0].starts_with("world-wood-")){
            const auto i=Number<unsigned>(f[0].substr(11));Need(i<12,"COOK_WOOD_GROUP");target=&wood[i];}
        else throw std::runtime_error("COOK_HARD_KIND");
        Need(Number<unsigned>(f[1])==target->size(),"COOK_HARD_ORDER");
        target->push_back({{Number<float>(f[2]),Number<float>(f[3]),Number<float>(f[4])},
            {Number<float>(f[5]),Number<float>(f[6]),Number<float>(f[7])}});
    }
    Need(input.eof()&&retained.size()==1024&&before.size()==229&&after.size()==230,"COOK_HARD_POPULATION");
    std::array<G::Group,12> groups;for(unsigned i=0;i<12;++i)groups[i]=wood[i];
    const G::ValidatedWood verified(groups,W::WoodIdentity);const W::ValidatedWorld world(before,after,verified);
    std::ifstream softInput(soft);
    Need(bool(std::getline(softInput,line))&&line=="family,id,variant,anchor_x,anchor_y,anchor_z,pose_x,pose_y,pose_z,rx,ry,rz,sx,sy,sz,radius,suppressed,source_fingerprint,actual_fingerprint,handle","COOK_SOFT_HEADER");
    std::vector<R::Row> softRows;softRows.reserve(R::Total);
    while(std::getline(softInput,line)){Need(softRows.size()<R::Total&&line.size()<=2048,"COOK_SOFT_BUDGET");const auto f=Fields<20>(line);
        R::Family family{};if(f[0]=="forest-grass")family=R::Family::ForestGrass;
        else if(f[0]=="roadside-broadleaf")family=R::Family::RoadsideBroadleaf;
        else if(f[0]=="core-herb")family=R::Family::CoreHerb;else throw std::runtime_error("COOK_SOFT_FAMILY");
        const auto suppressed=Number<unsigned>(f[16]);Need(suppressed<=1,"COOK_SOFT_BOOLEAN");
        softRows.push_back({family,Number<unsigned>(f[1]),Number<unsigned>(f[2]),
            {Number<float>(f[3]),Number<float>(f[4]),Number<float>(f[5])},
            {{Number<float>(f[6]),Number<float>(f[7]),Number<float>(f[8])},{Number<float>(f[9]),Number<float>(f[10]),Number<float>(f[11])},
            {Number<float>(f[12]),Number<float>(f[13]),Number<float>(f[14])}},Number<float>(f[15]),suppressed==1,
            Number<std::uint64_t>(f[17]),Number<std::uint64_t>(f[18]),Number<std::uint64_t>(f[19])});
    }
    Need(softInput.eof(),"COOK_SOFT_READ");(void)R::Validate(softRows);
    std::array<Mesh3D,5> meshes;std::array<const Mesh3D*,5> sources;
    for(unsigned i=0;i<5;++i){auto loaded=LoadAevryndMeshFile(assets/"mistwatch-grass/meshes"/Mistwatch::GrassMeshName(i));
        Need(bool(loaded),"COOK_RAW_SOURCE");Mistwatch::NormalizeGrassAsset(loaded.mesh);meshes[i]=std::move(loaded.mesh);sources[i]=&meshes[i];}
    Mesh3D ground;
    for(const auto name:{".cliff-core.aemesh",".cliff-road.aemesh",".cliff-gravel.aemesh"}){
        auto loaded=LoadAevryndMeshFile(prefix.string()+name);Need(bool(loaded),"COOK_ACTUAL_DOMAIN");std::map<unsigned,unsigned> remap;
        for(auto index:loaded.mesh.indices){auto it=remap.find(index);if(it==remap.end()){
            const auto next=static_cast<unsigned>(ground.vertices.size());ground.vertices.push_back(loaded.mesh.vertices[index]);it=remap.emplace(index,next).first;}
            ground.indices.push_back(it->second);}
    }
    const auto terrain=Mistwatch::WestCliffRelief::Build(Mistwatch::CreateTerrain()).terrain;
    for(const auto& v:ground.vertices)Need(terrain.Contains(v.x,v.z)&&std::abs(double(v.y)-terrain.SampleHeight(v.x,v.z))<=.000002,"COOK_GROUND_COLLISION_DISAGREEMENT");
    std::cout<<"COOK_FULL_PROOF_BEGIN population=128\n"<<std::flush;const auto started=std::chrono::steady_clock::now();
    // 以目前正式 Plan 重算全部幾何及所有門，不讀取舊 safe 旗標或舊派生網格。
    const auto plan=Mistwatch::RouteGrassPlan::Create(sources,ground,terrain,world,retained,softRows);
    const auto duration=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count();
    const auto encoded=K::Encode(plan,K::ParseDigest(AEVRYND_ROUTE_GRASS_COOK_BUILD_SHA256));
    Need(std::filesystem::create_directories(output),"COOK_OUTPUT_CREATE");
    K::WriteFresh(output/"route-grass.aecook",encoded.Data());
    for(unsigned i=0;i<5;++i){K::Bytes bytes;bytes.Mesh(meshes[i]);K::WriteFresh(output/("source-"+std::to_string(i)+".aemesh"),bytes.Data());}
    K::Bytes actual;actual.Mesh(ground);K::WriteFresh(output/"ground.aemesh",actual.Data());
    K::Bytes heightBytes;const auto& settings=terrain.Settings();heightBytes.Word(0x52475431u);heightBytes.Word(1);
    for(float f:{settings.origin.x,settings.origin.y,settings.origin.z,settings.cellSize})heightBytes.Float(f);
    heightBytes.Word(settings.width);heightBytes.Word(settings.depth);
    for(unsigned z=0;z<settings.depth;++z)for(unsigned x=0;x<settings.width;++x)heightBytes.Float(terrain.Height(x,z));
    K::WriteFresh(output/"terrain.bin",heightBytes.Data());
    std::ofstream receipt(output/"cook.txt");receipt<<std::setprecision(17)<<"schema=mistwatch.route-grass-cook.1\npopulation="<<plan.instances.size()
        <<"\nplan_fingerprint="<<plan.fingerprint<<"\nsurface_fingerprint="<<plan.surfaceFingerprint<<"\ncontext_fingerprint="
        <<Mistwatch::RouteGrassContext::Fingerprint(plan.context)<<"\nsource_runtime_build_sha256="<<AEVRYND_ROUTE_GRASS_COOK_BUILD_SHA256
        <<"\nbundle_sha256="<<K::Hex(K::Hash(encoded.Data()))<<"\nbundle_bytes="<<encoded.Data().size()<<"\nproof_ns="<<duration
        <<"\ngpu_buffer_readback=0\nnative_pass=0\nart_accepted=0\nruntime_admission_implemented=0\n";
    for(unsigned i=0;i<16;++i)receipt<<"context_"<<Mistwatch::RouteGrassContext::Names[i]<<'='<<plan.context.words[i]<<'\n';
    for(unsigned i=0;i<128;++i){const auto& a=plan.instances[i];const auto& p=a.plant;const auto& q=a.proof;K::Bytes bytes;bytes.Mesh(p.derivedMesh);
        const auto meshPath=output/("grass-"+std::to_string(i)+".aemesh");K::WriteFresh(meshPath,bytes.Data());
        const auto loaded=LoadAevryndMeshFile(meshPath);Need(bool(loaded)&&C::Detail::MeshFingerprint(loaded.mesh)==q.derivedFingerprint,"COOK_GEOMETRY_FILE_READBACK");
        receipt<<"plant_"<<i<<"="<<a.ordinal<<','<<a.bin<<','<<p.variant<<','<<q.sourceFingerprint<<','<<q.derivedFingerprint<<','<<q.allFaces<<','
            <<q.sourceFootPieces<<','<<q.windPinnedVertices<<','<<q.contact.minimumGapBound<<','<<q.contact.maximumFootGapBound<<','
            <<K::Hex(K::Hash(bytes.Data()))<<'\n';
    }
    receipt.close();Need(bool(receipt),"COOK_RECEIPT_WRITE");std::cout<<"COOK_COMPLETE population=128 proof_ns="<<duration
        <<" bytes="<<encoded.Data().size()<<" sha256="<<K::Hex(K::Hash(encoded.Data()))<<" GPU_verified=0 runtime_admission=0\n";return 0;
}catch(const std::exception& ex){std::cerr<<"COOK_FAIL "<<ex.what()<<'\n';return 1;}}
