#include "MistwatchPlayerLocomotion.h"
#include "MistwatchObjectiveGuide.h"
#include <Aevrynd/StaticAabbIndex3D.h>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
using namespace Aevrynd;
namespace {
unsigned checks{},frames{},changedFrames{};std::ofstream trace;
void Need(bool v,const char* reason){++checks;if(!v)throw std::runtime_error(reason);}
struct Source {
    HeightField3D field{{{-50,0,-20},201,241,.5f}},old{{{-50,0,-20},201,241,.5f}};
    std::vector<BoxCollider3D> boxes;std::unique_ptr<StaticAabbIndex3D> index;
    std::vector<std::uint32_t> scratch=std::vector<std::uint32_t>(10000),ids=scratch;
    std::vector<BoxCollider3D> local;
    Source(const char* heights,const char* solids){
        std::ifstream in(heights);std::string line;std::getline(in,line);unsigned n{};
        while(std::getline(in,line)){std::istringstream row(line);float x,z,a,b;Need(bool(row>>x>>z>>a>>b),"actual complete terrain row");
            const unsigned ix=unsigned((x+50)*2),iz=unsigned((z+20)*2);Need(field.SetHeight(ix,iz,b)&&old.SetHeight(ix,iz,a),"actual terrain coordinate");++n;}
        Need(n==201*241,"complete 48441 height samples");field.RecalculateRange();old.RecalculateRange();
        boxes.resize(47014);std::ifstream raw(solids,std::ios::binary);raw.read(reinterpret_cast<char*>(boxes.data()),std::streamsize(boxes.size()*sizeof(BoxCollider3D)));
        Need(bool(raw)&&raw.peek()==std::char_traits<char>::eof(),"complete unchanged static world");
        std::vector<ClosedAabb3D> bounds;for(const auto& b:boxes)bounds.push_back({{double(b.center.x)-b.extents.x,double(b.center.y)-b.extents.y,double(b.center.z)-b.extents.z},{double(b.center.x)+b.extents.x,double(b.center.y)+b.extents.y,double(b.center.z)+b.extents.z}});
        index=std::make_unique<StaticAabbIndex3D>(bounds);Need(index->Valid()&&index->Size()==47014,"original index cap unchanged");
    }
    std::span<const BoxCollider3D> Nearby(Vec3 p){
        std::size_t count{};Need(index->QueryOrdinals({{double(p.x)-8,-1000,double(p.z)-8},{double(p.x)+8,1000,double(p.z)+8}},scratch,ids,count),"no truncation local query");
        local.clear();for(std::size_t i=0;i<count;++i)local.push_back(boxes.at(ids[i]));return local;
    }
};
bool Same(Vec3 a,Vec3 b){return std::memcmp(&a,&b,sizeof(Vec3))==0;}
void Scenario(Source& source,const char* name,Vec3 start,unsigned kind){
    CharacterControllerSettings3D settings;settings.jumpGraceSeconds=.1f;settings.maximumTerrainUphillGrade=settings.maximumTerrainStandingGrade=1.f;
    CharacterController3D controller(settings);start.y=source.field.SampleHeight(start.x,start.z);controller.Reset(start);
    auto initial=source.Nearby(start);Need(controller.Step({},initial,{},source.field,1.f/60.f)&&controller.Grounded(),"actual initial bank grounding");
    Mistwatch::Explorer explorer;const float heading=kind==0?-1.57079632679f:kind==2?3.14159265359f:0.f;
    explorer.yaw=explorer.requestedYaw=heading;explorer.Update({},1.f/60.f,false,heading);
    Need(explorer.PlaceFeet(controller.Position(),true,false,source.field,initial,{}),"actual initial complete explorer feet");
    Mistwatch::PlayerLocomotion owner;Mistwatch::Evasion evasion;
    float peakJump{},peakSpeed{};unsigned airborne{},crouched{},altered{};
    for(unsigned tick=0;tick<180;++tick){
        Mistwatch::PlayerLocomotionInput input;
        if(kind==0&&tick<120){input.movement={-1,0,0};input.running=tick>=60;}
        if(kind==1)input.jumpPressed=tick==15;
        if(kind==2&&tick<60){input.movement={0,0,-1};input.crouched=true;}
        const auto local=source.Nearby(controller.Position());
        auto reference=controller;auto referenceOwner=owner;auto referencePose=explorer;auto referenceEvasion=evasion;
        const auto actual=owner.Advance(controller,evasion,explorer,input,1.f/60.f,local,{},source.field,[]{return true;});
        const auto expected=referenceOwner.Advance(reference,referenceEvasion,referencePose,input,1.f/60.f,source.boxes,{},source.field,[]{return true;});
        if(!actual.accepted||!expected.accepted){std::cerr<<"FIRST_POSE_DIVERGENCE scenario="<<name<<" tick="<<tick<<" actual_line="<<actual.poseFailureLine<<" full_line="<<expected.poseFailureLine<<" reason="<<unsigned(actual.footPlacementDiagnostic.reason)<<" substep="<<actual.failedSubstep<<" x="<<actual.failedPosition.x<<" y="<<actual.failedPosition.y<<" z="<<actual.failedPosition.z<<'\n';}
        Need(actual.accepted&&expected.accepted,"production owner and actual foot pose must accept");
        Need(actual.steps==2&&expected.steps==2&&controller.StateHash()==reference.StateHash()&&owner.StateHash()==referenceOwner.StateHash(),"local/full complete motion hash match");
        Need(explorer.pose.globalMatrices.size()==referencePose.pose.globalMatrices.size()&&std::memcmp(explorer.pose.globalMatrices.data(),referencePose.pose.globalMatrices.data(),explorer.pose.globalMatrices.size()*sizeof(Matrix4))==0&&explorer.planted==referencePose.planted,"actual full displayed pose match");
        const auto p=controller.Position(),v=controller.Velocity();const float ground=source.field.SampleHeight(p.x,p.z);
        Need(Mistwatch::Session::Finite(p)&&Mistwatch::Session::Finite(v)&&p.y>=ground-.0001f,"finite actor not below actual terrain");
        if(controller.Grounded())Need(std::abs(p.y-ground)<.0001f,"grounded bank contact equals real heightfield");else ++airborne;
        for(unsigned side=0;side<2;++side)Need(Same(explorer.footTargets[side],referencePose.footTargets[side])&&explorer.footSwings[side].SpeedBound()<=60,"unchanged full foot target and swing bound");
        const bool changed=std::bit_cast<unsigned>(ground)!=std::bit_cast<unsigned>(source.old.SampleHeight(p.x,p.z));altered+=unsigned(changed);crouched+=unsigned(controller.Settings().height<1.5f);
        peakJump=(std::max)(peakJump,p.y-ground);peakSpeed=(std::max)(peakSpeed,std::hypot(v.x,v.z));++frames;changedFrames+=unsigned(changed);
        trace<<name<<'\t'<<tick<<'\t'<<p.x<<'\t'<<p.y<<'\t'<<p.z<<'\t'<<ground<<'\t'<<controller.Grounded()<<'\t'<<controller.Settings().height<<'\t'<<changed<<'\t'<<local.size()<<"\t1\n";
    }
    const auto finish=controller.Position();
    Need(altered>=20,"scenario truly exercises changed bank");
    if(kind==0)Need(start.x-finish.x>5&&peakSpeed>5&&controller.Grounded(),"walk run stop makes real bank progress");
    if(kind==1)Need(peakJump>1&&airborne>20&&controller.Grounded(),"real jump and landing on changed bank");
    if(kind==2)Need(crouched==60&&controller.Settings().height==1.8f&&start.z-finish.z>.5f&&controller.Grounded(),"real crouch walk stand restoration");
    std::cout<<"BANK_SCENARIO_PASS name="<<name<<" frames=180 altered="<<altered<<" peak_jump="<<peakJump<<" peak_speed="<<peakSpeed<<" finish="<<finish.x<<','<<finish.y<<','<<finish.z<<'\n';
}

// 每幀依正式展示指引移動，實際通過牆體碰撞、東側入口與蹲姿拾取。
void VerifyShelterNavigation(Source& source) {
    using namespace Mistwatch;
    const auto& roof=CameraCeilingSolids().back();
    const float eastEdge=roof.center.x+roof.extents.x;
    Camera3D camera;camera.position={0,3,-6};camera.target={0,1,0};
    const Encounter enemy;
    const Evasion evasion;
    CharacterControllerSettings3D settings;
    settings.maximumTerrainUphillGrade=settings.maximumTerrainStandingGrade=1.f;
    constexpr float deltaTime=1.f/60.f;
    for(float zOffset:{0.f,-3.f,3.f}){
        Session session;session.collected=7;session.repairMaterials=6;
        Vec3 feet{roof.center.x-roof.extents.x-.7f,0,roof.center.z+zOffset};
        feet.y=source.field.SampleHeight(feet.x,feet.z);
        CharacterController3D controller(settings);controller.Reset(feet);
        bool enteredEast=false;
        unsigned steps=0;
        for(;steps<480;++steps){
            session.feet=controller.Position();
            const auto local=source.Nearby(session.feet);
            const bool crouched=controller.Settings().height<1.5f;
            const auto guide=BuildObjectiveGuide(session,enemy,evasion,camera,crouched,controller.Grounded(),
                local,source.field,NoTrackedRelay,settings.radius);
            Need(guide.valid&&guide.index==0&&session.repairMaterials==6,"shelter guidance retains the tool interaction");
            if(guide.useReady){
                Need(enteredEast&&crouched&&controller.Grounded()&&session.CollectRepairMaterial(0,true),
                    "engine movement permits the original crouched tool pickup");
                break;
            }
            if(!guide.shelterApproach&&!enteredEast){
                Need(session.feet.x>eastEdge,"shelter route offers entry only from the open east side");
                Need(controller.TrySetHeight(1.4f,local),"crouch at the east opening");
                enteredEast=true;
            }
            const float dx=guide.target.x-session.feet.x,dz=guide.target.z-session.feet.z;
            const float remaining=std::hypot(dx,dz);
            Need(std::isfinite(remaining)&&remaining>.001f,"shelter route supplies a usable direction");
            Need(controller.Step({{dx/remaining,0,dz/remaining},false,.5f},local,
                std::span<const ConvexAabbSweep3D* const>{},source.field,deltaTime),"shelter movement step on actual terrain");
        }
        if(session.repairMaterials!=7){
            const auto p=controller.Position();
            std::cerr<<"SHELTER_GUIDANCE_FIRST_DIVERGENCE start_z_offset="<<zOffset
                <<" steps="<<steps<<" feet="<<p.x<<','<<p.y<<','<<p.z<<" entered_east="<<enteredEast<<'\n';
        }
        Need(session.repairMaterials==7,"guided path reaches the crouched tool pickup before timeout");
        std::cout<<"SHELTER_GUIDED_TRAVERSAL_PASS start_z_offset="<<zOffset<<" steps="<<steps<<'\n';
    }
}
}
int main(int argc,char**argv){
    try{
        Need(argc==4,"explicit actual field source and trace");Source source(argv[1],argv[2]);trace.open(argv[3]);trace.precision(9);trace<<"scenario\ttick\tx\ty\tz\tground\tgrounded\theight\tchanged_terrain\tlocal_count\tfull_match\n";
        Scenario(source,"east-south-walk-run-stop",{16,0,30},0);
        Scenario(source,"east-north-jump-land",{13,0,43},1);
        Scenario(source,"west-south-crouch-walk-stand",{-13,0,29},2);
        VerifyShelterNavigation(source);
        trace.close();Need(bool(trace),"trace fully written");std::cout<<"RIVERBANK_PRODUCTION_LOCOMOTION_PASS scenarios=3 frames="<<frames<<" changed_frames="<<changedFrames<<" checks="<<checks<<"\n";
    }catch(const std::exception& e){std::cerr<<"FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}
}
