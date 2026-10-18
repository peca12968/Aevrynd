#include "MistwatchBoarHitReaction.h"
#include "MistwatchLoadPreparation.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <bit>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace Aevrynd;
using namespace Mistwatch;
namespace Hit=Mistwatch::BoarHitReaction;
unsigned checks{},groups{},skinFrames{};
void Need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec3 a,Vec3 b){return Bits(a.x,b.x)&&Bits(a.y,b.y)&&Bits(a.z,b.z);}
bool Same(const JointTransform3D& a,const JointTransform3D& b){
    return Same(a.translation,b.translation)&&Same(a.rotation,b.rotation)&&Same(a.scale,b.scale)&&
        Bits(a.quaternion.x,b.quaternion.x)&&Bits(a.quaternion.y,b.quaternion.y)&&
        Bits(a.quaternion.z,b.quaternion.z)&&Bits(a.quaternion.w,b.quaternion.w)&&a.usesQuaternion==b.usesQuaternion;
}
bool Same(const Matrix4& a,const Matrix4& b){
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)if(!Bits(a[r][c],b[r][c]))return false;return true;
}
bool Same(const AnimationPose3D& a,const AnimationPose3D& b){
    if(a.localTransforms.size()!=b.localTransforms.size()||a.globalMatrices.size()!=b.globalMatrices.size())return false;
    for(std::size_t i=0;i<a.localTransforms.size();++i)if(!Same(a.localTransforms[i],b.localTransforms[i]))return false;
    for(std::size_t i=0;i<a.globalMatrices.size();++i)if(!Same(a.globalMatrices[i],b.globalMatrices[i]))return false;return true;
}
bool Same(const Vertex3D& a,const Vertex3D& b){return Same(Vec3{a.x,a.y,a.z},Vec3{b.x,b.y,b.z})&&
    a.normal==b.normal&&a.color==b.color&&Bits(a.u,b.u)&&Bits(a.v,b.v);}
bool Same(const Encounter& a,const Encounter& b){return Same(a.feet,b.feet)&&Same(a.strikeTarget,b.strikeTarget)&&
    a.health==b.health&&a.phase==b.phase&&Bits(a.phaseTime,b.phaseTime)&&
    Bits(a.playerCooldown,b.playerCooldown)&&a.countered==b.countered;}
Encounter Opening(){
    Encounter e;const auto front=e.feet+Vec3{0,0,2},back=e.feet+Vec3{0,0,-2};
    Need(e.Tick(front,.01f,false)==0&&e.phase==EnemyPhase::Windup,"REAL_WINDUP");
    unsigned ticks{};while(e.phase==EnemyPhase::Windup&&ticks++<100)Need(e.Tick(back,.01f,false)==0,"DODGE_OUTSIDE_STRIKE");
    Need(e.Valid()&&e.phase==EnemyPhase::Recovery&&!e.countered,"REAL_OPENING");return e;
}
Encounter HitOnce(){auto e=Opening();const auto p=e.feet+Vec3{0,0,-2};
    Need(e.Tick(p,.01f,true)==0&&e.health==2&&e.countered&&e.playerCooldown==.65f,"REAL_COUNTER_HIT");return e;}
Encounter LateHit(){auto e=Opening();const auto p=e.feet+Vec3{0,0,-2};unsigned ticks{};
    while(e.phaseTime>.01f&&ticks++<130)e.Tick(p,.01f,false);
    Need(e.Valid()&&e.phase==EnemyPhase::Recovery&&e.phaseTime>0&&e.phaseTime<=.01f,"REAL_LAST_RECOVERY_TICK");
    e.Tick(p,.01f,true);Need(e.Valid()&&e.phase==EnemyPhase::Pursuit&&e.health==2&&e.countered&&
        e.playerCooldown==.65f,"LATE_HIT_TO_PURSUIT");return e;}
void Timeline(){
    auto e=Opening();Need(Hit::Build(e).status==Hit::Status::Inactive,"UNUSED_OPENING_UNCHANGED");
    const auto p=e.feet+Vec3{0,0,-2};e.Tick(p,.01f,true);
    auto plan=Hit::Build(e);Need(plan.status==Hit::Status::Active&&plan.elapsed==0&&
        std::abs(plan.pitch-(-.078f))<1.e-7f,"FIRST_HIT_IMPULSE");
    const auto hp=e.health;e.Tick(p,.01f,true);Need(e.health==hp,"NO_SECOND_DAMAGE_SAME_WINDOW");
    unsigned active{};while(e.playerCooldown>0){
        const auto before=e;plan=Hit::Build(e);Need(Same(before,e),"PLAN_CHANGED_ENCOUNTER");
        if(plan.status==Hit::Status::Active)++active;
        e.Tick(p,.01f,false);
    }
    Need(active>=26&&active<=28&&Hit::Build(e).status==Hit::Status::Inactive,"FINITE_REAL_COOLDOWN_WINDOW");++groups;
}
void Gates(){
    auto e=HitOnce();Need(Hit::Build(e,false).status==Hit::Status::Inactive,"DISABLED");
    e=LateHit();Need(Hit::Build(e).status==Hit::Status::Active,"LATE_HIT_PURSUIT_REACTION");
    e.Tick(e.feet+Vec3{0,0,-2},.01f,false);
    Need(e.Valid()&&e.phase==EnemyPhase::Windup&&!e.countered&&Hit::Build(e).status==Hit::Status::Active,"LATE_HIT_NEXT_WINDUP_REACTION");
    e=Opening();Need(Hit::Build(e).status==Hit::Status::Inactive,"RECOVERY_WITHOUT_HIT");
    e=Encounter{};Need(Hit::Build(e).status==Hit::Status::Inactive,"FRESH_PURSUIT_WITHOUT_HIT");
    e.Tick(e.feet+Vec3{0,0,2},.01f,false);Need(Hit::Build(e).status==Hit::Status::Inactive,"FRESH_WINDUP_WITHOUT_HIT");
    e=HitOnce();e.phase=EnemyPhase::Defeated;e.health=0;Need(e.Valid()&&Hit::Build(e).status==Hit::Status::Inactive,"DEFEATED_NO_RECOIL");
    e=HitOnce();e.playerCooldown=0;Need(Hit::Build(e).status==Hit::Status::Inactive,"OLD_USED_WINDOW");
    for(float value:{-.01f,.66f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        e=HitOnce();e.playerCooldown=value;Need(Hit::Build(e).status==Hit::Status::Invalid,"INVALID_COOLDOWN");}
    e=HitOnce();e.feet.x=std::numeric_limits<float>::quiet_NaN();Need(Hit::Build(e).status==Hit::Status::Invalid,"INVALID_ENCOUNTER");++groups;
}
void CurveAndPause(){
    auto e=HitOnce();float minimum{};
    for(unsigned i=0;i<=650;++i){
        e.playerCooldown=.65f-float(i)*.001f;if(e.playerCooldown<0)e.playerCooldown=0;
        const auto plan=Hit::Build(e);Need(plan.status!=Hit::Status::Invalid,"VALID_CURVE_SAMPLE");
        if(plan.status==Hit::Status::Active){Need(std::isfinite(plan.pitch)&&plan.pitch>=-.12f&&plan.pitch<0,"PITCH_BOUND");minimum=(std::min)(minimum,plan.pitch);}
        else Need(e.playerCooldown<=.65f-.28f,"NO_EARLY_DISAPPEAR");
    }
    Need(std::abs(minimum+.12f)<1.e-5f,"FIXED_PEAK");
    Boar tail;Need(tail.Update({},0,EnemyPhase::Recovery,1,{0,0,1}),"TAIL_BASE_POSE");
    Hit::Scratch scratch;Need(scratch.Initialize(tail.model),"TAIL_SCRATCH");
    const float boundary=Hit::Cooldown-Hit::Duration;
    for(float cooldown:{std::nextafter(boundary,0.f),boundary,std::nextafter(boundary,1.f)}){
        e=HitOnce();e.playerCooldown=cooldown;const auto edge=Hit::Build(e);
        if(edge.status==Hit::Status::Active)Need(edge.pitch<0&&Hit::Apply(tail.model,tail.pose,edge,scratch),"TAIL_ACTIVE_APPLIES");
        else Need(Hit::Cooldown-cooldown>=Hit::Duration,"TAIL_INACTIVE_EXACT_BOUNDARY");
    }
    const float last=std::nextafter(Hit::Duration,0.f);const Hit::Plan lastPlan{Hit::Status::Active,last,Hit::Pitch(last)};
    Need(lastPlan.pitch<0&&Hit::Apply(tail.model,tail.pose,lastPlan,scratch),"LAST_ACTIVE_FLOAT_APPLIES");
    e=HitOnce();e.Tick(e.feet+Vec3{0,0,-2},.04f,false);const auto paused=e;const auto plan=Hit::Build(e);
    for(unsigned i=0;i<120;++i){const auto again=Hit::Build(e);Need(Same(e,paused)&&again.status==plan.status&&
        Bits(again.elapsed,plan.elapsed)&&Bits(again.pitch,plan.pitch),"PAUSED_TIMER_SAME_POSE");}++groups;
}
SkinnedModel3D RoundTrip(){Boar original;std::vector<std::byte> bytes;Need(EncodeSkinnedModelAsset(original.model,bytes),"ENCODE_AESK1");
    const auto loaded=LoadSkinnedModelAsset(bytes);Need(bool(loaded),"LOAD_AESK1");return loaded.model;}
void Compatibility(){
    Boar b;Hit::Scratch scratch;Need(scratch.Initialize(b.model),"CANONICAL_COMPATIBILITY");
    const auto loaded=RoundTrip();Need(scratch.Initialize(loaded),"AESK1_STRUCTURAL_NOT_NAME_ID");
    auto bad=loaded;bad.skeleton.joints[2].parent=0;Need(!scratch.Initialize(bad),"WRONG_PARENT_REJECTED");
    bad=loaded;bad.inverseBindMatrices[1][0][3]+=.01f;Need(!scratch.Initialize(bad),"WRONG_INVERSE_BIND_REJECTED");
    bad=loaded;bad.skeleton.joints.pop_back();Need(!scratch.Initialize(bad),"WRONG_JOINT_COUNT");++groups;
}
void SkinCase(const SkinnedModel3D& model,const HeightField3D& terrain,Vec3 feet,float yaw,bool late=false){
    Boar b;b.model=model;
    Hit::Scratch scratch;Need(scratch.Initialize(b.model),"SCRATCH_INITIALIZE");
    SkinningWorkspace3D wsA,wsB;Mesh3D before,after;
    const auto world=CreateTransformMatrix(feet,{0,yaw,0},{1,1,1});
    unsigned frames{};
    auto enemy=late?LateHit():HitOnce();
    for(unsigned i=0;i<28;++i){
        if(i)enemy.Tick(enemy.feet+Vec3{0,0,-2},.01f,false);
        Need(b.Update({},.01f,enemy.phase,(std::max)(0.f,enemy.phaseTime),{std::sin(yaw),0,std::cos(yaw)})&&
            b.PlaceFeet(feet,terrain),"REAL_GROUNDED_ENCOUNTER_POSE");
        const auto original=b.pose;const auto targets=b.footTargets;const auto planted=b.planted;
        Need(SkinModelInto(b.model,b.pose,wsA,before),"REAL_BASE_SKIN");
        const auto plan=Hit::Build(enemy);Need(plan.status==Hit::Status::Active&&Hit::Apply(b.model,b.pose,plan,scratch),"APPLY_ACTIVE_RENDER_POSE");
        Need(Same(b.pose,original)&&b.footTargets.size()==4&&b.planted==planted,"SIMULATION_POSE_READ_ONLY");
        Need(LoadPreparationDetail::FinitePose(b.model.skeleton,scratch.pose),"FINITE_ACTUAL_GLOBAL_POSE");
        for(unsigned j=0;j<16;++j){
            if(j!=1)Need(Same(b.pose.localTransforms[j],scratch.pose.localTransforms[j]),"ONLY_HEAD_LOCAL_CHANGED");
            if(j!=1&&j!=2)Need(Same(b.pose.globalMatrices[j],scratch.pose.globalMatrices[j]),"NON_DESCENDANT_GLOBAL_EXACT");
        }
        Need(!Same(b.pose.globalMatrices[1],scratch.pose.globalMatrices[1])&&!Same(b.pose.globalMatrices[2],scratch.pose.globalMatrices[2]),"HEAD_AND_CHILD_MOVED");
        for(unsigned leg=0;leg<4;++leg){const auto oldFoot=TransformPoint(world*b.pose.globalMatrices[5+leg*3],{});
            const auto newFoot=TransformPoint(world*scratch.pose.globalMatrices[5+leg*3],{});
            Need(Same(oldFoot,newFoot)&&Same(targets[leg],b.footTargets[leg])&&Length(newFoot-targets[leg])<=.005f,"FOUR_FEET_UNCHANGED_IK");}
        Need(SkinModelInto(b.model,scratch.pose,wsB,after),"REAL_REACTION_SKIN");
        Need(before.indices==after.indices&&before.vertices.size()==after.vertices.size(),"TOPOLOGY_UNCHANGED");
        unsigned changed{},footVertices{};
        for(std::size_t v=0;v<after.vertices.size();++v){const auto& source=b.model.influences[v];const auto& a=before.vertices[v];const auto& z=after.vertices[v];
            bool head{},foot{};for(unsigned slot=0;slot<4;++slot)if(source.weights[slot]){
                head|=source.joints[slot]==1||source.joints[slot]==2;
                foot|=source.joints[slot]==5||source.joints[slot]==8||source.joints[slot]==11||source.joints[slot]==14;}
            if(!head)Need(Same(a,z),"NON_HEAD_SKIN_BIT_EXACT");else if(!Same(a,z))++changed;
            Need(a.color==z.color&&Bits(a.u,z.u)&&Bits(a.v,z.v),"SKIN_MATERIAL_ATTRIBUTES_EXACT");
            if(foot){Need(Same(a,z),"HOOF_VERTICES_EXACT");++footVertices;const auto p=TransformPoint(world,{z.x,z.y,z.z});
                Need(terrain.Contains(p.x,p.z)&&p.y-terrain.SampleHeight(p.x,p.z)>=-.02f,"HOOF_TERRAIN_CLEARANCE");}
            const auto n=UnpackNormalSnorm8(z.normal);const float length=Length({n.x,n.y,n.z});
            Need(Session::Finite({z.x,z.y,z.z})&&std::isfinite(length)&&length>.9f&&length<1.1f,"FINITE_SKIN_NORMALS");
            Need(std::hypot(z.x,z.z)<=EncounterBodyRadius&&z.y>=EncounterBodyBottom&&
                z.y<=EncounterBodyBottom+EncounterBodyHeight,"ORIGINAL_CONSERVATIVE_BODY_VOLUME");
        }
        Need(changed>0&&footVertices>0,"ACTUAL_WEIGHTED_HEAD_AND_FOOT_INVENTORY");
        const auto bounds=after.Bounds();Need(Session::Finite(bounds.center)&&Session::Finite(bounds.extents)&&
            std::isfinite(bounds.radius)&&bounds.radius>0,"FINITE_SKIN_BOUNDS");++frames;++skinFrames;
    }
    Need(frames==28,"BOUNDED_REACTION_SAMPLES");
}
void FlatSkin(){const auto model=RoundTrip();HeightField3D terrain({{-4,0,-4},17,17,.5f});terrain.Fill(0);
    for(float yaw:{0.f,1.5707963f,3.14159265f,-1.5707963f})SkinCase(model,terrain,{0,0,0},yaw);++groups;}
void CanonicalTerrainSkin(){const auto model=RoundTrip();const auto terrain=CreateTerrain();
    SkinCase(model,terrain,{12,terrain.SampleHeight(12,48),48},0.f);
    SkinCase(model,terrain,{12,terrain.SampleHeight(12,48),48},1.5707963f,true);++groups;}
void ReuseAndReject(){
    Boar b;Need(b.Update({},0,EnemyPhase::Recovery,1,{0,0,1}),"BASE_RECOVERY");Hit::Scratch s;Need(s.Initialize(b.model),"REUSE_PREPARE");
    const auto* locals=s.pose.localTransforms.data();const auto* globals=s.pose.globalMatrices.data();
    const auto localCap=s.pose.localTransforms.capacity(),globalCap=s.pose.globalMatrices.capacity();
    const auto original=b.pose;const auto plan=Hit::Build(HitOnce());
    for(unsigned i=0;i<128;++i)Need(Hit::Apply(b.model,b.pose,plan,s)&&s.pose.localTransforms.data()==locals&&
        s.pose.globalMatrices.data()==globals&&s.pose.localTransforms.capacity()==localCap&&s.pose.globalMatrices.capacity()==globalCap&&
        Same(b.pose,original),"NO_ACCUMULATION_OR_VECTOR_REALLOCATION");
    auto snapshot=s.pose;Need(!Hit::Apply(b.model,b.pose,{},s)&&Same(snapshot,s.pose),"INACTIVE_SCRATCH_EXACT");
    auto forged=plan;forged.pitch=-1;Need(!Hit::Apply(b.model,b.pose,forged,s)&&Same(snapshot,s.pose),"FORGED_PLAN_REJECTED");
    auto invalid=b.pose;invalid.localTransforms[1].rotation.x=std::numeric_limits<float>::quiet_NaN();
    Need(!Hit::Apply(b.model,invalid,plan,s)&&Same(snapshot,s.pose),"INVALID_INPUT_POSE_REJECTED");
    invalid=b.pose;invalid.localTransforms[1].usesQuaternion=true;
    Need(!Hit::Apply(b.model,invalid,plan,s)&&Same(snapshot,s.pose),"UNSUPPORTED_HEAD_REPRESENTATION");
    Need(!Hit::Apply(b.model,s.pose,plan,s)&&Same(snapshot,s.pose),"ALIASED_OWNER_REJECTED");++groups;
}
void ImmutableSaveAndModel(){
    Boar b;const auto terrain=CreateTerrain();SaveGame saved;saved.session.collected=7;saved.session.repairMaterials=7;
    saved.session.bridgeRepaired=true;saved.session.feet={0,0,44};saved.encounter=HitOnce();
    Need(saved.Valid(),"REAL_HIT_SAVE_VALID");const auto text=saved.Serialize();
    Need(b.Update({},.01f,saved.encounter.phase,saved.encounter.phaseTime,{0,0,1})&&b.PlaceFeet(saved.encounter.feet,terrain),"SAVE_POSE_READY");
    std::vector<std::byte> encoded;Need(EncodeSkinnedModelAsset(b.model,encoded),"SOURCE_MODEL_BYTES");
    const auto pose=b.pose;const auto feet=b.lastFeet;const auto footTargets=b.footTargets;const auto planted=b.planted;
    Hit::Scratch s;Need(s.Initialize(b.model)&&Hit::Apply(b.model,b.pose,Hit::Build(saved.encounter),s),"SAVE_READONLY_APPLY");
    std::vector<std::byte> after;Need(EncodeSkinnedModelAsset(b.model,after)&&after==encoded&&saved.Serialize()==text&&
        Same(pose,b.pose)&&Same(feet,b.lastFeet)&&planted==b.planted,"MODEL_SAVE_OWNER_UNCHANGED");
    for(unsigned leg=0;leg<4;++leg)Need(Same(footTargets[leg],b.footTargets[leg]),"FOOT_TARGET_SOURCE_UNCHANGED");++groups;
}
}
int main(){try{
    Timeline();Gates();CurveAndPause();Compatibility();FlatSkin();CanonicalTerrainSkin();ReuseAndReject();ImmutableSaveAndModel();
    Need(groups==8&&skinFrames==168,"EXACT_GROUPS_AND_SKIN_FRAMES");
    std::cout<<"BOAR_HIT_REACTION_CPU_PASS groups="<<groups<<" checks="<<checks<<" skin_frames="<<skinFrames<<" GPU=false\n";return 0;
}catch(const std::exception& error){std::cerr<<"BOAR_HIT_REACTION_FIRST_DIVERGENCE="<<error.what()<<" checks="<<checks<<'\n';return 1;}}
