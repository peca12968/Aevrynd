#include <Aevrynd/Renderer.h>
#include <Aevrynd/Engine.h>
#include <array>
#include <iostream>
using namespace Aevrynd;
int main(){
    static_assert(sizeof(LocalReflectionProbe3D)==48);
    const std::array<LocalReflectionProbe3D,2> probes{{{{7},{-2,-2,-2},{2,2,2},{0,0,0},1},{{8},{-1,-1,-1},{1,1,1},{0,0,0},2}}};
    const std::array all{LocalReflectionMaterialMask3D::All,LocalReflectionMaterialMask3D::All};
    const std::array water{LocalReflectionMaterialMask3D::All,LocalReflectionMaterialMask3D::Water};
    for(unsigned m=0;m<4;++m){
        const auto material=MaterialClass3D(m);
        if(SelectLocalReflectionProbeForMaterial3D({},probes,all,material)!=SelectLocalReflectionProbe3D({},probes))return 1;
        if(SelectLocalReflectionProbeForMaterial3D({},probes,water,material)!=(material==MaterialClass3D::Water?1u:0u))return 2;
        for(unsigned mask=0;mask<16;++mask)if(LocalReflectionMaterialAllowed3D(LocalReflectionMaterialMask3D(mask),material)!=bool(mask&(1u<<m)))return 3;
    }
    if(SelectLocalReflectionProbeForMaterial3D({},probes,{},MaterialClass3D::Water)!=SIZE_MAX)return 4;
    auto rejected=all;rejected.fill(LocalReflectionMaterialMask3D(16));
    if(SelectLocalReflectionProbeForMaterial3D({},probes,rejected,MaterialClass3D::Water)!=SIZE_MAX||LocalReflectionMaterialAllowed3D(LocalReflectionMaterialMask3D::All,MaterialClass3D(255)))return 5;
    auto none=all;none.fill(LocalReflectionMaterialMask3D::None);
    if(SelectLocalReflectionProbeForMaterial3D({},probes,none,MaterialClass3D::Water)!=SIZE_MAX||SelectLocalReflectionProbeForMaterial3D({100,0,0},probes,all,MaterialClass3D::Water)!=SIZE_MAX)return 6;
    std::cout<<"MATERIAL_DOMAIN_CPU_PASS probe="<<sizeof(LocalReflectionProbe3D)<<" draw="<<sizeof(DrawList)<<" renderer="<<sizeof(D3D12Renderer)<<" engine="<<sizeof(Engine)<<" mesh="<<sizeof(MeshInstance3D)<<" skinned="<<sizeof(SkinnedMeshInstance3D)<<" stats="<<sizeof(RendererStats)<<'\n';
}
