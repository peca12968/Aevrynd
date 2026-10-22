#include "MistwatchBearingBridge.h"
#include <Aevrynd/CharacterController3D.h>
#include <fstream>
#include <iostream>
#include <string>
#include <array>

using namespace Aevrynd;
namespace {
unsigned checks{};
void Require(bool b,const char* label){++checks;if(!b)throw std::runtime_error(label);}
struct SceneProbe {
    unsigned calls{};
    bool MaterialBox(Vec3 center,Vec3 size,Material3D material){
        Require(std::isfinite(center.y)&&size.x>0&&size.y>0&&size.z>0&&material.castsShadow,"ACTUAL_BOX_SUBMISSION_INPUT");
        ++calls;return true;
    }
};
}
int main(int argc,char** argv){
    try{
        Require(argc==2,"ACTUAL_CURRENT_HEIGHTFIELD_ARGUMENT");
        HeightField3D field({{-50,0,-20},201,241,.5f});
        std::ifstream input(argv[1]);std::string header;std::getline(input,header);
        Require(header=="x\tz\tbefore\tafter","ACTUAL_RIVERBANK_SCHEMA");
        float x{},z{},before{},after{};unsigned rows{};
        while(input>>x>>z>>before>>after){
            Require(rows<48441&&x==-50+float(rows%201)*.5f&&z==-20+float(rows/201)*.5f,"ALL_ORDERED_REAL_HEIGHT_SAMPLES");
            Require(field.SetHeight(rows%201,rows/201,after),"HEIGHT_SET");++rows;
        }
        Require(input.eof()&&rows==48441,"COMPLETE_ACTUAL_TERRAIN");field.RecalculateRange();
        const auto model=Mistwatch::BearingBridge::Build(field);
        Require(model.parts.size()==57&&model.additions.size()==25,"COMPLETE_FINITE_PARTS_AND_COLLISIONS");
        std::array<unsigned,7> roles{};std::size_t box{};
        for(const auto& p:model.parts){
            ++roles[unsigned(p.role)];
            if(p.role==Mistwatch::BearingBridge::Role::Plank){
                Require(p.center.y+p.size.y*.5f==0&&p.size.x==4,"UNCHANGED_WALKING_SURFACE");continue;
            }
            const auto& b=model.additions.at(box++);
            Require(b.center.x==p.center.x&&b.center.y==p.center.y&&b.center.z==p.center.z&&b.extents.x*2==p.size.x&&b.extents.y*2==p.size.y&&b.extents.z*2==p.size.z,"DISPLAYED_FINITE_AXIS_BOX_HAS_EXACT_COLLISION");
        }
        Require(roles==std::array<unsigned,7>{32,3,2,8,4,4,4},"REAL_BEARING_COMPONENT_COUNTS");
        for(const auto& b:model.beds)Require(b[0]<=b[1]&&b[2]>b[1]&&b[2]<-1.62f,"BED_FOUNDATION_BEARING_HEIGHTS");
        SceneProbe scene;Mistwatch::BearingBridge::Submit(scene,model,{},{});Require(scene.calls==57,"ALL_REAL_FINITE_PARTS_SUBMITTED");
        std::vector<BoxCollider3D> solids{{{0,-.1f,36},{2,.1f,4}}};
        solids.insert(solids.end(),model.additions.begin(),model.additions.end());
        CharacterController3D controller;controller.Reset({0,0,36});
        Require(controller.Step({},solids,std::span<const ConvexAabbSweep3D* const>{},field,1.f/60)&&controller.Grounded(),"BRIDGE_CENTER_REAL_SUPPORT");
        for(unsigned i=0;i<120;++i)Require(controller.Step({{1,0,0},false,1},solids,std::span<const ConvexAabbSweep3D* const>{},field,1.f/60),"REAL_RAIL_APPROACH_STEP");
        const float blocked=controller.Position().x;
        Require(blocked>1.25f&&blocked<=1.40f&&controller.Grounded()&&std::abs(controller.Position().y)<.0001f,"RAILING_BLOCKS_NOT_VISUAL_ONLY");
        for(unsigned i=0;i<20;++i)Require(controller.Step({{-1,0,0},false,1},solids,std::span<const ConvexAabbSweep3D* const>{},field,1.f/60),"REAL_RAIL_RETREAT_STEP");
        Require(blocked-controller.Position().x>1.f&&controller.Grounded(),"RAIL_CONTACT_REVERSIBLE");
        HeightField3D high({{-50,0,-20},201,241,.5f});high.Fill(0);bool rejected=false;
        try{(void)Mistwatch::BearingBridge::Build(high);}catch(const std::runtime_error&){rejected=true;}
        Require(rejected,"INVALID_FLOATING_FOUNDATION_FAIL_CLOSED");
        std::cout<<"FINITE_BEARING_BRIDGE_PASS parts=57 new_boxes=25 samples="<<rows<<" rail_blocked_x="<<blocked<<" checks="<<checks<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"BRIDGE_CONSTRUCTION_FIRST_DIVERGENCE="<<e.what()<<"\n";return 1;}
}
