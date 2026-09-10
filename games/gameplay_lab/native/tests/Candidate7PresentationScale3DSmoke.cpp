#include "Candidate7PresentationScale3D.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <bit>
#include <iomanip>
#include <iostream>
#include <limits>
using namespace Aevrynd;
using namespace GameplayLab;
namespace {
bool Same(const Matrix4& a,const Matrix4& b){return std::bit_cast<std::array<std::uint32_t,16>>(a)==std::bit_cast<std::array<std::uint32_t,16>>(b);}
bool HeightExact(const CharacterPresentationScale3D& p){
 Matrix4 world;if(!BuildCharacterPresentationWorld3D(p,{3,5,-7},.73f,world))return false;
 const auto bottom=TransformPoint(world,{0,p.bindBounds.minimum.y,0}),top=TransformPoint(world,{0,p.bindBounds.maximum.y,0});
 return std::abs(bottom.y-5.f)<=1e-6f&&std::abs(top.y-bottom.y-p.standingHeight)<=1e-6f;
}
void Print(const char* name,const CharacterBindBounds3D& bounds,const CharacterPresentationScale3D& p){
 std::cout<<name<<" min="<<bounds.minimum.x<<','<<bounds.minimum.y<<','<<bounds.minimum.z
 <<" max="<<bounds.maximum.x<<','<<bounds.maximum.y<<','<<bounds.maximum.z
 <<" height="<<bounds.maximum.y-bounds.minimum.y<<" commonScale="<<p.uniformScale
 <<" originYOffset="<<p.modelOriginWorldYOffset<<" target="<<p.standingHeight<<'\n';
}
}
int main(int argc,char** argv){
 std::cout<<std::setprecision(12);
 CharacterPresentationScale3D p;
 const CharacterBindBounds3D valid{{-.4f,-.02f,-.2f},{.4f,1.48f,.2f}};
 if(!BuildCharacterPresentationScale3D(valid,1.8f,p)||!p.Valid()||!HeightExact(p))return 1;
 for(const float bad:{0.f,-1.f,1e-20f,1e20f,(std::numeric_limits<float>::infinity)(),(std::numeric_limits<float>::quiet_NaN)()}){
  auto invalid=valid;invalid.maximum.y=invalid.minimum.y+bad;
  if(BuildCharacterPresentationScale3D(invalid,1.8f,p)||p.Valid())return 2;
  if(BuildCharacterPresentationScale3D(valid,bad,p)||p.Valid())return 3;
 }
 auto invalid=valid;invalid.minimum.x=invalid.maximum.x;
 if(BuildCharacterPresentationScale3D(invalid,1.8f,p))return 4;
 invalid=valid;invalid.minimum.z=(std::numeric_limits<float>::quiet_NaN)();
 if(BuildCharacterPresentationScale3D(invalid,1.8f,p))return 5;
 invalid=valid;invalid.maximum.x=10000.f;
 if(BuildCharacterPresentationScale3D(invalid,1.8f,p))return 21;
 CharacterBindBounds3D emptyBounds;
 if(MeasureCharacterBindBounds3D({},emptyBounds))return 22;
 std::array<CharacterBindBounds3D,4> synthetic{valid,valid,valid,valid};synthetic[3].maximum.y=2.f;
 if(!BuildCandidate7PresentationScale3D(synthetic,1.8f,p)||!HeightExact(p)||p.bindHeight!=2.02f)return 6;
 auto poisoned=synthetic;poisoned[2].maximum.x=poisoned[2].minimum.x;
 if(BuildCandidate7PresentationScale3D(poisoned,1.8f,p))return 7;
 if(argc!=7){std::cerr<<"usage: test body upper lower hair legacyF2 legacyM2\n";return 8;}
 std::array<CharacterBindBounds3D,4> componentBounds{};
 std::array<SkinnedModelAssetDetailedResult,6> assets;
 for(unsigned i=0;i<6;++i){
  assets[i]=LoadSkinnedModelAssetFileDetailed(argv[i+1]);
  if(!assets[i]){std::cerr<<"LOAD_FAILED "<<argv[i+1]<<' '<<assets[i].error<<'\n';return 9;}
  CharacterBindBounds3D bounds;
  if(!MeasureCharacterBindBounds3D(assets[i].asset.model.bindMesh.vertices,bounds))return 10;
  if(i<4){componentBounds[i]=bounds;if(assets[i].asset.model.skeleton.joints.size()!=53)return 11;
    if(i&&assets[i].asset.model.skeletonCompatibilityId!=assets[0].asset.model.skeletonCompatibilityId)return 12;
  }else{
    CharacterPresentationScale3D measured;if(!BuildCharacterPresentationScale3D(bounds,1.8f,measured)||!HeightExact(measured))return 13;
    Print(i==4?"LEGACY_F2":"LEGACY_M2",bounds,measured);
  }
 }
 if(!BuildCandidate7PresentationScale3D(componentBounds,1.8f,p)||!HeightExact(p))return 14;
 Print("CANDIDATE7_UNION",p.bindBounds,p);
 std::cout<<"UNIT_SCALE_HEIGHT_ERROR="<<p.standingHeight-p.bindHeight<<'\n';
 // 各件單獨正規化是反例；服装短於全身，不能與 body 各自套站高。
 CharacterPresentationScale3D upperOnly;
 if(!BuildCharacterPresentationScale3D(componentBounds[1],1.8f,upperOnly)||upperOnly.uniformScale==p.uniformScale)return 23;
 constexpr std::array<const char*,4> names{"BODY","UPPER","LOWER","HAIR"};
 std::array<Matrix4,4> worlds;if(!BuildCandidate7PresentationWorlds3D(p,{3,5,-7},.73f,worlds))return 15;
 for(unsigned i=0;i<4;++i){
  Print(names[i],componentBounds[i],p);if(!Same(worlds[0],worlds[i]))return 16;
  for(const auto& vertex:assets[i].asset.model.bindMesh.vertices){const auto world=TransformPoint(worlds[i],{vertex.x,vertex.y,vertex.z});if(world.y<5.f-1e-6f||world.y>6.8f+1e-6f)return 17;}
 }
 // 武器 socket／足部／相機與 receipt 取得同一 world，不再另算第二個比例。
 Matrix4 consumer;if(!BuildCharacterPresentationWorld3D(p,{3,5,-7},.73f,consumer)||!Same(worlds[0],consumer))return 18;
 Matrix4 inverse;if(!BuildCharacterPresentationWorldInverse3D(p,{3,5,-7},.73f,inverse))return 25;
 for(const auto pair:{consumer*inverse,inverse*consumer})for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)
   if(std::abs(pair[row][column]-(row==column?1.f:0.f))>1e-5f)return 26;
 auto mismatched=worlds[0];mismatched[0][0]*=.5f;if(Same(worlds[0],mismatched))return 24;
 if(BuildCandidate7PresentationWorlds3D(p,{0,(std::numeric_limits<float>::infinity)(),0},0,worlds))return 19;
 p.uniformScale=0;if(BuildCharacterPresentationWorld3D(p,{0,0,0},0,consumer))return 20;
 std::cout<<"CANDIDATE7_PRESENTATION_SCALE=PASS components=4 sharedWorld=1 joints=53 gpu=not-run integration=not-applied\n";
 return 0;
}
