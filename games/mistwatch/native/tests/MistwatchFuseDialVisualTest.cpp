#include "MistwatchFuseDialVisual.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace Mistwatch;
using namespace Aevrynd;
namespace {
unsigned checks{};
void Check(bool condition,const char* name){++checks;if(!condition)throw std::runtime_error(name);}
bool Close(float a,float b,float epsilon=.00001f){return std::abs(a-b)<=epsilon;}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool Same(Color a,Color b){return a.r==b.r&&a.g==b.g&&a.b==b.b&&a.a==b.a;}
bool Same(const FuseDialVisual& a,const FuseDialVisual& b){
    if(a.value!=b.value||!Same(a.centre,b.centre)||!Same(a.panelSize,b.panelSize)||
        !Same(a.pointerTip,b.pointerTip)||!Same(a.panelColor,b.panelColor)||!Same(a.indicatorColor,b.indicatorColor))return false;
    for(unsigned i=0;i<4;++i)if(!Same(a.tickCentres[i],b.tickCentres[i]))return false;
    for(std::size_t i=0;i<a.triangles.size();++i){const auto& x=a.triangles[i];const auto& y=b.triangles[i];
        if(!Same(x.a,y.a)||!Same(x.b,y.b)||!Same(x.c,y.c)||!Same(x.color,y.color))return false;}
    return true;
}
Session FuseSession(){Session s;s.collected=7;s.repairMaterials=7;s.bridgeRepaired=true;
    s.guardianDefeated=true;s.feet=Session::fuse;return s;}
constexpr Vec3 Cabinet{Session::fuse.x-.9f,.45f,Session::fuse.z};
void Geometry(const FuseDialVisual& visual){
    Check(Session::Finite(visual.centre)&&Session::Finite(visual.pointerTip),"finite centres");
    Check(visual.triangles.size()==47,"fixed triangle budget");
    for(unsigned value=0;value<4;++value){const auto delta=visual.tickCentres[value]-visual.centre;
        Check(Close(std::hypot(delta.y,delta.z),.062f),"equal tick radius");
        const auto next=visual.tickCentres[(value+1)%4]-visual.centre;
        Check(Close(delta.y*next.y+delta.z*next.z,0),"orthogonal ticks");}
    for(const auto& triangle:visual.triangles){
        const auto normal=Cross(triangle.b-triangle.a,triangle.c-triangle.a);
        Check(normal.x>1.e-7f&&normal.y==0&&normal.z==0,"nondegenerate +X winding");
        for(const auto point:{triangle.a,triangle.b,triangle.c}){
            Check(Session::Finite(point),"finite vertex");
            Check(point.x>visual.centre.x+.015f&&point.x<=visual.centre.x+.026f,"front clears plate");
            Check(std::abs(point.y-visual.centre.y)<.105f&&std::abs(point.z-visual.centre.z)<.105f,"plate footprint");}
    }
}
}
int main(){try{
    // 真 Session 狀態、輸入與作者輸出，不另寫假轉盤規則。
    unsigned unlocks{};
    for(unsigned packed=0;packed<64;++packed){auto session=FuseSession();session.fuseDials=packed;
        Check(session.Valid(),"valid pre-clue dial state");const auto before=session.Serialize();
        const auto visual=CreateFuseDialVisuals(session,Cabinet);Check(bool(visual),"64-code geometry");
        Check(session.Serialize()==before,"visual is read-only");
        session.fuseClueRead=true;const auto read=CreateFuseDialVisuals(session,Cabinet);
        Check(bool(read),"read-clue geometry");
        for(unsigned wheel=0;wheel<3;++wheel){
            Check(Same((*visual)[wheel],(*read)[wheel]),"clue cannot reveal answer visually");
            Check((*visual)[wheel].value==session.FuseDial(wheel),"actual Session value");
            Check(Same((*visual)[wheel].indicatorColor,Color{1,.65f,.12f,1}),"all uncollected codes same color");}
        unlocks+=session.CollectFuse()?1u:0u;
    }
    Check(unlocks==1,"original 63 rejection/1 success gate");
    for(unsigned wheel=0;wheel<3;++wheel){auto session=FuseSession();
        const auto initial=CreateFuseDialVisuals(session,Cabinet);Check(bool(initial),"initial geometry");
        for(unsigned value=0;value<4;++value){const auto visual=CreateFuseDialVisuals(session,Cabinet);
            Check(bool(visual),"3 wheels x 4 values");Geometry((*visual)[wheel]);
            constexpr std::array<Vec3,4> direction{{{0,1,0},{0,0,-1},{0,-1,0},{0,0,1}}};
            const auto delta=(*visual)[wheel].pointerTip-(*visual)[wheel].centre;
            Check(Close(delta.y,direction[value].y*.053f)&&Close(delta.z,direction[value].z*.053f),"clockwise viewed from +X");
            for(unsigned other=0;other<3;++other)if(other!=wheel)
                Check(Same((*visual)[other],(*initial)[other]),"input changes only its own wheel");
            Check(session.CycleFuseDial(wheel),"actual key cycle");}
        const auto cycled=CreateFuseDialVisuals(session,Cabinet);
        Check(bool(cycled)&&Same((*initial)[wheel],(*cycled)[wheel]),"four cycles return exact geometry");
    }
    auto session=FuseSession();session.fuseClueRead=true;session.fuseDials=Session::solvedFuseDials;
    const auto locked=CreateFuseDialVisuals(session,Cabinet);Check(bool(locked),"solved but not collected");
    Check(session.CollectFuse(),"true collect transition");const auto open=CreateFuseDialVisuals(session,Cabinet);
    Check(bool(open),"collected geometry");
    for(unsigned wheel=0;wheel<3;++wheel){
        Check(Same((*open)[wheel].indicatorColor,Color{.2f,1,.3f,1}),"collected green only");
        Check(Same((*locked)[wheel].pointerTip,(*open)[wheel].pointerTip),"unlock does not move pointer");
        Check(Close((*open)[wheel].centre.z,Cabinet.z-.25f+float(wheel)*.25f),"old wheel ordering");
        Check(Close((*open)[wheel].centre.y,.7f)&&Close((*open)[wheel].centre.x,Cabinet.x+.36f),"old anchoring");
        if(wheel)Check(Close((*open)[wheel].centre.z-(*open)[wheel-1].centre.z,.25f),"equal wheel spacing");
    }
    const auto text=session.Serialize();Session restored;Check(restored.Restore(text),"actual restore");
    const auto restoredVisual=CreateFuseDialVisuals(restored,Cabinet);Check(bool(restoredVisual),"restored visual");
    for(unsigned wheel=0;wheel<3;++wheel)Check(Same((*open)[wheel],(*restoredVisual)[wheel]),"restore no separate UI state");
    for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),1001.f,-1001.f})
        for(unsigned axis=0;axis<3;++axis){auto bad=Cabinet;if(axis==0)bad.x=invalid;else if(axis==1)bad.y=invalid;else bad.z=invalid;
            Check(!CreateFuseDialVisuals(session,bad),"invalid centre rejected");}
    auto bad=session;bad.fuseDials=64;Check(!CreateFuseDialVisuals(bad,Cabinet),"invalid dial bits rejected");
    bad=session;bad.feet.x=std::numeric_limits<float>::quiet_NaN();Check(!CreateFuseDialVisuals(bad,Cabinet),"invalid Session rejected");
    auto outside=FuseSession();outside.feet={100,0,100};const auto before=outside.fuseDials;
    Check(!outside.CycleFuseDial(0)&&outside.fuseDials==before,"outside input original gate");
    auto dead=FuseSession();dead.health=0;Check(!dead.CycleFuseDial(0),"dead input original gate");
    std::cout<<"FUSE_DIAL_VISUAL_CPU_PASS; wheel_value_cases=12; packed_codes=64; unlocks="<<unlocks
        <<"; triangles_per_wheel=47; checks="<<checks<<"; native_accepted=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE="<<error.what()<<"; checks="<<checks<<'\n';return 1;}}
