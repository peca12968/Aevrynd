#pragma once
#include "MistwatchLandscapeTwigDraw.h"
#include <array>
#include <bit>
#include <span>

namespace Mistwatch::ShoulderDraw {
inline constexpr std::uint64_t InstanceNamespace=0x4d57534847525631ull;
inline constexpr std::size_t SlotCount=48;
struct Expected {
    Aevrynd::MeshResource3D mesh{};
    Aevrynd::Transform3D pose{};
    Aevrynd::Material3D material{};
};
struct Result {
    unsigned opaque{},shadowOnly{},draws{};
    std::size_t triangles{};
    // 0：未提交；1：不透明；2：僅陰影。未提交不推論為資源遺失。
    std::array<std::uint8_t,SlotCount> visibility{};
};
class Observer {
public:
    explicit Observer(std::span<const Expected> expected) {
        Need(expected.empty()||expected.size()==SlotCount,"SHOULDER_EXPECTED_COUNT");
        enabled_=!expected.empty();
        for(std::size_t i=0;i<expected.size();++i) {
            expected_[i]=expected[i];const auto& e=expected_[i];
            Need(bool(e.mesh)&&e.mesh.vertexCount>0&&e.mesh.indexCount%3==0&&
                 e.mesh.vertexCount==e.mesh.handle.vertexCount&&e.mesh.indexCount==e.mesh.handle.indexCount&&
                 e.mesh.handle.contentPrimary&&e.mesh.handle.contentSecondary,"SHOULDER_EXPECTED_RESOURCE");
            Need(e.mesh.TriangleCount()<=(std::numeric_limits<std::size_t>::max)()-maximumTriangles_,
                 "SHOULDER_EXPECTED_OVERFLOW");maximumTriangles_+=e.mesh.TriangleCount();
            for(float f:{e.pose.position.x,e.pose.position.y,e.pose.position.z,e.pose.rotation.x,
                         e.pose.rotation.y,e.pose.rotation.z,e.pose.scale.x,e.pose.scale.y,e.pose.scale.z})
                Need(std::isfinite(f),"SHOULDER_EXPECTED_POSE");
            Need(e.pose.scale.x>0&&e.pose.scale.y>0&&e.pose.scale.z>0,"SHOULDER_EXPECTED_SCALE");
            matrices_[i]=e.pose.Matrix();normals_[i]=Aevrynd::CreateNormalMatrix(matrices_[i]);
            for(std::size_t r=0;r<4;++r)for(std::size_t c=0;c<4;++c)
                Need(std::isfinite(matrices_[i][r][c])&&std::isfinite(normals_[i][r][c]),"SHOULDER_EXPECTED_MATRIX");
            const auto& m=e.material;
            for(float f:{m.tint.r,m.tint.g,m.tint.b,m.tint.a,m.surface.roughness,m.surface.metallic,
                         m.surface.emissive,m.surface.occlusion,m.textureScale,m.normalScale,m.alphaCutoff})
                Need(std::isfinite(f),"SHOULDER_EXPECTED_MATERIAL_FINITE");
            Need(m.textures&&m.textures.semanticIdentity&&m.useUv&&m.textureScale>0&&
                 (m.alphaMode==Aevrynd::MaterialAlphaMode3D::Opaque||m.alphaMode==Aevrynd::MaterialAlphaMode3D::Mask)&&
                 (m.materialClass==Aevrynd::MaterialClass3D::Surface||m.materialClass==Aevrynd::MaterialClass3D::Foliage),
                 "SHOULDER_EXPECTED_MATERIAL");
            // 這三欄僅蒙皮路徑使用；本靜態契約不接受無法提交的非預設值。
            Need(m.shadingModel==Aevrynd::CharacterShadingModel3D::Default&&
                 m.baseTextureScale==0.f&&m.detailTextureScale==0.f,"SHOULDER_UNSUBMITTED_MATERIAL");
        }
    }
    void Observe(const Aevrynd::MeshInstance3D& item) {
        Need(!failed_,"SHOULDER_OBSERVER_FAILED");
        try { ObserveChecked(item); }catch(...) { failed_=true;throw; }
    }
    [[nodiscard]] Result Finish()const {Need(!failed_,"SHOULDER_OBSERVER_FAILED");return result_;}
private:
    static void Need(bool value,const char* reason){if(!value)throw std::invalid_argument(reason);}
    static bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
    static bool MatrixBits(const Aevrynd::Matrix4& a,const Aevrynd::Matrix4& b){
        for(std::size_t r=0;r<4;++r)for(std::size_t c=0;c<4;++c)if(!Bits(a[r][c],b[r][c]))return false;
        return true;
    }
    void ObserveChecked(const Aevrynd::MeshInstance3D& item){
        using namespace Aevrynd;
        if(item.identity.id.high!=InstanceNamespace)return;
        Need(enabled_,"SHOULDER_DISABLED_SUBMISSION");
        Need(item.identity.generation==1&&item.identity.id.low>=1&&item.identity.id.low<=SlotCount&&
             !item.identity.hlodClusterId&&!item.identity.sharedLodDither&&!item.identity.hlodProxy,"SHOULDER_IDENTITY");
        const auto slot=std::size_t(item.identity.id.low-1);const auto& e=expected_[slot];const auto& m=e.material;
        Need(!result_.visibility[slot],"SHOULDER_DUPLICATE");
        Need(item.layer==MeshRenderLayer3D::Opaque||item.layer==MeshRenderLayer3D::ShadowOnly,"SHOULDER_LAYER");
        Need(LandscapeTwigDraw::ExactResourceBindings(item,e.mesh.handle,m.textures),"SHOULDER_RESOURCE");
        Need(MatrixBits(item.transform,matrices_[slot])&&MatrixBits(item.normalTransform,normals_[slot]),"SHOULDER_MATRIX");
        Need(!item.motionValid&&MatrixBits(item.previousTransform,matrices_[slot]),"SHOULDER_STATIC_MOTION");
        Need(item.tint==PackColorRgba8(m.tint)&&item.surface==PackSurface3D(m.surface)&&
             Bits(item.textureScale,(std::max)(.001f,m.textureScale))&&
             Bits(item.normalScale,Clamp(m.normalScale,-16.f,15.96875f))&&item.useUv==m.useUv&&
             item.materialClass==m.materialClass&&item.alphaMode==m.alphaMode&&
             Bits(item.alphaCutoff,Clamp(m.alphaCutoff,0.f,1.f))&&item.doubleSided==m.doubleSided,
             "SHOULDER_MATERIAL");
        Need(Bits(item.occlusionOpacity,1.f)&&Bits(item.lodCoverage,1.f)&&item.lodSeed==0,"SHOULDER_COVERAGE");
        // 正式 Scene 可因陰影距離關閉投影；不可反向開啟原材質禁止的投影。
        Need((!item.castsShadow||m.castsShadow)&&
             (item.layer!=MeshRenderLayer3D::ShadowOnly||item.castsShadow)&&
             (item.castsShadow?item.shadowCascadeMask!=0:item.shadowCascadeMask==0)&&
             (item.shadowCascadeMask&~DirectionalShadowAllCascades3D)==0&&
             item.shadowCascadeSelection==ShadowCascadeSelection3D::Automatic,"SHOULDER_SHADOW");
        const bool opaque=item.layer==MeshRenderLayer3D::Opaque;
        result_.visibility[slot]=opaque?1:2;++result_.draws;
        if(opaque)++result_.opaque;else ++result_.shadowOnly;
        result_.triangles+=e.mesh.TriangleCount();
    }
    std::array<Expected,SlotCount> expected_{};
    std::array<Aevrynd::Matrix4,SlotCount> matrices_{},normals_{};
    Result result_{};std::size_t maximumTriangles_{};bool enabled_{},failed_{};
};
}
