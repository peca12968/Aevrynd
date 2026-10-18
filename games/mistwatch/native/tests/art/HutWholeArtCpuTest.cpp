// 以凍結原資產對照正式新木屋；物理回歸仍針對現行產品執行。
#include "MistwatchWorld.h"
#include "MistwatchCamera.h"
#include <Aevrynd/BeveledBox3D.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <limits>
#include <stdexcept>
#ifndef AEVRYND_HUT_BASELINE_HEADER
#define AEVRYND_HUT_BASELINE_HEADER "fixtures/hut-before-whole/MistwatchRangerHut.h"
#endif
#ifndef AEVRYND_HUT_ORIGINAL_TEST
#define AEVRYND_HUT_ORIGINAL_TEST "../MistwatchRangerHutTest.cpp"
#endif
namespace HutSourceBaseline {using namespace Mistwatch;}
#define Mistwatch HutSourceBaseline
#include AEVRYND_HUT_BASELINE_HEADER
#undef Mistwatch
#include "MistwatchRangerHut.h"
#include "MistwatchHutWoodMaterial.h"
// 原 2916 camera sweeps、130 seal rays、21 opening rays 全文不改地跑真候選。
#define main OriginalHutRegressionMain
#include AEVRYND_HUT_ORIGINAL_TEST
#undef main

// 由獨立既有TU提供固定世界函數；此檔只呼叫，不能以current前後自比冒充基準。
std::vector<Aevrynd::BoxCollider3D> FormalPropWorldSnapshot(bool,Mistwatch::FirLayoutSelector);

namespace {
using namespace Aevrynd;
using Part=Mistwatch::RangerHutPart;
using Kind=Mistwatch::RangerHutPartKind;
void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
bool SamePoint(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool SameVertex(const Vertex3D& a,const Vertex3D& b){static_assert(sizeof(Vertex3D)==28);return std::memcmp(&a,&b,28)==0;}
bool SameMesh(const Mesh3D& a,const Mesh3D& b){
    return a.indices==b.indices&&a.vertices.size()==b.vertices.size()&&
        std::equal(a.vertices.begin(),a.vertices.end(),b.vertices.begin(),SameVertex);
}
bool SameFloatBits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool SamePointBits(Vec3 a,Vec3 b){return SameFloatBits(a.x,b.x)&&SameFloatBits(a.y,b.y)&&SameFloatBits(a.z,b.z);}
bool CurrentWorldExact(const std::vector<BoxCollider3D>& a,const std::vector<BoxCollider3D>& b){
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)
        if(!SamePointBits(a[i].center,b[i].center)||!SamePointBits(a[i].extents,b[i].extents))return false;
    return true;
}
bool CurrentPartCoverage(const Mesh3D& mesh,const std::vector<Part>& parts){
    if(!mesh.Valid()||parts.empty())return false;
    std::size_t covered{};
    for(const auto& part:parts){
        if(part.firstIndex!=covered||part.indexCount==0||part.indexCount%3!=0||covered>mesh.indices.size()||
            part.indexCount>mesh.indices.size()-covered)return false;
        covered+=part.indexCount;
    }
    return covered==mesh.indices.size();
}
bool CurrentUvMagnitude(const Mesh3D& mesh,bool authored){
    if(!mesh.Valid())return false;
    for(const auto& vertex:mesh.vertices)if(!std::isfinite(vertex.u)||!std::isfinite(vertex.v))return false;
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        const auto& a=mesh.vertices[mesh.indices[i]],b=mesh.vertices[mesh.indices[i+1]],c=mesh.vertices[mesh.indices[i+2]];
        const double determinant=(double(b.u)-a.u)*(double(c.v)-a.v)-(double(b.v)-a.v)*(double(c.u)-a.u);
        if(!std::isfinite(determinant)||(authored&&std::abs(determinant)<=1.e-9))return false;
    }
    return true;
}
bool CurrentTriangleQuality(const Mesh3D& mesh){
    if(!mesh.Valid())return false;
    for(const auto& vertex:mesh.vertices)
        if(!Mistwatch::Session::Finite({vertex.x,vertex.y,vertex.z}))return false;
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        const auto& a=mesh.vertices[mesh.indices[i]],b=mesh.vertices[mesh.indices[i+1]],c=mesh.vertices[mesh.indices[i+2]];
        // 獨立double叉積與每一個角點，原Prop門檻不可改成只檢第一法線或正dot。
        const double ux=double(b.x)-a.x,uy=double(b.y)-a.y,uz=double(b.z)-a.z;
        const double vx=double(c.x)-a.x,vy=double(c.y)-a.y,vz=double(c.z)-a.z;
        const double nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
        const double length=std::sqrt(nx*nx+ny*ny+nz*nz);
        if(!std::isfinite(length)||length*.5<=1.e-8)return false;
        for(const auto* vertex:{&a,&b,&c}){
            const auto n=UnpackNormalSnorm8(vertex->normal);
            const double normalLength=std::sqrt(double(n.x)*n.x+double(n.y)*n.y+double(n.z)*n.z);
            if(!std::isfinite(normalLength)||normalLength<=.98||normalLength>=1.02||
                (nx*n.x+ny*n.y+nz*n.z)/(length*normalLength)<=.999)return false;
        }
    }
    return true;
}
bool CurrentOldModeControls(const std::array<Mesh3D,3>& meshes){
    const auto& baseline=meshes[0];
    if(baseline.vertices.size()!=3432||baseline.indices.size()!=5124)return false;
    for(std::size_t mode=1;mode<3;++mode)
        if(meshes[mode].vertices.size()!=3432||meshes[mode].indices!=baseline.indices)return false;
    std::size_t moved{},uvChanged{};
    for(std::size_t i=0;i<baseline.vertices.size();++i){
        const auto& a=baseline.vertices[i];const auto& material=meshes[1].vertices[i];const auto& trim=meshes[2].vertices[i];
        if(std::memcmp(&a,&material,20)!=0||!SameFloatBits(a.u,trim.u)||!SameFloatBits(a.v,trim.v))return false;
        if(std::memcmp(&a,&trim,20)!=0)++moved;
        if(!SameFloatBits(a.u,material.u)||!SameFloatBits(a.v,material.v))++uvChanged;
    }
    return moved>0&&uvChanged==3432;
}
bool CurrentFrameAt(const Mesh3D& mesh,const std::vector<Part>& parts,bool visible,Kind expected,Vec3 sample,float side){
    if(!mesh.Valid())return false;
    const Vec3 from{sample.x,sample.y,side*3.2f},to{sample.x,sample.y,side*2.7f},direction=to-from;
    float fraction=std::numeric_limits<float>::infinity();std::size_t first=mesh.indices.size();
    // 保留HutMaterial原始1e-7 determinant、0<t<1及最近triangle，非dense ray的另一套epsilon。
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        const auto& va=mesh.vertices[mesh.indices[i]],vb=mesh.vertices[mesh.indices[i+1]],vc=mesh.vertices[mesh.indices[i+2]];
        const Vec3 a{va.x,va.y,va.z},b{vb.x,vb.y,vb.z},c{vc.x,vc.y,vc.z};
        const auto edge=b-a,other=c-a,h=Cross(direction,other);const float det=Dot(edge,h);
        if(std::abs(det)<1.e-7f)continue;
        const float inverse=1.f/det;const auto s=from-a;const float u=inverse*Dot(s,h);
        if(u<0||u>1)continue;
        const auto q=Cross(s,edge);const float v=inverse*Dot(direction,q),t=inverse*Dot(other,q);
        if(v>=0&&u+v<=1&&t>0&&t<1&&t<fraction){fraction=t;first=i;}
    }
    if(!std::isfinite(fraction))return false;
    const Part* hit{};
    for(const auto& part:parts)if(first>=part.firstIndex&&first-part.firstIndex<part.indexCount){
        if(hit)return false;
        hit=&part;
    }
    if(!hit||hit->kind!=(visible?expected:Kind::Host))return false;
    if(visible){
        if(std::abs(hit->centre.x-sample.x)>=1.e-5f||
            !SameFloatBits(hit->centre.z,expected==Kind::Sill?side*2.951f:side*2.958f))return false;
        const float z=side*(3.2f-.5f*fraction);
        if(std::abs(z-side*(expected==Kind::Sill?2.999f:2.994f))>=1.e-5f)return false;
    }
    return true;
}
bool CurrentFrameSamples(const Mesh3D& mesh,const std::vector<Part>& parts,bool visible){
    for(const float side:{-1.f,1.f}){
        for(const float x:{-2.235f,-.075f,.075f,2.235f})for(const float y:{.95f,1.55f,2.15f})
            if(!CurrentFrameAt(mesh,parts,visible,Kind::Jamb,{x,y,0},side))return false;
        for(const float x:{-1.15f,1.15f}){
            if(!CurrentFrameAt(mesh,parts,visible,Kind::Sill,{x,.858f,0},side)||
                !CurrentFrameAt(mesh,parts,visible,Kind::UpperFrame,{x,2.24f,0},side))return false;
        }
    }
    return true;
}
bool OriginalPartScopeExact(const Mesh3D& mesh,const std::vector<Part>& parts,const Mesh3D& source,
    const std::vector<HutSourceBaseline::RangerHutPart>& sourceParts){
    if(sourceParts.size()!=157)return false;
    // 原14個Host為屋頂箱、五內襯、六柱、兩山牆底帶；本次只授權其可見色／UV。
    constexpr std::array<std::size_t,14> originalHosts{0,1,41,57,64,65,66,67,83,90,91,92,139,148};
    std::array<bool,157> expected{},used{};
    for(const auto index:originalHosts){
        if(sourceParts[index].kind!=HutSourceBaseline::RangerHutPartKind::Host)return false;
        expected[index]=true;
    }
    for(std::size_t i=0;i<sourceParts.size();++i){
        const auto kind=sourceParts[i].kind;
        if(kind==HutSourceBaseline::RangerHutPartKind::EntryFrame||kind==HutSourceBaseline::RangerHutPartKind::Sill||
            kind==HutSourceBaseline::RangerHutPartKind::UpperFrame||kind==HutSourceBaseline::RangerHutPartKind::Roof||
            kind==HutSourceBaseline::RangerHutPartKind::Gable)expected[i]=true;
    }
    if(std::count(expected.begin(),expected.end(),true)!=68)return false;
    constexpr std::array<std::size_t,5> rawLinings{1,41,57,67,83};
    const auto rawPartEqual=[&](const Part& part,const HutSourceBaseline::RangerHutPart& old){
        if(static_cast<unsigned>(old.kind)!=static_cast<unsigned>(part.kind)||!SamePoint(old.centre,part.centre)||
            old.indexCount!=part.indexCount||part.indexCount==0||part.firstIndex>mesh.indices.size()||
            old.firstIndex>source.indices.size()||part.indexCount>mesh.indices.size()-part.firstIndex||
            old.indexCount>source.indices.size()-old.firstIndex)return false;
        const auto newBegin=mesh.indices.begin()+part.firstIndex,oldBegin=source.indices.begin()+old.firstIndex;
        const auto newBase=*std::min_element(newBegin,newBegin+part.indexCount);
        const auto oldBase=*std::min_element(oldBegin,oldBegin+old.indexCount);
        for(std::size_t j=0;j<part.indexCount;++j){
            const auto ni=mesh.indices[part.firstIndex+j],oi=source.indices[old.firstIndex+j];
            if(ni>=mesh.vertices.size()||oi>=source.vertices.size()||ni-newBase!=oi-oldBase)return false;
            const auto sourceIndex=static_cast<std::size_t>(&old-sourceParts.data());
            const bool raw=std::find(rawLinings.begin(),rawLinings.end(),sourceIndex)!=rawLinings.end();
            // 未授權的五內襯仍28byte raw；其他原結構只授權color/UV，xyz/normal/index不變。
            if(raw){if(!SameVertex(mesh.vertices[ni],source.vertices[oi]))return false;}
            else {
                if(std::memcmp(&mesh.vertices[ni],&source.vertices[oi],16)!=0)return false;
                Aevrynd::Color expectedColor{};
                switch(part.kind){
                case Kind::EntryFrame:expectedColor={.80f,.79f,.74f,1};break;
                case Kind::Sill:case Kind::UpperFrame:expectedColor={.86f,.85f,.80f,1};break;
                case Kind::Roof:expectedColor={.76f,.75f,.71f,1};break;
                case Kind::Gable:expectedColor={.92f,.91f,.87f,1};break;
                default:expectedColor={.70f,.69f,.65f,1};break;
                }
                if(mesh.vertices[ni].color!=PackColorRgba8(expectedColor))return false;
            }
        }return true;
    };
    unsigned matched{};
    for(const auto& part:parts){
        if(part.kind==Kind::WallBoard||part.kind==Kind::Jamb||part.kind==Kind::CornerStile||part.kind==Kind::Fascia)continue;
        std::size_t match=sourceParts.size(),count{};
        // 同中心Gable不可當歧義；位置／法線流與相對index仍須唯一，owned色亦逐項驗證。
        for(std::size_t i=0;i<sourceParts.size();++i)if(expected[i]&&rawPartEqual(part,sourceParts[i])){match=i;++count;}
        if(count!=1||used[match])return false;
        used[match]=true;++matched;
    }
    return matched==68&&used==expected;
}
bool SameMaterial(const Material3D& a,const Material3D& b){
    return a.tint.r==b.tint.r&&a.tint.g==b.tint.g&&a.tint.b==b.tint.b&&a.tint.a==b.tint.a&&
        a.surface.roughness==b.surface.roughness&&a.surface.metallic==b.surface.metallic&&
        a.surface.emissive==b.surface.emissive&&a.surface.occlusion==b.surface.occlusion&&
        a.castsShadow==b.castsShadow&&a.textures.value==b.textures.value&&
        a.textures.semanticIdentity==b.textures.semanticIdentity&&a.textureScale==b.textureScale&&
        a.useUv==b.useUv&&a.materialClass==b.materialClass&&a.shadingModel==b.shadingModel&&
        a.alphaMode==b.alphaMode&&a.alphaCutoff==b.alphaCutoff&&a.doubleSided==b.doubleSided&&
        a.normalScale==b.normalScale&&a.baseTextureScale==b.baseTextureScale&&a.detailTextureScale==b.detailTextureScale;
}
bool WeatheredSourcePolicy(const MaterialAssetDefinition3D& definition){
    constexpr std::array<std::uint64_t,3> bytes{943056,807719,4886175};
    constexpr std::array<const char*,3> sha{
        "698c09b337375a9aae12a76ad09ff2fda11d735e6c1bbb18d72875bd61988db6",
        "c79f640dc2d7ef81d69de463ef6676b7c540d598fb0e462a6eb39eb5462770a9",
        "743411acf7332d8176d4e402237bb4b2d28a89fdc9ba160bb1eee2de6fbfb74b"};
    if(definition.name!="hut-weathered-wood"||definition.textures.baseColorPath!="poly-haven/worn_planks/1k/base-color.jpg"||
        definition.textures.roughnessPath!="poly-haven/worn_planks/1k/roughness.jpg"||
        definition.textures.normalPath!="poly-haven/worn_planks/1k/normal-dx.png"||definition.textures.maximumResolution!=1024||
        definition.textures.surfaceEncoding!=MaterialSurfaceMapEncoding3D::RoughnessR||
        definition.textures.alphaMode!=MaterialAlphaMode3D::Opaque||definition.textures.alphaCutoff!=.5f)return false;
    for(std::size_t i=0;i<3;++i)if(definition.textures.integrity[i].expectedBytes!=bytes[i]||
        definition.textures.integrity[i].expectedSha256!=sha[i])return false;
    for(const auto& layer:definition.textures.additionalLayers)if(!layer.baseColorPath.empty()||!layer.roughnessPath.empty()||
        !layer.normalPath.empty())return false;
    return true;
}
bool SoundMesh(const Mesh3D& mesh,bool uv){
    if(!mesh.Valid())return false;
    for(const auto& a:mesh.vertices){
        if(!Mistwatch::Session::Finite({a.x,a.y,a.z})||!std::isfinite(a.u)||!std::isfinite(a.v)||
            std::abs(a.x)>2.5001f||std::abs(a.z)>3.0001f||a.y<-.00001f||a.y>3.8501f)return false;
        const auto n=UnpackNormalSnorm8(a.normal);
        if(std::abs(Length(Vec3{n.x,n.y,n.z})-1.f)>.02f)return false;
        if(a.x<-2.2999f&&a.y<2.5499f&&std::abs(a.z)<2.7999f)return false;
        if(std::abs(a.z)>2.7999f&&a.y>.9001f&&a.y<2.1999f&&
            ((a.x>-2.1499f&&a.x<-.1501f)||(a.x>.1501f&&a.x<2.1499f)))return false;
    }
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        const auto& a=mesh.vertices[mesh.indices[i]],b=mesh.vertices[mesh.indices[i+1]],c=mesh.vertices[mesh.indices[i+2]];
        const auto cross=Cross(Vec3{b.x-a.x,b.y-a.y,b.z-a.z},Vec3{c.x-a.x,c.y-a.y,c.z-a.z});
        const auto normal=UnpackNormalSnorm8(a.normal);
        if(Length(cross)<.0000001f||Dot(cross,{normal.x,normal.y,normal.z})<=0)return false;
        // Vertex3D 沒有 tangent 欄；非零真 UV Jacobian 是 shader 導數切線的必要條件。
        const double determinant=(double(b.u)-a.u)*(double(c.v)-a.v)-(double(b.v)-a.v)*(double(c.u)-a.u);
        if(uv&&(!std::isfinite(determinant)||determinant==0))return false;
    }
    return true;
}
struct RayHit {float t{2};std::size_t triangle{std::numeric_limits<std::size_t>::max()};};
RayHit FirstHit(Vec3 from,Vec3 to,const Mesh3D& mesh){
    RayHit hit;const auto direction=to-from;
    for(std::size_t i=0;i<mesh.indices.size();i+=3){
        const auto& va=mesh.vertices[mesh.indices[i]],vb=mesh.vertices[mesh.indices[i+1]],vc=mesh.vertices[mesh.indices[i+2]];
        const Vec3 a{va.x,va.y,va.z},b{vb.x,vb.y,vb.z},c{vc.x,vc.y,vc.z};
        const auto edge=b-a,other=c-a,h=Cross(direction,other);const float det=Dot(edge,h);
        if(std::abs(det)<.000001f)continue;
        const float inverse=1.f/det;const auto s=from-a;const float u=inverse*Dot(s,h);
        if(u<0||u>1)continue;
        const auto q=Cross(s,edge);const float v=inverse*Dot(direction,q),t=inverse*Dot(other,q);
        if(v>=0&&u+v<=1&&t>.0001f&&t<.9999f&&t<hit.t)hit={t,i/3};
    }
    return hit;
}
const Mesh3D* denseDiagnosticBaseline{};
bool denseDiagnosticPrinted{};
bool DenseFailure(const char* kind,Vec3 from,Vec3 to,const RayHit& hit){
    // 只觀測真candidate首錯；相同query查原來源，結果不能改為通過。
    if(denseDiagnosticBaseline&&!denseDiagnosticPrinted){
        denseDiagnosticPrinted=true;
        const auto baseline=FirstHit(from,to,*denseDiagnosticBaseline);
        const auto triangle=[](const RayHit& h){
            if(h.triangle==std::numeric_limits<std::size_t>::max())std::cerr<<"none";
            else std::cerr<<h.triangle;
        };
        std::cerr<<std::setprecision(std::numeric_limits<float>::max_digits10)
            <<"DENSE_RAY_FIRST_FAILURE kind="<<kind<<" from="<<from.x<<','<<from.y<<','<<from.z
            <<" to="<<to.x<<','<<to.y<<','<<to.z<<" candidate_hit="<<(hit.t<1)
            <<" candidate_t="<<hit.t<<" candidate_triangle=";triangle(hit);
        std::cerr<<" baseline_hit="<<(baseline.t<1)<<" baseline_t="<<baseline.t<<" baseline_triangle=";
        triangle(baseline);std::cerr<<'\n';
    }
    return false;
}
bool DenseWalls(const Mesh3D& mesh){
    // 與候選 rows 無關的 25mm 網格；只向實牆射線，窗孔另測。
    for(unsigned y=1;y<104;++y)for(unsigned z=1;z<240;++z){
        const Vec3 from{2.6f,.025f*float(y),-3.f+.025f*float(z)},to{2.0f,.025f*float(y),-3.f+.025f*float(z)};
        const auto hit=FirstHit(from,to,mesh);
        if(hit.t>1)return DenseFailure("wall-east",from,to,hit);
    }
    for(const float side:{-1.f,1.f})for(unsigned y=1;y<104;++y)for(unsigned x=1;x<200;++x){
        const float px=-2.5f+.025f*float(x),py=.025f*float(y);
        if(py>.9f&&py<2.2f&&((px>-2.15f&&px<-.15f)||(px>.15f&&px<2.15f)))continue;
        const Vec3 from{px,py,side*3.1f},to{px,py,side*2.65f};const auto hit=FirstHit(from,to,mesh);
        if(hit.t>1)return DenseFailure(side<0?"wall-south":"wall-north",from,to,hit);
    }
    return true;
}
bool DenseOpenings(const Mesh3D& mesh){
    for(const float side:{-1.f,1.f})for(const float center:{-1.15f,1.15f})
        for(unsigned x=1;x<40;++x)for(unsigned y=1;y<26;++y){
            const float px=center-1.f+.05f*float(x),py=.9f+.05f*float(y);
            const Vec3 from{px,py,side*3.1f},to{px,py,side*2.65f};const auto hit=FirstHit(from,to,mesh);
            if(hit.t<1)return DenseFailure(side<0?"window-south":"window-north",from,to,hit);
        }
    for(unsigned y=1;y<51;++y)for(unsigned z=1;z<112;++z){
        const float py=.05f*float(y),pz=-2.8f+.05f*float(z);
        const Vec3 from{-2.7f,py,pz},to{-2.1f,py,pz};const auto hit=FirstHit(from,to,mesh);
        if(hit.t<1)return DenseFailure("entry-west",from,to,hit);
    }
    return true;
}
bool SameSolids(const std::vector<BoxCollider3D>& a,const std::vector<BoxCollider3D>& b){
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)if(!SamePoint(a[i].center,b[i].center)||!SamePoint(a[i].extents,b[i].extents))return false;
    return true;
}
bool WeatheredUvPolicy(const Mesh3D& mesh,const std::vector<Part>& parts){
    // 獨立原圖帶界，不引用作者的HutWornPlankBands或UV函式。
    constexpr std::array<Vec2,4> bands{{{132.f/1024.f,216.f/1024.f},{360.f/1024.f,436.f/1024.f},
        {694.f/1024.f,758.f/1024.f},{932.f/1024.f,1006.f/1024.f}}};
    const float angle=std::atan(.85f/2.48f),cosine=std::cos(angle),sine=std::sin(angle);
    for(const auto& part:parts){
        Vec3 low{100,100,100},high{-100,-100,-100};
        for(std::size_t i=part.firstIndex;i<part.firstIndex+part.indexCount;++i){
            const auto& v=mesh.vertices[mesh.indices[i]];
            low={(std::min)(low.x,v.x),(std::min)(low.y,v.y),(std::min)(low.z,v.z)};
            high={(std::max)(high.x,v.x),(std::max)(high.y,v.y),(std::max)(high.z,v.z)};
        }
        const auto size=high-low;
        const bool lining=part.kind==Kind::Host&&(size.x<.037f||size.z<.037f);
        if(lining)continue; // 五個完整raw內襯仍由獨立來源逐byte驗證。
        Color palette{};
        switch(part.kind){
        case Kind::WallBoard:palette={.96f,.95f,.92f,1};break;
        case Kind::Jamb:palette={.90f,.89f,.85f,1};break;
        case Kind::Sill:case Kind::UpperFrame:palette={.86f,.85f,.80f,1};break;
        case Kind::EntryFrame:palette={.80f,.79f,.74f,1};break;
        case Kind::Roof:palette={.76f,.75f,.71f,1};break;
        case Kind::Fascia:palette={.88f,.87f,.82f,1};break;
        case Kind::CornerStile:palette={.74f,.73f,.68f,1};break;
        case Kind::Gable:palette={.92f,.91f,.87f,1};break;
        default:palette={.70f,.69f,.65f,1};break;
        }
        for(std::size_t i=part.firstIndex;i<part.firstIndex+part.indexCount;++i)
            if(mesh.vertices[mesh.indices[i]].color!=PackColorRgba8(palette))return false;
        for(std::size_t i=part.firstIndex;i<part.firstIndex+part.indexCount;i+=3){
            const auto& a=mesh.vertices[mesh.indices[i]],b=mesh.vertices[mesh.indices[i+1]],c=mesh.vertices[mesh.indices[i+2]];
            const std::array face{a,b,c};
            bool withinOneBand=false;
            for(const auto band:bands)if(std::all_of(face.begin(),face.end(),[&](const Vertex3D& v){
                return std::isfinite(v.u)&&std::isfinite(v.v)&&v.u>=band.x&&v.u<=band.y&&v.v>=0&&v.v<5.f;
            }))withinOneBand=true;
            if(!withinOneBand)return false;
            const double determinant=(double(b.u)-a.u)*(double(c.v)-a.v)-(double(b.v)-a.v)*(double(c.u)-a.u);
            if(!std::isfinite(determinant)||determinant<=0)return false;
        }
        // 原框與屋面單位box前面，獨立驗真沿紋V/1.4m；不是僅useUv旗標。
        const auto first=part.firstIndex;
        const auto& a=mesh.vertices[mesh.indices[first]];
        Vec3 grain{};bool physical=true;
        if(part.kind==Kind::Jamb)grain={0,1,0};
        else if(part.kind==Kind::Sill||part.kind==Kind::UpperFrame)grain={1,0,0};
        else if(part.kind==Kind::Roof&&std::abs(part.centre.z)>.1f)grain={cosine,part.centre.x>0?-sine:sine,0};
        else physical=false;
        if(physical){
            bool measured=false;
            for(std::size_t k=1;k<3;++k){
                const auto& q=mesh.vertices[mesh.indices[first+k]];
                const Vec3 delta{q.x-a.x,q.y-a.y,q.z-a.z};
                const float length=Dot(delta,grain);
                if(std::abs((q.v-a.v)-length/1.4f)>1.e-5f)return false;
                measured=measured||std::abs(length)>.1f;
            }
            if(!measured)return false;
        }
    }
    return true;
}
void AddObstruction(Mesh3D& mesh,Vec3 center){
    const auto box=Mesh3D::CreateUnitBox();const auto base=static_cast<std::uint32_t>(mesh.vertices.size());
    for(auto v:box.vertices){v.x=v.x*.2f+center.x;v.y=v.y*.2f+center.y;v.z=v.z*.2f+center.z;mesh.vertices.push_back(v);}
    for(const auto index:box.indices)mesh.indices.push_back(base+index);
}
}

int main(){try{
    using namespace Aevrynd;
    const auto worldBefore=Mistwatch::WorldSolids(true),hutSolids=Mistwatch::RangerHutSolids();
    const auto cameraRoof=Mistwatch::RangerHutCameraRoofSolid();
    const auto source=HutSourceBaseline::CreateRangerHut();
    Require(source.vertices.size()==3432&&source.indices.size()==5124,"Original source geometry identity changed");
    Require(OriginalHutRegressionMain()==0,"Original full hut/camera/rays regression failed");
    Require(SameSolids(worldBefore,Mistwatch::WorldSolids(true))&&SameSolids(hutSolids,Mistwatch::RangerHutSolids()),"Original collision changed");
    const auto roofAfter=Mistwatch::RangerHutCameraRoofSolid();
    Require(SamePoint(cameraRoof.center,roofAfter.center)&&SamePoint(cameraRoof.extents,roofAfter.extents),"Camera roof changed");
    unsigned currentWorldSnapshots{};
    for(const bool repaired:{false,true})for(const auto selector:{Mistwatch::FirLayoutSelector::Legacy,Mistwatch::FirLayoutSelector::ForestWood}){
        Require(CurrentWorldExact(FormalPropWorldSnapshot(repaired,selector),Mistwatch::WorldSolids(repaired,selector)),
            "Current WorldSolids differ from independent original snapshot");
        ++currentWorldSnapshots;
    }
    const std::array candidateSettings{Mistwatch::RangerHutArtSettings{false,false},
        Mistwatch::RangerHutArtSettings{true,false},Mistwatch::RangerHutArtSettings{false,true},Mistwatch::RangerHutArtSettings{true,true}};
    std::array<Mesh3D,4> currentModes;std::array<std::vector<Part>,4> currentParts;
    std::size_t currentUvTriangles{};unsigned currentPartCoverages{},currentFrameRays{},currentHostRays{};
    Material3D material{{.7f,.6f,.5f,1},{.81f,.07f,.01f,.91f},false,{73,9981},.5f,false};
    material.baseTextureScale=.3f;material.detailTextureScale=2.f;
    const auto definition=Mistwatch::HutWeatheredWoodDefinition();
    Require(WeatheredSourcePolicy(definition),"Original three-source definition failed closed policy");
    unsigned sourceNegative{};
    for(std::size_t i=0;i<3;++i){
        auto bad=definition;++bad.textures.integrity[i].expectedBytes;
        Require(!WeatheredSourcePolicy(bad),"Wrong source byte declaration passed");++sourceNegative;
        bad=definition;bad.textures.integrity[i].expectedSha256[0]='0';
        Require(!WeatheredSourcePolicy(bad),"Wrong source SHA declaration passed");++sourceNegative;
    }
    Require(definition.name=="hut-weathered-wood"&&
        definition.textures.baseColorPath=="poly-haven/worn_planks/1k/base-color.jpg"&&
        definition.textures.roughnessPath=="poly-haven/worn_planks/1k/roughness.jpg"&&
        definition.textures.normalPath=="poly-haven/worn_planks/1k/normal-dx.png"&&
        definition.textures.maximumResolution==1024&&definition.textures.surfaceEncoding==MaterialSurfaceMapEncoding3D::RoughnessR,
        "Original weathered-wood three-map paths/encoding changed");
    constexpr std::array<std::uint64_t,3> bytes{943056,807719,4886175};
    constexpr std::array<const char*,3> sha{
        "698c09b337375a9aae12a76ad09ff2fda11d735e6c1bbb18d72875bd61988db6",
        "c79f640dc2d7ef81d69de463ef6676b7c540d598fb0e462a6eb39eb5462770a9",
        "743411acf7332d8176d4e402237bb4b2d28a89fdc9ba160bb1eee2de6fbfb74b"};
    for(std::size_t i=0;i<3;++i)Require(definition.textures.integrity[i].expectedBytes==bytes[i]&&
        definition.textures.integrity[i].expectedSha256==sha[i],"Original weathered-wood source integrity changed");
    const auto& authored=definition.material;
    Require(authored.tint.r==1&&authored.tint.g==1&&authored.tint.b==1&&authored.tint.a==1&&
        authored.surface.roughness==.8f&&authored.surface.metallic==0&&authored.surface.emissive==0&&authored.surface.occlusion==.8f&&
        authored.castsShadow&&authored.useUv&&authored.textureScale==1&&authored.normalScale==.22f&&
        authored.alphaMode==MaterialAlphaMode3D::Opaque&&!authored.doubleSided&&!authored.textures,
        "Hut-only white-tint authored PBR metadata changed");
    for(const auto& layer:definition.textures.additionalLayers)Require(layer.baseColorPath.empty()&&layer.roughnessPath.empty()&&
        layer.normalPath.empty(),"Hut-only material gained unrelated layers");
    auto loaded=definition.material;loaded.textures={91,77213};
    for(const auto settings:candidateSettings)for(const bool noNormal:{false,true}){
        const auto original=material;
        const auto actual=Mistwatch::HutWeatheredWoodMaterial(material,loaded,settings,noNormal);
        auto expected=settings.authoredUv&&settings.visibleTrim?loaded:
            HutSourceBaseline::RangerHutMaterial(material,{settings.authoredUv,settings.visibleTrim},noNormal);
        if(settings.authoredUv&&settings.visibleTrim)expected.normalScale=noNormal?0.f:.22f;
        Require(SameMaterial(actual,expected)&&SameMaterial(material,original),"Actual new material branch or shared wood changed");
    }
    {bool rejected=false;try{(void)Mistwatch::HutWeatheredWoodMaterial(material,{},{});}catch(const std::invalid_argument&){rejected=true;}
        Require(rejected,"Unloaded hut atlas accepted as shared wood");}
    for(unsigned mode=0;mode<4;++mode){
        const auto setting=candidateSettings[mode];
        const HutSourceBaseline::RangerHutArtSettings old{setting.authoredUv,setting.visibleTrim};
        for(const bool noNormal:{false,true})Require(SameMaterial(Mistwatch::RangerHutMaterial(material,setting,noNormal),
            HutSourceBaseline::RangerHutMaterial(material,old,noNormal)),"Material metadata changed");
        currentModes[mode]=Mistwatch::CreateRangerHut(setting,&currentParts[mode]);
        Require(CurrentPartCoverage(currentModes[mode],currentParts[mode]),"Current four-mode complete contiguous part coverage failed");
        ++currentPartCoverages;
        Require(SoundMesh(currentModes[mode],setting.authoredUv)&&CurrentUvMagnitude(currentModes[mode],setting.authoredUv),
            "Current four-mode finite mesh or original UV magnitude failed");
        if(setting.authoredUv)currentUvTriangles+=currentModes[mode].indices.size()/3;
        Require(CurrentFrameSamples(currentModes[mode],currentParts[mode],setting.visibleTrim),
            "Current full frame/host first-hit samples or original exterior plane failed");
        if(setting.visibleTrim)currentFrameRays+=32;else currentHostRays+=32;
        if(mode<3){
            const auto& candidate=currentModes[mode];const auto& parts=currentParts[mode];
            std::vector<HutSourceBaseline::RangerHutPart> original;
            const auto baseline=HutSourceBaseline::CreateRangerHut(old,&original);
            Require(SameMesh(candidate,baseline)&&parts.size()==original.size(),"Other three modes changed raw geometry");
            for(std::size_t i=0;i<parts.size();++i)Require(static_cast<unsigned>(parts[i].kind)==static_cast<unsigned>(original[i].kind)&&
                parts[i].firstIndex==original[i].firstIndex&&parts[i].indexCount==original[i].indexCount&&SamePoint(parts[i].centre,original[i].centre),"Other mode parts changed");
        }
    }
    const std::array<Mesh3D,3> currentOldModes{currentModes[0],currentModes[1],currentModes[2]};
    Require(CurrentOldModeControls(currentOldModes),"Current three original mode topology/position/normal/colour/UV controls failed");
    bool invalidModeRejected=false;
    try{(void)Mistwatch::ParseRangerHutArtMode(L"invalid");}catch(const std::invalid_argument&){invalidModeRejected=true;}
    Require(invalidModeRejected,"Current invalid art mode accepted");
    Require(SameMesh(Mistwatch::CreateLegacyRangerHut(),HutSourceBaseline::CreateLegacyRangerHut()),"Legacy hut changed");
    const auto& mesh=currentModes[3];const auto& parts=currentParts[3];
    std::vector<HutSourceBaseline::RangerHutPart> sourceParts;HutSourceBaseline::CreateRangerHut({},&sourceParts);
    Require(mesh.vertices.size()==6864&&mesh.indices.size()==9600&&parts.size()==132,"Fixed wall design counts changed");
    Require(mesh.vertices.size()<=2*source.vertices.size()&&mesh.indices.size()<=2*source.indices.size(),"Two-times geometry budget exceeded");
    Require(SameMesh(mesh,Mistwatch::CreateRangerHut())&&SoundMesh(mesh,true),"Mesh deterministic/finite/winding/UV guard failed");
    Require(CurrentTriangleQuality(mesh),"Current Combined original double area/all-three packed normal/angular quality failed");
    const auto bounds=mesh.Bounds(),oldBounds=source.Bounds();
    Require(SamePoint(bounds.center,oldBounds.center)&&SamePoint(bounds.extents,oldBounds.extents),"Building silhouette bounds changed");
    unsigned unchanged{},boards{},jambs{},corners{},fascias{};
    for(const auto& part:parts){
        Require(part.firstIndex+part.indexCount<=mesh.indices.size(),"Part index range invalid");
        if(part.kind==Kind::WallBoard||part.kind==Kind::Jamb){
            Require(part.indexCount==132,"Beveled part topology changed");
            if(part.kind==Kind::WallBoard)++boards;else ++jambs;
            continue;
        }
        if(part.kind==Kind::Fascia){Require(part.indexCount==36,"Fascia full box topology changed");++fascias;continue;}
        if(part.kind==Kind::CornerStile){Require(part.indexCount==36,"Corner box topology changed");++corners;continue;}
        ++unchanged;
    }
    Require(boards==48&&jambs==8&&unchanged==68&&corners==4&&fascias==4,"Source parts or fixed whole-art count changed");
    Require(OriginalPartScopeExact(mesh,parts,source,sourceParts),"Original geometry/index and five raw linings not one-to-one");
    {auto bad=parts;bad.back()=parts.front();Require(!OriginalPartScopeExact(mesh,bad,source,sourceParts),"Duplicate protected part reused original identity");}
    {auto bad=mesh;const auto lastGable=std::find_if(parts.rbegin(),parts.rend(),[](const Part& p){return p.kind==Kind::Gable;});
        Require(lastGable!=parts.rend(),"Original protected gable absent");bad.vertices[bad.indices[lastGable->firstIndex]].color^=1u;
        Require(!OriginalPartScopeExact(bad,parts,source,sourceParts),"Changed protected raw vertex passed identity");}
    Require(WeatheredUvPolicy(mesh,parts),"Weathered single-plank signed UV or physical V grain failed");
    {auto bad=mesh;const auto frame=std::find_if(parts.begin(),parts.end(),[](const Part& p){return p.kind==Kind::Jamb;});
        Require(frame!=parts.end(),"Physical grain frame absent");
        for(std::size_t i=frame->firstIndex;i<frame->firstIndex+frame->indexCount;++i)bad.vertices[bad.indices[i]].v*=1.4f;
        Require(!WeatheredUvPolicy(bad,parts),"Wrong physical grain scale passed");}
    {auto bad=mesh;const auto frame=std::find_if(parts.begin(),parts.end(),[](const Part& p){return p.kind==Kind::Jamb;});
        bad.vertices[bad.indices[frame->firstIndex]].u=.5f;Require(!WeatheredUvPolicy(bad,parts),"Off-plank U passed");}
    // 四條簷口的全尺寸與落位由固定设计獨立要求，不用候選bounds當expected。
    unsigned fasciaNumber{};
    for(const auto& part:parts)if(part.kind==Kind::Fascia){
        const Vec3 center=fasciaNumber<2?Vec3{fasciaNumber==0?-2.44f:2.44f,2.92f,0}:
            Vec3{0,2.92f,fasciaNumber==2?-2.94f:2.94f};
        const Vec3 size=fasciaNumber<2?Vec3{.12f,.14f,5.988f}:Vec3{4.88f,.14f,.08f};
        Require(SamePoint(part.centre,center),"Fascia fixed placement changed");
        std::array<bool,8> endpoints{};
        for(std::size_t i=part.firstIndex;i<part.firstIndex+part.indexCount;++i){
            const auto& v=mesh.vertices[mesh.indices[i]];
            const Vec3 low=center-size*.5f,high=center+size*.5f;
            Require((v.x==low.x||v.x==high.x)&&(v.y==low.y||v.y==high.y)&&(v.z==low.z||v.z==high.z),
                "Fascia complete source endpoint changed");
            const unsigned mask=(v.x==high.x?1u:0u)|(v.y==high.y?2u:0u)|(v.z==high.z?4u:0u);endpoints[mask]=true;
        }
        Require(std::all_of(endpoints.begin(),endpoints.end(),[](bool value){return value;}),"Fascia missing closed box corner");
        ++fasciaNumber;
    }
    // 原firstfalse固定query仍測：基線真空，候選必由新角木先hit，不是跳過邊界。
    const Vec3 originalFailureFrom{2.4749999f,.900000036f,-3.0999999f},originalFailureTo{2.4749999f,.900000036f,-2.6500001f};
    const auto originalFailureHit=FirstHit(originalFailureFrom,originalFailureTo,source);
    const auto repairedHit=FirstHit(originalFailureFrom,originalFailureTo,mesh);
    Require(originalFailureHit.t>1&&repairedHit.t<1,"Original inherited corner-gap contrast changed");
    const auto isCornerTriangle=[&](std::size_t triangle){
        if(triangle==std::numeric_limits<std::size_t>::max())return false;
        for(const auto& p:parts)if(p.kind==Kind::CornerStile&&triangle*3>=p.firstIndex&&triangle*3<p.firstIndex+p.indexCount)return true;
        return false;
    };
    Require(isCornerTriangle(repairedHit.triangle),"Inherited corner gap not closed by actual corner geometry");
    // 四角獨立尺寸／全高度見證。來源範圍±2.5保留，80mm料寬與20mm搭接不是epsilon。
    unsigned cornerNumber{};
    for(const auto& part:parts)if(part.kind==Kind::CornerStile){
        const bool west=cornerNumber%2==0;const std::size_t wall=cornerNumber<2?3u:8u;
        const float z=hutSolids[wall].center.z-Mistwatch::Session::rangerLog.z;
        const Vec3 low{west?-2.5f:2.42f,.88f,z-.07f},high{west?-2.42f:2.5f,2.22f,z+.07f};
        std::array<bool,8> found{};
        for(std::size_t i=part.firstIndex;i<part.firstIndex+part.indexCount;++i){
            const auto& v=mesh.vertices[mesh.indices[i]];
            if((v.x!=low.x&&v.x!=high.x)||(v.y!=low.y&&v.y!=high.y)||(v.z!=low.z&&v.z!=high.z))
                throw std::runtime_error("Corner endpoint escaped independent original-source bounds");
            const unsigned mask=(v.x==high.x?1u:0u)|(v.y==high.y?2u:0u)|(v.z==high.z?4u:0u);found[mask]=true;
        }
        Require(std::all_of(found.begin(),found.end(),[](bool value){return value;}),"Corner box missing true endpoint");
        // 西入口立框覆蓋至約-2.48；±2.49取四角原來真正未覆蓋的外側帶。
        const float x=west?-2.49f:2.49f,side=cornerNumber<2?-1.f:1.f;
        for(const float y:{.900000036f,1.55f,2.19999981f}){
            const Vec3 from{x,y,side*3.1f},to{x,y,side*2.65f};const auto hit=FirstHit(from,to,mesh);
            const auto sourceHit=FirstHit(from,to,source);
            if(!(hit.t<1&&isCornerTriangle(hit.triangle))){
                const auto triangle=[](const RayHit& h){
                    if(h.triangle==std::numeric_limits<std::size_t>::max())std::cerr<<"none";
                    else std::cerr<<h.triangle;
                };
                std::cerr<<std::setprecision(std::numeric_limits<float>::max_digits10)
                    <<"CORNER_WITNESS_FIRST_FAILURE cornerNumber="<<cornerNumber<<" x="<<x<<" y="<<y
                    <<" from="<<from.x<<','<<from.y<<','<<from.z<<" to="<<to.x<<','<<to.y<<','<<to.z
                    <<" source_hit="<<(sourceHit.t<1)<<" source_t="<<sourceHit.t<<" source_triangle=";triangle(sourceHit);
                std::cerr<<" candidate_hit="<<(hit.t<1)<<" candidate_t="<<hit.t<<" candidate_triangle=";triangle(hit);
                std::cerr<<" isCornerTriangle="<<isCornerTriangle(hit.triangle)<<'\n';
            }
            Require(hit.t<1&&isCornerTriangle(hit.triangle),"Full-height corner repair witness failed");
        }
        // 原下牆內襯的頂緣可被浮點射線命中；來源空帶只在原 0.9–2.2m 的嚴格內部驗證。
        // 外柱止於 |x|=2.45，西入口框止於約 -2.48；±2.49 的三個內部高度不穿原構件。
        for(const float y:{1.f,1.55f,2.1f}){
            const Vec3 from{x,y,side*3.1f},to{x,y,side*2.65f};
            const auto sourceHit=FirstHit(from,to,source),hit=FirstHit(from,to,mesh);
            Require(sourceHit.t>1&&hit.t<1&&isCornerTriangle(hit.triangle),"Interior corner source-void/repair witness failed");
        }++cornerNumber;
    }
    denseDiagnosticBaseline=&source;denseDiagnosticPrinted=false;
    const bool denseValid=DenseWalls(mesh)&&DenseOpenings(mesh);
    denseDiagnosticBaseline=nullptr;
    Require(denseValid,"Dense wall seal or original opening failed");
    // 外框首 hit 必須真落原窗側框，不能只由內襯碰撞代理算通過。
    const auto foregroundJambs=[&](const Mesh3D& candidate){
        for(const auto& part:parts)if(part.kind==Kind::Jamb){
            const float side=part.centre.z>0?1.f:-1.f;
            const auto hit=FirstHit({part.centre.x,1.55f,side*3.1f},{part.centre.x,1.55f,side*2.65f},candidate);
            if(hit.triangle==std::numeric_limits<std::size_t>::max()||
                hit.triangle*3<part.firstIndex||hit.triangle*3>=part.firstIndex+part.indexCount)return false;
        }return true;
    };
    Require(foregroundJambs(mesh),"Window frame not actual first foreground hit");
    unsigned negative{};
    {auto bad=mesh;bad.indices[0]=static_cast<std::uint32_t>(bad.vertices.size());Require(!SoundMesh(bad,true),"Bad index passed");++negative;}
    {auto bad=mesh;bad.vertices[0].x=std::numeric_limits<float>::quiet_NaN();Require(!SoundMesh(bad,true),"NaN passed");++negative;}
    {auto bad=mesh;bad.vertices[0].z=4;Require(!SoundMesh(bad,true),"Expanded bounds passed");++negative;}
    {auto bad=mesh;std::swap(bad.indices[1],bad.indices[2]);Require(!SoundMesh(bad,true),"Reverse signed winding passed");++negative;}
    {auto bad=mesh;auto& v=bad.vertices[bad.indices[0]];const auto n=UnpackNormalSnorm8(v.normal);v.normal=PackNormalSnorm8({-n.x,-n.y,-n.z});
        Require(!SoundMesh(bad,true),"Opposite signed normal passed");++negative;}
    {auto bad=mesh;bad.indices[1]=bad.indices[0];Require(!SoundMesh(bad,true),"Degenerate triangle passed");++negative;}
    {auto bad=mesh;for(unsigned i=0;i<3;++i){auto& v=bad.vertices[bad.indices[i]];v.u=0;v.v=0;}Require(!SoundMesh(bad,true),"Zero UV tangent basis passed");++negative;}
    {auto bad=mesh;AddObstruction(bad,{-1.15f,1.55f,2.9f});Require(!DenseOpenings(bad),"Blocked window passed");++negative;}
    {auto bad=mesh;AddObstruction(bad,{-2.4f,1.3f,0});Require(!DenseOpenings(bad),"Blocked entrance passed");++negative;}
    {auto bad=mesh;const Part* lining{};for(const auto& p:parts)if(p.kind==Kind::Host&&p.centre.x>2.30f&&p.centre.x<2.34f&&p.centre.y==1.3f)lining=&p;
        Require(lining!=nullptr,"Original east lining not found");bad.indices.erase(bad.indices.begin()+lining->firstIndex,bad.indices.begin()+lining->firstIndex+lining->indexCount);
        Require(!DenseWalls(bad),"Unsealed board gaps passed dense rays");++negative;}
    {auto bad=mesh;const auto jamb=std::find_if(parts.begin(),parts.end(),[](const Part& p){return p.kind==Kind::Jamb;});
        Require(jamb!=parts.end(),"Original jamb absent");std::vector<bool> moved(bad.vertices.size());
        for(std::size_t i=jamb->firstIndex;i<jamb->firstIndex+jamb->indexCount;++i){const auto index=bad.indices[i];
            if(!moved[index]){bad.vertices[index].z-=std::copysign(.04f,jamb->centre.z);moved[index]=true;}}
        Require(!foregroundJambs(bad),"Buried jamb passed first-hit control");++negative;}
    {auto bad=mesh;const auto firstCorner=std::find_if(parts.begin(),parts.end(),[](const Part& p){return p.kind==Kind::CornerStile;});
        Require(firstCorner!=parts.end(),"Four structural corners absent");
        bad.indices.erase(bad.indices.begin()+firstCorner->firstIndex,bad.indices.begin()+firstCorner->firstIndex+4*36);
        Require(FirstHit(originalFailureFrom,originalFailureTo,bad).t>1&&!DenseWalls(bad),"Removed four corners passed original dense domain");}
    // 新增current守門的代表性負控；原11個mesh負控與其所有門檻全文保留。
    unsigned currentNegative{};
    {auto bad=parts;Require(bad.size()>1,"Current coverage control has no parts");bad[1].firstIndex=bad[0].firstIndex;
        Require(!CurrentPartCoverage(mesh,bad),"Overlapping current part ranges accepted");++currentNegative;}
    {Mesh3D bad;bad.vertices={mesh.vertices[mesh.indices[0]],mesh.vertices[mesh.indices[1]],mesh.vertices[mesh.indices[2]]};bad.indices={0,1,2};
        bad.vertices[0].u=0;bad.vertices[0].v=0;bad.vertices[1].u=1;bad.vertices[1].v=0;bad.vertices[2].u=0;bad.vertices[2].v=5.e-10f;
        Require(!CurrentUvMagnitude(bad,true),"Finite nonzero but sub-threshold current UV accepted");++currentNegative;}
    {auto bad=mesh;const auto frame=std::find_if(parts.begin(),parts.end(),[](const Part& p){return p.kind==Kind::UpperFrame;});
        Require(frame!=parts.end(),"Current full frame plane control missing");std::vector<bool> moved(bad.vertices.size());
        for(std::size_t i=frame->firstIndex;i<frame->firstIndex+frame->indexCount;++i){const auto index=bad.indices[i];
            if(!moved[index]){bad.vertices[index].z+=std::copysign(.001f,frame->centre.z);moved[index]=true;}}
        Require(!CurrentFrameSamples(bad,parts,true),"Shifted actual upper-frame exterior plane accepted");++currentNegative;}
    {auto bad=currentOldModes;bad[1].vertices[0].x+=.001f;
        Require(!CurrentOldModeControls(bad),"Current material-only changed position accepted");++currentNegative;}
    {auto bad=mesh;bad.vertices[bad.indices[2]].normal=0;
        Require(!CurrentTriangleQuality(bad),"Third corner non-unit packed normal accepted");++currentNegative;}
    {auto bad=mesh;auto& vertex=bad.vertices[bad.indices[1]];const auto unpacked=UnpackNormalSnorm8(vertex.normal);
        const Vec3 n{unpacked.x,unpacked.y,unpacked.z};const auto axis=std::abs(n.x)<.9f?Vec3{1,0,0}:Vec3{0,1,0};
        const auto tilted=Normalize(n+Normalize(Cross(n,axis))*.08f);vertex.normal=PackNormalSnorm8(tilted);
        Require(!CurrentTriangleQuality(bad),"Second corner positive-dot but excessive normal angle accepted");++currentNegative;}
    {Mesh3D bad;bad.vertices={mesh.vertices[mesh.indices[0]],mesh.vertices[mesh.indices[1]],mesh.vertices[mesh.indices[2]]};bad.indices={0,1,2};
        bad.vertices[0].x=0;bad.vertices[0].y=0;bad.vertices[0].z=0;
        bad.vertices[1].x=.0001f;bad.vertices[1].y=0;bad.vertices[1].z=0;
        bad.vertices[2].x=0;bad.vertices[2].y=.0001f;bad.vertices[2].z=0;
        Require(!CurrentTriangleQuality(bad),"Finite positive but sub-threshold double triangle area accepted");++currentNegative;}
    {auto bad=worldBefore;Require(!bad.empty(),"Independent world negative control empty");
        bad[0].extents.x=std::nextafter(bad[0].extents.x,std::numeric_limits<float>::infinity());
        Require(!CurrentWorldExact(worldBefore,bad),"Changed exact current collider component accepted");++currentNegative;}
    Require(currentPartCoverages==4&&currentUvTriangles==4908&&currentFrameRays==64&&currentHostRays==64&&
        currentWorldSnapshots==4&&currentNegative==8,"Fixed current coverage/control counts changed");
    std::cout<<"HUT_WHOLE_ART_CONTRACT source_vertices=3432 source_triangles=1708 render_vertices="<<mesh.vertices.size()
        <<" render_triangles="<<mesh.indices.size()/3<<" wall_boards="<<boards<<" jambs="<<jambs<<" corner_stiles="<<corners<<" fascia="<<fascias<<" source_pose_parts="<<unchanged<<" protected_raw_parts=5"
        <<" other_modes_raw_exact=3 material_exact=8 weathered_material_branches=8 atlas_negative_controls=2"
        <<" material_source_negative_controls="<<sourceNegative<<" original_camera_regression=1 protected_part_negative_controls=2 corner_negative_controls=1 negative_controls="<<negative<<'\n';
    std::cout<<"HUT_CURRENT_PRODUCT_CONTRACT modes=4 part_coverages="<<currentPartCoverages
        <<" authored_uv_triangles="<<currentUvTriangles<<" frame_first_rays="<<currentFrameRays<<" negative_host_rays="<<currentHostRays
        <<" invalid_mode_rejected=1 old_mode_controls=1 combined_quality_triangles="<<mesh.indices.size()/3
        <<" normal_corner_checks="<<mesh.indices.size()<<" world_snapshots="<<currentWorldSnapshots
        <<" current_negative_controls="<<currentNegative<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE: "<<e.what()<<'\n';return 1;}}
