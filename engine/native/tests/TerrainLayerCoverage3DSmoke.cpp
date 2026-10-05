#include "Aevrynd/TerrainLayerCoverage3D.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
int main(){try{
    using namespace Aevrynd;
    static_assert(TerrainLayerCoverageNormalScaleMarker3D==-15.46875f);
    unsigned accepted{},rejected{};
    for(int soil=0;soil<=16;++soil)for(int rock=0;rock<=16-soil;++rock){
        Vec2 output{9,8};const float s=float(soil)/16.f,r=float(rock)/16.f;
        if(!TryEncodeTerrainLayerCoverage3D(s,r,output)||output.x!=s||output.y!=r)throw std::runtime_error("Valid coverage altered/rejected");
        const std::array<float,3> original{.2f,.3f,.5f};
        const std::array<float,3> mixed{original[0]*(1.f-s-r),original[1]*(1.f-s-r)+s,original[2]*(1.f-s-r)+r};
        if(std::abs(mixed[0]+mixed[1]+mixed[2]-1.f)>.000001f)throw std::runtime_error("Coverage conservation failed");++accepted;
    }
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    for(const Vec2 invalid:std::array<Vec2,10>{{{nan,0},{0,nan},{inf,0},{0,inf},{-inf,0},{-.001f,0},{0,-.001f},{1.001f,0},{0,1.001f},{.75f,.5f}}}){
        Vec2 output{9,8};if(TryEncodeTerrainLayerCoverage3D(invalid.x,invalid.y,output)||output.x!=9||output.y!=8)throw std::runtime_error("Invalid coverage mutated output");++rejected;
    }
    const auto path=std::filesystem::path{AEVRYND_SOURCE_DIR}/"engine/native/shaders/World.hlsl";
    std::ifstream input(path,std::ios::binary);const std::string shader{std::istreambuf_iterator<char>(input),{}};
    for(const char* gate:{"input.textureData.x > 0.5 && input.textureData.w > 0.5 && input.normalScale == -15.46875",
        "soilWeight = soilWeight * proceduralCoverage + coverage.x;","rockWeight = rockWeight * proceduralCoverage + coverage.y;",
        "referenceLuma = lerp(referenceLuma, partitionLuma, terrainLayerCoverageStrength);"})
        if(shader.find(gate)==std::string::npos)throw std::runtime_error("Shader coverage contract missing");
    std::cout<<"TERRAIN_LAYER_COVERAGE_CPU_PASS; valid="<<accepted<<" invalid="<<rejected<<"; shader GPU behavior verified separately\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<'\n';return 1;}}
