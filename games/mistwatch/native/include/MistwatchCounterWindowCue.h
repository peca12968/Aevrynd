#pragma once
#include "MistwatchObjectiveGuide.h"
#include <Aevrynd/Scene3D.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace Mistwatch::CounterWindowCue {
inline constexpr unsigned Brackets=4,SegmentsPerBracket=4,MaximumLines=16;
inline constexpr float Radius=.95f,Width=2.f,GroundOffset=.06f;
inline constexpr Aevrynd::Color Cyan{.15f,.9f,1.f,1.f};
enum class Status {Hidden,Ready,Invalid};
enum class Reason {None,State,Guide,Enemy,Terrain,Endpoint,Plan};
struct Segment {Aevrynd::Vec3 a{},b{};};
struct Plan {Status status{Status::Invalid};Reason reason{Reason::Plan};std::array<Segment,MaximumLines> segments{};unsigned count{};};
struct Receipt {Status status{Status::Invalid};Reason reason{Reason::Plan};unsigned submitted{};};
inline Plan Invalid(Reason why){Plan p;p.reason=why;return p;}
inline bool ValidTerrain(const Aevrynd::HeightField3D& terrain) noexcept {
    const auto& s=terrain.Settings();
    return s.width>=2&&s.depth>=2&&Session::Finite(s.origin)&&std::isfinite(s.cellSize)&&s.cellSize>0&&
        std::uint64_t(s.width)*s.depth==terrain.SampleCount()&&
        std::isfinite(s.origin.x+float(s.width-1)*s.cellSize)&&std::isfinite(s.origin.z+float(s.depth-1)*s.cellSize);
}
inline bool ValidPoint(Aevrynd::Vec3 p,const Aevrynd::HeightField3D& terrain) {
    if(!Session::Finite(p)||!terrain.Contains(p.x,p.z))return false;
    const float h=terrain.SampleHeight(p.x,p.z);
    return std::isfinite(h)&&std::isfinite(h+GroundOffset)&&p.y==h+GroundOffset;
}
inline bool ValidSegment(const Segment& line,const Aevrynd::HeightField3D& terrain) {
    if(!ValidPoint(line.a,terrain)||!ValidPoint(line.b,terrain))return false;
    const float length=Aevrynd::Length(line.b-line.a);
    return std::isfinite(length)&&length>.0001f;
}
// 只展示同一個正式 Guide 的可反擊狀態，不重算距離、冷卻、遮擋或傷害規則。
inline Plan Build(const Session& session,const ObjectiveGuide& guide,const Encounter& enemy,
                  const Aevrynd::HeightField3D& terrain,bool started,bool paused) {
    if(!session.Valid())return Invalid(Reason::State);
    if(!started||paused||!session.Alive()||session.CurrentStage()!=Stage::Encounter)
        return {Status::Hidden,Reason::None,{},0};
    if(!guide.valid)return Invalid(Reason::Guide);
    if(!enemy.Valid())return Invalid(Reason::Enemy);
    if(!guide.counterReady)return {Status::Hidden,Reason::None,{},0};
    if(!ValidTerrain(terrain))return Invalid(Reason::Terrain);
    Plan plan;plan.status=Status::Ready;plan.reason=Reason::None;
    const auto point=[&](float angle,Aevrynd::Vec3& p){
        p=enemy.feet+Aevrynd::Vec3{std::cos(angle)*Radius,0,std::sin(angle)*Radius};
        if(!Session::Finite(p)||!terrain.Contains(p.x,p.z))return false;
        const float height=terrain.SampleHeight(p.x,p.z);
        if(!std::isfinite(height)||!std::isfinite(height+GroundOffset))return false;
        p.y=height+GroundOffset;return true;
    };
    constexpr float quarter=std::numbers::pi_v<float>*.5f,halfArc=std::numbers::pi_v<float>/12.f;
    for(unsigned bracket=0;bracket<Brackets;++bracket)for(unsigned segment=0;segment<SegmentsPerBracket;++segment){
        const float begin=float(bracket)*quarter-halfArc+float(segment)*(2.f*halfArc/SegmentsPerBracket);
        const float end=float(bracket)*quarter-halfArc+float(segment+1)*(2.f*halfArc/SegmentsPerBracket);
        Segment line;
        if(!point(begin,line.a)||!point(end,line.b)||!ValidSegment(line,terrain))return Invalid(Reason::Endpoint);
        plan.segments[plan.count++]=line;
    }
    return plan;
}
// 全部端點先驗完才呼叫任何 Scene.Line；非法候選回傳明確狀態且不留下部分圖元。
inline Receipt Submit(Aevrynd::Scene3D& scene,const Plan& plan,const Aevrynd::HeightField3D& terrain) {
    if(plan.status==Status::Invalid)return {Status::Invalid,plan.reason,0};
    if(plan.status==Status::Hidden)return plan.count==0?Receipt{Status::Hidden,Reason::None,0}:Receipt{Status::Invalid,Reason::Plan,0};
    if(plan.status!=Status::Ready||plan.reason!=Reason::None||plan.count!=MaximumLines)
        return {Status::Invalid,Reason::Plan,0};
    if(!ValidTerrain(terrain))return {Status::Invalid,Reason::Terrain,0};
    for(const auto& line:plan.segments)if(!ValidSegment(line,terrain))return {Status::Invalid,Reason::Endpoint,0};
    for(const auto& line:plan.segments)scene.Line(line.a,line.b,Width,Cyan);
    return {Status::Ready,Reason::None,MaximumLines};
}
}
