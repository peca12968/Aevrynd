#pragma once
#include "MistwatchObjectiveGuide.h"
#include <Aevrynd/Renderer.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace Mistwatch::SurveyMap {
inline constexpr float Top=176.f,Bottom=560.f,MinimumTextPixels=18.2f;
inline constexpr unsigned Columns=25,Rows=30,TileBudget=Columns*Rows,MarkerBudget=4;
inline bool ShouldShow(bool started,bool paused,bool alive,bool journalOpen) noexcept {
    return started&&paused&&alive&&!journalOpen;
}
enum class Kind {Bridge,Supply,Material,Encounter,Journal,Fuse,Generator,Relay,Beacon};
struct Bounds {double minX{},minZ{},maxX{},maxZ{};};
struct Rect {float x{},y{},width{},height{};};
struct Tile {unsigned sourceX{},sourceZ{};float height{};};
struct Marker {Kind kind{Kind::Bridge};unsigned index{};Aevrynd::Vec3 world{};Aevrynd::Vec2 unit{};bool completed{};};
struct Data {
    bool valid{};Stage stage{Stage::Supplies};Bounds bounds{};Rect plot{};
    std::array<Tile,TileBudget> tiles{};unsigned tileCount{};
    std::array<Marker,MarkerBudget> markers{};unsigned markerCount{};
    Aevrynd::Vec3 playerWorld{};Aevrynd::Vec2 player{},cameraForward{};
    float minimumHeight{},maximumHeight{};
    std::optional<float> waterSurface;
    bool relayControls{};unsigned trackedRelay{NoTrackedRelay},previewRelay{NoTrackedRelay};
};
inline bool Project(const Bounds& b,Aevrynd::Vec3 p,Aevrynd::Vec2& result) noexcept {
    if(!Session::Finite(p)||!std::isfinite(b.minX)||!std::isfinite(b.minZ)||
       !std::isfinite(b.maxX)||!std::isfinite(b.maxZ)||b.maxX<=b.minX||b.maxZ<=b.minZ||
       !std::isfinite(b.maxX-b.minX)||!std::isfinite(b.maxZ-b.minZ)||
       p.x<b.minX||p.x>b.maxX||p.z<b.minZ||p.z>b.maxZ)return false;
    const double x=(double(p.x)-b.minX)/(b.maxX-b.minX),y=(b.maxZ-double(p.z))/(b.maxZ-b.minZ);
    if(!std::isfinite(x)||!std::isfinite(y)||x<0||x>1||y<0||y>1)return false;
    result={float(x),float(y)};return true;
}
inline Aevrynd::Vec2 Screen(const Data& d,Aevrynd::Vec2 unit) noexcept {
    return {d.plot.x+unit.x*d.plot.width,d.plot.y+unit.y*d.plot.height};
}
// 只讀當幀地形與已揭露任務；不複製 Guide 的自由文字或任何謎題答案。
inline Data Build(const Session& session,const ObjectiveGuide& guide,const Aevrynd::Camera3D& camera,
                  const Aevrynd::HeightField3D& terrain,unsigned tracked=NoTrackedRelay,
                  unsigned preview=NoTrackedRelay,bool relayControls=false,std::optional<float> waterSurface=std::nullopt) {
    Data d;
    if(!session.Valid()||!session.Alive()||!guide.valid||!Session::Finite(camera.position)||!Session::Finite(camera.target)||
       !Session::Finite(camera.up)||!std::isfinite(camera.projection.verticalFieldOfView)||
       !std::isfinite(camera.projection.orthographicHeight)||!std::isfinite(camera.projection.nearPlane)||
       !std::isfinite(camera.projection.farPlane))return d;
    if(waterSurface&&!std::isfinite(*waterSurface))return d;
    d.waterSurface=waterSurface;
    const auto& s=terrain.Settings();
    if(s.width<2||s.depth<2||!Session::Finite(s.origin)||!std::isfinite(s.cellSize)||s.cellSize<=0||
       std::uint64_t(s.width)*s.depth!=terrain.SampleCount())return d;
    // 與 HeightField3D::Contains／SamplePosition 使用相同 float 端點，不外推虛構範圍。
    d.bounds={s.origin.x,s.origin.z,double(s.origin.x+float(s.width-1)*s.cellSize),double(s.origin.z+float(s.depth-1)*s.cellSize)};
    const double width=d.bounds.maxX-d.bounds.minX,depth=d.bounds.maxZ-d.bounds.minZ;
    if(!std::isfinite(width)||!std::isfinite(depth)||width<=0||depth<=0||
       std::abs(d.bounds.maxX)>(std::numeric_limits<float>::max)()||
       std::abs(d.bounds.maxZ)>(std::numeric_limits<float>::max)()||
       !Project(d.bounds,session.feet,d.player))return Data{};
    d.stage=session.CurrentStage();d.playerWorld=session.feet;
    if((d.stage==Stage::Complete&&guide.hasTarget)||(d.stage!=Stage::Complete&&!guide.hasTarget))return Data{};
    Aevrynd::Vec2 guidePoint{};
    if(guide.hasTarget&&!Project(d.bounds,guide.target,guidePoint))return Data{};
    const double fx=double(camera.target.x)-camera.position.x,fz=double(camera.target.z)-camera.position.z;
    const double length=std::hypot(fx,fz);
    if(!std::isfinite(length)||length<=.001)return Data{};
    d.cameraForward={float(fx/length),float(-fz/length)};
    const double scale=(std::min)(300./width,336./depth);
    const double pw=width*scale,ph=depth*scale;
    if(!std::isfinite(scale)||!std::isfinite(pw)||!std::isfinite(ph)||pw<1||ph<1)return Data{};
    d.plot={20.f+float((300.-pw)*.5),208.f+float((336.-ph)*.5),float(pw),float(ph)};
    d.minimumHeight=(std::numeric_limits<float>::infinity)();d.maximumHeight=-d.minimumHeight;
    for(unsigned row=0;row<Rows;++row)for(unsigned col=0;col<Columns;++col) {
        // 每格讀取其近中心的真實高度節點，北方對應畫面上方。
        const unsigned x=unsigned((double(col)+.5)/Columns*double(s.width-1));
        const unsigned z=unsigned((double(Rows-row)-.5)/Rows*double(s.depth-1));
        const float h=terrain.Height(x,z);
        if(!std::isfinite(h))return Data{};
        d.tiles[d.tileCount++]={x,z,h};
        d.minimumHeight=(std::min)(d.minimumHeight,h);d.maximumHeight=(std::max)(d.maximumHeight,h);
    }
    const auto add=[&](Kind kind,unsigned index,Aevrynd::Vec3 p,bool completed=false) {
        if(d.markerCount>=MarkerBudget)return false;
        Marker m{kind,index,p,{},completed};if(!Project(d.bounds,p,m.unit))return false;
        d.markers[d.markerCount++]=m;return true;
    };
    if(!add(Kind::Bridge,0,Session::bridge,session.bridgeRepaired))return Data{};
    const auto pending=[&](const auto& points,unsigned bits,Kind kind) {
        for(unsigned i=0;i<points.size();++i)if(!(bits&(1u<<i)))if(!add(kind,i,points[i]))return false;
        return true;
    };
    bool ok=true;
    switch(d.stage) {
    case Stage::Supplies:ok=pending(Session::supplies,session.collected,Kind::Supply);break;
    case Stage::RepairMaterials:ok=pending(Session::repairSites,session.repairMaterials,Kind::Material);break;
    case Stage::Bridge:case Stage::Complete:break;
    case Stage::Encounter:ok=add(Kind::Encounter,0,guide.target);break;
    case Stage::Fuse:ok=session.fuseClueRead?add(Kind::Fuse,0,Session::fuse):add(Kind::Journal,0,Session::rangerLog);break;
    case Stage::Generator:ok=add(Kind::Generator,0,Session::generator);break;
    case Stage::Relays:ok=pending(Session::relaySites,session.relayLinked,Kind::Relay);break;
    case Stage::Beacon:ok=add(Kind::Beacon,0,Session::beacon);break;
    }
    if(!ok)return Data{};
    if(tracked>NoTrackedRelay||preview>NoTrackedRelay)return Data{};
    d.relayControls=relayControls&&d.stage==Stage::Relays;
    if(d.relayControls){
        if(CanTrackRelay(session,tracked))d.trackedRelay=tracked;
        if(CanTrackRelay(session,preview))d.previewRelay=preview;
    }
    d.valid=true;return d;
}
inline std::string MarkerLabel(const Marker& m) {
    switch(m.kind) {
    case Kind::Bridge:return m.completed?"已知橋梁：已修復":"已知橋梁：尚未修復";
    case Kind::Supply:return "待收集補給箱 "+std::to_string(m.index+1);
    case Kind::Material:return m.index==0?"待收集：西側棚內工具":m.index==1?"待收集：東南木料":"待收集：西岸木料";
    case Kind::Encounter:return "當前遭遇目標（暫停位置）";
    case Kind::Journal:return "當前線索：巡林員日誌";
    case Kind::Fuse:return "當前目標：保險絲鎖櫃";
    case Kind::Generator:return "當前目標：發電機";
    case Kind::Relay:return "待連接中繼站 "+std::to_string(m.index+1);
    case Kind::Beacon:return "當前目標：救援訊號台";
    }
    throw std::runtime_error("SURVEY_MAP_UNKNOWN_MARKER");
}
inline Aevrynd::Color MarkerColor(unsigned index) {
    constexpr std::array<Aevrynd::Color,4> colors{{{.95f,.76f,.36f,1},{.34f,.84f,1,1},{1,.48f,.62f,1},{.74f,.64f,1,1}}};
    return colors.at(index);
}
struct TextLine {std::string text;Aevrynd::Vec2 position{},size{};float pixels{MinimumTextPixels};};
struct Layout {std::array<TextLine,12> lines{};unsigned count{};};
inline Layout MeasureLayout(const Aevrynd::DrawList& draw,const Data& d) {
    Layout result;if(!d.valid)return result;
    const auto add=[&](std::string text,float pixels=MinimumTextPixels) {
        if(result.count>=result.lines.size())throw std::runtime_error("SURVEY_MAP_TEXT_BUDGET");
        const Aevrynd::Vec2 pos{348.f,184.f+float(result.count)*32.f};
        const auto size=draw.MeasureSystemText(text,pixels);
        if(!std::isfinite(size.x)||!std::isfinite(size.y)||size.x<=0||size.y<=0||pixels<MinimumTextPixels||
           pos.x<348||pos.y<Top||pos.x+size.x>1260||pos.y+size.y>Bottom-8)
            throw std::runtime_error("SURVEY_MAP_MEASURED_TEXT_OUTSIDE_PANEL");
        result.lines[result.count++]={std::move(text),pos,size,pixels};
    };
    add("巡守地圖 · 暫停檢視",20.f);
    add("地形示意，不能判斷可通行");
    add(d.waterSurface?"北方（+Z）向上；綠色為高度，藍色為水域採樣":"北方（+Z）向上；色階僅表示地形高度");
    add("白點：目前位置；白箭頭：目前鏡頭方向");
    for(unsigned i=0;i<d.markerCount;++i){
        const auto& marker=d.markers[i];auto label=MarkerLabel(marker);
        if(d.relayControls&&marker.kind==Kind::Relay){
            constexpr std::array names{"西側山脊中繼站","東側瞭望中繼站","西北中繼站"};
            label=names.at(marker.index);
        }
        if(marker.kind==Kind::Relay&&marker.index==d.trackedRelay)label+="（追蹤）";
        if(marker.kind==Kind::Relay&&marker.index==d.previewRelay)label+="（待確認）";
        add(std::move(label));
    }
    if(d.stage==Stage::Complete)add("本次巡守已完成，沒有未完成任務點");
    if(d.relayControls){add("←／→ 選站；Enter 確認追蹤；Esc 繼續");add("追蹤只固定方位，不改變近場操作");}
    add("只顯示當前已知任務；沒有規劃安全路線");
    return result;
}
struct DrawReceipt {bool drawn{};unsigned tiles{},markers{},textLines{};};
inline DrawReceipt Draw(Aevrynd::DrawList& draw,const Data& d) {
    if(!d.valid)return {};
    if(d.tileCount!=TileBudget||d.markerCount>MarkerBudget||!d.markerCount||
       !std::isfinite(d.minimumHeight)||!std::isfinite(d.maximumHeight)||d.minimumHeight>d.maximumHeight||
       !std::isfinite(d.plot.x)||!std::isfinite(d.plot.y)||!std::isfinite(d.plot.width)||!std::isfinite(d.plot.height)||
       d.plot.width<1||d.plot.height<1||d.plot.x<20||d.plot.y<208||d.plot.x+d.plot.width>320.01f||d.plot.y+d.plot.height>544.01f)
        throw std::runtime_error("SURVEY_MAP_INVALID_DRAW_DATA");
    const auto unit=[](Aevrynd::Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=0&&p.x<=1&&p.y>=0&&p.y<=1;};
    Aevrynd::Vec2 projected{};
    if(!unit(d.player)||!Project(d.bounds,d.playerWorld,projected)||projected.x!=d.player.x||projected.y!=d.player.y||
       !std::isfinite(d.cameraForward.x)||!std::isfinite(d.cameraForward.y)||
       std::abs(std::hypot(double(d.cameraForward.x),double(d.cameraForward.y))-1.)>.00001)
        throw std::runtime_error("SURVEY_MAP_INVALID_PLAYER_OR_HEADING");
    for(unsigned i=0;i<d.markerCount;++i) {
        const auto& m=d.markers[i];bool indexValid=false;
        switch(m.kind) {
        case Kind::Supply:case Kind::Material:case Kind::Relay:indexValid=m.index<3;break;
        case Kind::Bridge:case Kind::Encounter:case Kind::Journal:case Kind::Fuse:case Kind::Generator:case Kind::Beacon:indexValid=m.index==0;break;
        }
        if(!indexValid||!unit(m.unit)||!Project(d.bounds,m.world,projected)||projected.x!=m.unit.x||projected.y!=m.unit.y)
            throw std::runtime_error("SURVEY_MAP_INVALID_MARKER");
    }
    if(d.trackedRelay>NoTrackedRelay||d.previewRelay>NoTrackedRelay||
       ((d.trackedRelay<3||d.previewRelay<3||d.relayControls)&&d.stage!=Stage::Relays)||
       (!d.relayControls&&(d.trackedRelay<3||d.previewRelay<3)))
        throw std::runtime_error("SURVEY_MAP_INVALID_TRACKING");
    for(unsigned selected:{d.trackedRelay,d.previewRelay})if(selected<3){
        unsigned found{};for(unsigned i=0;i<d.markerCount;++i){const auto& marker=d.markers[i];
            if(marker.kind==Kind::Relay&&marker.index==selected&&!marker.completed)++found;}
        if(found!=1)throw std::runtime_error("SURVEY_MAP_UNKNOWN_TRACKED_RELAY");
    }
    if(d.waterSurface&&!std::isfinite(*d.waterSurface))throw std::runtime_error("SURVEY_MAP_INVALID_WATER_SURFACE");
    const auto layout=MeasureLayout(draw,d);
    // 先完成全部資料與字型驗證，再提交任何圖元，不以縮字或裁切掩蓋失敗。
    for(const auto& t:d.tiles)if(!std::isfinite(t.height)||t.height<d.minimumHeight||t.height>d.maximumHeight)
        throw std::runtime_error("SURVEY_MAP_INVALID_TILE");
    draw.Quad({640,(Top+Bottom)*.5f},{1264,Bottom-Top},{.025f,.045f,.055f,.96f});
    const float cw=d.plot.width/Columns,ch=d.plot.height/Rows;
    const double range=double(d.maximumHeight)-d.minimumHeight;
    for(unsigned row=0;row<Rows;++row)for(unsigned col=0;col<Columns;++col) {
        const auto& t=d.tiles[row*Columns+col];
        const float a=range>0?float((double(t.height)-d.minimumHeight)/range):.5f;
        // 採樣格只標示低於實際風暴水位的地形；橋梁仍由獨立任務標記表示。
        const auto color=d.waterSurface&&t.height<*d.waterSurface?
            Aevrynd::Color{.12f,.35f,.55f,1}:Aevrynd::Color{.10f+.30f*a,.20f+.31f*a,.18f+.28f*a,1};
        draw.Quad({d.plot.x+(float(col)+.5f)*cw,d.plot.y+(float(row)+.5f)*ch},{cw,ch},color);
    }
    for(unsigned i=0;i<d.markerCount;++i) {
        const auto p=Screen(d,d.markers[i].unit);draw.Circle(p,5.f,MarkerColor(i),16);
        if(d.markers[i].kind==Kind::Relay){
            if(d.markers[i].index==d.trackedRelay)draw.Circle(p,9.f,{.55f,1.f,.76f,1},24);
            if(d.markers[i].index==d.previewRelay)draw.Circle(p,12.f,{1,1,1,1},24);
        }
        draw.Circle({334.f,184.f+float(i+4)*32.f+10.f},4.f,MarkerColor(i),12);
    }
    const auto player=Screen(d,d.player);
    draw.Circle(player,4.f,{1,1,1,1},16);
    // 非方形世界仍以世界公尺等比例投影鏡頭朝向，並縮短箭頭使其留在地圖內。
    const Aevrynd::Vec2 delta{d.cameraForward.x*20.f,d.cameraForward.y*20.f};
    float fraction=1.f;
    if(delta.x>0)fraction=(std::min)(fraction,(d.plot.x+d.plot.width-player.x)/delta.x);
    if(delta.x<0)fraction=(std::min)(fraction,(d.plot.x-player.x)/delta.x);
    if(delta.y>0)fraction=(std::min)(fraction,(d.plot.y+d.plot.height-player.y)/delta.y);
    if(delta.y<0)fraction=(std::min)(fraction,(d.plot.y-player.y)/delta.y);
    if(fraction>0)draw.Line(player,{player.x+delta.x*fraction,player.y+delta.y*fraction},2.f,{1,1,1,1});
    // 固定方位箭頭不受玩家剛好站在世界邊界影響；只顯示實際鏡頭方向。
    const Aevrynd::Vec2 origin{304,192},tip{304+d.cameraForward.x*10,192+d.cameraForward.y*10};
    draw.Line(origin,tip,2,{1,1,1,1});
    draw.Line(tip,{tip.x-d.cameraForward.x*5+d.cameraForward.y*3,tip.y-d.cameraForward.y*5-d.cameraForward.x*3},2,{1,1,1,1});
    draw.Line(tip,{tip.x-d.cameraForward.x*5-d.cameraForward.y*3,tip.y-d.cameraForward.y*5+d.cameraForward.x*3},2,{1,1,1,1});
    for(unsigned i=0;i<layout.count;++i)draw.SystemText(layout.lines[i].position,layout.lines[i].text,layout.lines[i].pixels,{.9f,.95f,.96f,1});
    return {true,TileBudget,d.markerCount,layout.count};
}
}
