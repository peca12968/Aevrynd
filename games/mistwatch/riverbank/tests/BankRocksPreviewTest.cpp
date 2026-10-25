#include "MistwatchBankRocks.h"
#include "MistwatchErodedBankConstruction.h"
#include "MistwatchBoulderDetail.h"
#include <Aevrynd/CharacterController3D.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
int main(int argc,char**argv){try{
    using namespace Aevrynd;
    if(argc!=6&&(argc!=7||std::string_view(argv[6])!="--rebuild-current-final-bank"))throw std::runtime_error("EXACT_FIELD_GUARDS_SOLIDS_MESH_AND_NEW_REPORT_REQUIRED");
    HeightField3D field({{-50,0,-20},201,241,.5f});std::ifstream input(argv[1]);std::string line;std::getline(input,line);
    HeightField3D original=field;
    if(line!="x\tz\tbefore\tafter")throw std::runtime_error("FIELD_SCHEMA");unsigned count{};
    while(std::getline(input,line)){std::istringstream row(line);float x,z,b,a;std::string extra;
        if(!(row>>x>>z>>b>>a)||(row>>extra)||count>=48441||x!=-50+float(count%201)*.5f||z!=-20+float(count/201)*.5f||!std::isfinite(a))throw std::runtime_error("FIELD_ROW");
        if(!field.SetHeight(count%201,count/201,a)||!original.SetHeight(count%201,count/201,b))throw std::runtime_error("FIELD_WRITE");++count;
    }
    if(count!=48441)throw std::runtime_error("INCOMPLETE_FIELD");field.RecalculateRange();
    std::vector<Mistwatch::RiverbankConstruction::Guard> guards;std::ifstream gf(argv[2]);std::getline(gf,line);
    if(line!="x\tz\tex\tez")throw std::runtime_error("GUARD_SCHEMA");
    while(std::getline(gf,line)){std::istringstream row(line);Mistwatch::RiverbankConstruction::Guard g;std::string extra;if(!(row>>g.x>>g.z>>g.ex>>g.ez)||(row>>extra))throw std::runtime_error("GUARD_ROW");guards.push_back(g);}
    if(argc==7){original.RecalculateRange();field=Mistwatch::ErodedBank::Build(original,field,guards).field;}
    static_assert(sizeof(BoxCollider3D)==24);std::vector<BoxCollider3D> old(47014);std::ifstream bf(argv[3],std::ios::binary);
    bf.read(reinterpret_cast<char*>(old.data()),std::streamsize(old.size()*sizeof(BoxCollider3D)));if(!bf||bf.peek()!=std::char_traits<char>::eof())throw std::runtime_error("EXACT_SOURCE_BOXES");
    const auto mesh=Mistwatch::BoulderDetail::LoadBoulderDetailMesh(argv[4]);
    bool strictRejected=false;try{static_cast<void>(Mistwatch::BankRocks::Build(field,mesh,guards,old));}catch(const std::runtime_error&e){strictRejected=std::string(e.what()).starts_with("BANK_ROCK_FLOATING_OR_INVISIBLE_SEATING:0");}
    if(!strictRejected)throw std::runtime_error("ORIGINAL_STRICT_REJECTION_NOT_PRESERVED");
    const auto model=Mistwatch::BankRocks::BuildRejectedPreview(field,mesh,guards,old);
    if(model.rejectedHeightParts!=4||model.measurements.size()!=8)throw std::runtime_error("KNOWN_PREVIEW_FAILURE_NOT_RECORDED");
    unsigned vertices{};for(const auto&p:model.parts){const auto matrix=p.pose.Matrix();
        for(const auto&v:mesh.vertices){const auto w=TransformPoint(matrix,{v.x,v.y,v.z});
            for(const auto pair:std::array<std::array<float,3>,3>{{{w.x,p.collider.center.x,p.collider.extents.x},{w.y,p.collider.center.y,p.collider.extents.y},{w.z,p.collider.center.z,p.collider.extents.z}}})
                if(double(pair[0])<double(pair[1])-pair[2]||double(pair[0])>double(pair[1])+pair[2])throw std::runtime_error("DISPLAY_VERTEX_OUTSIDE_CONSERVATIVE_COLLISION");
            ++vertices;
        }
    }
    const auto&box=model.additions.front();HeightField3D flat({{-50,0,-20},201,241,.5f});flat.Fill(box.center.y-box.extents.y);
    CharacterController3D body;body.Reset({box.center.x-box.extents.x-1,flat.SampleHeight(0,0),box.center.z});
    for(unsigned i=0;i<120;++i)if(!body.Step({{1,0,0},false,1},std::span<const BoxCollider3D>{&box,1},std::span<const ConvexAabbSweep3D* const>{},flat,1.f/60))throw std::runtime_error("PROXY_BODY_STEP");
    const auto blocked=body.Position();if(blocked.x>=box.center.x-box.extents.x)throw std::runtime_error("ROCK_PROXY_NOT_BLOCKING_BODY");
    for(unsigned i=0;i<20;++i)if(!body.Step({{-1,0,0},false,1},std::span<const BoxCollider3D>{&box,1},std::span<const ConvexAabbSweep3D* const>{},flat,1.f/60))throw std::runtime_error("PROXY_RETREAT_STEP");
    if(blocked.x-body.Position().x<.5f)throw std::runtime_error("PROXY_CONTACT_NOT_REVERSIBLE");
    if(std::string_view(argv[5])!="--no-report"){
    if(std::filesystem::exists(argv[5]))throw std::runtime_error("NO_REPORT_OVERWRITE");
    std::ofstream out(argv[5]);out.precision(9);out<<"x\ty\tz\trx\try\trz\tsx\tsy\tsz\tcx\tcy\tcz\tex\tey\tez\tmaximum_above\tmaximum_support_gap\tsupport_vertices\n";
    for(const auto&p:model.parts){const auto&t=p.pose;const auto&b=p.collider;out<<t.position.x<<'\t'<<t.position.y<<'\t'<<t.position.z<<'\t'<<t.rotation.x<<'\t'<<t.rotation.y<<'\t'<<t.rotation.z<<'\t'<<t.scale.x<<'\t'<<t.scale.y<<'\t'<<t.scale.z<<'\t'<<b.center.x<<'\t'<<b.center.y<<'\t'<<b.center.z<<'\t'<<b.extents.x<<'\t'<<b.extents.y<<'\t'<<b.extents.z<<'\t'<<p.maximumAbove<<'\t'<<p.maximumSupportGap<<'\t'<<p.supportVertices<<'\n';}
    out.close();if(!out)throw std::runtime_error("REPORT_WRITE");
    }
    std::cout<<"BANK_ROCKS_PREVIEW_CONTRACT_PASS_STRICT_GATE_STILL_REJECTED rocks="<<model.parts.size()<<" enclosed_display_vertices="<<vertices<<" conservative_proxy_body_and_retreat=pass flat_fixture_only=1\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
