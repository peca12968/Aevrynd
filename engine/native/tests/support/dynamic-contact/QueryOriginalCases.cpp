#include "CharacterDynamicContact3D.h"
#include "MistwatchEncounter.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>

using namespace Aevrynd;
namespace {
unsigned checks{},sampledPaths{},combatDirections{};
void Require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
constexpr Vec3 box{.45f,.9f,.45f};
constexpr DynamicCylinder3D body{{0,.5f,0},1.3f,.8f};
BoxCylinderContact3D Query(Vec3 p,Vec3 d){BoxCylinderContact3D r;Require(QueryBoxCylinderContact3D(p,box,d,body,r),"valid query rejected");return r;}
void Core(){
    const auto q=Query({-3,.9f,0},{6,0,0});
    Require(q.kind==BoxCylinderContactKind3D::SideEntry,"axis entry kind");
    Require(std::abs(q.first-(3.-double(box.x)-body.radius)/6.)<1e-14,"axis TOI");
    Require(q.horizontalNormal.x==-1&&q.horizontalNormal.y==0&&q.horizontalNormal.z==0,"axis normal");
    const auto diagonal=Query({-3,.9f,-3},{6,0,6});
    const double expected=(3.-double(box.x)-double(body.radius)/std::sqrt(2.))/6.;
    Require(diagonal.kind==BoxCylinderContactKind3D::SideEntry&&std::abs(diagonal.first-expected)<1e-14,"rounded corner TOI");
    Require(std::abs(diagonal.horizontalNormal.x+1.f/std::sqrt(2.f))<1e-6f,"corner normal");
    Require(Query({-100,.9f,0},{200,0,0}).kind==BoxCylinderContactKind3D::SideEntry,"high speed tunneling");
    Require(Query({-3,5,0},{6,0,0}).kind==BoxCylinderContactKind3D::Clear,"above body blocked");
    Require(Query({0,4,0},{0,-5,0}).kind==BoxCylinderContactKind3D::VerticalEntry,"fall-through event lost");
    Require(Query({0,4,0},{0,-5,0}).verticalNormal.y==1,"fall normal");
    Require(Query({0,-4,0},{0,5,0}).verticalNormal.y==-1,"rising normal");
    Require(Query({0,.9f,0},{}).kind==BoxCylinderContactKind3D::StartedOverlapping,"center overlap lost");
    const auto center=Query({0,.9f,0},{0,0,-1});
    Require(center.horizontalNormal.z==-1&&center.horizontalNormal.x==0,"center escape tie does not follow requested face");
    Require(center.horizontalPenetration>double(body.radius),"inside rectangle depth lost");
    // 可精確表示的 1m / .5m 接觸，驗證 tangent 與 endpoint 不被 tolerance 灌水。
    const DynamicCylinder3D exactBody{{0,0,0},1.f,.5f};BoxCylinderContact3D r;
    Require(QueryBoxCylinderContact3D({-1.5f,0,0},{.5f,.5f,.5f},{-1,0,0},exactBody,r)&&r.kind==BoxCylinderContactKind3D::Clear,"outward boundary blocked");
    Require(QueryBoxCylinderContact3D({-1.5f,0,0},{.5f,.5f,.5f},{1,0,0},exactBody,r)&&r.kind==BoxCylinderContactKind3D::SideEntry&&r.first==0,"inward boundary missed");
    Require(QueryBoxCylinderContact3D({-2.5f,0,0},{.5f,.5f,.5f},{1,0,0},exactBody,r)&&r.kind==BoxCylinderContactKind3D::Clear,"endpoint-only contact blocked");
    Require(QueryBoxCylinderContact3D({-2,0,1.5f},{.5f,.5f,.5f},{4,0,0},exactBody,r)&&r.kind==BoxCylinderContactKind3D::Clear,"tangent path blocked");
    Require(QueryBoxCylinderContact3D({-2.5f,2,0},{.5f,.5f,.5f},{2,-2,0},exactBody,r)&&r.kind==BoxCylinderContactKind3D::SimultaneousEntry&&r.first==.5,"simultaneous entry lost");
}
void SampledGeometry(){
    using namespace DynamicContactDetail3D;
    for(unsigned index=0;index<512;++index){
        const float angle=float(index)*.23817f;
        const Vec3 p{3.7f*std::cos(angle),index%3==0?4.f:.9f,3.1f*std::sin(angle)};
        const Vec3 d{-6.1f*std::cos(angle+.19f),index%3==0?-5.f:0.f,-5.7f*std::sin(angle+.19f)};
        const auto r=Query(p,d);
        bool sampledInside=false;
        for(unsigned step=0;step<=128;++step){
            const double t=double(step)/128.;
            const double x=double(p.x)+double(d.x)*t,z=double(p.z)+double(d.z)*t;
            const double y=double(p.y)-body.center.y+double(d.y)*t;
            const bool inside=SignedRoundedDistance(x,z,box.x,box.z,body.radius)<-1e-7&&std::abs(y)<double(box.y)+body.halfHeight-1e-7;
            if(inside){sampledInside=true;Require(r.kind!=BoxCylinderContactKind3D::Clear,"sampled interior accepted Clear");Require(r.first<=t+1e-12,"TOI later than known interior");}
            if(r.kind!=BoxCylinderContactKind3D::Clear&&t+1e-9<r.first)Require(!inside,"interior before reported entry");
        }
        if(r.kind==BoxCylinderContactKind3D::Clear)Require(!sampledInside,"clear route has interior sample");
        ++sampledPaths;
    }
}
void CombatBoundary(){
    for(unsigned direction=0;direction<8;++direction){
        const float a=float(direction)*.785398163f;
        const Vec3 start{3.5f*std::cos(a),.9f,3.5f*std::sin(a)};
        const Vec3 d{-2*start.x,0,-2*start.z};const auto hit=Query(start,d);
        Require(hit.kind==BoxCylinderContactKind3D::SideEntry,"combat contact missed");
        const Vec3 point=start+d*float(hit.first);const float distance=std::hypot(point.x,point.z);
        Require(distance<2.2f&&distance>1.7f,"contact makes existing windup unreachable");
        Mistwatch::Encounter enemy;enemy.feet={0,0,0};enemy.strikeTarget={0,0,0};
        const Vec3 player{point.x,0,point.z};unsigned damage{};
        damage+=enemy.Tick(player,1.f/120.f,false);
        Require(enemy.phase==Mistwatch::EnemyPhase::Windup,"real Encounter failed Windup at rounded contact");
        for(unsigned tick=0;tick<120&&enemy.phase==Mistwatch::EnemyPhase::Windup;++tick)damage+=enemy.Tick(player,1.f/120.f,false);
        Require(enemy.phase==Mistwatch::EnemyPhase::Recovery&&damage==20,"real Encounter failed timed Recovery");
        Require(enemy.Tick(player,1.f/120.f,true)==0&&enemy.health==2&&enemy.countered,"real counter unreachable");
        ++combatDirections;
    }
}
void RejectInvalid(){
    BoxCylinderContact3D original;original.kind=BoxCylinderContactKind3D::VerticalEntry;original.first=.25;original.verticalNormal={0,1,0};
    const auto reject=[&](Vec3 p,Vec3 e,Vec3 d,DynamicCylinder3D b){auto r=original;
        std::array<std::byte,sizeof r> before;std::memcpy(before.data(),&r,sizeof r);
        Require(!QueryBoxCylinderContact3D(p,e,d,b,r),"invalid input accepted");
        Require(std::memcmp(&r,before.data(),sizeof r)==0,"invalid query mutated output");};
    const auto component=[](Vec3& p,unsigned axis,float value){switch(axis){case 0:p.x=value;break;case 1:p.y=value;break;default:p.z=value;break;}};
    const float nan=(std::numeric_limits<float>::quiet_NaN)(),inf=(std::numeric_limits<float>::infinity)();
    for(const float invalid:{nan,inf,-inf})for(unsigned slot=0;slot<14;++slot){
        Vec3 p{-3,.9f,0},e=box,d{6,0,0};auto b=body;
        if(slot<3)component(p,slot,invalid);else if(slot<6)component(e,slot-3,invalid);
        else if(slot<9)component(d,slot-6,invalid);else if(slot<12)component(b.center,slot-9,invalid);
        else if(slot==12)b.radius=invalid;else b.halfHeight=invalid;
        reject(p,e,d,b);
    }
    for(const float invalid:{0.f,-1.f,nan,inf}){auto b=body;b.radius=invalid;reject({-3,.9f,0},box,{6,0,0},b);
        b=body;b.halfHeight=invalid;reject({-3,.9f,0},box,{6,0,0},b);}
    for(unsigned axis=0;axis<3;++axis)for(const float invalid:{0.f,-1.f}){auto e=box;component(e,axis,invalid);reject({-3,.9f,0},e,{6,0,0},body);}
    const float largest=(std::numeric_limits<float>::max)();
    reject({largest,0,0},box,{largest,0,0},body);
    auto enormous=body;enormous.radius=largest;reject({-3,.9f,0},{largest,.9f,.45f},{6,0,0},enormous);
}
}
int RunDynamicContactOriginalCases(){try{Core();SampledGeometry();CombatBoundary();RejectInvalid();
    std::cout<<"BOX_CYLINDER_QUERY_PROTOTYPE_CPU_PASS checks="<<checks<<" sampled_paths="<<sampledPaths<<" actual_Encounter_combat_directions="<<combatDirections
        <<" controller_integrated=0 native=0 GPU=0 vertical_entry_resolved=0 full_actor_transaction=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"QUERY_PROTOTYPE_FIRST_DIVERGENCE "<<e.what()<<" checks="<<checks<<'\n';return 1;}}
