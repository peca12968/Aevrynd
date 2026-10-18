#include "MistwatchBoulderDetail.h"
#include "MistwatchWorld.h"
#include "MistwatchEntranceWorldAuthor.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
using namespace Aevrynd;
namespace D=Mistwatch::BoulderDetail;
static std::vector<std::byte> Read(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Source read failed");
    const std::vector<char> chars{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
    std::vector<std::byte> out(chars.size());std::memcpy(out.data(),chars.data(),chars.size());return out;
}
int main()try {
    const auto need=[](bool good,const char* why){if(!good)throw std::runtime_error(why);};
    const auto root=std::filesystem::path(MISTWATCH_SOURCE_ROOT)/"engine/assets/realistic-cc0-first-batch/cooked/meshes/boulder-01";
    const auto source=root/"boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh";
    const auto raw=Read(source);auto original=LoadAevryndMeshFile(source);
    need(bool(original)&&raw.size()==3071924&&original.mesh.vertices.size()==81373&&original.mesh.indices.size()==198366,"Independent LOD0 identity");
    need(D::ExactRawMesh(original.mesh,raw),"Exact raw source accepted");
    const auto rooted=D::LoadBoulderDetailMesh(source);
    need(rooted.indices==original.mesh.indices,"Rooting preserves all indices");
    for(std::size_t i=0;i<rooted.vertices.size();++i){const auto& a=original.mesh.vertices[i];const auto& b=rooted.vertices[i];
        need(a.normal==b.normal&&a.color==b.color&&std::bit_cast<std::uint32_t>(a.u)==std::bit_cast<std::uint32_t>(b.u)&&
             std::bit_cast<std::uint32_t>(a.v)==std::bit_cast<std::uint32_t>(b.v),"All packed normals, color and UV preserved");
        const float x=(a.x-(-.11363622546f))*.54416590434f,y=(a.y-(-.07774113119f))*.54416590434f,z=(a.z-.02950257063f)*.54416590434f;
        need(b.x==x&&b.y==y&&b.z==z,"Independent original root and scale retained");
    }
    const auto bounds=rooted.Bounds();const auto lo=bounds.center-bounds.extents,hi=bounds.center+bounds.extents;
    need(std::abs(lo.x-(-.3452860713f))<2.e-7f&&std::abs(lo.y-.00223008008f)<2.e-7f&&std::abs(lo.z-(-.4959508181f))<2.e-7f&&
         std::abs(hi.x-.3469667733f)<2.e-7f&&std::abs(hi.y-.5484802723f)<2.e-7f&&std::abs(hi.z-.5000547171f)<2.e-7f,"Independent LOD0 bounds");
    auto old=LoadAevryndMeshFile(root/Mistwatch::BoulderMeshName);need(bool(old),"Real LOD2 loads");
    need(old.mesh.vertices.size()==56938&&old.mesh.indices.size()==79833,"Old identity unchanged");
    Mistwatch::RootBoulderMesh(old.mesh);const auto oldBounds=old.mesh.Bounds();
    need(std::abs(oldBounds.center.x)<1.e-7f&&std::abs(oldBounds.center.z)<1.e-7f&&std::abs(oldBounds.extents.z-.5f)<1.e-7f,"Old LOD2 gate remains exact");
    const float protrusion=D::OriginalEnvelopeProtrusion(rooted);
    need(protrusion>0.f&&std::abs(protrusion-.0000547170639f)<1.e-7f,"LOD0 tiny protrusion is reported, not hidden as strict-fit");
    // 舊著地與碰撞不改：最大既有 Z 比例為 2.7*1.3，外凸仍須交由主代理獨立判定。
    need(protrusion*3.51f<.000193f,"Worst authored scale protrusion quantified below 0.193mm");
    const auto terrain=Mistwatch::CreateTerrain();
    const auto firs=Mistwatch::CreateMatureFirLayout(terrain,Mistwatch::FirLayoutSelector::ForestWood);
    const auto props=Mistwatch::ForestProps(terrain,firs.roots);
    unsigned rocks{},bottomChecks{},withdrawn{};float worldProtrusion{},maximumFloat=-100.f,maximumBurial{};
    // 入口作者只移除三個岩石角色，其餘原位保留；直接驗證每個保留實體的原 collider。
    for(std::size_t id=0;id<props.size();++id){const auto& p=props[id];if(p.FallenLog())continue;
        if(Mistwatch::EntranceWorldDetail::Withdraw(Mistwatch::EntranceRole::Rock,static_cast<unsigned>(id))){++withdrawn;continue;}
        ++rocks;const auto matrix=p.transform.Matrix();
        for(const auto& v:rooted.vertices){const auto world=TransformPoint(matrix,{v.x,v.y,v.z});const auto delta=world-p.collider.center;
            const float excess=(std::max)({0.f,std::abs(delta.x)-p.collider.extents.x,
                std::abs(delta.y)-p.collider.extents.y,std::abs(delta.z)-p.collider.extents.z});
            worldProtrusion=(std::max)(worldProtrusion,excess);
            need(excess<=.001f,"Actual authored LOD0 vertices obey unchanged 1mm collider tolerance");
            if(v.y<=.035f){const float gap=world.y-terrain.SampleHeight(world.x,world.z);
                maximumFloat=(std::max)(maximumFloat,gap);maximumBurial=(std::max)(maximumBurial,-gap);++bottomChecks;
                need(gap<=.05f&&gap>=-.25f,"Actual authored LOD0 bottom obeys unchanged grounding limits");}
        }
    }
    need(rocks==29&&withdrawn==3&&bottomChecks>0,"Current retained entrance rock cohort and bottom coverage");
    auto bad=raw;bad[16]^=std::byte{1};need(!D::ExactRawMesh(original.mesh,bad),"Modified source hash rejected");
    bad=raw;bad.pop_back();need(!D::ExactRawMesh(original.mesh,bad),"Truncated source rejected");
    auto altered=original.mesh;altered.vertices[0].u+=.01f;need(!D::ExactRawMesh(altered,raw),"UV corruption rejected");
    altered=original.mesh;altered.vertices[0].normal^=1u;need(!D::ExactRawMesh(altered,raw),"Normal corruption rejected");
    altered=rooted;altered.vertices[0].z=1.f;need(!D::ExactRootedBounds(altered),"Out-of-envelope geometry rejected");
    altered=rooted;altered.vertices[0].x=std::numeric_limits<float>::quiet_NaN();need(!D::ExactRootedBounds(altered),"Nonfinite geometry rejected");
    const auto rejects=[&](const std::filesystem::path& path){try{const auto mesh=D::LoadBoulderDetailMesh(path);(void)mesh;return false;}catch(const std::exception&){return true;}};
    need(rejects(root/"missing-boulder-detail-invalid.aemesh"),"Missing file rejected");
    need(rejects(root/Mistwatch::BoulderMeshName),"Wrong LOD file rejected");
    // 不寫入真來源；同一路徑唯讀共享鎖存活時，不得取得可寫入的控制柄。
    const HANDLE writer=CreateFileW(source.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    need(writer!=INVALID_HANDLE_VALUE,"Independent source read lock opens");
    const HANDLE denied=CreateFileW(source.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    const bool blocked=denied==INVALID_HANDLE_VALUE;if(denied!=INVALID_HANDLE_VALUE)CloseHandle(denied);CloseHandle(writer);
    need(blocked,"Shared source lock denies writes");
    std::cout<<"BOULDER_EXPECTED vertices=81373\nBOULDER_EXPECTED indices=198366\nBOULDER_EXPECTED triangles=66122\n"
        <<"BOULDER_EXPECTED preserved_vertex_attributes=81373\nBOULDER_EXPECTED baseline_triangles=26611\n"
        <<std::setprecision(10)<<"BOULDER_EXPECTED local_envelope_protrusion="<<protrusion<<'\n';
    std::cout<<"BOULDER_WORLD rocks="<<rocks<<" bottom_checks="<<bottomChecks<<" maximum_protrusion="<<worldProtrusion
        <<" maximum_float="<<maximumFloat<<" maximum_burial="<<maximumBurial<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
