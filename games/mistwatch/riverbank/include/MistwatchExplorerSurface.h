#pragma once
#include "MistwatchExplorer.h"
#include <array>
#include <cmath>
#include <limits>
#include <ostream>

namespace Mistwatch::ExplorerSurface {
struct Window {std::uint32_t firstIndex{},indexCount{};ExplorerArtMaterial material{};bool eye{};};
struct Plan {std::array<Window,9> windows{};};
struct Submission {std::uint32_t acceptedWindows{},skinWindows{},eyeWindows{},hairWindows{},clothWindows{},texturedWindows{},lensIndices{};};
inline bool FiniteColor(Aevrynd::Color c){for(float x:{c.r,c.g,c.b,c.a})if(!std::isfinite(x)||x<0||x>1)return false;return true;}
inline bool ValidMaterial(const Aevrynd::Material3D& m){
    if(!FiniteColor(m.tint))return false;
    for(float x:{m.surface.roughness,m.surface.metallic,m.surface.emissive,m.surface.occlusion})if(!std::isfinite(x)||x<0||x>1)return false;
    if(!std::isfinite(m.textureScale)||m.textureScale<=0||!std::isfinite(m.normalScale)||m.normalScale<-16.f||m.normalScale>15.96875f||
        !std::isfinite(m.baseTextureScale)||m.baseTextureScale<0||!std::isfinite(m.detailTextureScale)||m.detailTextureScale<0||
        !std::isfinite(m.alphaCutoff)||m.alphaCutoff<0||m.alphaCutoff>1)return false;
    if(m.materialClass!=Aevrynd::MaterialClass3D::Surface||Aevrynd::NormalizeCharacterShadingModel3D(m.shadingModel)!=m.shadingModel||
        Aevrynd::NormalizeMaterialAlphaMode3D(m.alphaMode)!=m.alphaMode||m.alphaMode==Aevrynd::MaterialAlphaMode3D::Blend)return false;
    return m.alphaMode!=Aevrynd::MaterialAlphaMode3D::Mask||(m.textures&&m.useUv);
}
inline bool BuildPlan(const Aevrynd::SkinnedModel3D& model,const ExplorerArtReceipt& art,Plan& out){
    out={};const auto& mesh=model.bindMesh;
    if(mesh.indices.empty()||mesh.indices.size()>(std::numeric_limits<std::uint32_t>::max)())return false;
    std::uint64_t cursor{};for(unsigned i=0;i<art.ranges.size();++i){const auto& r=art.ranges[i];
        if(r.material!=static_cast<ExplorerArtMaterial>(i)||r.firstIndex!=cursor||!r.indexCount||r.indexCount%3u)return false;
        cursor+=r.indexCount;if(cursor>mesh.indices.size())return false;}
    if(cursor!=mesh.indices.size())return false;
    const auto& skin=art.ranges[static_cast<unsigned>(ExplorerArtMaterial::Skin)];
    const std::uint64_t skinEnd=std::uint64_t(skin.firstIndex)+skin.indexCount;
    std::uint64_t previous=skin.firstIndex;
    for(const auto& lens:art.eyeLensSpans){
        const std::uint64_t end=std::uint64_t(lens.firstIndex)+lens.indexCount,vertexEnd=std::uint64_t(lens.firstVertex)+lens.vertexCount;
        if(lens.firstIndex%3u||lens.indexCount!=702u||lens.vertexCount!=131u||lens.firstIndex<=previous||end>=skinEnd||vertexEnd>mesh.vertices.size())return false;
        for(std::uint32_t index=lens.firstIndex;std::uint64_t(index)<end;++index){const auto v=mesh.indices[index];
            if(v<lens.firstVertex||std::uint64_t(v)>=vertexEnd||v>=model.influences.size())return false;
            const auto& influence=model.influences[v];if(influence.joints[0]!=2||influence.weights[0]!=65535||
                influence.weights[1]||influence.weights[2]||influence.weights[3])return false;}
        previous=end;
    }
    unsigned at{};const auto append=[&](std::uint32_t first,std::uint32_t count,ExplorerArtMaterial material,bool eye){
        out.windows.at(at++)={first,count,material,eye};};
    for(const auto& r:art.ranges){if(r.material!=ExplorerArtMaterial::Skin){append(r.firstIndex,r.indexCount,r.material,false);continue;}
        std::uint32_t first=r.firstIndex;
        for(const auto& lens:art.eyeLensSpans){append(first,lens.firstIndex-first,r.material,false);append(lens.firstIndex,lens.indexCount,r.material,true);first=lens.firstIndex+lens.indexCount;}
        append(first,r.firstIndex+r.indexCount-first,r.material,false);}
    return at==out.windows.size();
}
inline Aevrynd::Material3D WindowMaterial(const Window& window,const std::array<Aevrynd::Material3D,5>& materials){
    auto result=materials.at(static_cast<unsigned>(window.material));
    if(window.eye){result.surface.roughness=.16f;result.shadingModel=Aevrynd::CharacterShadingModel3D::Eye;}
    return result;
}
inline bool Submit(Aevrynd::DrawList& draw,const Aevrynd::SkinnedModel3D& model,const ExplorerArtReceipt& art,
    Aevrynd::SkinnedMeshHandle3D mesh,Aevrynd::SkinPaletteRange3D palette,Aevrynd::Matrix4 transform,
    const std::array<Aevrynd::Material3D,5>& materials,Submission& receipt){
    receipt={};Plan plan;if(!BuildPlan(model,art,plan)||!mesh||!draw.ContainsSkinPalette(palette)||
        palette.skeletonCompatibilityId!=model.skeletonCompatibilityId||mesh.skeletonCompatibilityId!=model.skeletonCompatibilityId||
        palette.jointCount!=15||mesh.jointCount!=15||mesh.indexCount!=model.bindMesh.indices.size()||
        draw.skinnedMeshInstances.size()+plan.windows.size()>Aevrynd::DrawList::MaximumSkinnedMeshInstancesPerFrame)return false;
    for(const auto& row:transform.values)for(float x:row)if(!std::isfinite(x))return false;
    for(const auto& window:plan.windows)if(!ValidMaterial(WindowMaterial(window,materials)))return false;
    // 尾欄完整轉送既有API；不是新renderer規則。先驗證所有window，避免已知非法材質造成半提交。
    for(const auto& window:plan.windows){const auto m=WindowMaterial(window,materials);
        if(!draw.SkinnedMeshInstance(mesh,palette,transform,m.tint,m.surface,m.textures,m.textureScale,m.useUv,m.materialClass,
            m.castsShadow,Aevrynd::DirectionalShadowAllCascades3D,window.firstIndex,window.indexCount,m.shadingModel,
            m.alphaMode,m.alphaCutoff,m.doubleSided,{},Aevrynd::Matrix4::Identity(),false,m.normalScale,m.baseTextureScale,m.detailTextureScale))return false;
        ++receipt.acceptedWindows;receipt.skinWindows+=m.shadingModel==Aevrynd::CharacterShadingModel3D::Skin;
        receipt.eyeWindows+=m.shadingModel==Aevrynd::CharacterShadingModel3D::Eye;
        receipt.hairWindows+=m.shadingModel==Aevrynd::CharacterShadingModel3D::Hair;
        receipt.clothWindows+=m.shadingModel==Aevrynd::CharacterShadingModel3D::Cloth;
        receipt.texturedWindows+=bool(m.textures);if(window.eye)receipt.lensIndices+=window.indexCount;
    }
    return receipt.acceptedWindows==9&&receipt.eyeWindows==2&&receipt.lensIndices==1404;
}
inline bool WriteReceipt(std::ostream& out,const Aevrynd::RendererStats& stats,const Submission& submission){
    // actual是renderer當幀accepted aggregate；本場景只有explorer使用四種character route。
    // 引擎沒有per-lens GPU counter，不把draw-list bool重命名成GPU accepted。
    const bool routes=submission.acceptedWindows==9&&submission.eyeWindows==2&&submission.lensIndices==1404&&
        stats.lastSubmittedSkinShadedInstances==submission.skinWindows&&stats.lastSubmittedEyeShadedInstances==submission.eyeWindows&&
        stats.lastSubmittedHairShadedInstances==submission.hairWindows&&stats.lastSubmittedClothShadedInstances==submission.clothWindows&&
        stats.lastSubmittedTexturedSkinnedInstances==submission.texturedWindows;
    out<<"\nexplorer_surface_windows="<<submission.acceptedWindows<<"\nexplorer_surface_skin_windows="<<submission.skinWindows
        <<"\nexplorer_surface_eye_windows="<<submission.eyeWindows<<"\nexplorer_surface_textured_windows="<<submission.texturedWindows
        <<"\nexplorer_lens_drawlist_accepted="<<submission.eyeWindows<<"\nexplorer_lens_indices="<<submission.lensIndices
        <<"\nrenderer_accepted_skin_instances="<<stats.lastSubmittedSkinShadedInstances
        <<"\nrenderer_accepted_eye_instances="<<stats.lastSubmittedEyeShadedInstances
        <<"\nrenderer_accepted_hair_instances="<<stats.lastSubmittedHairShadedInstances
        <<"\nrenderer_accepted_cloth_instances="<<stats.lastSubmittedClothShadedInstances
        <<"\nrenderer_accepted_textured_skinned_instances="<<stats.lastSubmittedTexturedSkinnedInstances
        <<"\nexplorer_surface_routes_match="<<(routes?1:0)<<'\n';
    return routes&&bool(out);
}
}
