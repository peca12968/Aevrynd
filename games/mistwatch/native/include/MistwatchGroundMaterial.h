#pragma once
#include <Aevrynd/MaterialLibrary3D.h>

namespace Mistwatch {
inline Aevrynd::MaterialAssetDefinition3D GroundMaterialDefinition(bool legacy=false) {
    Aevrynd::MaterialAssetDefinition3D definition{"ground",
        {"poly-haven/forest-ground-04/slot-0-base.dds","poly-haven/forest-ground-04/slot-0-arm.dds",2048,
         "poly-haven/forest-ground-04/slot-0-normal.dds"},
        {{1,1,1,1},{.92f,0,0,.8f},false,{},.25f}};
    definition.material.normalScale=.65f;
    definition.textures.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::PackedOrm;
    constexpr const char* hashes[]{
        "00850c7c033be2bcb2818c0ab3db91a62c8353eba746daede3c0de373c3e32b4",
        "3427bf0a83e0c33326a207f700305620402fd0fb6c532240f204bcebefeabcc4",
        "56e687b2a208f2eaaf9b0e3670530f0a36e5e88d6e74b26090633daaa4e9db93"};
    for(unsigned channel=0;channel<3;++channel)definition.textures.integrity[channel]={5592580,hashes[channel]};
    // 舊材質僅供截圖單變量比較，正式玩家預設不使用此分支。
    if(legacy)definition.textures={"ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg",
        "ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",1024,
        "ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"};
    return definition;
}
inline Aevrynd::MaterialAssetDefinition3D GroundDiagnosticDefinition(bool legacy,bool neutralBase,bool neutralSurface) {
    auto definition=GroundMaterialDefinition(legacy);definition.name="ground-diagnostic";
    // 只替換被隔離的通道，其他 DDS 保留原 SHA；不退回舊版來源。
    if(neutralBase){definition.textures.baseColorPath="diagnostics/neutral-base.bmp";definition.textures.integrity[0]={};}
    if(neutralSurface){
        definition.textures.roughnessPath="diagnostics/neutral-surface.bmp";definition.textures.integrity[1]={};
        // 舊中性 BMP 僅有灰階粗糙度，不能誤讀為 AO／金屬度。
        definition.textures.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::RoughnessR;
    }
    return definition;
}
}
