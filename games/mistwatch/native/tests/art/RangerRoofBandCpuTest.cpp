#include "MistwatchRangerRoofBand.h"
#include "MistwatchCamera.h"
#include "MistwatchHutWoodMaterial.h"
#include <iomanip>
#include <iostream>
#include <string>

// 原全文含自己的main巨集，先載入所有global依存，再用namespace保留全文。
// 原FormalWorldSnapshotIntegration.cpp仍是獨立TU，不以此處current前後自比取代。
std::vector<Aevrynd::BoxCollider3D> FormalPropWorldSnapshot(bool,Mistwatch::FirLayoutSelector);
#ifdef main
#error V1212_requires_original_main_macro
#endif
namespace V1212Original {
#include "HutWholeArtCpuTest.cpp"
std::vector<Aevrynd::BoxCollider3D> FormalPropWorldSnapshot(bool repaired,Mistwatch::FirLayoutSelector selector){
    return ::FormalPropWorldSnapshot(repaired,selector);
}
}

namespace V1212 {
using namespace Aevrynd;
using Part=Mistwatch::RangerHutPart;
unsigned checks{},negatives{};
const char* phase="initialization";
void Check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Point(Vec3 a,Vec3 b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
bool PartEqual(const Part& a,const Part& b){
    return a.kind==b.kind&&a.firstIndex==b.firstIndex&&a.indexCount==b.indexCount&&Point(a.centre,b.centre);
}
double J(const Mesh3D& mesh,std::size_t first){
    const auto& a=mesh.vertices[mesh.indices[first]];const auto& b=mesh.vertices[mesh.indices[first+1]];
    const auto& c=mesh.vertices[mesh.indices[first+2]];
    return (double(b.u)-a.u)*(double(c.v)-a.v)-(double(b.v)-a.v)*(double(c.u)-a.u);
}
// 原unit-box的六個face順序與法線，獨立推導transverse t；不讀候選U或作者helper。
// -Z/+Z面沿X紋、跨Y；-X/+X端面沿Z紋、跨Y；+Y/-Y面沿X紋、跨Z。
float ExpectedU(std::size_t record){
    const auto face=record/4u,corner=record%4u;
    const float y=corner<2u?-.5f:.5f;
    float t{};
    if(face==0u||face==3u)t=y+.5f;
    else if(face==1u||face==2u)t=.5f-y;
    else if(face==4u){const float z=corner<2u?-.5f:.5f;t=z+.5f;}
    else if(face==5u){const float z=corner<2u?.5f:-.5f;t=.5f-z;}
    else throw std::runtime_error("independent source face ordinal invalid");
    return 932.f/1024.f+t*(74.f/1024.f);
}
bool Validate(const Mesh3D& mesh,const std::vector<Part>& parts,const Mesh3D& source,
              const std::vector<Part>& sourceParts,std::string* reason=nullptr){
    const auto fail=[&](const char* text,std::size_t item){
        if(reason)*reason=std::string(text)+" item="+std::to_string(item);return false;
    };
    if(!mesh.Valid()||!source.Valid()||mesh.vertices.size()!=6864||source.vertices.size()!=6864||
       mesh.indices.size()!=9600||mesh.indices!=source.indices||parts.size()!=132||sourceParts.size()!=132)return fail("membership/index",0);
    for(std::size_t p=0;p<parts.size();++p)if(!PartEqual(parts[p],sourceParts[p]))return fail("part fields raw",p);
    const auto a=mesh.Bounds(),b=source.Bounds();
    if(!Point(a.center,b.center)||!Point(a.extents,b.extents)||!Bits(a.radius,b.radius))return fail("bounds raw",0);
    std::vector<bool> owned(mesh.vertices.size());std::size_t ownedRecords{};
    for(unsigned slot=0;slot<24u;++slot){
        const Vec3 center{slot<12u?-1.24f:1.24f,3.335f,-2.75f+.5f*float(slot%12u)};
        std::size_t found=parts.size(),count{};
        for(std::size_t p=0;p<sourceParts.size();++p)
            if(sourceParts[p].kind==Mistwatch::RangerHutPartKind::Roof&&Point(sourceParts[p].centre,center)){found=p;++count;}
        if(count!=1u||sourceParts[found].indexCount!=36u)return fail("source roof center/range",slot);
        const auto& part=sourceParts[found];
        if(part.firstIndex>source.indices.size()||part.indexCount>source.indices.size()-part.firstIndex)return fail("source roof range bounds",slot);
        const auto begin=source.indices.begin()+part.firstIndex;
        const std::size_t base=*std::min_element(begin,begin+36u);
        if(base>mesh.vertices.size()-24u)return fail("source roof vertex bounds",slot);
        for(std::size_t k=0;k<24u;++k){
            const auto i=base+k;if(owned[i])return fail("duplicate owned record",i);owned[i]=true;++ownedRecords;
            if(!Bits(mesh.vertices[i].u,ExpectedU(k)))return fail("source-derived fixed U",i);
        }
        const auto primitive=Mesh3D::CreateUnitBox();
        for(std::size_t k=0;k<36u;++k)if(source.indices[part.firstIndex+k]!=base+primitive.indices[k])return fail("source primitive index",slot);
        const float side=slot<12u?-1.f:1.f,angle=std::atan(.85f/2.48f);
        const Vec3 size{std::hypot(2.48f,.85f),.1f,.494f};
        const auto transform=CreateTransformMatrix(center,{0,0,-side*angle},size);
        const auto normalTransform=CreateNormalMatrix(transform);
        for(std::size_t k=0;k<24u;++k){const auto& local=primitive.vertices[k];const auto& old=source.vertices[base+k];
            const auto position=TransformPoint(transform,{local.x,local.y,local.z});const auto n=UnpackNormalSnorm8(local.normal);
            if(!Point({old.x,old.y,old.z},position)||old.normal!=PackNormalSnorm8(TransformNormal(normalTransform,{n.x,n.y,n.z}))||
               old.color!=PackColorRgba8({.76f,.75f,.71f,1}))return fail("source slope/size/normal/color",base+k);
            const auto face=k/4u;
            const float along=(face==2u||face==3u)?local.z:local.x;
            const float extent=(face==2u||face==3u)?size.z:size.x;
            // 原append來源：主坡板虛擬ordinal114..137，不是Combined的part ordinal。
            const float expectedV=(along+.5f)*extent/1.4f+.17f*float((114u+slot)%5u);
            if(!Bits(old.v,expectedV))return fail("source grain/phase",base+k);
        }
    }
    if(ownedRecords!=576u)return fail("owned count",ownedRecords);
    for(std::size_t i=0;i<mesh.vertices.size();++i){const auto& old=source.vertices[i];const auto& v=mesh.vertices[i];
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v))return fail("finite",i);
        if(std::memcmp(&old,&v,20)!=0||!Bits(old.v,v.v)||(!owned[i]&&!Bits(old.u,v.u)))return fail("nonowned byte",i);
        auto inverse=v;if(owned[i])inverse.u=old.u;
        if(std::memcmp(&inverse,&old,28)!=0)return fail("whole inverse",i);
    }
    for(std::size_t first=0;first<mesh.indices.size();first+=3){const double old=J(source,first),now=J(mesh,first);
        if(!std::isfinite(old)||!std::isfinite(now)||old<=1.e-9||now<=1.e-9)return fail("original signed/magnitude UV",first/3u);
    }
    return true;
}
template<class Change>void Reject(const Mesh3D& candidate,const std::vector<Part>& parts,const Mesh3D& source,
                                const std::vector<Part>& sourceParts,Change change,const char* message){
    auto bad=candidate;auto badParts=parts;change(bad,badParts);++negatives;
    Check(!Validate(bad,badParts,source,sourceParts),message);
}
template<class Change>void RejectSource(const Mesh3D& source,const std::vector<Part>& parts,Change change,const char* message){
    auto bad=source;auto badParts=parts;change(bad,badParts);const auto snapshot=bad;const auto partSnapshot=badParts;bool threw=false;
    try{(void)Mistwatch::RangerRoofBand::ApplyFixedRoofBand(bad,badParts);}catch(const std::runtime_error&){threw=true;}
    ++negatives;Check(threw,message);Check(V1212Original::SameMesh(bad,snapshot),"rejected source changed in place");
    for(std::size_t p=0;p<badParts.size();++p)Check(PartEqual(badParts[p],partSnapshot[p]),"rejected source parts changed in place");
}
}

int main(){try{
    using namespace V1212;phase="unchanged-full-original-suite";
    Check(V1212Original::main()==0,"unmodified HutWholeArtCpuTest failed");
    std::vector<Part> parts;const auto source=Mistwatch::CreateRangerHut({},&parts);const auto partsBefore=parts;
    const auto worldBefore=Mistwatch::WorldSolids(true),solidsBefore=Mistwatch::RangerHutSolids();
    const auto cameraBefore=Mistwatch::RangerHutCameraRoofSolid();
    phase="candidate-source-closure";
    const auto candidate=Mistwatch::RangerRoofBand::ApplyFixedRoofBand(source,parts);
    const auto repeated=Mistwatch::RangerRoofBand::CreateRangerHutRoofBand();
    std::string detail;const bool valid=Validate(candidate,parts,source,partsBefore,&detail);Check(valid,detail.c_str());
    Check(V1212Original::SameMesh(candidate,repeated),"public factory repeat all vertex/index bits");
    phase="candidate-original-physical-and-uv-gates";
    Check(V1212Original::SoundMesh(candidate,true)&&V1212Original::CurrentUvMagnitude(candidate,true)&&
          V1212Original::CurrentTriangleQuality(candidate)&&V1212Original::CurrentPartCoverage(candidate,parts)&&
          V1212Original::WeatheredUvPolicy(candidate,parts)&&V1212Original::CurrentFrameSamples(candidate,parts,true),
          "original candidate physical/signed UV/grain/frame gate");
    Check(V1212Original::DenseWalls(candidate)&&V1212Original::DenseOpenings(candidate),"original unchanged dense queries on true candidate");
    Check(V1212Original::CurrentWorldExact(worldBefore,Mistwatch::WorldSolids(true))&&
          V1212Original::CurrentWorldExact(solidsBefore,Mistwatch::RangerHutSolids()),"world/collider bits");
    const auto cameraAfter=Mistwatch::RangerHutCameraRoofSolid();
    Check(Point(cameraBefore.center,cameraAfter.center)&&Point(cameraBefore.extents,cameraAfter.extents),"camera collider bits");
    phase="local-negative-controls";
    // 從來源枚舉固定centre定位，負控不得讀作者範圍helper。
    const auto first=std::find_if(parts.begin(),parts.end(),[](const Part& p){return Point(p.centre,{-1.24f,3.335f,-2.75f});});
    Check(first!=parts.end(),"first roof witness absent");const auto roof=source.indices[first->firstIndex];
    const auto underlay=std::find_if(parts.begin(),parts.end(),[](const Part& p){return p.kind==Mistwatch::RangerHutPartKind::Roof&&p.centre.z==0.f&&p.centre.y<3.335f;});
    const auto ridge=std::find_if(parts.begin(),parts.end(),[](const Part& p){return Point(p.centre,{0,3.78f,0});});
    Check(underlay!=parts.end()&&ridge!=parts.end(),"protected roof witnesses absent");
    const auto liningIndex=source.indices[underlay->firstIndex],ridgeIndex=source.indices[ridge->firstIndex];
    ++negatives;Check(!Validate(source,parts,source,parts),"old four-band pattern accepted as new contract");
    Reject(candidate,parts,source,parts,[&](Mesh3D& m,auto&){m.vertices[roof].u=.5f;},"off-band U");
    Reject(candidate,parts,source,parts,[&](Mesh3D& m,auto&){m.vertices[roof].v+=.17f;},"changed V/phase");
    Reject(candidate,parts,source,parts,[&](Mesh3D& m,auto&){m.vertices[roof].x=std::nextafter(m.vertices[roof].x,INFINITY);},"changed XYZ");
    Reject(candidate,parts,source,parts,[&](Mesh3D& m,auto&){m.vertices[roof].normal^=1u;},"changed packed normal");
    Reject(candidate,parts,source,parts,[&](Mesh3D& m,auto&){m.vertices[roof].color^=1u;},"changed color");
    Reject(candidate,parts,source,parts,[](Mesh3D& m,auto&){m.vertices[0].u+=.01f;},"protected nonroof U");
    Reject(candidate,parts,source,parts,[&](Mesh3D& m,auto&){m.vertices[liningIndex].u+=.01f;},"protected thin roof U");
    Reject(candidate,parts,source,parts,[&](Mesh3D& m,auto&){m.vertices[ridgeIndex].u+=.01f;},"protected ridge U");
    Reject(candidate,parts,source,parts,[&](Mesh3D& m,auto&){std::swap(m.indices[first->firstIndex+1],m.indices[first->firstIndex+2]);},"reverse index winding");
    Reject(candidate,parts,source,parts,[](Mesh3D& m,auto&){m.indices[0]=static_cast<std::uint32_t>(m.vertices.size());},"out-of-range index");
    Reject(candidate,parts,source,parts,[](Mesh3D&,auto& p){p[0].centre.x+=.001f;},"Part center raw");
    Reject(candidate,parts,source,parts,[](Mesh3D&,auto& p){p.back()=p.front();},"duplicate Part ownership");
    Reject(candidate,parts,source,parts,[](Mesh3D& m,auto&){m.vertices.pop_back();},"vertex membership");
    Reject(candidate,parts,source,parts,[&](Mesh3D& m,auto&){m.vertices[roof].u=std::numeric_limits<float>::quiet_NaN();},"nonfinite U");
    {auto bad=candidate;for(std::size_t k=0;k<4;++k)bad.vertices[roof+k].u=2.f*(969.f/1024.f)-bad.vertices[roof+k].u;
        ++negatives;Check(J(bad,first->firstIndex)<0&&!Validate(bad,parts,source,parts),"signed U reversal");}
    {auto bad=candidate;for(std::size_t k=0;k<4;++k)bad.vertices[roof+k].u=932.f/1024.f;
        ++negatives;Check(J(bad,first->firstIndex)==0&&!Validate(bad,parts,source,parts),"zero UV determinant");}
    RejectSource(source,parts,[&](Mesh3D& m,auto&){m.vertices[roof].u+=.001f;},"source UV drift did not fail closed");
    RejectSource(source,parts,[](Mesh3D&,auto& p){p.back()=p.front();},"source duplicate Part did not fail closed");
    RejectSource(source,parts,[](Mesh3D& m,auto&){m.vertices[0].x=std::numeric_limits<float>::quiet_NaN();},"source finite drift did not fail closed");
    {const auto definition=Mistwatch::HutWeatheredWoodDefinition();auto bad=definition;++bad.textures.integrity[0].expectedBytes;
        ++negatives;Check(!V1212Original::WeatheredSourcePolicy(bad),"texture integrity wrong domain");
        auto material=definition.material;material.normalScale=0;
        ++negatives;Check(!V1212Original::SameMaterial(material,definition.material),"material wrong domain");}
    {auto bad=worldBefore;bad[0].extents.x=std::nextafter(bad[0].extents.x,INFINITY);
        ++negatives;Check(!V1212Original::CurrentWorldExact(bad,worldBefore),"world wrong domain");}
    std::size_t changed{};for(std::size_t i=0;i<source.vertices.size();++i)changed+=!Bits(source.vertices[i].u,candidate.vertices[i].u);
    // 24連續source ordinals含每帶6板：18板×24records改U，另6板原已是目標帶。
    Check(changed==18u*24u,"source-derived changed U count");
    std::cout<<"V1212_ROOF_BAND_CPU_PASS checks="<<checks<<" negatives="<<negatives<<" owned_records=576 protected_records=6288 changed_u_records="<<changed
             <<" vertices=6864 indices=9600 triangles=3200 parts=132 owned_parts=24 protected_parts=108"
             <<" original_full_suite=1 inverse=1 native=false art=false game=false\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE V1212 phase="<<V1212::phase<<" reason="<<error.what()<<'\n';return 1;}}
