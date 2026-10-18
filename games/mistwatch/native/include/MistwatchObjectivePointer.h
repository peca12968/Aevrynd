#pragma once
#include "MistwatchObjectiveGuide.h"
#include "MistwatchFieldJournal.h"
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace Mistwatch::ObjectivePointer {
inline constexpr FieldPresentation::PanelRect Panel{616,80,48,28};
inline constexpr Aevrynd::Vec2 Center{640,94};
inline constexpr float Radius=9,OutlineWidth=4,InkWidth=2;
inline constexpr unsigned MaximumSegments=4,MaximumLines=8;
inline constexpr Aevrynd::Color Background{.02f,.04f,.04f,.92f};
inline constexpr Aevrynd::Color Outline{.015f,.025f,.025f,1};
inline constexpr Aevrynd::Color Mint{.55f,.94f,.77f,1};
enum class Status {Hidden,Arrow,AtTarget,Invalid};
struct Segment {Aevrynd::Vec2 a{},b{};};
struct Plan {Status status{Status::Hidden};std::array<Segment,MaximumSegments> segments{};unsigned count{};};
struct Receipt {Status status{Status::Hidden};unsigned lines{},quads{};bool submitted{};};

// 只讀已知目標的相對方位；不解謎、不推導路線，也不更改鏡頭或任務。
inline Plan Build(const Session& session,const ObjectiveGuide& guide,bool started,bool paused,bool journalOpen) {
    Plan result;
    if(!session.Valid()){result.status=Status::Invalid;return result;}
    if(!started||paused||!session.Alive()||journalOpen||session.CurrentStage()==Stage::Encounter||
        session.CurrentStage()==Stage::Complete)return result;
    if(!guide.valid||!guide.hasTarget||!Session::Finite(guide.target)||
        std::abs(guide.target.x)>10000||std::abs(guide.target.y)>10000||std::abs(guide.target.z)>10000||
        !std::isfinite(guide.distance)||guide.distance<0||!std::isfinite(guide.relativeRadians)||
        guide.relativeRadians< -std::numbers::pi||guide.relativeRadians>std::numbers::pi||
        (guide.relayTrackingActive&&(!CanTrackRelay(session,guide.navigationRelayIndex)))){
        result.status=Status::Invalid;return result;
    }
    if(guide.distance<=.001){
        result.status=Status::AtTarget;result.count=4;
        constexpr std::array<Aevrynd::Vec2,4> diamond{{{640,88},{646,94},{640,100},{634,94}}};
        for(unsigned i=0;i<4;++i)result.segments[i]={diamond[i],diamond[(i+1)%4]};
    }else{
        result.status=Status::Arrow;result.count=3;
        const Aevrynd::Vec2 forward{float(std::sin(guide.relativeRadians)),float(-std::cos(guide.relativeRadians))};
        const Aevrynd::Vec2 right{-forward.y,forward.x};
        const auto tip=Center+forward*Radius,tail=Center-forward*6.f,base=tip-forward*5.f;
        result.segments[0]={tail,tip};result.segments[1]={tip,base+right*4.f};result.segments[2]={tip,base-right*4.f};
    }
    return result;
}

inline bool Valid(const Plan& plan)noexcept {
    if(plan.status==Status::Hidden)return plan.count==0;
    if((plan.status!=Status::Arrow&&plan.status!=Status::AtTarget)||
        plan.count!=(plan.status==Status::Arrow?3u:4u))return false;
    // 先檢查整批端點與最寬描邊，才允許任何底板或線段寫進DrawList。
    constexpr float margin=OutlineWidth*.5f;
    for(unsigned i=0;i<plan.count;++i){const auto& segment=plan.segments[i];
        for(const auto point:{segment.a,segment.b}){
            if(!std::isfinite(point.x)||!std::isfinite(point.y)||point.x<Panel.left+margin||
                point.x>Panel.Right()-margin||point.y<Panel.top+margin||point.y>Panel.Bottom()-margin)return false;
        }
        if(std::hypot(segment.b.x-segment.a.x,segment.b.y-segment.a.y)<=.001f)return false;
    }
    return true;
}

inline Receipt Submit(Aevrynd::DrawList& draw,const Plan& plan) {
    Receipt receipt;
    if(!Valid(plan)){receipt.status=Status::Invalid;return receipt;}
    receipt.status=plan.status;if(plan.status==Status::Hidden)return receipt;
    const std::size_t needed=6+12*plan.count;
    if(needed>draw.vertices.max_size()-draw.vertices.size()){receipt.status=Status::Invalid;return receipt;}
    // 預留這一批的固定2D頂點；不取得紋理、網格、骨架或任何GPU資源。
    draw.vertices.reserve(draw.vertices.size()+needed);
    draw.Quad(Center,{Panel.width,Panel.height},Background);
    for(unsigned i=0;i<plan.count;++i)draw.Line(plan.segments[i].a,plan.segments[i].b,OutlineWidth,Outline);
    for(unsigned i=0;i<plan.count;++i)draw.Line(plan.segments[i].a,plan.segments[i].b,InkWidth,Mint);
    receipt.lines=plan.count*2;receipt.quads=1;receipt.submitted=true;return receipt;
}
}
