#include "MistwatchForestFloorMask.h"
#include "MistwatchLandscape.h"
#include <Aevrynd/TerrainLayerCoverage3D.h>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
void Mix(std::uint64_t& hash,std::uint32_t word) {
    for(unsigned byte=0;byte<4;++byte){hash^=(word>>(byte*8))&255u;hash*=1099511628211ull;}
}
void Mix(std::uint64_t& hash,float value){Mix(hash,std::bit_cast<std::uint32_t>(value));}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
}

int main() try {
    using namespace Mistwatch;
    const auto terrain=CreateTerrain();
    const auto legacyMesh=CreateTerrainMesh(terrain,true);
    const auto repeatMesh=CreateTerrainMesh(terrain,true);
    const auto typedLegacyMesh=CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::Legacy);
    const auto candidateMesh=CreateTerrainMesh(terrain,true,ForestFloorMaskSelector::ForestWood);
    if(legacyMesh.vertices.size()!=repeatMesh.vertices.size()||legacyMesh.indices!=repeatMesh.indices)return 1;
    if(typedLegacyMesh.vertices.size()!=legacyMesh.vertices.size()||typedLegacyMesh.indices!=legacyMesh.indices||
        candidateMesh.vertices.size()!=legacyMesh.vertices.size()||candidateMesh.indices!=legacyMesh.indices)return 10;

    std::uint64_t legacyMeshFp=14695981039346656037ull,candidateFp=14695981039346656037ull;
    std::size_t legacyCoverageVertices{},legacyInteriorGap{},candidateSoilVertices{},candidateRockVertices{};
    std::size_t candidateSoilStrong{},candidateRockStrong{};
    std::size_t legacyTrailVertices{},candidateTrailVertices{},legacyTrailCore{},candidateTrailCore{},bridgeChecks{};
    double legacyTrailWeight{},candidateTrailWeight{},candidateSoilWeight{},candidateRockWeight{};
    float maximumSum{};
    for(std::size_t index=0;index<legacyMesh.vertices.size();++index) {
        const auto& vertex=legacyMesh.vertices[index];
        const auto& repeated=repeatMesh.vertices[index];
        const auto& typedLegacy=typedLegacyMesh.vertices[index];
        const auto& authoredVertex=candidateMesh.vertices[index];
        if(!Same(vertex.x,repeated.x)||!Same(vertex.y,repeated.y)||!Same(vertex.z,repeated.z)||
            vertex.color!=repeated.color||!Same(vertex.u,repeated.u)||!Same(vertex.v,repeated.v))return 2;
        if(!Same(vertex.x,typedLegacy.x)||!Same(vertex.y,typedLegacy.y)||!Same(vertex.z,typedLegacy.z)||
            vertex.color!=typedLegacy.color||!Same(vertex.u,typedLegacy.u)||!Same(vertex.v,typedLegacy.v))return 11;
        if(!Same(vertex.x,authoredVertex.x)||!Same(vertex.y,authoredVertex.y)||!Same(vertex.z,authoredVertex.z))return 12;
        for(float value:{vertex.x,vertex.y,vertex.z,vertex.u,vertex.v})Mix(legacyMeshFp,value);
        Mix(legacyMeshFp,vertex.color);

        const float river=RiverSedimentWeight(vertex.x,vertex.z);
        const auto normal=terrain.SampleNormal(vertex.x,vertex.z);
        const auto legacy=ComposeForestFloorCoverage(vertex.x,vertex.z,normal,river,ForestFloorMaskSelector::Legacy);
        if(!Same(legacy.soil,.15f*river)||!Same(legacy.rock,.85f*river))return 3;
        const auto candidate=ComposeForestFloorCoverage(vertex.x,vertex.z,normal,river,ForestFloorMaskSelector::ForestWood);
        const auto repeatedCandidate=ComposeForestFloorCoverage(vertex.x,vertex.z,normal,river,ForestFloorMaskSelector::ForestWood);
        if(!Same(candidate.soil,repeatedCandidate.soil)||!Same(candidate.rock,repeatedCandidate.rock))return 4;
        Aevrynd::Vec2 encoded{};
        if(!Aevrynd::TryEncodeTerrainLayerCoverage3D(candidate.soil,candidate.rock,encoded))return 5;
        if(!Same(authoredVertex.u,candidate.soil)||!Same(authoredVertex.v,candidate.rock))return 13;
        maximumSum=(std::max)(maximumSum,candidate.soil+candidate.rock);
        candidateSoilVertices+=candidate.soil>0.f;
        candidateRockVertices+=candidate.rock>0.f;
        candidateSoilStrong+=candidate.soil>=.25f;
        candidateRockStrong+=candidate.rock>=.25f;
        candidateSoilWeight+=candidate.soil;candidateRockWeight+=candidate.rock;
        legacyCoverageVertices+=vertex.u>0.f||vertex.v>0.f;
        if(river==0.f&&vertex.u==0.f&&vertex.v==0.f&&!(vertex.z>32.f&&vertex.z<40.f))++legacyInteriorGap;
        for(float value:{candidate.soil,candidate.rock})Mix(candidateFp,value);

        const float oldTrail=CentralTrailVisualWeight(vertex.x,vertex.z,ForestFloorMaskSelector::Legacy);
        const float newTrail=CentralTrailVisualWeight(vertex.x,vertex.z,ForestFloorMaskSelector::ForestWood);
        legacyTrailVertices+=oldTrail>0.f;candidateTrailVertices+=newTrail>0.f;
        legacyTrailCore+=oldTrail>=.8f;candidateTrailCore+=newTrail>=.8f;
        legacyTrailWeight+=oldTrail;candidateTrailWeight+=newTrail;
        if(vertex.z>32.f&&vertex.z<40.f) {
            ++bridgeChecks;
            if(oldTrail!=0.f||newTrail!=0.f||!Same(candidate.soil,legacy.soil)||!Same(candidate.rock,legacy.rock))return 6;
        }
    }
    if(candidateTrailVertices>=legacyTrailVertices||candidateTrailCore>=legacyTrailCore||
        candidateTrailWeight>=legacyTrailWeight||!candidateSoilVertices||!candidateRockVertices||maximumSum>1.000001f){
        std::cerr<<"candidate metric gate failed legacySupport="<<legacyTrailVertices<<" candidateSupport="<<candidateTrailVertices
            <<" legacyCore="<<legacyTrailCore<<" candidateCore="<<candidateTrailCore<<" legacyWeight="<<legacyTrailWeight
            <<" candidateWeight="<<candidateTrailWeight<<" soil="<<candidateSoilVertices<<" rock="<<candidateRockVertices
            <<" maxSum="<<maximumSum<<'\n';return 7;
    }

    std::size_t seamChecks{};
    const auto seam=[&](float x,float z) {
        const auto coreNormal=terrain.SampleNormal(x,z),landscapeNormal=LandscapeNormal(terrain,x,z);
        const float river=RiverSedimentWeight(x,z);
        const auto a=ComposeForestFloorCoverage(x,z,coreNormal,river,ForestFloorMaskSelector::ForestWood);
        const auto b=ComposeForestFloorCoverage(x,z,landscapeNormal,river,ForestFloorMaskSelector::ForestWood);
        ++seamChecks;
        if(!Same(coreNormal.x,landscapeNormal.x)||!Same(coreNormal.y,landscapeNormal.y)||!Same(coreNormal.z,landscapeNormal.z)||
            !Same(a.soil,b.soil)||!Same(a.rock,b.rock))throw std::runtime_error("core-landscape seam mismatch");
    };
    for(float z=-20.f;z<=100.f;z+=.5f){seam(-50.f,z);seam(50.f,z);}
    for(float x=-49.5f;x<50.f;x+=.5f){seam(x,-20.f);seam(x,100.f);}

    const auto legacyLandscape=CreateLandscapeMesh(terrain,true);
    const auto typedLegacyLandscape=CreateLandscapeMesh(terrain,true,ForestFloorMaskSelector::Legacy);
    const auto candidateLandscape=CreateLandscapeMesh(terrain,true,ForestFloorMaskSelector::ForestWood);
    if(legacyLandscape.indices!=typedLegacyLandscape.indices||legacyLandscape.indices!=candidateLandscape.indices||
        legacyLandscape.vertices.size()!=typedLegacyLandscape.vertices.size()||
        legacyLandscape.vertices.size()!=candidateLandscape.vertices.size())return 14;
    std::size_t meshSeamChecks{};
    for(std::size_t index=0;index<legacyLandscape.vertices.size();++index) {
        const auto& base=legacyLandscape.vertices[index];const auto& typed=typedLegacyLandscape.vertices[index];
        const auto& authored=candidateLandscape.vertices[index];
        if(!Same(base.x,typed.x)||!Same(base.y,typed.y)||!Same(base.z,typed.z)||base.color!=typed.color||
            !Same(base.u,typed.u)||!Same(base.v,typed.v))return 15;
        if(!Same(base.x,authored.x)||!Same(base.y,authored.y)||!Same(base.z,authored.z)||base.color!=authored.color)return 16;
        const auto expected=ComposeForestFloorCoverage(authored.x,authored.z,LandscapeNormal(terrain,authored.x,authored.z),
            RiverSedimentWeight(authored.x,authored.z),ForestFloorMaskSelector::ForestWood);
        if(!Same(authored.u,expected.soil)||!Same(authored.v,expected.rock))return 17;
        const bool boundary=(authored.x== -50.f||authored.x==50.f||authored.z== -20.f||authored.z==100.f)&&
            authored.x>=-50.f&&authored.x<=50.f&&authored.z>=-20.f&&authored.z<=100.f;
        if(boundary) {
            const unsigned ix=unsigned(std::lround((authored.x+50.f)*2.f));
            const unsigned iz=unsigned(std::lround((authored.z+20.f)*2.f));
            const auto& core=candidateMesh.vertices[std::size_t(iz)*201u+ix];
            if(!Same(authored.x,core.x)||!Same(authored.y,core.y)||!Same(authored.z,core.z)||
                !Same(authored.u,core.u)||!Same(authored.v,core.v))return 18;
            ++meshSeamChecks;
        }
    }

    bool rejected=false;
    try{(void)CentralTrailVisualWeight(0,0,static_cast<ForestFloorMaskSelector>(255));}catch(const std::invalid_argument&){rejected=true;}
    if(!rejected)return 8;
    rejected=false;
    try{(void)ComposeForestFloorCoverage(0,0,{0,1,0},1.01f,ForestFloorMaskSelector::ForestWood);}catch(const std::invalid_argument&){rejected=true;}
    if(!rejected)return 9;
    const auto fullRiver=ComposeForestFloorCoverage(7,11,{.4f,.7f,.2f},1.f,ForestFloorMaskSelector::ForestWood);
    if(!Same(fullRiver.soil,.15f)||!Same(fullRiver.rock,.85f))return 19;
    rejected=false;
    try{(void)CreateTerrainMesh(terrain,true,static_cast<ForestFloorMaskSelector>(255));}catch(const std::invalid_argument&){rejected=true;}
    if(!rejected)return 20;

    std::cout<<"PASS forest-floor-mask candidate\n"
        <<"terrain_vertices="<<legacyMesh.vertices.size()<<" legacy_mesh_fp="<<legacyMeshFp<<"\n"
        <<"current_layer_coverage_vertices="<<legacyCoverageVertices
        <<" current_zero_coverage_nonbridge_nonriver="<<legacyInteriorGap<<"\n"
        <<"legacy_trail_support="<<legacyTrailVertices<<" candidate_trail_support="<<candidateTrailVertices
        <<" legacy_trail_core="<<legacyTrailCore<<" candidate_trail_core="<<candidateTrailCore<<"\n"
        <<"legacy_trail_weight_sum="<<legacyTrailWeight<<" candidate_trail_weight_sum="<<candidateTrailWeight<<"\n"
        <<"candidate_soil_vertices="<<candidateSoilVertices<<" candidate_rock_vertices="<<candidateRockVertices
        <<" candidate_soil_strong="<<candidateSoilStrong<<" candidate_rock_strong="<<candidateRockStrong<<"\n"
        <<"candidate_soil_weight_sum="<<candidateSoilWeight<<" candidate_rock_weight_sum="<<candidateRockWeight
        <<" candidate_max_sum="<<maximumSum<<" candidate_fp="<<candidateFp<<"\n"
        <<"bridge_checks="<<bridgeChecks<<" analytic_seam_checks="<<seamChecks<<" mesh_seam_checks="<<meshSeamChecks
        <<" invalid_cases=3 full_river_exact=1\n";
    return 0;
} catch(const std::exception& error) {
    std::cerr<<"FAIL exception="<<error.what()<<'\n';return 100;
}
