#include "GameplayLabGame.h"
#include <iostream>
using namespace Aevrynd;
using namespace GameplayLab;
#include "AuthorityGate.inc"
int main(){
 unsigned gateChecks{};
 for(unsigned flags=0;flags<32;++flags)for(bool valid:{false,true})for(bool layout:{false,true})for(auto hash:{0ull,7ull,9ull}){
 const bool expected=valid?(layout&&hash==7ull):flags==0;
 const bool actual=GameplayGodPoseAuthorityPass3D((flags&1)!=0,(flags&2)!=0,(flags&4)!=0,(flags&8)!=0,(flags&16)!=0,valid,layout,hash,7);
 if(actual!=expected){std::cerr<<"FAIL phase truth table flags="<<flags;return 4;}
 ++gateChecks;
 }
 struct Rules{bool female{};bool Female()const{return female;}}rules_;
 struct Clock{bool ready{};bool ReadyToStage()const{return ready;}}captureScenarioClock_;
 AnimationPose3D femalePose_,malePose_,monsterPose_,captureFrozenFemalePose_,captureFrozenMalePose_,captureFrozenMonsterPose_;
 for(auto* p:{&femalePose_,&malePose_,&monsterPose_}){p->globalMatrices.resize(1);p->globalMatrices[0].values[0][0]=1;}
 malePose_.globalMatrices[0].values[0][0]=2;
 captureFrozenFemalePose_=femalePose_;captureFrozenMalePose_=malePose_;captureFrozenMonsterPose_=monsterPose_;
 bool capturePoseFrozen_{},capturePairFrozen_{},captureTemporalWarmupActive_{};unsigned temporalFixtureWarmupFrames_{};
 const auto authority=[&](){
#include "AuthoritySelector.inc"
 };
 unsigned checks{},legacyMismatches{};
 for(unsigned flags=0;flags<32;++flags){
 capturePoseFrozen_=(flags&1)!=0;temporalFixtureWarmupFrames_=(flags&2)?8:0;capturePairFrozen_=(flags&4)!=0;captureTemporalWarmupActive_=(flags&8)!=0;captureScenarioClock_.ready=(flags&16)!=0;
 for(bool female:{false,true}){rules_.female=female;for(unsigned tick=0;tick<65;++tick){
 femalePose_.globalMatrices[0].values[0][0]=float(tick+3);malePose_.globalMatrices[0].values[0][0]=float(tick+4);
 const bool frozen=GameplayUseFrozenPose3D(capturePoseFrozen_,temporalFixtureWarmupFrames_!=0,capturePairFrozen_,captureTemporalWarmupActive_,captureScenarioClock_.ReadyToStage());
 const auto render=SelectGameplayPresentationPoses3D(frozen,femalePose_,malePose_,monsterPose_,captureFrozenFemalePose_,captureFrozenMalePose_,captureFrozenMonsterPose_);
 const auto expected=HashGameplayCapturePose3D(female?*render.female:*render.male);
 if(expected!=HashGameplayCapturePose3D(female?femalePose_:malePose_))++legacyMismatches;
 if(authority()!=expected){std::cerr<<"FAIL authority/render pose mismatch flags="<<flags<<" female="<<female<<" tick="<<tick<<"\n";return 1;}
 ++checks;
 }}}
 if(!legacyMismatches)return 2;
 // 固定更新發布後，準備、暖機、讀回及下一鏡解除凍結的切換不得改變本幀姿勢。
 for(bool female:{false,true}){
 rules_.female=female;capturePoseFrozen_=false;capturePairFrozen_=false;captureTemporalWarmupActive_=false;temporalFixtureWarmupFrames_=0;captureScenarioClock_.ready=false;
 captureFrozenFemalePose_=femalePose_;captureFrozenMalePose_=malePose_;
 const auto published=authority();
 for(unsigned stage=0;stage<7;++stage){
 captureScenarioClock_.ready=stage>=1&&stage<=3;capturePoseFrozen_=stage>=2&&stage<=5;
 captureTemporalWarmupActive_=stage==3;capturePairFrozen_=stage==4||stage==5;
 for(unsigned render=0;render<65;++render){
 const bool frozen=GameplayUseFrozenPose3D(capturePoseFrozen_,false,capturePairFrozen_,captureTemporalWarmupActive_,captureScenarioClock_.ReadyToStage());
 const auto selection=SelectGameplayPresentationPoses3D(frozen,femalePose_,malePose_,monsterPose_,captureFrozenFemalePose_,captureFrozenMalePose_,captureFrozenMonsterPose_);
 if(published!=HashGameplayCapturePose3D(female?*selection.female:*selection.male))return 3;
 }
 }
 }
 std::cout<<"PASS extracted production selector checks="<<checks<<" legacyLivePoseMismatches="<<legacyMismatches<<" phaseGateChecks="<<gateChecks<<" GPU=false\n";
}
