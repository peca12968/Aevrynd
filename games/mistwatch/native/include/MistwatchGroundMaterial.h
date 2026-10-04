#pragma once
#include <Aevrynd/MaterialLibrary3D.h>

namespace Mistwatch {
inline Aevrynd::MaterialAssetDefinition3D GroundMaterialDefinition(bool legacy=false,bool singleSurface=false) {
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
    if(legacy) {
        definition.textures={"ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg",
            "ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg",1024,
            "ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"};
        return definition;
    }
    // 同一執行檔的比較基準完整保留原森林 Surface，不能混入新層或 Terrain 行為。
    if(singleSurface)return definition;
    definition.material.materialClass=Aevrynd::MaterialClass3D::Terrain;
    auto& soil=definition.textures.additionalLayers[0];
    soil.baseColorPath="ambientcg/ground108/slot-0-base.dds";
    soil.roughnessPath="ambientcg/ground108/slot-0-arm.dds";
    soil.normalPath="ambientcg/ground108/slot-0-normal.dds";
    soil.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::PackedOrm;
    constexpr const char* soilHashes[]{
        "2703b20ee77d556daa64535af15433bb3157083174e6d71671aa052ccae11589",
        "ce0c636264a9c1c5cb5055259dde80b92c9bea3145dc8ea8f28a7f9f3903e5ed",
        "e63efd69f4d1e7978f1cd152c5b2115151bcbc208f9f661bc35af0ec6af80032"};
    auto& rock=definition.textures.additionalLayers[1];
    rock.baseColorPath="poly-haven/rock_surface/1k/base-color.jpg";
    rock.roughnessPath="poly-haven/rock_surface/1k/roughness.jpg";
    rock.normalPath="poly-haven/rock_surface/1k/normal-dx.jpg";
    rock.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::RoughnessR;
    constexpr std::uint64_t rockBytes[]{886291,642680,1161843};
    constexpr const char* rockHashes[]{
        "07edda2bbe4715b01aae2e4bac9f21f204cf4aae37b4733ef01de33281e5e60e",
        "ca710d895b01cf323bf00f53194c43504a39d2c964fbfc343c4f45c7d41b10dc",
        "1e1aa5b06f74d22734318cd93ecf49ffe7095c4b2307c5c9b3a4550207c6b1b7"};
    for(unsigned channel=0;channel<3;++channel) {
        soil.integrity[channel]={1398276,soilHashes[channel]};
        rock.integrity[channel]={rockBytes[channel],rockHashes[channel]};
    }
    return definition;
}
inline Aevrynd::MaterialAssetDefinition3D GroundDiagnosticDefinition(bool legacy,bool neutralBase,bool neutralSurface,bool singleSurface=false) {
    auto definition=GroundMaterialDefinition(legacy,singleSurface);definition.name="ground-diagnostic";
    // 只替換被隔離的通道，其他來源保留原封印；不退回舊版來源。
    if(neutralBase){definition.textures.baseColorPath="diagnostics/neutral-base.bmp";definition.textures.integrity[0]={};}
    if(neutralSurface){
        definition.textures.roughnessPath="diagnostics/neutral-surface.bmp";definition.textures.integrity[1]={};
        // 舊中性 BMP 僅有灰階粗糙度，不能誤讀為 AO／金屬度。
        definition.textures.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::RoughnessR;
    }
    // 三層必須隔離同一通道；Surface 基準的空槽仍維持空槽。
    for(auto& layer:definition.textures.additionalLayers) {
        if(layer.baseColorPath.empty())continue;
        if(neutralBase){layer.baseColorPath="diagnostics/neutral-base.bmp";layer.integrity[0]={};}
        if(neutralSurface){
            layer.roughnessPath="diagnostics/neutral-surface.bmp";layer.integrity[1]={};
            layer.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::RoughnessR;
        }
    }
    return definition;
}
}
