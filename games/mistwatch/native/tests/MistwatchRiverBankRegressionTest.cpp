#include "MistwatchWorld.h"
#include "MistwatchEvasion.h"
#include <Aevrynd/CharacterController3D.h>
#include <iostream>
#include <stdexcept>
namespace {
using namespace Aevrynd;using namespace Mistwatch;
void Require(bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);}
float LegacyHeight(float x,float z){
    const float width=4.f+2.f*RiverOuterBlend(x),distance=RiverDistance(x,z);
    return distance<width?-8.f*(1.f-RiverSmooth(distance/width)):HillElevation(x,z)*RiverSmooth((distance-width)/4.f);
}
// 保留已發布5000次演算法，獨立核對加長搜尋未重排已接受的植株。
ForestUnderstory LegacyUnderstory(const HeightField3D& terrain,std::span<const Vec3> trees,std::span<const ForestProp> props){
    ForestUnderstory result;std::uint32_t seed=0x4645524eu;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    for(unsigned attempt=0;attempt<5000&&(result.ferns.size()<420||result.saplings.size()<42);++attempt){
        const bool sapling=attempt%6==0;
        if((sapling&&result.saplings.size()>=42)||(!sapling&&result.ferns.size()>=420))continue;
        const float x=-31.f+62.f*random(),z=-9.f+97.f*random(),radius=sapling?1.4f:.9f;
        const float patch=.5f+.25f*std::sin(x*.35f+z*.17f)+.25f*std::cos(z*.28f-x*.13f);
        if(random()>patch||!PropClearsObjectives({{x,0,z},{radius,1,radius}}))continue;
        if(terrain.SampleNormal(x,z).y<.88f)continue;
        bool occupied=false;
        for(const auto tree:trees)occupied|=std::hypot(x-tree.x,z-tree.z)<(sapling?2.2f:1.f);
        for(const auto& prop:props)occupied|=std::abs(x-prop.collider.center.x)<prop.collider.extents.x+radius&&std::abs(z-prop.collider.center.z)<prop.collider.extents.z+radius;
        for(const auto& tree:result.saplings)occupied|=std::hypot(x-tree.position.x,z-tree.position.z)<(sapling?3.f:1.f);
        if(!sapling)for(const auto& fern:result.ferns)occupied|=std::hypot(x-fern.position.x,z-fern.position.z)<1.f;
        if(sapling)for(const auto& fern:result.ferns)occupied|=std::hypot(x-fern.position.x,z-fern.position.z)<1.f;
        if(occupied)continue;
        const float scale=sapling?.23f+.13f*random():.6f+.18f*random();
        Transform3D plant{{x,terrain.SampleHeight(x,z),z},{0,random()*6.2831853f,0},{scale,scale,scale}};
        (sapling?result.saplings:result.ferns).push_back(plant);
    }return result;
}
void Prefix(std::span<const Transform3D> old,std::span<const Transform3D> next){
    Require(next.size()>=old.size(),"Accepted prefix lost");
    for(std::size_t i=0;i<old.size();++i){const auto a=old[i].Matrix(),b=next[i].Matrix();
        for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)Require(a[r][c]==b[r][c],"Accepted RNG prefix reordered");}
}
bool Cross(const HeightField3D& terrain,float x,unsigned mode,bool legacy){
    const float center=RiverCenterZ(x),width=legacy?4.f+2.f*RiverOuterBlend(x):RiverOuterWidth(x);
    const float start=center-(std::max)(width,10.f)-3.f,end=center+(std::max)(width,10.f)+3.f;
    CharacterController3D actor;actor.Reset({x,terrain.SampleHeight(x,start),start});Evasion evasion;
    bool jumped{},dashed{};
    for(unsigned tick=0;tick<900;++tick){
        const auto p=actor.Position();
        if(p.y<-5.f)return false;
        if(p.z>=end)return true;
        const bool trigger=p.z>=center-width*.85f;
        const bool jump=mode==2&&trigger&&!jumped&&actor.Grounded();jumped|=jump;
        const bool request=mode==3&&trigger&&!dashed&&actor.Grounded();dashed|=request;
        const auto motion=evasion.Tick(1.f/60.f,request,actor.Grounded(),false,{0,0,1});
        actor.Step({motion.movement,jump,motion.dashing?Evasion::SpeedMultiplier:mode==0?1.f:1.5f},std::span<const BoxCollider3D>{},terrain,1.f/60.f);
    }return false;
}
}
int main(){try{
    const auto terrain=CreateTerrain();HeightField3D legacy(terrain.Settings());unsigned coreSamples{},changed{};
    for(unsigned z=0;z<241;++z)for(unsigned x=0;x<201;++x){
        const auto p=terrain.SamplePosition(x,z);const float old=LegacyHeight(p.x,p.z);legacy.SetHeight(x,z,old);
        if(std::abs(p.x)<=12.f){Require(terrain.Height(x,z)==old,"Central bridge corridor changed");++coreSamples;}
        changed+=terrain.Height(x,z)!=old;
    }legacy.RecalculateRange();Require(coreSamples==11809&&changed>1000,"Terrain proof scope lost");
    float oldPeak{},newPeak{};
    for(float x:{-40.f,-30.f,30.f,40.f})for(unsigned z=40;z<160;++z){
        const unsigned column=unsigned((x+50.f)*2.f);
        oldPeak=(std::max)(oldPeak,std::abs(legacy.Height(column,z+1)-legacy.Height(column,z))/.5f);
        newPeak=(std::max)(newPeak,std::abs(terrain.Height(column,z+1)-terrain.Height(column,z))/.5f);
    }
    std::cerr<<"GRID_SLOPE: baseline="<<oldPeak<<" candidate="<<newPeak<<'\n';
    Require(oldPeak>3.f&&newPeak<oldPeak*.55f&&newPeak<1.8f,"Actual grid river bank remains too steep");
    const auto trees=TreePositions();const auto props=ForestProps(terrain,trees);
    const auto original=LegacyUnderstory(terrain,trees,props),next=CreateUnderstory(terrain,trees,props),repeat=CreateUnderstory(terrain,trees,props);
    Require(next.saplings.size()==42&&next.ferns.size()==420,"Population target lost");
    Prefix(original.saplings,next.saplings);Prefix(original.ferns,next.ferns);Prefix(next.saplings,repeat.saplings);Prefix(next.ferns,repeat.ferns);
    unsigned cases{},legacyCrossings{},newCrossings{};
    for(float x:{-40.f,-32.f,-24.f,-16.f,-12.f,-6.f,0.f,6.f,12.f,16.f,24.f,32.f,40.f})for(unsigned mode=0;mode<4;++mode){
        const bool old=Cross(legacy,x,mode,true),candidate=Cross(terrain,x,mode,false);++cases;legacyCrossings+=old;newCrossings+=candidate;
        std::cout<<"CROSS_CASE: x="<<x<<" mode="<<mode<<" legacy="<<old<<" candidate="<<candidate<<'\n';
        if(candidate&&!old){std::cerr<<"New unrepaired river bypass: x="<<x<<" mode="<<mode<<'\n';return 2;}
    }
    std::cout<<"RIVER_BANK_REGRESSION_PASS; grid_peak="<<oldPeak<<" -> "<<newPeak<<"; core_samples="<<coreSamples
             <<"; changed="<<changed<<"; old_sapling_prefix="<<original.saplings.size()<<"; final=42/420; crossing_cases="<<cases
             <<"; legacy_crossings="<<legacyCrossings<<"; candidate_crossings="<<newCrossings<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
