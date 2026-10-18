#include <CharacterDynamicContact3D.h>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Aevrynd;
namespace {
using Exact = ExactTriangleContactDetail3D::Rational;
unsigned checks{}, rejected{}, ordering{}, axisCases{}, axisHits{}, axisClears{}, verticalCases{}, nearTangent{};
struct Observation { std::string name; BoxCylinderContact3D result; };
std::vector<Observation> observations;
void Need(bool value, const std::string& message) { ++checks; if (!value) throw std::runtime_error(message); }
Exact F(float value) { return Exact::Float(value); }
Exact Abs(Exact value) { return value.Sign() < 0 ? -value : value; }
Exact Positive(Exact value) { return value.Sign() > 0 ? value : Exact{}; }
Exact At(float initial, float delta, const Exact& t) { return F(initial) + F(delta) * t; }
bool StrictInside(Vec3 p, Vec3 e, Vec3 d, const DynamicCylinder3D& b, const Exact& t) {
    const auto qx = Positive(Abs(At(p.x, d.x, t) - F(b.center.x)) - F(e.x));
    const auto qz = Positive(Abs(At(p.z, d.z, t) - F(b.center.z)) - F(e.z));
    return Compare(qx*qx + qz*qz, F(b.radius)*F(b.radius)) < 0 &&
        Compare(Abs(At(p.y, d.y, t) - F(b.center.y)), F(e.y)+F(b.halfHeight)) < 0;
}
const char* Kind(BoxCylinderContactKind3D k) {
    switch(k) {
    case BoxCylinderContactKind3D::Clear: return "Clear";
    case BoxCylinderContactKind3D::SideEntry: return "SideEntry";
    case BoxCylinderContactKind3D::VerticalEntry: return "VerticalEntry";
    case BoxCylinderContactKind3D::SimultaneousEntry: return "SimultaneousEntry";
    case BoxCylinderContactKind3D::StartedOverlapping: return "StartedOverlapping";
    }
    throw std::runtime_error("Unknown contact enum");
}
BoxCylinderContact3D Query(const std::string& name, Vec3 p, Vec3 e, Vec3 d, const DynamicCylinder3D& b) {
    BoxCylinderContact3D r;
    Need(QueryBoxCylinderContact3D(p,e,d,b,r), name+": ordinary bounded query rejected");
    Need(std::isfinite(r.first) && r.first>=0 && r.first<=1, name+": finite event range");
    return r;
}
void Expect(const std::string& name, Vec3 p, Vec3 e, Vec3 d, const DynamicCylinder3D& b, BoxCylinderContactKind3D kind) {
    const auto r=Query(name,p,e,d,b);
    observations.push_back({name,r});
    Need(r.kind==kind, name+": expected "+Kind(kind)+", got "+Kind(r.kind));
    if(kind==BoxCylinderContactKind3D::Clear) {
        Need(r.first==1 && r.horizontalNormal.x==0 && r.horizontalNormal.z==0 && r.verticalNormal.y==0,
            name+": Clear output differs");
    } else if(kind==BoxCylinderContactKind3D::SimultaneousEntry) {
        Need(r.horizontalNormal.x!=0 && r.horizontalNormal.z!=0 && r.verticalNormal.y!=0, name+": both event normals required");
    } else if(kind==BoxCylinderContactKind3D::SideEntry) {
        Need(r.verticalNormal.y==0, name+": side event gained top normal");
    } else if(kind==BoxCylinderContactKind3D::VerticalEntry) {
        Need(r.horizontalNormal.x==0 && r.horizontalNormal.z==0 && r.verticalNormal.y!=0, name+": vertical event gained side normal");
    }
}
void Reject(const std::string& name,Vec3 p,Vec3 e,Vec3 d,const DynamicCylinder3D& b) {
    BoxCylinderContact3D r;
    r.kind=BoxCylinderContactKind3D::SimultaneousEntry;r.first=.375;
    r.horizontalNormal={3,4,5};r.verticalNormal={6,7,8};r.horizontalPenetration=9;
    std::array<std::byte,sizeof r> before{};std::memcpy(before.data(),&r,sizeof r);
    Need(!QueryBoxCylinderContact3D(p,e,d,b,r),name+": unresolved query accepted");
    Need(std::memcmp(before.data(),&r,sizeof r)==0,name+": false mutated output");
    ++rejected;
}
void FixedCounterexamples() {
    const float power=std::ldexp(1.f,60);
    const Vec3 p{-power,0,0},e{.5f,.5f,.5f},d{power*2,0,0};
    const DynamicCylinder3D b{{0,0,0},1,.5f};
    Need(StrictInside(p,e,d,b,Exact(1)/Exact(2)),"Q1 exact midpoint witness");
    Reject("Q1 collapsed sweep",p,e,d,b);
    const DynamicCylinder3D enormous{{power,0,0},power,.5f};
    Need(StrictInside({},e,{},enormous,Exact{}),"Q3 exact initial overlap witness");
    const auto penetration=F(e.x)+F(enormous.radius)-F(enormous.center.x);
    Need(Compare(penetration,F(.5f))==0,"Q3 exact half-meter escape");
    Reject("Q3 SDF cancellation",{},e,{},enormous);
    const float huge=(std::numeric_limits<float>::max)()*.75f;
    Need(Compare(F(huge)+F(huge),F((std::numeric_limits<float>::max)()))>0,"Q4 terminal box exceeds finite float bounds");
    Reject("Q4 endpoint volume",{},{huge,.5f,.5f},{huge,0,0},{{0,10,0},1,.5f});
    // 中等高速仍須受理，不能以極端案例當理由封鎖正常域的連續掃掠。
    const float regular=std::ldexp(1.f,18);
    const auto regularHit=Query("bounded high speed",{-regular,0,0},e,{regular*2,0,0},b);
    Need(regularHit.kind==BoxCylinderContactKind3D::SideEntry && regularHit.first<.5,"bounded high speed lost contact");
}
void EventOrdering() {
    const float below=std::nextafter(4.f,0.f),above=std::nextafter(4.f,8.f);
    const Exact tie=Exact(2)/Exact(3),step=Exact(1)/Exact(4096);
    for(const float scale:{.0625f,1.f,16.f})for(const float mirror:{-1.f,1.f}) {
        const Vec3 e{scale,scale,scale};const DynamicCylinder3D b{{},5*scale,scale};
        const Vec3 d{3*scale*mirror,-3*scale,0};
        for(const auto [initialY,kind]:std::array<std::pair<float,BoxCylinderContactKind3D>,3>{{
            {below,BoxCylinderContactKind3D::SideEntry},{4.f,BoxCylinderContactKind3D::SimultaneousEntry},{above,BoxCylinderContactKind3D::VerticalEntry}}}) {
            const Vec3 p{-7*scale*mirror,initialY*scale,-4*scale};
            const auto ty=(F(initialY)-Exact(2))/Exact(3);
            Need(Compare(ty,tie)==(initialY<4?-1:initialY>4?1:0),"one-ULP exact ordering oracle");
            Need(!StrictInside(p,e,d,b,tie-step) && StrictInside(p,e,d,b,tie+step),"event exact before/after witnesses");
            const std::string name="event-"+std::to_string(ordering);
            Expect(name,p,e,d,b,kind);++ordering;
        }
    }
    const Vec3 e{1,1,1};const DynamicCylinder3D b{{},5,1};
    const auto tyBelow=(F(below)-Exact(2))/Exact(3);
    const Vec3 leave{-3,4,-4},away{-3,-3,0};
    Need(!StrictInside(leave,e,away,b,tie-step) && !StrictInside(leave,e,away,b,tie+step),"Y entry/horizontal exit disjoint witnesses");
    Expect("horizontal already exits at Y entry",leave,e,away,b,BoxCylinderContactKind3D::Clear);
    Expect("Y enters after horizontal left",{-3,above,-4},e,away,b,BoxCylinderContactKind3D::Clear);
    Need(StrictInside({-3,below,-4},e,away,b,(tyBelow+tie)/Exact(2)),"thin Y-first exact interior witness");
    Expect("Y enters just before horizontal exit",{-3,below,-4},e,away,b,BoxCylinderContactKind3D::VerticalEntry);
    const float yBelow=std::nextafter(1.f,0.f),yAbove=std::nextafter(1.f,2.f);
    const Vec3 toward{3,1.5f,0};
    Expect("side entry only at Y exit",{-7,1,-4},e,toward,b,BoxCylinderContactKind3D::Clear);
    Expect("Y exits before side entry",{-7,yAbove,-4},e,toward,b,BoxCylinderContactKind3D::Clear);
    const auto yExit=(Exact(2)-F(yBelow))/F(1.5f);
    Need(Compare(yExit,tie)>0 && StrictInside({-7,yBelow,-4},e,toward,b,(yExit+tie)/Exact(2)),"side-first/Y-exit exact interior witness");
    Expect("side enters just before Y exit",{-7,yBelow,-4},e,toward,b,BoxCylinderContactKind3D::SideEntry);
    ordering+=6;
}
// 以整段線段至矩形的最小距離作獨立連續 oracle，而非以被測 SDF 或採樣代表 Clear。
void MixedAxis() {
    const Vec3 e{.5f,.5f,.5f};const DynamicCylinder3D b{{},1,.5f};
    for(const float x:{-4.f,-2.f,0.f,2.f,4.f})for(const float z:{-2.f,-1.5f,-1.f,-.5f,0.f,.5f,1.f,1.5f,2.f})
        for(const float dx:{-4.f,-1.f,0.f,1.f,4.f}) {
        const Vec3 p{x,0,z},d{dx,0,0};
        Exact t;
        if(dx!=0) {
            t=-F(x)/F(dx);
            if(Compare(t,Exact{})<0)t={};else if(Compare(t,Exact(1))>0)t=Exact(1);
        }
        const auto qx=Positive(Abs(At(x,dx,t))-F(e.x)),qz=Positive(Abs(F(z))-F(e.z));
        const bool hit=Compare(qx*qx+qz*qz,Exact(1))<0;
        const bool initial=StrictInside(p,e,d,b,Exact{});
        const auto r=Query("mixed-axis "+std::to_string(axisCases),p,e,d,b);
        Need((r.kind!=BoxCylinderContactKind3D::Clear)==hit,"mixed axis continuous hit/miss differs at case "+std::to_string(axisCases));
        if(hit) {
            ++axisHits;Need(StrictInside(p,e,d,b,t),"mixed axis exact interior witness");
            Need(r.kind==(initial?BoxCylinderContactKind3D::StartedOverlapping:BoxCylinderContactKind3D::SideEntry),"mixed axis event kind");
            Need(r.first<=t.Approximate(),"mixed axis TOI after exact witness");
            if(!initial) {
                const float before=float((std::max)(0.,r.first-1./4096.));
                const float after=float((std::min)(1.,r.first+1./4096.));
                Need(!StrictInside(p,e,d,b,F(before)),"mixed axis before first is interior");
                Need(StrictInside(p,e,d,b,F(after)),"mixed axis after first not interior");
            }
        } else {++axisClears;Need(r.first==1,"mixed axis Clear time");}
        ++axisCases;
    }
    Need(axisCases==225 && axisHits>0 && axisClears>0,"mixed axis hit/miss families empty");
}
void MixedVertical() {
    const Vec3 e{.5f,.5f,.5f};const DynamicCylinder3D b{{},1,.5f};
    for(const float y:{-3.f,-1.f,0.f,1.f,3.f})for(const float dy:{-4.f,-1.f,0.f,1.f,4.f}) {
        const Vec3 p{0,y,0},d{0,dy,0};Exact t;
        if(dy!=0){t=-F(y)/F(dy);if(Compare(t,Exact{})<0)t={};else if(Compare(t,Exact(1))>0)t=Exact(1);}
        const bool hit=Compare(Abs(At(y,dy,t)),Exact(1))<0;
        const auto r=Query("mixed-vertical "+std::to_string(verticalCases),p,e,d,b);
        Need((r.kind!=BoxCylinderContactKind3D::Clear)==hit,"mixed vertical continuous oracle differs");
        if(hit) {
            Need(StrictInside(p,e,d,b,t),"mixed vertical exact interior witness");
            Need(r.kind==(std::abs(y)<1?BoxCylinderContactKind3D::StartedOverlapping:BoxCylinderContactKind3D::VerticalEntry),"mixed vertical event kind");
        }
        ++verticalCases;
    }
    Need(verticalCases==25,"mixed vertical family incomplete");
}
void TangentControls() {
    const Vec3 e{.5f,.5f,.5f};const DynamicCylinder3D b{{},1,.5f};
    for(const auto [z,kind]:std::array<std::pair<float,BoxCylinderContactKind3D>,3>{{
        {std::nextafter(1.5f,0.f),BoxCylinderContactKind3D::SideEntry},{1.5f,BoxCylinderContactKind3D::Clear},
        {std::nextafter(1.5f,3.f),BoxCylinderContactKind3D::Clear}}}) {
        const auto qz=F(z)-F(e.z);Need((Compare(qz*qz,Exact(1))<0)==(kind==BoxCylinderContactKind3D::SideEntry),"axis near tangent rational sign");
        Expect("axis-near-tangent-"+std::to_string(nearTangent),{-4,0,z},e,{8,0,0},b,kind);++nearTangent;
    }
    // 3-4-5 支持平面：(-4,3) dot p = 32 為切線，位移 dot normal = 0。
    // 這組大 cross 相消控制以獨立平面／中點等式判斷，不重用 Disk discriminant。
    const Vec3 ce{1,1,1},cd{600000,0,800000};const DynamicCylinder3D cb{{},5,1};
    for(const auto [z,kind]:std::array<std::pair<float,BoxCylinderContactKind3D>,3>{{
        {std::nextafter(-399996.f,-800000.f),BoxCylinderContactKind3D::SideEntry},{-399996.f,BoxCylinderContactKind3D::Clear},
        {std::nextafter(-399996.f,0.f),BoxCylinderContactKind3D::Clear}}}) {
        const Vec3 p{-300005,0,z};
        const auto plane=-Exact(4)*F(p.x)+Exact(3)*F(p.z);
        Need(Compare(-Exact(4)*F(cd.x)+Exact(3)*F(cd.z),Exact{})==0,"cross cancellation path not parallel to support");
        const int side=Compare(plane,Exact(32));
        Need(side==(kind==BoxCylinderContactKind3D::SideEntry?-1:z==-399996.f?0:1),"independent supporting-plane side");
        Need(StrictInside(p,ce,cd,cb,Exact(1)/Exact(2))==(kind==BoxCylinderContactKind3D::SideEntry),"cross cancellation exact midpoint witness");
        Expect("cross-cancellation-"+std::to_string(nearTangent),p,ce,cd,cb,kind);++nearTangent;
    }
}
void Write(const char* status,const std::string& error) {
    std::ofstream o("green-controls.json");if(!o)throw std::runtime_error("Cannot write GREEN result");
    o<<std::setprecision(17)<<"{\"schema\":\"mistwatch.query-numeric-green.1\",\"status\":\""<<status<<"\","
        <<"\"scope\":\"query CPU fixture only; not controller/gameplay/native/GPU qualification\","
        <<"\"numeric_domain_ratio\":16777216,\"domain_is_not_proof_all_double_roots_exact\":true,"
        <<"\"checks\":"<<checks<<",\"unchanged_output_rejections\":"<<rejected<<",\"ordering_cases\":"<<ordering
        <<",\"axis_cases\":"<<axisCases<<",\"axis_hits\":"<<axisHits<<",\"axis_clears\":"<<axisClears
        <<",\"vertical_cases\":"<<verticalCases<<",\"near_tangent_cases\":"<<nearTangent
        <<",\"first_difference\":\""<<error<<"\",\"observations\":[";
    bool comma=false;for(const auto& v:observations){if(comma)o<<',';comma=true;
        o<<"{\"name\":\""<<v.name<<"\",\"kind\":\""<<Kind(v.result.kind)<<"\",\"first\":"<<v.result.first
            <<",\"horizontal_normal\":["<<v.result.horizontalNormal.x<<','<<v.result.horizontalNormal.y<<','<<v.result.horizontalNormal.z
            <<"],\"vertical_normal\":["<<v.result.verticalNormal.x<<','<<v.result.verticalNormal.y<<','<<v.result.verticalNormal.z<<"]}";}
    o<<"]}\n";o.close();if(!o)throw std::runtime_error("Cannot finish GREEN result");
}
}
int RunDynamicContactNumericCases() {
    try {
        FixedCounterexamples();EventOrdering();MixedAxis();MixedVertical();TangentControls();
        Write("QUERY_NUMERIC_CONTROLS_CPU_PASSED_NOT_CONTROLLER_OR_GAMEPLAY_QUALIFIED","");
        std::cout<<"QUERY_NUMERIC_CONTROLS_CPU_PASS checks="<<checks<<" reject_unchanged="<<rejected
            <<" ordering="<<ordering<<" independent_axis="<<axisCases<<" hits="<<axisHits<<" clears="<<axisClears
            <<" vertical="<<verticalCases<<" near_tangent="<<nearTangent<<" controller_integrated=0 native=0 GPU=0\n";
        return 0;
    }catch(const std::exception& e){
        Write("FIRST_DIVERGENCE_PRESERVED_NOT_GREEN",e.what());
        std::cerr<<"QUERY_NUMERIC_CONTROL_FIRST_DIVERGENCE "<<e.what()<<" checks="<<checks<<'\n';return 1;
    }
}
