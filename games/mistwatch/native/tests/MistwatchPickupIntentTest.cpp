#include "MistwatchPickupIntent.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace Mistwatch;
unsigned groups{},cases{},checks{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}

struct Fixture {
    Session session;
    Aevrynd::HeightField3D terrain;
    Fixture(){terrain.Configure({{-40,0,-40},128,128,1});terrain.Fill(0);}
    ObjectiveGuide Guide(bool behind=false)const {
        Aevrynd::Camera3D camera;
        camera.target=session.feet;camera.position=session.feet+Aevrynd::Vec3{0,4,behind?8.f:-8.f};
        return BuildObjectiveGuide(session,Encounter{},Evasion{},camera,false,true,{},terrain);
    }
    void Pickup(PickupKind kind,unsigned index,float distance=2.7f) {
        session={};
        if(kind==PickupKind::Supply)session.collected=7u^(1u<<index);
        else{session.collected=7;session.repairMaterials=7u^(1u<<index);}
        const auto target=kind==PickupKind::Supply?Session::supplies[index]:Session::repairSites[index];
        session.feet=target-Aevrynd::Vec3{0,0,distance};
        Need(session.Valid(),"FIXTURE_SESSION_INVALID");
    }
    void Distance(float distance) {
        const auto guide=Guide();Need(guide.valid&&guide.hasTarget,"FIXTURE_NO_TARGET");
        session.feet=guide.target-Aevrynd::Vec3{0,0,distance};
    }
};

void Empty(const PickupIntent& intent,const char* why) {
    Need(!intent.Armed()&&intent.Kind()==PickupKind::None&&intent.Index()==PickupIntent::NoIndex&&intent.Remaining()==0,why);
}
void RejectedArm(Fixture& fixture,ObjectiveGuide guide,const char* why) {
    PickupIntent intent;const auto before=fixture.session.Serialize();
    Need(!intent.Arm(fixture.session,guide),why);Empty(intent,"REJECTED_ARM_NOT_EMPTY");
    Need(fixture.session.Serialize()==before,"ARM_CHANGED_SESSION");++cases;
}

void FivePickups() {
    Fixture f;
    for(const auto kind:{PickupKind::Supply,PickupKind::Timber})for(unsigned i=0;i<3;++i){
        if(kind==PickupKind::Timber&&i==0)continue;
        f.Pickup(kind,i);const auto before=f.session.Serialize();PickupIntent intent;
        const auto guide=f.Guide();Need(guide.valid&&guide.index==i&&guide.bearing==ObjectiveBearing::Ahead,"REAL_GUIDE_ARM_FIXTURE");
        Need(intent.Arm(f.session,guide),"VALID_PICKUP_NOT_ARMED");
        Need(intent.Armed()&&intent.Kind()==kind&&intent.Index()==i&&intent.Remaining()==PickupIntent::Duration,"ARMED_NAMED_STATE");
        Need(f.session.Serialize()==before,"ARM_MUTATED_SESSION");
        Need(!intent.Advance(f.session,f.Guide(),.01),"REMOTE_PICKUP_SUCCEEDED");
        Need(intent.Armed()&&intent.Remaining()<PickupIntent::Duration&&f.session.Serialize()==before,"REMOTE_STEP_STATE");
        f.Distance(2.45f);auto expected=f.session;
        Need(kind==PickupKind::Supply?expected.Collect(i):expected.CollectRepairMaterial(i,false),"ORIGINAL_COLLECT_NOT_READY");
        Need(intent.Advance(f.session,f.Guide(),.01),"LEGAL_ENTRY_NOT_COLLECTED");
        Need(f.session.Serialize()==expected.Serialize(),"PICKUP_CHANGED_OTHER_SESSION_FIELDS");
        Empty(intent,"SUCCESS_NOT_CONSUMED");
        const auto after=f.session.Serialize();Need(!intent.Advance(f.session,f.Guide(),.01)&&f.session.Serialize()==after,"PICKUP_REPEATED");++cases;
    }
    ++groups;
}

void ArmRejections() {
    Fixture f;f.Pickup(PickupKind::Supply,0);
    RejectedArm(f,f.Guide(true),"BEHIND_ARMED");
    for(const float distance:{0.f,2.4f,2.5f,2.86f,20.f}){f.Pickup(PickupKind::Supply,0,distance);RejectedArm(f,f.Guide(),"OUTSIDE_APPROACH_RING_ARMED");}
    f.Pickup(PickupKind::Supply,0,2.84f);PickupIntent edge;Need(edge.Arm(f.session,f.Guide()),"INNER_UPPER_APPROACH_REJECTED");++cases;
    f.Pickup(PickupKind::Timber,0);RejectedArm(f,f.Guide(),"SHELTER_TOOL_ARMED");
    f.Pickup(PickupKind::Supply,0);f.session.feet.y=2.01f;RejectedArm(f,f.Guide(),"HEIGHT_GATE_ARMED");
    f.Pickup(PickupKind::Supply,0);f.session.health=0;RejectedArm(f,f.Guide(),"DEAD_ARMED");
    f.Pickup(PickupKind::Supply,0);f.session.collected=8;RejectedArm(f,f.Guide(),"INVALID_SESSION_ARMED");
    f.Pickup(PickupKind::Supply,0);const auto good=f.Guide();
    for(unsigned variant=0;variant<10;++variant){auto bad=good;
        switch(variant){
        case 0:bad.valid=false;break;case 1:bad.hasTarget=false;break;case 2:bad.index=3;break;
        case 3:bad.index=(std::numeric_limits<unsigned>::max)();break;
        case 4:bad.target.x+=1;break;case 5:bad.distance+=.01;break;
        case 6:bad.relativeRadians=(std::numeric_limits<double>::quiet_NaN)();break;
        case 7:bad.distance=(std::numeric_limits<double>::infinity)();break;
        case 8:bad.relativeRadians=1;break;case 9:bad.relayTrackingActive=true;break;
        }
        RejectedArm(f,bad,"MALFORMED_GUIDE_ARMED");
    }
    f.session={};f.session.collected=7;f.session.repairMaterials=7;
    Need(f.session.CurrentStage()==Stage::Bridge,"BRIDGE_FIXTURE_STAGE");RejectedArm(f,f.Guide(),"DEVICE_STAGE_ARMED");
    ++groups;
}

void ExpiryAndZeroTime() {
    Fixture f;f.Pickup(PickupKind::Supply,0);PickupIntent intent;
    Need(intent.Arm(f.session,f.Guide()),"EXPIRY_ARM");const auto remaining=intent.Remaining();const auto before=f.session.Serialize();
    Need(!intent.Advance(f.session,f.Guide(),0)&&intent.Remaining()==remaining&&f.session.Serialize()==before,"ZERO_TIME_CHANGED_STATE");++cases;
    Need(!intent.Advance(f.session,f.Guide(),.06)&&intent.Armed(),"EARLY_EXPIRED");
    f.Distance(2.4f);const auto entry=f.session.Serialize();
    Need(!intent.Advance(f.session,f.Guide(),.06),"EXPIRY_STEP_COLLECTED");
    Empty(intent,"EXPIRED_INTENT_RETAINED");Need(f.session.Serialize()==entry,"EXPIRY_CHANGED_SESSION");++cases;
    f.Pickup(PickupKind::Timber,1);Need(intent.Arm(f.session,f.Guide()),"AT_TARGET_ARM");
    f.Distance(0);Need(f.Guide().bearing==ObjectiveBearing::AtTarget,"REAL_AT_TARGET_GUIDE");
    Need(intent.Advance(f.session,f.Guide(),.01),"AT_TARGET_NOT_COLLECTED");Empty(intent,"AT_TARGET_NOT_CLEARED");++cases;
    f.Pickup(PickupKind::Supply,0);Need(intent.Arm(f.session,f.Guide()),"CLEAR_ARM");intent.Clear();intent.Clear();Empty(intent,"CLEAR_NOT_IDEMPOTENT");++cases;
    ++groups;
}

void IdentityAndHeading() {
    Fixture f;PickupIntent intent;
    for(unsigned variant=0;variant<5;++variant){
        f.Pickup(PickupKind::Supply,0);if(variant==0)f.session.collected=0;
        Need(intent.Arm(f.session,f.Guide()),"IDENTITY_ARM");
        if(variant==0)f.session.feet=Session::supplies[1]-Aevrynd::Vec3{0,0,2.4f};
        if(variant==1)f.session.collected=7;
        if(variant==2)f.session.health=0;
        if(variant==3)f.session.feet.y=2.01f;
        if(variant==4){f.session.collected=7;f.session.repairMaterials=7;f.session.bridgeRepaired=true;}
        const auto before=f.session.Serialize();
        Need(!intent.Advance(f.session,f.Guide(),.01),"CHANGED_TARGET_OR_STAGE_COLLECTED");
        Empty(intent,"CHANGED_TARGET_OR_STAGE_RETAINED");Need(f.session.Serialize()==before,"CANCEL_CHANGED_SESSION");++cases;
    }
    f.Pickup(PickupKind::Supply,0);Need(intent.Arm(f.session,f.Guide()),"TURN_ARM");
    const auto before=f.session.Serialize();Need(!intent.Advance(f.session,f.Guide(true),.01),"TURNED_AWAY_COLLECTED");
    Empty(intent,"TURNED_AWAY_RETAINED");Need(f.session.Serialize()==before,"TURN_CANCEL_CHANGED_SESSION");++cases;
    f.Pickup(PickupKind::Supply,0);Need(intent.Arm(f.session,f.Guide()),"REARM_ARM");
    Need(!intent.Arm(f.session,f.Guide(true)),"BAD_NEW_EDGE_ACCEPTED");Empty(intent,"BAD_NEW_EDGE_KEPT_OLD_INTENT");++cases;
    ++groups;
}

void InvalidAdvance() {
    Fixture f;PickupIntent intent;
    const std::array<double,4> invalid{-.01,.1001,(std::numeric_limits<double>::infinity)(),(std::numeric_limits<double>::quiet_NaN)()};
    for(const double dt:invalid){f.Pickup(PickupKind::Supply,0);Need(intent.Arm(f.session,f.Guide()),"INVALID_DT_ARM");
        f.Distance(2.4f);const auto before=f.session.Serialize();Need(!intent.Advance(f.session,f.Guide(),dt),"INVALID_DT_COLLECTED");
        Empty(intent,"INVALID_DT_RETAINED");Need(f.session.Serialize()==before,"INVALID_DT_CHANGED_SESSION");++cases;
    }
    for(unsigned variant=0;variant<4;++variant){f.Pickup(PickupKind::Supply,0);Need(intent.Arm(f.session,f.Guide()),"INVALID_GUIDE_ARM");
        f.Distance(2.4f);auto bad=f.Guide();
        if(variant==0)bad.target.y=(std::numeric_limits<float>::quiet_NaN)();
        if(variant==1)bad.index=(std::numeric_limits<unsigned>::max)();
        if(variant==2)bad.relativeRadians=4;
        if(variant==3)bad.distance=-1;
        const auto before=f.session.Serialize();Need(!intent.Advance(f.session,bad,.01),"INVALID_GUIDE_COLLECTED");
        Empty(intent,"INVALID_GUIDE_RETAINED");Need(f.session.Serialize()==before,"INVALID_GUIDE_CHANGED_SESSION");++cases;
    }
    ++groups;
}

void CandidateCopies() {
    Fixture f;f.Pickup(PickupKind::Supply,0);PickupIntent original;
    Need(original.Arm(f.session,f.Guide()),"COPY_ARM");const auto before=f.session.Serialize();
    auto rejectedIntent=original;auto rejectedSession=f.session;
    Need(!rejectedIntent.Advance(rejectedSession,f.Guide(),.01),"COPY_REMOTE_COLLECTED");
    Need(original.Remaining()==PickupIntent::Duration&&original.Armed()&&f.session.Serialize()==before,"DISCARDED_CANDIDATE_LEAKED_TIMER");++cases;
    f.Distance(2.4f);auto acceptedIntent=original;auto acceptedSession=f.session;
    Need(acceptedIntent.Advance(acceptedSession,f.Guide(),.01),"COPY_ENTRY_NOT_COLLECTED");
    Need(original.Armed()&&original.Index()==0&&original.Remaining()==PickupIntent::Duration&&f.session.collected==6,"DISCARDED_CANDIDATE_LEAKED_PICKUP");
    original=acceptedIntent;f.session=acceptedSession;Empty(original,"COPY_COMMIT_RETAINED_INTENT");
    Need(f.session.collected==7,"COPY_COMMIT_LOST_PICKUP");++cases;
    // 此處只示範候選複製；不冒稱已驗證真 Main 的暫停、存讀檔或回滾接線。
    ++groups;
}
}

int main(){try{
    FivePickups();ArmRejections();ExpiryAndZeroTime();IdentityAndHeading();InvalidAdvance();CandidateCopies();
    Need(groups==6,"GROUP_COUNT_CHANGED");
    std::cout<<"PICKUP_INTENT_PURE_CPU_PASS groups="<<groups<<" cases="<<cases<<" checks="<<checks
             <<" Main=false GPU=false\n";return 0;
}catch(const std::exception& error){std::cerr<<"PICKUP_INTENT_FIRST_DIVERGENCE reason="<<error.what()<<'\n';return 1;}}
