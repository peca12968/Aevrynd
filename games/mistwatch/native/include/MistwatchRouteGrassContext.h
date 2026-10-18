#pragma once
#include "MistwatchRouteGrassAuthor.h"
#include "MistwatchRouteGrassSupport.h"
#include "MistwatchUnderstoryRetention.h"

namespace Mistwatch::RouteGrassContext {
using namespace Aevrynd;
namespace D=CoreHerbClusters::Detail;
inline constexpr unsigned Schema=1;
inline constexpr std::array<std::string_view,16> Names{
    "source_0","source_1","source_2","source_3","source_4","ground","terrain",
    "world_before","world_after","wood","retained","forest_grass","roadside_broadleaf","core_herb","author","policy"};
// FNV 是內容差異封印，不是授權或密碼學簽章；離線套件仍須由建置端核准完整 SHA256。
struct Seal {std::array<std::uint64_t,16> words{};bool operator==(const Seal&)const=default;};
inline std::uint64_t Terrain(const HeightField3D& terrain){
    const auto& s=terrain.Settings();
    D::Need(Session::Finite(s.origin)&&std::isfinite(s.cellSize)&&s.cellSize>0&&s.width>=2&&s.depth>=2&&
        terrain.SampleCount()==std::size_t(s.width)*s.depth&&terrain.SampleCount()<=4194304,
        "ROUTE_GRASS_CONTEXT_TERRAIN");
    const double endX=double(s.origin.x)+double(s.width-1)*s.cellSize,endZ=double(s.origin.z)+double(s.depth-1)*s.cellSize;
    D::Need(std::abs(double(s.origin.x))<=1000000&&std::abs(double(s.origin.y))<=1000000&&
        std::abs(double(s.origin.z))<=1000000&&std::abs(endX)<=1000000&&std::abs(endZ)<=1000000,
        "ROUTE_GRASS_CONTEXT_TERRAIN_RANGE");
    std::uint64_t h=14695981039346656037ull;D::Word(h,0x52475431u);D::Word(h,Schema);
    for(float f:{s.origin.x,s.origin.y,s.origin.z,s.cellSize})D::Float(h,f);
    D::Word(h,s.width);D::Word(h,s.depth);
    for(unsigned z=0;z<s.depth;++z)for(unsigned x=0;x<s.width;++x){const float y=terrain.Height(x,z);
        D::Need(std::isfinite(y),"ROUTE_GRASS_CONTEXT_TERRAIN_HEIGHT");D::Float(h,y);}
    return h;
}
inline std::uint64_t Author(const HeightField3D& terrain){
    (void)Terrain(terrain);
    std::uint64_t h=14695981039346656037ull;D::Word(h,0x52474131u);D::Word(h,Schema);
    D::Word(h,RouteGrassAuthor::Rows);D::Word(h,RouteGrassAuthor::Columns);D::Word(h,RouteGrassAuthor::Target);
    for(const auto& p:RouteGrassAuthor::Create(terrain)){
        D::Word(h,p.ordinal);D::Word(h,p.bin);D::Word(h,p.variant);
        for(float f:{p.root.x,p.root.y,p.root.z,p.scale})D::Float(h,f);
    }return h;
}
inline std::uint64_t Policy(){
    std::uint64_t h=14695981039346656037ull;D::Word(h,0x52475031u);D::Word(h,Schema);
    // 算法版本與既有接地、風及間距契約；建置收據另外綁定全部實作來源。
    D::Word(h,1);for(float f:{CoreHerbClusters::WindRadius,CoreHerbClusters::WindUp,
        CoreHerbClusters::RootPinnedHeight,CoreHerbClusters::RootWindFadeHeight,
        OuterUnderstory::ContactLimit,OuterUnderstory::SlopeLimit,OuterUnderstory::HeightLimit,
        .03f,.65f,.000002f,.00005f})D::Float(h,f);
    return h;
}
inline Seal Create(const std::array<const Mesh3D*,5>& sources,const Mesh3D& actualGround,
    const HeightField3D& terrain,const WoodlandBridgeWorld::ValidatedWorld& world,
    std::span<const BoxCollider3D> retained,std::span<const UnderstoryRetention::Row> soft){
    const auto summary=UnderstoryRetention::Validate(soft);world.RequireValid();
    D::Need(retained.size()==1024&&CanopyVolume::ValidWoodParts(retained),"ROUTE_GRASS_CONTEXT_RETAINED");
    LandscapeSpeciesDetail::ValidateMesh(actualGround,true);Seal result;
    for(unsigned i=0;i<5;++i){D::Need(sources[i]!=nullptr,"ROUTE_GRASS_CONTEXT_SOURCE");
        LandscapeSpeciesDetail::ValidateMesh(*sources[i],false);
        D::Need(sources[i]->vertices.size()==GrassVertexCounts[i]&&sources[i]->indices.size()==3u*GrassTriangleCounts[i],
            "ROUTE_GRASS_CONTEXT_SOURCE_TOPOLOGY");
        result.words[i]=D::MeshFingerprint(*sources[i]);
        D::Need(result.words[i]==RouteGrassSupport::SourceFingerprints[i],"ROUTE_GRASS_CONTEXT_SOURCE_IDENTITY");}
    result.words[5]=D::MeshFingerprint(actualGround);result.words[6]=Terrain(terrain);
    result.words[7]=WoodlandBridgeWorld::BodyFingerprint(world.Bodies(false),false);
    result.words[8]=WoodlandBridgeWorld::BodyFingerprint(world.Bodies(true),true);
    result.words[9]=world.Wood().Identity().fingerprint;
    result.words[10]=14695981039346656037ull;D::Word(result.words[10],0x52474831u);D::Word(result.words[10],Schema);
    D::Boxes(result.words[10],retained);
    for(unsigned i=0;i<3;++i)result.words[11+i]=summary.fingerprints[i];
    result.words[14]=Author(terrain);result.words[15]=Policy();return result;
}
inline void RequireSame(const Seal& approved,const Seal& current){
    for(unsigned i=0;i<Names.size();++i){D::Need(approved.words[i]!=0&&current.words[i]!=0,"ROUTE_GRASS_CONTEXT_EMPTY");
        if(approved.words[i]!=current.words[i])throw std::invalid_argument(std::string("ROUTE_GRASS_CONTEXT_MISMATCH_")+std::string(Names[i]));}
}
inline std::uint64_t Fingerprint(const Seal& seal){
    std::uint64_t h=14695981039346656037ull;D::Word(h,0x52474331u);D::Word(h,Schema);
    for(auto word:seal.words){D::Need(word!=0,"ROUTE_GRASS_CONTEXT_EMPTY");D::Wide(h,word);}return h;
}
}
