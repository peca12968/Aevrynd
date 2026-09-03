#include "Aevrynd/Renderer.h"
#include "Aevrynd/Camera.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

using namespace Aevrynd;

namespace {
bool Same(const PointLightGpu3D& a,const PointLightGpu3D& b){return std::memcmp(&a,&b,sizeof(a))==0;}

enum class SphereTileResult {Outside,ProjectedIntersection,ConservativeIntersection};
struct NdcRectangle {Vec2 minimum{},maximum{};};

SphereTileResult ConservativeSphereTileIntersection(const Matrix4& viewProjection,Vec3 center,float sourceRadius,NdcRectangle tile){
 const float radius=std::abs(sourceRadius);const Vec4 clip=viewProjection*Vec4{center.x,center.y,center.z,1.f};
 const Vec3 rowX{viewProjection[0][0],viewProjection[0][1],viewProjection[0][2]},rowY{viewProjection[1][0],viewProjection[1][1],viewProjection[1][2]},rowZ{viewProjection[2][0],viewProjection[2][1],viewProjection[2][2]},rowW{viewProjection[3][0],viewProjection[3][1],viewProjection[3][2]};
 const std::array<float,4> support{radius*Length(rowX),radius*Length(rowY),radius*Length(rowZ),radius*Length(rowW)};
 const bool finite=std::isfinite(clip.x)&&std::isfinite(clip.y)&&std::isfinite(clip.z)&&std::isfinite(clip.w)&&std::ranges::all_of(support,[](float value){return std::isfinite(value);});
 if(!finite)return SphereTileResult::ConservativeIntersection;
 const float epsilon=1e-5f*(1.f+std::abs(clip.z)+std::abs(clip.w)+support[2]+support[3]);
 if(clip.w+support[3]<-epsilon||clip.z+support[2]<-epsilon)return SphereTileResult::Outside;
 if(clip.w-clip.z+radius*Length(rowW-rowZ)<-epsilon)return SphereTileResult::Outside;
 if(clip.w-support[3]<=epsilon||clip.z-support[2]<=epsilon)return SphereTileResult::ConservativeIntersection;
 const float denominatorMinimum=clip.w-support[3],denominatorMaximum=clip.w+support[3];
 const std::array<float,4> projectedX{(clip.x-support[0])/denominatorMinimum,(clip.x-support[0])/denominatorMaximum,(clip.x+support[0])/denominatorMinimum,(clip.x+support[0])/denominatorMaximum};
 const std::array<float,4> projectedY{(clip.y-support[1])/denominatorMinimum,(clip.y-support[1])/denominatorMaximum,(clip.y+support[1])/denominatorMinimum,(clip.y+support[1])/denominatorMaximum};
 const auto [minimumX,maximumX]=std::ranges::minmax_element(projectedX);const auto [minimumY,maximumY]=std::ranges::minmax_element(projectedY);constexpr float padding=1e-5f;
 if(*maximumX+padding<tile.minimum.x||*minimumX-padding>tile.maximum.x||*maximumY+padding<tile.minimum.y||*minimumY-padding>tile.maximum.y)return SphereTileResult::Outside;
 return SphereTileResult::ProjectedIntersection;
}
}

int main(){
 static_assert(sizeof(PointLightGpu3D)==32&&sizeof(ForwardPlusTileGridEntry3D)==16);
 if(ForwardPlusTileCount3D(0)!=0||ForwardPlusTileCount3D(1)!=1||ForwardPlusTileCount3D(16)!=1||ForwardPlusTileCount3D(17)!=2)return 2;
 if(ForwardPlusTileCount3D(1279)!=80||ForwardPlusTileCount3D(719)!=45)return 3;
 if(ForwardPlusIndexCapacity3D(1279,719)!=std::size_t{80}*45*64)return 4;

 PointLight3D invalid{};invalid.position.x=std::numeric_limits<float>::quiet_NaN();
 PointLight3D a{{3.f,2.f,1.f},7.f,{.2f,.4f,.6f,1.f},2.f};
 PointLight3D b{{-3.f,4.f,5.f},9.f,{.9f,.7f,.5f,1.f},4.f};
 std::array<PointLight3D,4> first{a,invalid,b,a};
 std::array<PointLight3D,4> second{a,b,a,invalid};
 const auto canonicalA=CanonicalizePointLights3D(first),canonicalB=CanonicalizePointLights3D(second);
 if(canonicalA.count!=3||canonicalA.rejected!=1||canonicalA.fingerprint!=canonicalB.fingerprint)return 5;
 for(std::uint32_t index=0;index<canonicalA.count;++index)if(!Same(canonicalA.lights[index],canonicalB.lights[index]))return 6;

 std::vector<PointLight3D> excessive(MaximumForwardPlusPointLights3D+7,a);
 for(std::size_t index=0;index<excessive.size();++index)excessive[index].position.x=float(index);
 const auto capped=CanonicalizePointLights3D(excessive);
 if(capped.count!=MaximumForwardPlusPointLights3D||capped.rejected!=7)return 7;
 std::reverse(excessive.begin(),excessive.end());const auto reversed=CanonicalizePointLights3D(excessive);
 if(reversed.fingerprint!=capped.fingerprint)return 10;

 // tile 溢位只標記快速清單不足；fallback 的 canonical count 必須保留全部燈。
 ForwardPlusTileGridEntry3D overflow{0,MaximumForwardPlusLightsPerTile3D,1,0};
 const std::uint32_t shaderIterationCount=overflow.overflow?capped.count:overflow.count;
 if(shaderIterationCount!=256)return 8;

 DrawList list;list.AddPointLight(invalid);list.AddPointLight(a);
 for(std::size_t index=1;index<MaximumForwardPlusPointLights3D+4;++index)list.AddPointLight(a);
 if(list.PointLights().size()!=MaximumForwardPlusPointLights3D)return 9;
 DrawList forwardList,reverseList;std::vector<PointLight3D> submitted(300,a);for(std::size_t index=0;index<submitted.size();++index)submitted[index].position.x=float(index);for(const auto& light:submitted)forwardList.AddPointLight(light);for(auto iterator=submitted.rbegin();iterator!=submitted.rend();++iterator)reverseList.AddPointLight(*iterator);const auto forwardCanonical=CanonicalizePointLights3D(forwardList.PointLights()),reverseCanonical=CanonicalizePointLights3D(reverseList.PointLights());if(forwardCanonical.count!=256||forwardCanonical.fingerprint!=reverseCanonical.fingerprint)return 11;

 CameraProjection perspective{};perspective.nearPlane=.2f;perspective.farPlane=20.f;perspective.verticalFieldOfView=1.f;
 const Matrix4 perspectiveProjection=CreateProjectionMatrix(perspective,16.f/9.f);const NdcRectangle cornerTile{{.8f,.8f},{1.f,1.f}};
 // 包含相機與跨越近平面的球都沒有有界透視投影，必須保守送進每個 tile。
 if(ConservativeSphereTileIntersection(perspectiveProjection,{0,0,.1f},.5f,cornerTile)!=SphereTileResult::ConservativeIntersection)return 12;
 if(ConservativeSphereTileIntersection(perspectiveProjection,{0,0,.25f},.1f,cornerTile)!=SphereTileResult::ConservativeIntersection)return 13;
 // 只有在整個影響球都落於 eye plane 後方時才可剔除。
 if(ConservativeSphereTileIntersection(perspectiveProjection,{0,0,-2.f},.25f,{{-1,-1},{1,1}})!=SphereTileResult::Outside)return 14;

 Matrix4 asymmetricPerspective=perspectiveProjection;asymmetricPerspective[0][2]=.35f;asymmetricPerspective[1][2]=-.2f;
 if(ConservativeSphereTileIntersection(asymmetricPerspective,{0,0,5.f},.1f,{{.15f,-.4f},{.55f,0.f}})!=SphereTileResult::ProjectedIntersection)return 15;
 if(ConservativeSphereTileIntersection(asymmetricPerspective,{0,0,5.f},.1f,{{-.95f,-.4f},{-.75f,0.f}})!=SphereTileResult::Outside)return 16;

 CameraProjection orthographic{};orthographic.mode=ProjectionMode::Orthographic;orthographic.orthographicHeight=4.f;orthographic.nearPlane=.2f;orthographic.farPlane=20.f;const Matrix4 orthographicProjection=CreateProjectionMatrix(orthographic,1.f);
 if(ConservativeSphereTileIntersection(orthographicProjection,{1.5f,0,5.f},.2f,{{.6f,-.2f},{.9f,.2f}})!=SphereTileResult::ProjectedIntersection)return 17;
 if(ConservativeSphereTileIntersection(orthographicProjection,{1.5f,0,5.f},.2f,{{-.9f,-.2f},{-.6f,.2f}})!=SphereTileResult::Outside)return 18;
 std::cout<<"FORWARD_PLUS_CONTRACT_OK lights="<<capped.count<<" fingerprint="<<capped.fingerprint<<'\n';
 return 0;
}
