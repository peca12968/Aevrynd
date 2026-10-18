#define Mistwatch MistwatchSurfaceV285Reference
#include "reference-v285/include/MistwatchExplorer.h"
#undef Mistwatch
#include "MistwatchExplorerSurface.h"
#include <bit>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace Aevrynd;using namespace Mistwatch::ExplorerSurface;
unsigned checks{};
void Require(bool ok,const std::string& name){++checks;if(!ok)throw std::runtime_error(name);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool GeometryVertex(const Vertex3D& a,const Vertex3D& b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z)&&a.normal==b.normal&&Bits(a.u,b.u)&&Bits(a.v,b.v);}
void Matrix(const Matrix4& a,const Matrix4& b){for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)Require(Bits(a[r][c],b[r][c]),"rig matrix raw changed");}
std::vector<bool> ExactSurface(const Mistwatch::Explorer& now,const MistwatchSurfaceV285Reference::Explorer& old){
    const auto& a=now.model;const auto& b=old.model;
    Require(a.Valid()&&b.Valid(),"existing model validity");
    Require(a.bindMesh.vertices.size()==7861&&a.bindMesh.indices.size()/3u==10592,"original measured geometry counts");
    Require(a.bindMesh.vertices.size()==b.bindMesh.vertices.size()&&a.bindMesh.indices==b.bindMesh.indices,"all true triangle indices raw changed");
    Require(a.skeleton.joints.size()==15&&a.skeletonCompatibilityId==b.skeletonCompatibilityId,"15 rig compatibility");
    for(unsigned joint=0;joint<15;++joint){Require(a.skeleton.joints[joint].parent==b.skeleton.joints[joint].parent,"rig hierarchy");
        Matrix(a.skeleton.joints[joint].bindTransform.Matrix(),b.skeleton.joints[joint].bindTransform.Matrix());Matrix(a.inverseBindMatrices[joint],b.inverseBindMatrices[joint]);
        Require(now.art.criticalVertices[joint]==old.art.criticalVertices[joint],"critical joint IDs changed");}
    Require(a.tangents==b.tangents,"authored tangent data changed");
    std::vector<bool> allowed(a.bindMesh.vertices.size()),head(allowed.size());
    for(auto v:now.art.criticalVertices[2])head.at(v)=true;
    Require(std::is_sorted(now.art.skinColorVertices.begin(),now.art.skinColorVertices.end()),"ordered SkinColor inventory");
    for(auto v:now.art.skinColorVertices){Require(head.at(v)&&!allowed[v],"SkinColor inventory outside head / duplicated");allowed[v]=true;}
    unsigned changed{};
    for(std::size_t i=0;i<a.bindMesh.vertices.size();++i){const auto& v=a.bindMesh.vertices[i];const auto& r=b.bindMesh.vertices[i];
        Require(GeometryVertex(v,r),"xyz/normal/UV raw changed vertex "+std::to_string(i));
        Require(a.influences[i].joints==b.influences[i].joints&&a.influences[i].weights==b.influences[i].weights,"influences raw changed");
        if(v.color!=r.color){Require(allowed[i],"color changed outside actual SkinColor vertices");++changed;}
        const auto c=UnpackColorRgba8(v.color),prior=UnpackColorRgba8(r.color);
        Require(FiniteColor(c)&&c.a==1,"finite linear albedo");
        if(allowed[i])Require(std::abs(c.r-prior.r)<.075f&&std::abs(c.g-prior.g)<.075f&&std::abs(c.b-prior.b)<.075f,"macro albedo excessive delta");
    }
    Require(changed>100,"authored macro color change missing");
    Require(now.art.joins.size()==14&&now.art.joins.size()==old.art.joins.size(),"14 joins");
    for(std::size_t i=0;i<now.art.joins.size();++i)Require(now.art.joins[i].joint==old.art.joins[i].joint&&now.art.joins[i].boundary==old.art.joins[i].boundary,"join boundary raw changed");
    for(unsigned i=0;i<5;++i){const auto& x=now.art.ranges[i];const auto& y=old.art.ranges[i];Require(x.firstIndex==y.firstIndex&&x.indexCount==y.indexCount&&static_cast<unsigned>(x.material)==static_cast<unsigned>(y.material),"five material ranges raw changed");}
    // 真bind頂點比對唇色，而不只斷言作者函式返回值。
    const auto nearest=[&](float x,float y){std::uint32_t result{};float best=100;
        for(auto v:now.art.skinColorVertices){const auto& p=a.bindMesh.vertices[v];if(p.z<.05f)continue;
            const float score=(p.x-x)*(p.x-x)+(p.y-y)*(p.y-y);if(score<best){best=score;result=v;}}return result;};
    const auto lip=nearest(0,1.591f),skin=nearest(.043f,1.593f);
    const auto difference=[](Color x,Color y){return std::sqrt((x.r-y.r)*(x.r-y.r)+(x.g-y.g)*(x.g-y.g)+(x.b-y.b)*(x.b-y.b));};
    const float before=difference(UnpackColorRgba8(b.bindMesh.vertices[lip].color),UnpackColorRgba8(b.bindMesh.vertices[skin].color));
    const float after=difference(UnpackColorRgba8(a.bindMesh.vertices[lip].color),UnpackColorRgba8(a.bindMesh.vertices[skin].color));
    Require(after>before+.02f,"actual lip / adjacent skin separation not improved");
    std::cout<<"surface_geometry_exact vertices="<<a.bindMesh.vertices.size()<<" triangles="<<a.bindMesh.indices.size()/3u
        <<" altered_skin_colors="<<changed<<" lip_color_distance_before="<<before<<" after="<<after<<'\n';
    return allowed;
}
void Coverage(const Mistwatch::Explorer& explorer){
    Plan plan;Require(BuildPlan(explorer.model,explorer.art,plan),"valid exact lens plan rejected");
    std::vector<unsigned> uses(explorer.model.bindMesh.indices.size());unsigned eyes{},skin{};
    for(const auto& w:plan.windows){Require(w.indexCount>0&&w.firstIndex%3u==0&&w.indexCount%3u==0,"window triangle boundary");
        for(std::uint32_t i=w.firstIndex;i<w.firstIndex+w.indexCount;++i)++uses.at(i);
        if(w.eye){Require(w.material==Mistwatch::ExplorerArtMaterial::Skin&&w.indexCount==702,"Eye is not original 234-triangle lens");++eyes;}else skin+=w.material==Mistwatch::ExplorerArtMaterial::Skin;}
    Require(eyes==2&&skin==3,"Skin/Eye/Skin/Eye/Skin partition");
    for(auto count:uses)Require(count==1,"index coverage gap or duplicate draw");
    for(const auto& lens:explorer.art.eyeLensSpans)for(std::uint32_t v=lens.firstVertex;v<lens.firstVertex+lens.vertexCount;++v)
        Require(!std::binary_search(explorer.art.skinColorVertices.begin(),explorer.art.skinColorVertices.end(),v),"eye lens recolored as skin");
    // 原始全range與Lens border頂點共享但lens span只包含眼球三角形；不得把eyelid拿來做Eye。
    auto bad=explorer.art;bad.eyeLensSpans[0].firstIndex+=1;Require(!BuildPlan(explorer.model,bad,plan),"unaligned lens accepted");
    bad=explorer.art;bad.eyeLensSpans[0].indexCount+=3;Require(!BuildPlan(explorer.model,bad,plan),"eyelid extension accepted");
    bad=explorer.art;bad.eyeLensSpans[1]=bad.eyeLensSpans[0];Require(!BuildPlan(explorer.model,bad,plan),"duplicate lens accepted");
    bad=explorer.art;std::swap(bad.eyeLensSpans[0],bad.eyeLensSpans[1]);Require(!BuildPlan(explorer.model,bad,plan),"unordered lenses accepted");
    bad=explorer.art;bad.eyeLensSpans[0].firstIndex=(std::numeric_limits<std::uint32_t>::max)()-2u;Require(!BuildPlan(explorer.model,bad,plan),"overflow / outside skin span accepted");
    bad=explorer.art;bad.eyeLensSpans[0].vertexCount=132;Require(!BuildPlan(explorer.model,bad,plan),"extra eyelid vertex accepted");
    bad=explorer.art;++bad.ranges[1].firstIndex;Require(!BuildPlan(explorer.model,bad,plan),"original five-range gap accepted");
    auto mesh=explorer.model;const auto& lens=explorer.art.eyeLensSpans[0];
    mesh.bindMesh.indices[lens.firstIndex]=lens.firstVertex+lens.vertexCount;Require(!BuildPlan(mesh,explorer.art,plan),"eyelid triangle masquerades as lens");
    mesh=explorer.model;mesh.influences[lens.firstVertex].joints[0]=1;Require(!BuildPlan(mesh,explorer.art,plan),"wrong lens influence accepted");
}
SkinnedMeshHandle3D Handle(const Mistwatch::Explorer& explorer){return {101,explorer.model.skeletonCompatibilityId,15,201,0,0,
    static_cast<std::uint32_t>(explorer.model.bindMesh.vertices.size()),static_cast<std::uint32_t>(explorer.model.bindMesh.indices.size())};}
void Forwarding(const Mistwatch::Explorer& explorer){
    DrawList draw;SkinningWorkspace3D workspace;const auto palette=AddSkinPaletteFromPose(draw,explorer.model,explorer.pose,workspace);
    auto materials=Mistwatch::ExplorerArtMaterials();Submission receipt;Plan plan;
    Require(BuildPlan(explorer.model,explorer.art,plan)&&Submit(draw,explorer.model,explorer.art,Handle(explorer),palette,Matrix4::Identity(),materials,receipt),"normal production submission rejected");
    Require(receipt.acceptedWindows==9&&receipt.skinWindows==3&&receipt.eyeWindows==2&&receipt.hairWindows==1&&receipt.clothWindows==1&&receipt.texturedWindows==0&&receipt.lensIndices==1404,"exact production route counts");
    for(unsigned i=0;i<plan.windows.size();++i){const auto& v=draw.skinnedMeshInstances[i];const auto m=WindowMaterial(plan.windows[i],materials);
        Require(v.firstIndex==plan.windows[i].firstIndex&&v.indexCount==plan.windows[i].indexCount&&v.shadingModel==m.shadingModel,"draw plan route mismatch");
        Require(v.surface==PackSurface3D(m.surface)&&v.tint==PackColorRgba8(m.tint),"material scalar/color forwarding");
        Require(v.normalScale==m.normalScale&&v.alphaMode==m.alphaMode&&v.alphaCutoff==m.alphaCutoff&&v.doubleSided==m.doubleSided&&v.useUv==m.useUv,"production material tails");
    }
    RendererStats synthetic{};synthetic.lastSubmittedSkinShadedInstances=3;synthetic.lastSubmittedEyeShadedInstances=2;
    synthetic.lastSubmittedHairShadedInstances=1;synthetic.lastSubmittedClothShadedInstances=1;std::ostringstream log;
    Require(WriteReceipt(log,synthetic,receipt),"synthetic audit writer positive");--synthetic.lastSubmittedEyeShadedInstances;
    Require(!WriteReceipt(log,synthetic,receipt),"synthetic audit writer misses GPU route divergence");
    draw.Clear();const auto palette2=AddSkinPaletteFromPose(draw,explorer.model,explorer.pose,workspace);
    // dummy texture handle僅測CPU傳遞；不宣稱resident texture或GPU取樣通過。
    for(auto& m:materials){m.textures={301};m.useUv=true;m.textureScale=.75f;m.normalScale=-.5f;m.baseTextureScale=1.125f;m.detailTextureScale=2.25f;
        m.alphaMode=MaterialAlphaMode3D::Mask;m.alphaCutoff=.375f;m.doubleSided=true;m.castsShadow=false;}
    Require(Submit(draw,explorer.model,explorer.art,Handle(explorer),palette2,Matrix4::Identity(),materials,receipt),"finite material-tail test rejected");
    for(const auto& v:draw.skinnedMeshInstances)Require(v.textures.value==301&&v.useUv&&v.textureScale==.75f&&v.normalScale==-.5f&&
        v.baseTextureScale==1.125f&&v.detailTextureScale==2.25f&&v.alphaMode==MaterialAlphaMode3D::Mask&&v.alphaCutoff==.375f&&v.doubleSided&&!v.castsShadow&&v.shadowCascadeMask==0,"explicit tail lost");
    const auto reject=[&](std::array<Material3D,5> invalid,Matrix4 transform=Matrix4::Identity()){
        DrawList clean;const auto p=AddSkinPaletteFromPose(clean,explorer.model,explorer.pose,workspace);Submission rejected;
        Require(!Submit(clean,explorer.model,explorer.art,Handle(explorer),p,transform,invalid,rejected)&&clean.skinnedMeshInstances.empty(),"invalid input partial draw-list mutation");};
    const auto valid=Mistwatch::ExplorerArtMaterials();auto invalid=valid;invalid[0].normalScale=std::numeric_limits<float>::quiet_NaN();reject(invalid);
    invalid=valid;invalid[1].baseTextureScale=std::numeric_limits<float>::infinity();reject(invalid);
    invalid=valid;invalid[2].surface.roughness=-.1f;reject(invalid);invalid=valid;invalid[3].detailTextureScale=-1.f;reject(invalid);
    invalid=valid;invalid[4].alphaMode=MaterialAlphaMode3D::Blend;reject(invalid);
    invalid=valid;invalid[0].alphaMode=MaterialAlphaMode3D::Mask;reject(invalid);
    invalid=valid;invalid[0].tint.r=std::numeric_limits<float>::quiet_NaN();reject(invalid);
    invalid=valid;invalid[0].textureScale=0;reject(invalid);
    invalid=valid;invalid[0].materialClass=MaterialClass3D::Water;reject(invalid);
    auto transform=Matrix4::Identity();transform[0][0]=std::numeric_limits<float>::quiet_NaN();reject(valid,transform);
    DrawList capacity;const auto capacityPalette=AddSkinPaletteFromPose(capacity,explorer.model,explorer.pose,workspace);
    capacity.skinnedMeshInstances.resize(DrawList::MaximumSkinnedMeshInstancesPerFrame-8u);
    Require(!Submit(capacity,explorer.model,explorer.art,Handle(explorer),capacityPalette,Matrix4::Identity(),valid,receipt)&&
        capacity.skinnedMeshInstances.size()==DrawList::MaximumSkinnedMeshInstancesPerFrame-8u,"capacity fail-close");
    std::cout<<"surface_submission windows=9 skin=3 eye=2 lens_triangles=468 textured=0 finite_negatives=1 dummy_textures_cpu_only=1 gpu_executed=0\n";
}
void Poses(Mistwatch::Explorer& now,MistwatchSurfaceV285Reference::Explorer& old,const std::vector<bool>& allowed){
    SkinningWorkspace3D aWork,bWork;Mesh3D a,b;
    for(unsigned action=0;action<3;++action){const auto delta=action==1?Vec3{0,0,.12f}:Vec3{};now.Update(delta,1.f/60.f,action==2);old.Update(delta,1.f/60.f,action==2);
        Require(SkinModelInto(now.model,now.pose,aWork,a)&&SkinModelInto(old.model,old.pose,bWork,b),"surface pose skin rejected");
        Require(a.indices==b.indices,"pose indices changed");
        for(std::size_t i=0;i<a.vertices.size();++i){Require(GeometryVertex(a.vertices[i],b.vertices[i]),"stand/walk/crouch geometry normal raw changed");
            Require(allowed[i]||a.vertices[i].color==b.vertices[i].color,"posed non-SkinColor changed");}
        for(unsigned joint=0;joint<15;++joint)Matrix(now.pose.globalMatrices[joint],old.pose.globalMatrices[joint]);}
}
}
int main(){try{Mistwatch::Explorer now;MistwatchSurfaceV285Reference::Explorer old;const auto allowed=ExactSurface(now,old);Coverage(now);Forwarding(now);Poses(now,old,allowed);
    std::cout<<"ExplorerSurfaceCpuTest PASS checks="<<checks<<" native_image_verified=0 microtextures_completed=0 realism_claimed=0\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<" checks="<<checks<<'\n';return 1;}}
