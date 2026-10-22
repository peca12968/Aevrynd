// 由真森林tree1/tick25的地形上升＋靜態樹枝下緣拒絕，整理成獨立可重現案例。
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/ConvexAabbSweep3D.h>
#include <array>
#include <bit>
#include <iostream>
#include <stdexcept>
using namespace Aevrynd;
unsigned checks{};
void Need(bool v,const char* w){++checks;if(!v)throw std::runtime_error(w);}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec3 a,Vec3 b){return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
HeightField3D Ramp(float grade=.3f){HeightField3D f({{-4,0,-2},65,33,.125f});for(unsigned z=0;z<33;++z)for(unsigned x=0;x<65;++x)f.SetHeight(x,z,grade*f.SamplePosition(x,z).x);f.RecalculateRange();return f;}
CharacterControllerSettings3D Settings(){CharacterControllerSettings3D s;s.jumpGraceSeconds=.1f;return s;}
std::array<BoxCollider3D,1> Roof(float bottom=1.824f){return {{{{0,bottom+.1f,0},{2,.1f,2}}}};}
void SupportedStop(unsigned variant){
    const auto terrain=Ramp();const auto roof=Roof(variant==3?1.8f:1.824f);CharacterController3D p(Settings());p.Reset({0,0,0});
    const std::span<const ConvexAabbSweep3D* const> empty;Need(p.Step({},roof,empty,terrain,1.f/60.f)&&p.Grounded(),"verified_initial_terrain_support");
    ConvexAabbSweep3D hull;std::array<Vec3,8> points{};unsigned i{};for(float x:{-2.f,2.f})for(float y:{1.824f,2.024f})for(float z:{-2.f,2.f})points[i++]={x,y,z};Need(hull.Prepare(points),"finite_actual_convex_underside");const std::array<const ConvexAabbSweep3D*,1> hulls{{&hull}};
    const auto before=p.CaptureMotionState();const auto initialStats=p.Stats();const float dt=variant==1?1.f/30.f:variant==2?.1f:1.f/60.f;const Vec3 move=variant==1?Normalize(Vec3{1,0,1}):Vec3{1,0,0};
    const bool accepted=variant==4?p.Step({move,false,1},{},hulls,terrain,dt):p.Step({move,false,1},roof,empty,terrain,dt);
    std::cout<<"VARIANT="<<variant<<"; ACCEPTED="<<accepted<<"; FEET="<<p.Position().x<<","<<p.Position().y<<","<<p.Position().z<<"; GROUNDED="<<p.Grounded()<<std::endl;
    Need(accepted,"supported_uphill_underside_must_commit_a_verified_safe_block_not_reject_entire_tick");
    Need(p.Grounded()&&Same(p.Position().y,terrain.SampleHeight(p.Position().x,p.Position().z)),"blocked_prefix_retains_actual_terrain_support");
    Need(p.Position().x>=before.feet.x&&p.Position().x<.081f,"cannot_advance_head_through_underside");
    if(variant!=3)Need(p.Position().x>before.feet.x,"preserve_available_valid_prefix_progress_not_always_discard_whole_tick");
    Need(p.Stats().steps==initialStats.steps+1&&p.Stats().collisions>initialStats.collisions,"applied_block_records_tick_and_actual_contact");
    Need(p.Velocity().x==0&&p.Velocity().y==0&&(variant==1?p.Velocity().z>0:p.Velocity().z==0),"blocked_supported_motion_removes_uphill_but_keeps_valid_contour_input");
    if(variant==1)Need(p.Position().z>.04f,"diagonal_input_retains_contour_travel_in_same_tick");
    const auto stopped=p.Position();
    for(unsigned tick=0;tick<6;++tick){const auto previous=p.Position();Need(variant==4?p.Step({move,false,1},{},hulls,terrain,dt):p.Step({move,false,1},roof,empty,terrain,dt),"repeated_supported_push_remains_valid");Need(p.Position().x>=stopped.x&&p.Position().x<.081f&&p.Grounded(),"repeated_push_no_penetration_or_falling");if(variant==1)Need(p.Position().z>previous.z,"repeated_diagonal_input_keeps_contour_motion_not_total_freeze");}
    Need(variant==4?p.Step({{-1,0,0},false,1},{},hulls,terrain,dt):p.Step({{-1,0,0},false,1},roof,empty,terrain,dt),"reverse_input_releases_from_supported_underside");Need(p.Position().x<stopped.x,"reverse_motion_not_frozen_by_prior_contact");
}
void InvalidEntry(){const auto terrain=Ramp();const auto roof=Roof();CharacterController3D p(Settings());p.Reset({0,.1f,0});const auto before=p.CaptureMotionState();const auto hash=p.StateHash();Need(!p.Step({{1,0,0},false,1},roof,{},terrain,1.f/60.f),"preexisting_roof_penetration_still_rejected");Need(Same(before.feet,p.Position())&&p.StateHash()==hash,"invalid_entry_entire_state_unchanged");}
void OrdinaryGround(){const auto terrain=Ramp();CharacterController3D p(Settings());p.Reset({0,0,0});Need(p.Step({}, {}, {},terrain,1.f/60.f)&&p.Step({{1,0,0},false,1},{},{},terrain,1.f/60.f),"unobstructed_supported_ramp_unchanged");Need(p.Position().x>.09f&&p.Grounded()&&p.Velocity().x>0,"not_globally_stopping_normal_ramp_motion");}
int main()try{for(unsigned i=0;i<5;++i)SupportedStop(i);InvalidEntry();OrdinaryGround();std::cout<<"SUPPORTED_UPHILL_UNDERSIDE_PASS scenarios=7 checks="<<checks<<" GPU=0\n";return 0;}
catch(const std::exception& e){std::cerr<<"FIRST_SUPPORTED_UPHILL_UNDERSIDE_DIVERGENCE="<<e.what()<<std::endl;return 1;}
