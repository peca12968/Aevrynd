#if defined(AEVRYND_ORIGINAL_SOURCE_BEFORE)
#include "MistwatchMiddleLayer.h"
#include "MistwatchFallenLogProfile.h"
#else
#include "MistwatchOriginalForestSource.h"
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <type_traits>

namespace {
using namespace Mistwatch;using namespace Aevrynd;
unsigned checks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
struct Export {
    std::ofstream stream;
    explicit Export(const std::filesystem::path& p):stream(p,std::ios::binary){Need(bool(stream),"EXPORT_OPEN");}
    void Word(std::uint32_t word){for(unsigned i=0;i<4;++i)stream.put(char((word>>(i*8))&255u));}
    void Float(float value){Word(std::bit_cast<std::uint32_t>(value));}
    void Vec(Vec3 v){Float(v.x);Float(v.y);Float(v.z);}
    void Pose(const Transform3D& p){Vec(p.position);Vec(p.rotation);Vec(p.scale);}
    void Box(const BoxCollider3D& b){Vec(b.center);Vec(b.extents);}
    void Close(){stream.close();Need(bool(stream),"EXPORT_WRITE");}
};
template<class F> void Reject(F action,const char* expected){
    try{action();}catch(const std::invalid_argument& e){Need(std::string_view(e.what())==expected,"WRONG_REJECTION");return;}
    throw std::runtime_error("CORRUPT_TERRAIN_ACCEPTED");
}
void Run(const std::filesystem::path& path){
    Need(path.empty()||(path.is_absolute()&&!std::filesystem::exists(path)),"FRESH_OUTPUT");
    const auto terrain=CreateTerrain();
#if defined(AEVRYND_ORIGINAL_SOURCE_BEFORE)
    static_assert(OriginalEntryLayoutActive);
    const auto firs=CreateForestWoodMatureFirLayout(terrain);
    const auto props=ForestProps(terrain,firs.roots);
    const auto understory=CreateUnderstory(terrain,firs.roots,props);
    const auto broadleaf=CreateBroadleafTrees(terrain,firs.roots,props,understory);
    const auto solids=WorldSolids(false,FirLayoutSelector::ForestWood);
    const auto middle=CreateMiddleLayer(terrain,firs.roots,props,understory,broadleaf,solids);
#else
    const auto source=OriginalForestSource::Create(terrain);
    static_assert(std::is_same_v<decltype(source.Firs()),const MatureFirLayout&>);
    static_assert(std::is_same_v<decltype(source.Props()),const std::vector<ForestProp>&>);
    const auto& firs=source.Firs();const auto& props=source.Props();const auto& understory=source.Understory();
    const auto& broadleaf=source.Broadleaf();const auto& solids=source.BaseSolids();const auto& middle=source.Middle();
    auto wrong=terrain;wrong.SetHeight(0,0,std::nextafter(wrong.Height(0,0),1.f));
    Reject([&]{(void)OriginalForestSource::Create(wrong);},"FALLEN_TERRAIN_IDENTITY");
    wrong=terrain;wrong.SetHeight(0,0,std::numeric_limits<float>::quiet_NaN());
    Reject([&]{(void)OriginalForestSource::Create(wrong);},"FALLEN_TERRAIN_NONFINITE");
    // 顯式來源不能偷偷把實際遊戲配置改回 Original。
    Need(FallenLogDetail::Same(Session::supplies[0],ActiveEntryLayout.supplies[0]),"ACTIVE_LAYOUT_MUTATED");
    const auto activeFirs=CreateForestWoodMatureFirLayout(terrain);
    const auto activeProps=ForestProps(terrain,activeFirs.roots);
    bool sameProps=activeProps.size()==props.size();
    if(sameProps)for(std::size_t i=0;i<props.size();++i)
        sameProps&=FallenLogDetail::Words(activeProps[i])==FallenLogDetail::Words(props[i]);
    Need(sameProps==OriginalEntryLayoutActive,"EXPLICIT_ORIGINAL_NOT_ISOLATED_FROM_ACTIVE");
    if constexpr(OriginalEntryLayoutActive){
        const auto original=WorldSolids(false,FirLayoutSelector::ForestWood);
        Need(original.size()==solids.size(),"ORIGINAL_WORLD_SOLID_COUNT");
        for(std::size_t i=0;i<solids.size();++i)Need(FallenLogDetail::SameBox(original[i],solids[i]),"ORIGINAL_WORLD_SOLID_BITS");
    }
#endif
    Need(firs.firs.size()==100&&firs.roots.size()==100&&firs.patchCentres.size()==24&&firs.reachedBudget,"FIR_POPULATION");
    Need(firs.fingerprint==FallenLogFirFingerprint,"OLD_FIR_SEAL_CHANGED");
    ValidateFallenLogTerrain(terrain);ValidateFallenLogOriginalProps(props);
    Need(understory.ferns.size()==420&&understory.saplings.size()==42&&broadleaf.size()==12,"CORE_POPULATION");
    Need(solids.size()==223&&middle.reachedBudget&&middle.trees.size()==24,"MIDDLE_POPULATION");
    Need(MiddleLayerTransformFingerprint(middle.trees)==10337542786750772359ull,"OLD_MIDDLE_SEAL_CHANGED");
    if(!path.empty()){
    Export out(path);
    // 不寫結構padding或只比雜湊；輸出每個原float32、角色索引及生成計數。
    out.Word(firs.centreAttempts);out.Word(firs.plantAttempts);out.Word(firs.rejectedClearance);out.Word(firs.rejectedSpacing);
    for(const auto& p:firs.patchCentres)out.Vec(p);
    for(const auto& f:firs.firs){out.Vec(f.root);out.Pose(f.render);out.Box(f.collider);out.Word(f.patch);}
    for(const auto& p:firs.roots)out.Vec(p);
    for(const auto& p:props){out.Word(p.mesh);out.Pose(p.transform);out.Box(p.collider);}
    for(const auto& p:understory.ferns)out.Pose(p);
    for(const auto& p:understory.saplings)out.Pose(p);
    for(const auto& p:broadleaf){out.Pose(p.transform);out.Box(p.trunk);}
    for(const auto& b:solids)out.Box(b);
    out.Word(middle.attempted);out.Word(middle.rejectedClearance);out.Word(middle.rejectedTerrain);out.Word(middle.rejectedSpacing);
    for(const auto& p:middle.trees){out.Pose(p.transform);out.Box(p.trunk);}
    out.Close();
    }
    std::cout<<"ORIGINAL_FOREST_SOURCE_CPU_PASS checks="<<checks<<" layout="<<unsigned(ActiveEntryLayout.id)
        <<" firs=100 props=36 ferns=420 saplings=42 broadleaf=12 middle=24 base_solids=223 full_world=0 native=0\n";
}
}
int main(int argc,char** argv){try{
    if(argc<1||argc>2)throw std::invalid_argument("ARGS");
    Run(argc==2?std::filesystem::path(argv[1]):std::filesystem::path{});return 0;
}catch(const std::exception& e){std::cerr<<"ORIGINAL_FOREST_SOURCE_FIRST_DIVERGENCE "<<e.what()<<" checks="<<checks<<'\n';return 1;}}
