#include "GameplayOccluderMask3D.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>

using namespace Aevrynd;
using namespace GameplayLab;

int main() {
  const Camera3D camera=MakeThirdPersonCamera({0,1,0},0,.1f,4.f);
  GameplayOccluderMask3D result{};
  const std::array boxes{
      GameplayOccluderBox3D{151,{0,1,1},{2,2,.4f},0},
      GameplayOccluderBox3D{152,{2.5f,1,2},{.5f,.5f,.5f},0}};
  if(!AnalyzeGameplayOccluderMask3D(camera,boxes,320,180,result)||
     result.stableLow!=151||!result.coveredPixels||result.coverage<=.01f||
     !result.maskHash||result.pixels.size()!=320u*180u)return 1;
  const auto repeated=result;
  if(!AnalyzeGameplayOccluderMask3D(camera,boxes,320,180,result)||
     result.maskHash!=repeated.maskHash||result.coveredPixels!=repeated.coveredPixels)return 2;
  // 穿過近裁切面的方盒仍必須產生保守像素mask，不能退化成角點漏算。
  const Vec3 forward=Normalize(camera.target-camera.position);
  const std::array clipped{GameplayOccluderBox3D{154,camera.position+forward*(camera.projection.nearPlane*1.25f),{.3f,.3f,.4f},0}};
  if(!AnalyzeGameplayOccluderMask3D(camera,clipped,320,180,result)||
     !result.nearPlaneClipped||result.coverage<.25f){std::cerr<<"near="<<result.nearPlaneClipped<<" coverage="<<result.coverage<<" pixels="<<result.coveredPixels<<'\n';return 3;}
  auto invalid=boxes;invalid[0].size.x=0;
  if(AnalyzeGameplayOccluderMask3D(camera,invalid,320,180,result))return 4;
  invalid=boxes;invalid[0].yaw=std::numeric_limits<float>::quiet_NaN();
  if(AnalyzeGameplayOccluderMask3D(camera,invalid,320,180,result))return 5;
  if(AnalyzeGameplayOccluderMask3D(camera,boxes,0,180,result))return 6;
  return 0;
}
