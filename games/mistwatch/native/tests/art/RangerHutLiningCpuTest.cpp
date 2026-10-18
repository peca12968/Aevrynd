#include "MistwatchRangerHutLining.h"
#include "MistwatchCamera.h"
#include "MistwatchHutWoodMaterial.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

std::vector<Aevrynd::BoxCollider3D> FormalPropWorldSnapshot(bool,Mistwatch::FirLayoutSelector);
// 原RoofBand全文內再含原HutWhole全文；兩原件不改字節，不送新退襯給舊raw合同。
namespace V1274Original {
#include "RangerRoofBandCpuTest.cpp"
}
namespace Mistwatch {
inline Aevrynd::Mesh3D CreateV1274RegressionHut(){return RangerHutLining::CreateRangerHutLining();}
}
// 只替換原物理回歸的兩個factory token；原2916/130/21查詢與所有門檻不變。
namespace V1274Physical {
#define CreateRangerHut CreateV1274RegressionHut
#define main CandidateRegressionMain
#include "MistwatchRangerHutTest.cpp"
#undef main
#undef CreateRangerHut
}

namespace V1274 {
using namespace Aevrynd;
using Part=Mistwatch::RangerHutPart;
using Kind=Mistwatch::RangerHutPartKind;
namespace O=V1274Original::V1212Original;
unsigned checks{},negatives{},boardRays{},seamRays{},closedBoxes{},joins{};
const char* phase="initialization";
void Check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Point(Vec3 a,Vec3 b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
bool PartEqual(const Part& a,const Part& b){return a.kind==b.kind&&a.firstIndex==b.firstIndex&&a.indexCount==b.indexCount&&Point(a.centre,b.centre);}
float Coordinate(Vec3 p,unsigned axis){return axis==0?p.x:p.z;}
float Coordinate(const Vertex3D& v,unsigned axis){return axis==0?v.x:v.z;}
void Coordinate(Vec3& p,unsigned axis,float value){if(axis==0)p.x=value;else p.z=value;}
void Coordinate(Vertex3D& v,unsigned axis,float value){if(axis==0)v.x=value;else v.z=value;}
struct ExpectedSlot {std::size_t solid,part,base,first,boards;unsigned axis;float side;Vec3 offset,extent;};
// 由原append計數：先24v/36idx箱，再每板96v/132idx；不是候選觀測golden。
constexpr std::array<ExpectedSlot,5> expected{{
    {1,1,24,36,24,0,1,{2.4f,1.3f,0},{.1f,1.3f,3}},
    {3,26,2352,3240,9,2,-1,{0,.45f,-2.9f},{2.5f,.45f,.1f}},
    {4,36,3240,4464,3,2,-1,{0,2.4f,-2.9f},{2.5f,.2f,.1f}},
    {8,43,3624,5004,9,2,1,{0,.45f,2.9f},{2.5f,.45f,.1f}},
    {9,53,4512,6228,3,2,1,{0,2.4f,2.9f},{2.5f,.2f,.1f}}
}};
struct Geometry {Vec3 wall,size,oldCentre,newCentre,liningSize,out;};
Geometry ExpectedGeometry(const ExpectedSlot& slot){
    // 保留原world相加再相減的float32路徑，不能用四捨五入的local centre。
    const auto world=Mistwatch::Session::rangerLog+slot.offset;
    const auto wall=world-Mistwatch::Session::rangerLog,size=slot.extent*2.f;
    auto old=wall,lining=size,next=wall;Coordinate(old,slot.axis,Coordinate(wall,slot.axis)-slot.side*.078f);
    Coordinate(lining,slot.axis,.036f);
    const float boardDepth=Coordinate(size,slot.axis)-.06f;
    const float inner=Coordinate(wall,slot.axis)-slot.side*(boardDepth*.5f);
    Coordinate(next,slot.axis,inner+slot.side*(.012f+.036f*.5f));
    Vec3 out{};Coordinate(out,slot.axis,slot.side);return {wall,size,old,next,lining,out};
}
bool Validate(const Mesh3D& mesh,const std::vector<Part>& parts,const Mesh3D& source,const std::vector<Part>& original,std::string* detail=nullptr){
    const auto fail=[&](const char* why,std::size_t i){if(detail)*detail=std::string(why)+" item="+std::to_string(i);return false;};
    if(!mesh.Valid()||!source.Valid()||mesh.vertices.size()!=6864||source.vertices.size()!=6864||mesh.indices.size()!=9600||
       mesh.indices!=source.indices||parts.size()!=132||original.size()!=132)return fail("membership/index",0);
    const auto solids=Mistwatch::RangerHutSolids();const auto box=Mesh3D::CreateUnitBox();
    if(solids.size()!=13||box.vertices.size()!=24||box.indices.size()!=36)return fail("source membership",0);
    std::vector<int> axis(mesh.vertices.size(),-1);auto inverse=mesh;auto inverseParts=parts;
    for(const auto& slot:expected){const auto g=ExpectedGeometry(slot);const auto& p=original[slot.part];
        if(!Point(solids[slot.solid].center,Mistwatch::Session::rangerLog+slot.offset)||!Point(solids[slot.solid].extents,slot.extent)||
           p.kind!=Kind::Host||p.firstIndex!=slot.first||p.indexCount!=36||!Point(p.centre,g.oldCentre))return fail("fixed source append identity",slot.part);
        if(parts[slot.part].kind!=p.kind||parts[slot.part].firstIndex!=p.firstIndex||parts[slot.part].indexCount!=p.indexCount||
           !Point(parts[slot.part].centre,g.newCentre))return fail("independent lining centre",slot.part);
        const auto oldMatrix=CreateTransformMatrix(g.oldCentre,{},g.liningSize),newMatrix=CreateTransformMatrix(g.newCentre,{},g.liningSize);
        for(std::size_t k=0;k<36;++k)if(source.indices[slot.first+k]!=slot.base+box.indices[k])return fail("source box connectivity",slot.first+k);
        for(std::size_t k=0;k<24;++k){const auto i=slot.base+k;const auto& local=box.vertices[k];
            const Vec3 old=TransformPoint(oldMatrix,{local.x,local.y,local.z}),now=TransformPoint(newMatrix,{local.x,local.y,local.z});
            if(axis[i]!=-1||!Point({source.vertices[i].x,source.vertices[i].y,source.vertices[i].z},old)||
               !Bits(Coordinate(mesh.vertices[i],slot.axis),Coordinate(now,slot.axis)))return fail("source-derived single-axis endpoint",i);
            axis[i]=static_cast<int>(slot.axis);Coordinate(inverse.vertices[i],slot.axis,Coordinate(source.vertices[i],slot.axis));
        }
        Coordinate(inverseParts[slot.part].centre,slot.axis,Coordinate(p.centre,slot.axis));
    }
    if(std::count_if(axis.begin(),axis.end(),[](int i){return i>=0;})!=120)return fail("ownership count",0);
    for(std::size_t p=0;p<parts.size();++p){
        if(!PartEqual(inverseParts[p],original[p]))return fail("protected part/centre inverse",p);
        for(std::size_t k=0;k<parts[p].indexCount;++k){const auto i=mesh.indices[parts[p].firstIndex+k];
            if(axis[i]<0)continue;bool own=false;
            for(const auto& slot:expected)if(slot.part==p&&i>=slot.base&&i-slot.base<24)own=true;
            if(!own)return fail("protected alias",i);
        }
    }
    for(std::size_t i=0;i<mesh.vertices.size();++i)
        if(!O::SameVertex(inverse.vertices[i],source.vertices[i]))return fail("whole28-byte inverse/nonowned field",i);
    const auto a=mesh.Bounds(),b=source.Bounds();
    if(!Point(a.center,b.center)||!Point(a.extents,b.extents)||!Bits(a.radius,b.radius))return fail("whole bounds",0);
    // 不豁除UV/normal/門窗/面積；用原真候選方法及原threshold。
    if(!O::SoundMesh(mesh,true)||!O::CurrentTriangleQuality(mesh)||!O::CurrentUvMagnitude(mesh,true)||
       !O::CurrentPartCoverage(mesh,parts)||!O::WeatheredUvPolicy(mesh,parts))return fail("original physical/UV/normal gate",0);
    for(std::size_t first=0;first<mesh.indices.size();first+=3)
        if(!(V1274Original::V1212::J(mesh,first)>1.e-9))return fail("original positive signed UV",first/3);
    return true;
}
struct Extent {Vec3 low{100,100,100},high{-100,-100,-100};};
Extent PartBounds(const Mesh3D& mesh,const Part& part){
    Extent e;for(std::size_t k=0;k<part.indexCount;++k){const auto& v=mesh.vertices[mesh.indices[part.firstIndex+k]];
        e.low={(std::min)(e.low.x,v.x),(std::min)(e.low.y,v.y),(std::min)(e.low.z,v.z)};
        e.high={(std::max)(e.high.x,v.x),(std::max)(e.high.y,v.y),(std::max)(e.high.z,v.z)};
    }return e;
}
bool PositiveOverlap(const Extent& a,const Extent& b){
    return (std::min)(a.high.x,b.high.x)>(std::max)(a.low.x,b.low.x)&&
           (std::min)(a.high.y,b.high.y)>(std::max)(a.low.y,b.low.y)&&
           (std::min)(a.high.z,b.high.z)>(std::max)(a.low.z,b.low.z);
}
bool ClosedLining(const Mesh3D& mesh,const std::vector<Part>& parts,const ExpectedSlot& slot){
    if(!mesh.Valid()||parts.size()!=132)return false;
    const auto& part=parts[slot.part];if(part.firstIndex!=slot.first||part.indexCount!=36)return false;
    const auto e=PartBounds(mesh,part);const auto g=ExpectedGeometry(slot);const auto low=g.wall-g.size*.5f,high=g.wall+g.size*.5f;
    if(e.low.x<low.x||e.low.y<low.y||e.low.z<low.z||e.high.x>high.x||e.high.y>high.y||e.high.z>high.z)return false;
    std::array<bool,8> corners{};std::map<std::pair<unsigned,unsigned>,std::pair<unsigned,int>> edges;
    const auto corner=[&](const Vertex3D& v)->unsigned{
        if((v.x!=e.low.x&&v.x!=e.high.x)||(v.y!=e.low.y&&v.y!=e.high.y)||(v.z!=e.low.z&&v.z!=e.high.z))return 8;
        return (v.x==e.high.x?1u:0u)|(v.y==e.high.y?2u:0u)|(v.z==e.high.z?4u:0u);
    };
    double volume{};
    for(std::size_t k=0;k<36;k+=3){std::array<unsigned,3> ids{};std::array<Vec3,3> positions{};
        for(unsigned j=0;j<3;++j){const auto& v=mesh.vertices[mesh.indices[slot.first+k+j]];ids[j]=corner(v);if(ids[j]>=8)return false;
            corners[ids[j]]=true;positions[j]=Vec3{v.x,v.y,v.z}-g.newCentre;
        }
        for(unsigned j=0;j<3;++j){const auto a=ids[j],b=ids[(j+1u)%3u];if(a==b)return false;
            auto& edge=edges[{(std::min)(a,b),(std::max)(a,b)}];++edge.first;edge.second+=a<b?1:-1;
        }
        const auto& a=positions[0];const auto& b=positions[1];const auto& c=positions[2];
        volume+=(double(a.x)*(double(b.y)*c.z-double(b.z)*c.y)+double(a.y)*(double(b.z)*c.x-double(b.x)*c.z)+
                 double(a.z)*(double(b.x)*c.y-double(b.y)*c.x))/6.;
    }
    if(!std::all_of(corners.begin(),corners.end(),[](bool b){return b;})||edges.size()!=18||!(volume>0))return false;
    for(const auto& [key,value]:edges){(void)key;if(value.first!=2||value.second!=0)return false;}
    return 8-18+12==2;
}
std::size_t Owner(const O::RayHit& hit,const std::vector<Part>& parts){
    if(hit.t>1||hit.triangle==std::numeric_limits<std::size_t>::max())return parts.size();
    const auto index=hit.triangle*3;std::size_t result=parts.size();
    for(std::size_t p=0;p<parts.size();++p)if(index>=parts[p].firstIndex&&index-parts[p].firstIndex<parts[p].indexCount){
        if(result!=parts.size())return parts.size();result=p;
    }return result;
}
void Witness(bool value,const char* kind,std::size_t p,Vec3 from,Vec3 to,const O::RayHit& before,const O::RayHit& after){
    if(!value)std::cerr<<std::setprecision(std::numeric_limits<float>::max_digits10)<<"LINING_FIRST_HIT_FAILURE kind="<<kind
        <<" part="<<p<<" from="<<from.x<<','<<from.y<<','<<from.z<<" to="<<to.x<<','<<to.y<<','<<to.z
        <<" source_t="<<before.t<<" source_triangle="<<before.triangle<<" candidate_t="<<after.t<<" candidate_triangle="<<after.triangle<<'\n';
    Check(value,kind);
}
void BoardAndSeamWitnesses(const Mesh3D& source,const Mesh3D& mesh,const std::vector<Part>& parts){
    for(const auto& slot:expected){const auto g=ExpectedGeometry(slot);const bool alongZ=slot.axis==0;
        const unsigned rows=static_cast<unsigned>(std::round(g.size.y/.34f));const float rowHeight=g.size.y/float(rows);
        const float span=alongZ?g.size.z:g.size.x;const unsigned pieces=span>3.f?3u:1u;
        Check(std::size_t(rows)*pieces==slot.boards,"source board row/piece count");
        for(unsigned row=0;row<rows;++row){const float stagger=row%2u==0?.18f:-.18f;
            for(unsigned piece=0;piece<pieces;++piece){const std::size_t p=slot.part+1u+std::size_t(row)*pieces+piece;
                const float low=-span*.5f+span*float(piece)/float(pieces)+(piece>0?stagger:0.f);
                const float high=-span*.5f+span*float(piece+1u)/float(pieces)+(piece+1u<pieces?stagger:0.f);
                auto centre=g.wall;centre.y=g.wall.y-g.size.y*.5f+rowHeight*(float(row)+.5f);
                if(alongZ)centre.z+=(low+high)*.5f;else centre.x+=(low+high)*.5f;
                Check(parts[p].kind==Kind::WallBoard&&parts[p].indexCount==132&&Point(parts[p].centre,centre),"source board append/centre identity");
                const auto from=centre-g.out*.3f,to=centre+g.out*.2f;const auto before=O::FirstHit(from,to,source),after=O::FirstHit(from,to,mesh);
                Witness(Owner(before,parts)==slot.part&&Owner(after,parts)==p,"48-board interior first-hit",p,from,to,before,after);++boardRays;
                if(piece+1u<pieces){auto seam=centre;const float joint=-span*.5f+span*float(piece+1u)/float(pieces)+stagger;
                    if(alongZ)seam.z=g.wall.z+joint;else seam.x=g.wall.x+joint;
                    const auto a=seam-g.out*.3f,b=seam+g.out*.2f;const auto oldHit=O::FirstHit(a,b,source),newHit=O::FirstHit(a,b,mesh);
                    Witness(Owner(oldHit,parts)==slot.part&&Owner(newHit,parts)==slot.part,"vertical joint backed",slot.part,a,b,oldHit,newHit);++seamRays;
                }
            }
            if(row+1u<rows){auto seam=g.wall;seam.y=g.wall.y-g.size.y*.5f+rowHeight*float(row+1u);
                const auto a=seam-g.out*.3f,b=seam+g.out*.2f;const auto oldHit=O::FirstHit(a,b,source),newHit=O::FirstHit(a,b,mesh);
                Witness(Owner(oldHit,parts)==slot.part&&Owner(newHit,parts)==slot.part,"horizontal joint backed",slot.part,a,b,oldHit,newHit);++seamRays;
            }
        }
    }
    Check(boardRays==48&&seamRays==43,"fixed source witness coverage count");
}
void StructuralJoins(const Mesh3D& source,const Mesh3D& mesh,const std::vector<Part>& parts){
    for(const auto& slot:expected){const auto old=PartBounds(source,parts[slot.part]),now=PartBounds(mesh,parts[slot.part]);
        for(std::size_t p=0;p<parts.size();++p){if(p==slot.part)continue;const auto partner=PartBounds(mesh,parts[p]);
            if(!PositiveOverlap(now,partner))continue;
            const auto kind=parts[p].kind;Check(kind==Kind::Host||kind==Kind::WallBoard||kind==Kind::EntryFrame||kind==Kind::CornerStile,
                 "new lining has unrelated structural intersection");
            // AABB僅作保守partner排除，不冒充triangle交叉oracle；五薄襯/柱/頂板皆真箱。
            Check(PositiveOverlap(old,PartBounds(source,parts[p])),"new lining created a new overlap partner");++joins;
        }
    }
}
template<class Change>void Reject(const Mesh3D& mesh,const std::vector<Part>& parts,const Mesh3D& source,
                                 const std::vector<Part>& original,Change change,const char* text){
    auto bad=mesh;auto badParts=parts;change(bad,badParts);++negatives;Check(!Validate(bad,badParts,source,original),text);
}
template<class Change>void RejectSource(const Mesh3D& source,const std::vector<Part>& parts,Change change,const char* text){
    auto bad=source;auto badParts=parts;change(bad,badParts);const auto before=bad;const auto pBefore=badParts;bool threw=false;
    try{(void)Mistwatch::RangerHutLining::ApplyRecessedLinings(bad,badParts);}catch(const std::runtime_error&){threw=true;}
    ++negatives;Check(threw,text);Check(O::SameMesh(bad,before),"rejected input was mutated");
    for(std::size_t p=0;p<badParts.size();++p)Check(PartEqual(badParts[p],pBefore[p]),"rejected input parts mutated");
}
}

int main(){try{
    using namespace V1274;
    phase="unchanged-full-HutWhole-and-RoofBand";Check(V1274Original::main()==0,"unchanged full original suite failed");
    std::vector<Part> originalParts;const auto combined=Mistwatch::CreateRangerHut({},&originalParts);
    const auto source=Mistwatch::RangerRoofBand::CreateRangerHutRoofBand();const auto sourceBefore=source;const auto partsBefore=originalParts;
    const auto world=Mistwatch::WorldSolids(true),solids=Mistwatch::RangerHutSolids();const auto camera=Mistwatch::RangerHutCameraRoofSolid();
    phase="transaction-independent-expected-and-whole-inverse";
    const auto result=Mistwatch::RangerHutLining::ApplyRecessedLinings(source,originalParts);const auto& mesh=result.mesh;const auto& parts=result.parts;
    std::string detail;const bool valid=Validate(mesh,parts,source,originalParts,&detail);Check(valid,detail.c_str());
    // 舊五raw門只在還原身份後驗證；任何物理查詢仍用未還原的真candidate。
    std::vector<O::HutSourceBaseline::RangerHutPart> oldParts;
    const auto oldAsset=O::HutSourceBaseline::CreateRangerHut({},&oldParts);
    Check(!O::OriginalPartScopeExact(mesh,parts,oldAsset,oldParts),"new linings must not claim old five raw contract");
    auto restored=mesh;auto restoredParts=parts;
    for(const auto& slot:expected){
        for(std::size_t k=0;k<24;++k)Coordinate(restored.vertices[slot.base+k],slot.axis,Coordinate(source.vertices[slot.base+k],slot.axis));
        Coordinate(restoredParts[slot.part].centre,slot.axis,Coordinate(originalParts[slot.part].centre,slot.axis));
    }
    Check(O::OriginalPartScopeExact(restored,restoredParts,oldAsset,oldParts),"original source scope outside exact new ownership");
    Check(O::SameMesh(source,sourceBefore),"accepted input mutated");
    for(std::size_t p=0;p<originalParts.size();++p)Check(PartEqual(originalParts[p],partsBefore[p]),"accepted input Part mutated");
    Check(O::SameMesh(mesh,Mistwatch::RangerHutLining::CreateRangerHutLining()),"public factory repeat mismatch");
    Check(mesh.vertices.size()<=2u*3432u&&mesh.indices.size()/3u<=3416u,"original geometry cap");
    phase="candidate-original-2916camera-130seal-21openings";
    Check(V1274Physical::CandidateRegressionMain()==0,"original physical suite on true candidate failed");
    phase="candidate-original-dense-and-frame-planes";
    O::denseDiagnosticBaseline=&source;O::denseDiagnosticPrinted=false;
    const bool dense=O::DenseWalls(mesh)&&O::DenseOpenings(mesh);O::denseDiagnosticBaseline=nullptr;Check(dense,"original dense query failed");
    Check(O::CurrentFrameSamples(mesh,parts,true),"original frame first-hit/exterior plane");
    const Vec3 inheritedFrom{2.4749999f,.900000036f,-3.0999999f},inheritedTo{2.4749999f,.900000036f,-2.6500001f};
    const auto inheritedOld=O::FirstHit(inheritedFrom,inheritedTo,oldAsset),inheritedBefore=O::FirstHit(inheritedFrom,inheritedTo,source),
               inheritedNow=O::FirstHit(inheritedFrom,inheritedTo,mesh);
    const auto inheritedPart=Owner(inheritedNow,parts);
    Check(inheritedOld.t>1&&inheritedPart<parts.size()&&parts[inheritedPart].kind==Kind::CornerStile&&
          inheritedBefore.triangle==inheritedNow.triangle&&Bits(inheritedBefore.t,inheritedNow.t),"original inherited firstfalse/corner repair query");
    for(const auto& p:parts)if(p.kind==Kind::Jamb){const float side=p.centre.z>0?1.f:-1.f;
        const auto hit=O::FirstHit({p.centre.x,1.55f,side*3.1f},{p.centre.x,1.55f,side*2.65f},mesh);
        Check(Owner(hit,parts)==static_cast<std::size_t>(&p-parts.data()),"original foreground jamb first-hit");
    }
    for(unsigned c=0;c<4;++c){const bool west=c%2u==0;const float side=c<2?-1.f:1.f,x=west?-2.49f:2.49f;
        for(const float y:{.900000036f,1.55f,2.19999981f,1.f,2.1f}){
            const auto before=O::FirstHit({x,y,side*3.1f},{x,y,side*2.65f},source),after=O::FirstHit({x,y,side*3.1f},{x,y,side*2.65f},mesh);
            const auto p=Owner(after,parts);Check(p<parts.size()&&parts[p].kind==Kind::CornerStile&&before.triangle==after.triangle&&Bits(before.t,after.t),
                  "original full-height corner first-hit changed");
        }
    }
    phase="five-closed-boxes-48-visible-boards-and-backed-joints";
    for(const auto& slot:expected){Check(ClosedLining(mesh,parts,slot),"five directed closed boxes/containment");++closedBoxes;}
    BoardAndSeamWitnesses(source,mesh,parts);StructuralJoins(source,mesh,parts);
    for(const bool repaired:{false,true})for(const auto selector:{Mistwatch::FirLayoutSelector::Legacy,Mistwatch::FirLayoutSelector::ForestWood})
        Check(O::CurrentWorldExact(::FormalPropWorldSnapshot(repaired,selector),Mistwatch::WorldSolids(repaired,selector)),"independent full world snapshot");
    Check(O::CurrentWorldExact(world,Mistwatch::WorldSolids(true))&&O::CurrentWorldExact(solids,Mistwatch::RangerHutSolids()),"collider raw");
    const auto afterCamera=Mistwatch::RangerHutCameraRoofSolid();Check(Point(camera.center,afterCamera.center)&&Point(camera.extents,afterCamera.extents),"camera collider raw");
    phase="finite-negative-controls";
    ++negatives;Check(!Validate(source,originalParts,source,originalParts),"old occluding source accepted as recess");
    Reject(mesh,parts,source,originalParts,[](Mesh3D& m,auto&){m.vertices[24].x+=.001f;},"wrong fixed depth");
    Reject(mesh,parts,source,originalParts,[](Mesh3D& m,auto&){m.vertices[24].y+=.001f;},"owned Y wrong domain");
    Reject(mesh,parts,source,originalParts,[](Mesh3D& m,auto&){m.vertices[24].u+=.001f;},"owned UV wrong domain");
    Reject(mesh,parts,source,originalParts,[](Mesh3D& m,auto&){m.vertices[24].normal^=1u;},"owned normal wrong domain");
    Reject(mesh,parts,source,originalParts,[](Mesh3D& m,auto&){m.vertices[24].color^=1u;},"owned color wrong domain");
    Reject(mesh,parts,source,originalParts,[](Mesh3D& m,auto&){m.vertices[0].x+=.001f;},"protected XYZ");
    Reject(mesh,parts,source,originalParts,[](Mesh3D& m,auto&){m.vertices[0].u+=.001f;},"protected UV");
    Reject(mesh,parts,source,originalParts,[](Mesh3D&,auto& p){p[1].centre.z+=.001f;},"owned centre tangent wrong domain");
    Reject(mesh,parts,source,originalParts,[](Mesh3D&,auto& p){p.back()=p.front();},"duplicate part");
    {auto bad=mesh;bad.indices.erase(bad.indices.begin()+36,bad.indices.begin()+42);++negatives;
        Check(!Validate(bad,parts,source,originalParts)&&!ClosedLining(bad,parts,expected[0]),"open box end");}
    {auto bad=mesh;std::swap(bad.indices[37],bad.indices[38]);++negatives;
        Check(!O::CurrentTriangleQuality(bad)&&!ClosedLining(bad,parts,expected[0]),"reverse box face");}
    {auto bad=mesh;bad.indices[36]=static_cast<std::uint32_t>(bad.vertices.size());++negatives;Check(!O::SoundMesh(bad,true),"invalid index");}
    {auto bad=mesh;bad.vertices[24].x=std::numeric_limits<float>::quiet_NaN();++negatives;Check(!O::SoundMesh(bad,true),"nonfinite owned point");}
    {auto bad=mesh;bad.indices[37]=bad.indices[36];++negatives;Check(!O::CurrentTriangleQuality(bad),"degenerate lining face");}
    {auto bad=mesh;for(std::size_t i=24;i<48;++i)bad.vertices[i].x+=.4f;++negatives;Check(!ClosedLining(bad,parts,expected[0]),"backer through external wall");}
    {auto bad=mesh;bad.indices.erase(bad.indices.begin()+36,bad.indices.begin()+72);++negatives;Check(!O::DenseWalls(bad),"removed backer leaks original dense wall");}
    {auto bad=mesh;O::AddObstruction(bad,{-1.15f,1.55f,2.9f});++negatives;Check(!O::DenseOpenings(bad),"blocked window");}
    {auto bad=mesh;O::AddObstruction(bad,{-2.4f,1.3f,0});++negatives;Check(!O::DenseOpenings(bad),"blocked entry");}
    RejectSource(source,originalParts,[](Mesh3D& m,auto&){m.vertices[24].x+=.001f;},"source lining shape drift");
    RejectSource(source,originalParts,[](Mesh3D& m,auto&){m.vertices[0].u+=.001f;},"source protected UV drift");
    RejectSource(source,originalParts,[](Mesh3D&,auto& p){p[1].firstIndex=p[0].firstIndex;},"source range drift");
    RejectSource(source,originalParts,[](Mesh3D&,auto& p){p[1].centre.x+=.001f;},"source centre drift");
    RejectSource(source,originalParts,[](Mesh3D& m,auto&){m.vertices[24].x=std::numeric_limits<float>::quiet_NaN();},"source finite drift");
    RejectSource(combined,originalParts,[](Mesh3D&,auto&){},"pre-RoofBand source must fail closed");
    std::size_t changed{};for(std::size_t i=0;i<mesh.vertices.size();++i)changed+=!Bits(mesh.vertices[i].x,source.vertices[i].x)||!Bits(mesh.vertices[i].z,source.vertices[i].z);
    Check(changed==120&&closedBoxes==5,"fixed owned/closed coverage");
    std::cout<<"V1274_LINING_CPU_PASS checks="<<checks<<" negatives="<<negatives<<" changed_single_axis_records="<<changed
        <<" changed_centres=5 protected_records=6744 protected_parts=127 closed_boxes="<<closedBoxes
        <<" board_first_hits="<<boardRays<<" backed_joint_rays="<<seamRays<<" inherited_join_pairs="<<joins
        <<" vertices=6864 indices=9600 triangles=3200 parts=132 original_full_suite=1 candidate_camera_seal_opening=1 inverse=1"
        <<" native=false art=false game=false\n";return 0;
}catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE V1274 phase="<<V1274::phase<<" reason="<<e.what()<<'\n';return 1;}}
