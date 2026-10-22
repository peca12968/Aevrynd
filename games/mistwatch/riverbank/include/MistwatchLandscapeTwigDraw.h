#pragma once
#include <Aevrynd/Scene3D.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Mistwatch::LandscapeTwigDraw {
// handle 的相等只看數值；樹群非 twig 收據也必須核對完整內容及材質語意身分。
inline bool ExactResourceBindings(const Aevrynd::MeshInstance3D& item,
                                  const Aevrynd::MeshHandle3D& mesh,
                                  const Aevrynd::MaterialTextureSetHandle3D& textures) {
    return bool(mesh)&&mesh.vertexCount>0&&mesh.indexCount>0&&item.mesh.value==mesh.value&&
        item.mesh.contentPrimary==mesh.contentPrimary&&item.mesh.contentSecondary==mesh.contentSecondary&&
        item.mesh.vertexCount==mesh.vertexCount&&item.mesh.indexCount==mesh.indexCount&&
        item.textures.value==textures.value&&item.textures.semanticIdentity==textures.semanticIdentity;
}
// 運算次序對齊 Scene3D::MeshLodChain；呼叫端再套正式世界姿態。
inline Aevrynd::MeshBounds3D UnionBounds(const Aevrynd::MeshResource3D& nearMesh,
                                      const Aevrynd::MeshResource3D& farMesh) {
    using namespace Aevrynd;
    Vec3 minimum=nearMesh.bounds.center-nearMesh.bounds.extents;
    Vec3 maximum=nearMesh.bounds.center+nearMesh.bounds.extents;
    const Vec3 levelMinimum=farMesh.bounds.center-farMesh.bounds.extents;
    const Vec3 levelMaximum=farMesh.bounds.center+farMesh.bounds.extents;
    minimum.x=(std::min)(minimum.x,levelMinimum.x);minimum.y=(std::min)(minimum.y,levelMinimum.y);
    minimum.z=(std::min)(minimum.z,levelMinimum.z);
    maximum.x=(std::max)(maximum.x,levelMaximum.x);maximum.y=(std::max)(maximum.y,levelMaximum.y);
    maximum.z=(std::max)(maximum.z,levelMaximum.z);
    MeshBounds3D bounds;bounds.center=(minimum+maximum)*.5f;
    bounds.extents=(maximum-minimum)*.5f;bounds.radius=Length(bounds.extents);return bounds;
}
struct Expected {
    Aevrynd::MeshHandle3D nearMesh{},farMesh{};
    std::size_t nearTriangles{},farTriangles{};
    Aevrynd::MeshLodChainSelection3D selection{};
    Aevrynd::RenderObjectIdentity3D identity{};
    Aevrynd::Material3D material{};
};
struct Result {
    unsigned draws{},nearDraws{},farDraws{};
    std::size_t triangles{};
    Aevrynd::MeshRenderLayer3D layer{Aevrynd::MeshRenderLayer3D::Opaque};
    bool culled{true},transition{};
};

// 每個背景樹冠一份有限狀態，不擁有 GPU 資源，也不改動原始世界配置。
// selection 必須由 Scene 使用的相同 union bounds 與真正相機推導。
class Observer {
public:
    explicit Observer(Expected expected):expected_(expected) {
        const auto& e=expected_;
        Need(e.nearMesh&&e.farMesh&&e.nearMesh.value!=e.farMesh.value&&
            e.nearTriangles>0&&e.farTriangles>0&&e.nearTriangles>=e.farTriangles,
            "LANDSCAPE_TWIG_EXPECTED_MESH");
        Need(e.nearTriangles<=(std::numeric_limits<std::size_t>::max)()-e.farTriangles,
            "LANDSCAPE_TWIG_TRIANGLE_OVERFLOW");
        Need(bool(e.identity),"LANDSCAPE_TWIG_EXPECTED_IDENTITY");
        const auto& s=e.selection;
        Need(std::isfinite(s.farCoverage),"LANDSCAPE_TWIG_EXPECTED_COVERAGE");
        if(s.transition)
            Need(s.nearLevel==0&&s.farLevel==1&&s.farCoverage>0.f&&s.farCoverage<1.f&&
                s.primaryLevel==(s.farCoverage>=.5f?1u:0u),"LANDSCAPE_TWIG_EXPECTED_TRANSITION");
        else
            Need((s.primaryLevel==0&&s.nearLevel==0&&s.farLevel==1&&s.farCoverage==0.f)||
                 (s.primaryLevel==1&&s.nearLevel==1&&s.farLevel==1&&s.farCoverage==1.f),
                 "LANDSCAPE_TWIG_EXPECTED_ENDPOINT");
        const auto& m=e.material;
        for(float f:{m.tint.r,m.tint.g,m.tint.b,m.tint.a,m.surface.roughness,m.surface.metallic,
                     m.surface.emissive,m.surface.occlusion,m.textureScale,m.normalScale,m.alphaCutoff})
            Need(std::isfinite(f),"LANDSCAPE_TWIG_EXPECTED_MATERIAL_FINITE");
        Need(m.textures&&m.useUv&&m.alphaMode==Aevrynd::MaterialAlphaMode3D::Mask&&
             m.materialClass==Aevrynd::MaterialClass3D::Foliage,"LANDSCAPE_TWIG_EXPECTED_MASKED_FOLIAGE");
    }

    void Observe(const Aevrynd::MeshInstance3D& item) {
        // 失敗後永久無效，呼叫端不能忽略一次例外再取得通過收據。
        Need(!failed_,"LANDSCAPE_TWIG_OBSERVER_FAILED");
        try { ObserveChecked(item); }
        catch(...) { failed_=true; throw; }
    }

    [[nodiscard]] Result Finish() const {
        Need(!failed_,"LANDSCAPE_TWIG_OBSERVER_FAILED");
        if(result_.draws&&expected_.selection.transition)
            Need(result_.nearDraws==1&&result_.farDraws==1,"LANDSCAPE_TWIG_TRANSITION_MISSING_ENDPOINT");
        return result_;
    }

private:
    static void Need(bool valid,const char* message) {
        if(!valid)throw std::invalid_argument(message);
    }
    static bool SameMesh(Aevrynd::MeshHandle3D a,Aevrynd::MeshHandle3D b) {
        // handle 的 operator== 僅比較 value；收據也核對內容身分與形狀。
        return a.value==b.value&&a.contentPrimary==b.contentPrimary&&a.contentSecondary==b.contentSecondary&&
               a.vertexCount==b.vertexCount&&a.indexCount==b.indexCount;
    }
    void ObserveChecked(const Aevrynd::MeshInstance3D& item) {
        using namespace Aevrynd;
        Need(item.identity==expected_.identity,"LANDSCAPE_TWIG_IDENTITY");
        Need(item.layer==MeshRenderLayer3D::Opaque||item.layer==MeshRenderLayer3D::ShadowOnly,
             "LANDSCAPE_TWIG_LAYER");
        if(result_.draws)Need(item.layer==result_.layer,"LANDSCAPE_TWIG_MIXED_LAYERS");
        const auto& m=expected_.material;
        Need(item.tint==PackColorRgba8(m.tint)&&item.surface==PackSurface3D(m.surface)&&
             item.textures.value==m.textures.value&&item.textures.semanticIdentity==m.textures.semanticIdentity&&
             item.textureScale==(std::max)(.001f,m.textureScale)&&
             item.normalScale==Clamp(m.normalScale,-16.f,15.96875f)&&item.useUv==m.useUv&&
             item.materialClass==m.materialClass&&item.alphaMode==m.alphaMode&&
             item.alphaCutoff==Clamp(m.alphaCutoff,0.f,1.f)&&item.doubleSided==m.doubleSided&&
             item.occlusionOpacity==1.f,"LANDSCAPE_TWIG_MATERIAL");
        // 陰影距離可關閉 opaque 的投影；shadow-only 必須是真正投影提交。
        Need((!item.castsShadow||m.castsShadow)&&
             (item.layer!=MeshRenderLayer3D::ShadowOnly||item.castsShadow),"LANDSCAPE_TWIG_SHADOW");
        const bool isNear=SameMesh(item.mesh,expected_.nearMesh),isFar=SameMesh(item.mesh,expected_.farMesh);
        Need(isNear||isFar,"LANDSCAPE_TWIG_MESH");
        Need(result_.draws<2&&!(isNear?result_.nearDraws:result_.farDraws),"LANDSCAPE_TWIG_DUPLICATE_ENDPOINT");
        const auto& s=expected_.selection;
        if(s.transition) {
            Need(item.lodCoverage==s.farCoverage,"LANDSCAPE_TWIG_COVERAGE");
            Need((item.lodSeed&~15u)==0&&(item.lodSeed&8u)==(isNear?8u:0u),"LANDSCAPE_TWIG_INVERT");
            if(result_.draws)Need((item.lodSeed&7u)==seed_,"LANDSCAPE_TWIG_SEED");
        } else {
            Need(isNear==(s.primaryLevel==0)&&result_.draws==0,"LANDSCAPE_TWIG_WRONG_ENDPOINT");
            Need(item.lodCoverage==1.f&&item.lodSeed==0,"LANDSCAPE_TWIG_SINGLE_COVERAGE");
        }
        if(result_.draws)Need(item.castsShadow==castsShadow_,"LANDSCAPE_TWIG_PAIR_SHADOW");
        seed_=item.lodSeed&7u;castsShadow_=item.castsShadow;
        ++result_.draws;if(isNear)++result_.nearDraws;else ++result_.farDraws;
        result_.triangles+=isNear?expected_.nearTriangles:expected_.farTriangles;
        result_.layer=item.layer;result_.culled=false;result_.transition=s.transition;
    }
    Expected expected_;
    Result result_;
    unsigned seed_{};
    bool castsShadow_{},failed_{};
};
}
