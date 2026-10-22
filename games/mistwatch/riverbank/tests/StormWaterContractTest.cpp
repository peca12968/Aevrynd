#include "MistwatchStormRiver.h"
#include <fstream>
#include <iostream>
#include <string>
#include <cstring>
#include <limits>
using namespace Aevrynd;
namespace { unsigned checks{};void Need(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);} }
int main(int argc,char** argv){
    try{
        Need(argc==2,"CURRENT_FINAL_TERRAIN_REQUIRED");
        HeightField3D original({{-50,0,-20},201,241,.5f}),field(original);
        std::ifstream file(argv[1]);std::string line;std::getline(file,line);Need(line=="x\tz\tbefore\tafter","FIELD_SCHEMA");
        float x{},z{},a{},b{};unsigned rows{};
        while(file>>x>>z>>a>>b){Need(rows<48441&&x==-50+float(rows%201)*.5f&&z==-20+float(rows/201)*.5f,"REAL_FIELD_ORDER");
            Need(original.SetHeight(rows%201,rows/201,a)&&field.SetHeight(rows%201,rows/201,b),"FINITE_FIELD_INPUT");++rows;}
        Need(file.eof()&&rows==48441,"COMPLETE_CURRENT_FIELD");original.RecalculateRange();field.RecalculateRange();
        const auto stale=CreateHeightFieldWaterMesh3D(original,-4.f),refreshed=CreateHeightFieldWaterMesh3D(field,-4.f);
        Need(stale.Valid()&&refreshed.Valid(),"EXISTING_WATER_CAPABILITY");
        const bool differs=stale.vertices.size()!=refreshed.vertices.size()||stale.indices!=refreshed.indices||
            std::memcmp(stale.vertices.data(),refreshed.vertices.data(),stale.vertices.size()*sizeof(Vertex3D))!=0;
        Need(differs,"REPRO_FINAL_TERRAIN_REQUIRES_RECLIPPED_WATER");
        const auto mesh=Mistwatch::StormRiver::Build(field),plain=CreateHeightFieldWaterMesh3D(field,Mistwatch::StormRiver::Height);
        Need(mesh.Valid()&&mesh.vertices.size()==plain.vertices.size()&&mesh.indices==plain.indices,"CANONICAL_CLIPPED_TRIANGLE_TOPOLOGY_UNCHANGED");
        unsigned edge{},interior{};float maximumDryError{};
        for(std::size_t i=0;i<mesh.vertices.size();++i){
            const auto& v=mesh.vertices[i];const auto& p=plain.vertices[i];
            Need(v.x==p.x&&v.y==p.y&&v.z==p.z&&v.u==p.u&&v.v==p.v&&v.color==p.color&&v.y==Mistwatch::StormRiver::Height,"ACTUAL_SHORE_CLIP_NOT_NEW_COVER_GEOMETRY");
            const auto normal=UnpackNormalSnorm8(v.normal);const float depth=(std::max)(0.f,v.y-field.SampleHeight(v.x,v.z));
            const unsigned expected=unsigned(std::floor((std::min)(1.f,depth/Mistwatch::StormRiver::ShoreFade)*127.f));
            Need((v.normal>>24)==expected&&normal.x==0&&normal.y==1&&normal.z==0,"EXACT_DOWNWARD_QUANTIZED_SHORE_WAVE_WEIGHT");
            edge+=unsigned(expected==0);interior+=unsigned(expected==127);
            maximumDryError=(std::max)(maximumDryError,field.SampleHeight(v.x,v.z)-v.y);
        }
        Need(edge>100&&interior>100,"BOTH_FIXED_SHORE_AND_MOVING_INTERIOR");
        Need(maximumDryError<.0001f,"FLOAT_CLIP_ROUNDING_ONLY_NO_DRY_TERRAIN_PATCH");
        Need(CreateShoreWeightedHeightFieldWaterMesh3D(field,-1.4f,0).vertices.empty(),"ZERO_FADE_REJECTED");
        Need(CreateShoreWeightedHeightFieldWaterMesh3D(field,-1.4f,std::numeric_limits<float>::quiet_NaN()).vertices.empty(),"NONFINITE_FADE_REJECTED");
        std::cout<<"STALE_WATER_REPRODUCED old_vertices="<<stale.vertices.size()<<" reclipped_vertices="<<refreshed.vertices.size()<<"\n";
        std::cout<<"STORM_WATER_FINAL_SHORE_PASS vertices="<<mesh.vertices.size()<<" indices="<<mesh.indices.size()<<" shore="<<edge<<" interior="<<interior<<" maximum_float_clip_error="<<maximumDryError<<" checks="<<checks<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"STORM_WATER_FIRST_DIVERGENCE="<<e.what()<<"\n";return 1;}
}
