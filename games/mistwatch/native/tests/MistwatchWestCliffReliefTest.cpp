#include "MistwatchWestCliffRelief.h"
#include "MistwatchLandscape.h"
#include <iostream>
#include <limits>

namespace {
namespace R=Mistwatch::WestCliffRelief;
unsigned checks{},negative{};
void Need(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
template<class F>void Reject(F operation){bool threw=false;try{operation();}catch(const std::invalid_argument&){threw=true;}Need(threw,"Invalid input accepted");++negative;}
bool SameVertex(const Aevrynd::Vertex3D& a,const Aevrynd::Vertex3D& b){
    return R::Bits(a.x,b.x)&&R::Bits(a.y,b.y)&&R::Bits(a.z,b.z)&&a.normal==b.normal&&a.color==b.color&&R::Bits(a.u,b.u)&&R::Bits(a.v,b.v);
}
void SameMesh(const Aevrynd::Mesh3D& a,const Aevrynd::Mesh3D& b){
    Need(a.indices==b.indices&&a.vertices.size()==b.vertices.size(),"Mesh structure drift");
    for(std::size_t i=0;i<a.vertices.size();++i)Need(SameVertex(a.vertices[i],b.vertices[i]),"Unchanged mesh drift");
}
}
int main(){try{
    const auto canonical=Mistwatch::CreateTerrain();
    const auto result=R::Build(canonical);const auto& terrain=result.terrain;
    Need(R::SameExact(canonical,Mistwatch::CreateTerrain()),"Input mutated");
    Need(R::SameExact(terrain,R::Build(canonical).terrain),"Relief not deterministic");
    Need(R::ValidateExact(terrain).derivedFingerprint==result.receipt.derivedFingerprint,"Qualification mismatch");
    Need(result.receipt.changedSamples>150&&result.receipt.changedNormals>150,"Relief too small to exercise");
    Need(result.receipt.maximumCut>1.2f&&result.receipt.maximumCut<=R::MaximumCut,"Relief amplitude mismatch");
    Need(result.receipt.protectedCellsVerified>150,"Protected triangle domain not exercised");
    Need(R::Bits(canonical.MinimumHeight(),terrain.MinimumHeight())&&R::Bits(canonical.MaximumHeight(),terrain.MaximumHeight()),"Range changed");
    std::size_t changed{},normalChanged{};
    for(unsigned z=0;z<241;++z)for(unsigned x=0;x<201;++x){
        const auto p=canonical.SamplePosition(x,z);const float y=terrain.Height(x,z);
        Need(std::isfinite(y)&&y<=p.y&&p.y-y<=R::MaximumCut,"Cut invalid");
        const bool different=!R::Bits(y,p.y);changed+=different;
        const auto before=Aevrynd::PackNormalSnorm8(canonical.SampleNormal(p.x,p.z));
        const auto after=Aevrynd::PackNormalSnorm8(terrain.SampleNormal(p.x,p.z));normalChanged+=before!=after;
        if(p.x<=-49||p.x>=-41||p.z<=51||p.z>=65)Need(!different,"Outside support height changed");
        if(p.x<=R::GuardMinX||p.x>=R::GuardMaxX||p.z<=R::GuardMinZ||p.z>=R::GuardMaxZ)
            Need(before==after,"Normal escaped guard");
        if(x<=1||x>=199||z<=1||z>=239)Need(!different&&before==after,"Seam or seam neighbor changed");
    }
    Need(changed==result.receipt.changedSamples&&normalChanged==result.receipt.changedNormals,"Receipt counts mismatch");
    const auto ringBefore=Mistwatch::CreateLandscapeMesh(canonical,true,Mistwatch::ForestFloorMaskSelector::ForestWood,Mistwatch::BankCoverageSelector::Patchy);
    SameMesh(ringBefore,Mistwatch::CreateLandscapeMesh(terrain,true,Mistwatch::ForestFloorMaskSelector::ForestWood,Mistwatch::BankCoverageSelector::Patchy));
    const auto treesBefore=Mistwatch::LandscapeTrees(canonical),treesAfter=Mistwatch::LandscapeTrees(terrain);
    Need(treesBefore.size()==520&&treesBefore.size()==treesAfter.size(),"Landscape tree population changed");
    for(std::size_t i=0;i<treesBefore.size();++i){const auto& a=treesBefore[i];const auto& b=treesAfter[i];
        Need(R::Bits(a.position.x,b.position.x)&&R::Bits(a.position.y,b.position.y)&&R::Bits(a.position.z,b.position.z)&&
            R::Bits(a.rotation.y,b.rotation.y)&&R::Bits(a.scale.x,b.scale.x),"Landscape tree pose changed");}
    auto mesh=Mistwatch::CreateTerrainMesh(canonical,true,Mistwatch::ForestFloorMaskSelector::ForestWood,Mistwatch::BankCoverageSelector::Patchy);
    // 保留任意合法 authored UV 與 RGB，不把新地形誤當重新授權整個材質配方。
    for(std::size_t i=0;i<mesh.vertices.size();++i){auto& v=mesh.vertices[i];v.u=float(i%7)*.05f;v.v=.4f;v.color=(v.color&0xff000000u)|0x00563412u;}
    const auto original=mesh;const auto meshReceipt=R::UpdateMesh(mesh,canonical,terrain);
    Need(mesh.indices==original.indices&&mesh.vertices.size()==original.vertices.size(),"Topology changed");
    Need(meshReceipt.changedPositions==changed&&meshReceipt.changedNormals==normalChanged,"Mesh receipt mismatch");
    Need(meshReceipt.changedColors>0,"Height alpha not exercised");
    for(std::size_t i=0;i<mesh.vertices.size();++i){const auto& a=original.vertices[i];const auto& b=mesh.vertices[i];
        Need(R::Bits(a.x,b.x)&&R::Bits(a.z,b.z)&&R::Bits(a.u,b.u)&&R::Bits(a.v,b.v)&&(a.color&0xffffffu)==(b.color&0xffffffu),"Authored payload changed");
        const unsigned gx=unsigned((b.x+50.f)*2.f),gz=unsigned((b.z+20.f)*2.f);
        Need(R::Bits(b.y,terrain.Height(gx,gz))&&b.normal==Aevrynd::PackNormalSnorm8(terrain.SampleNormal(b.x,b.z)),"Actual stored field/mesh mismatch");
        if(R::Bits(a.y,b.y)&&a.normal==b.normal)Need(SameVertex(a,b),"Untouched vertex changed");
    }
    const auto zeroIndex=std::size_t(117)*201+67;
    Need(std::bit_cast<std::uint32_t>(original.vertices[zeroIndex].y)==0x80000000u,"Signed-zero regression fixture changed");
    Need(SameVertex(original.vertices[zeroIndex],mesh.vertices[zeroIndex]),"Stored signed zero was rewritten");
    // 逐格走兩個真三角面，以非對角、非中心採樣確認與實際碰撞插值一致。
    std::size_t triangleSamples{};
    for(unsigned z=140;z<172;++z)for(unsigned x=0;x<20;++x){
        const auto& a=mesh.vertices[z*201+x];const auto& b=mesh.vertices[z*201+x+1];
        const auto& c=mesh.vertices[(z+1)*201+x];const auto& d=mesh.vertices[(z+1)*201+x+1];
        for(const auto uv:{Aevrynd::Vec2{.17f,.29f},Aevrynd::Vec2{.83f,.61f},Aevrynd::Vec2{.25f,.75f}}){
            const float worldX=a.x+uv.x*.5f,worldZ=a.z+uv.y*.5f;
            // 使用真正浮點世界座標反解 barycentric，避免常數小數量化造成假差異。
            const float tx=(worldX-a.x)/.5f,tz=(worldZ-a.z)/.5f;
            const float y=tx+tz<=1?a.y+(b.y-a.y)*tx+(c.y-a.y)*tz:d.y+(c.y-d.y)*(1-tx)+(b.y-d.y)*(1-tz);
            Need(std::abs(y-terrain.SampleHeight(worldX,worldZ))<.00001f,"Triangle/collision interpolation mismatch");++triangleSamples;
        }
    }
    auto subset=original;subset.indices={0,201,1};subset.vertices.resize(202);
    (void)R::UpdateMesh(subset,canonical,terrain);Need(subset.indices==std::vector<std::uint32_t>({0,201,1}),"Subset indices drift");
    auto wrong=canonical;wrong.SetHeight(12,152,wrong.Height(12,152)-.1f);wrong.RecalculateRange();
    Reject([&]{(void)R::Build(wrong);});
    Reject([&]{(void)R::ValidateExact(canonical);});
    wrong=terrain;wrong.SetHeight(12,152,wrong.Height(12,152)-.01f);wrong.RecalculateRange();Reject([&]{(void)R::ValidateExact(wrong);});
    wrong=terrain;wrong.SetHeight(100,100,wrong.Height(100,100)-.01f);wrong.RecalculateRange();Reject([&]{(void)R::ValidateExact(wrong);});
    wrong=terrain;wrong.SetHeight(0,150,wrong.Height(0,150)-.01f);wrong.RecalculateRange();Reject([&]{(void)R::ValidateExact(wrong);});
    wrong=terrain;wrong.SetHeight(12,152,std::numeric_limits<float>::quiet_NaN());Reject([&]{(void)R::ValidateExact(wrong);});
    Reject([&]{(void)R::Build(Aevrynd::HeightField3D{});});
    Reject([&]{(void)R::ValidateExact(Aevrynd::HeightField3D{});});
    Reject([&]{(void)R::Cut(std::numeric_limits<float>::infinity(),55);});
    Reject([&]{R::ValidateProtectedRectangle(-49,-41,49.5f,55);});
    Reject([&]{R::ValidateProtectedRectangle(-3,3,51,65);});
    Reject([&]{R::ValidateProtectedRectangle(-23,-21,61,63);});
    auto invalid=original;invalid.vertices.back().y+=.1f;const auto retained=invalid;
    Reject([&]{(void)R::UpdateMesh(invalid,canonical,terrain);});SameMesh(invalid,retained);
    invalid=original;invalid.vertices.front().normal=0;Reject([&]{(void)R::UpdateMesh(invalid,canonical,terrain);});
    invalid=original;invalid.indices.back()=999999;Reject([&]{(void)R::UpdateMesh(invalid,canonical,terrain);});
    invalid=original;invalid.indices[1]=invalid.indices[0];Reject([&]{(void)R::UpdateMesh(invalid,canonical,terrain);});
    invalid=original;invalid.indices[1]=202;Reject([&]{(void)R::UpdateMesh(invalid,canonical,terrain);});
    invalid=original;Reject([&]{(void)R::UpdateMesh(invalid,canonical,canonical);});SameMesh(invalid,original);
    invalid=original;invalid.vertices.front().u=std::numeric_limits<float>::quiet_NaN();Reject([&]{(void)R::UpdateMesh(invalid,canonical,terrain);});
    std::cout<<"WEST_CLIFF_RELIEF_PASS checks="<<checks<<" negative="<<negative<<" changed="<<changed<<" normals="<<normalChanged
        <<" maximumCut="<<result.receipt.maximumCut<<" protectedCells="<<result.receipt.protectedCellsVerified<<" triangleSamples="<<triangleSamples<<" canonical="<<result.receipt.canonicalFingerprint
        <<" derived="<<result.receipt.derivedFingerprint<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<"WEST_CLIFF_RELIEF_FAIL "<<e.what()<<'\n';return 1;}}
