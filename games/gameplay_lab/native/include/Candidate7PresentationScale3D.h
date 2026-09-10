#pragma once
#include <Aevrynd/Mesh3D.h>
#include <array>
#include <cmath>
#include <span>

namespace GameplayLab {
struct CharacterBindBounds3D {Aevrynd::Vec3 minimum{},maximum{};};

// 這是公尺制人形資產輸入防護，不改控制器、相機或既有驗收門檻。
inline bool ValidCharacterBindBounds3D(const CharacterBindBounds3D& b) noexcept {
  const auto finite=[](Aevrynd::Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::abs(v.x)<=10.f&&std::abs(v.y)<=10.f&&std::abs(v.z)<=10.f;};
  const auto d=b.maximum-b.minimum;
  return finite(b.minimum)&&finite(b.maximum)&&d.x>1e-6f&&d.y>1e-6f&&d.z>1e-6f&&d.x<=4.f&&d.y<=4.f&&d.z<=4.f;
}

inline bool MeasureCharacterBindBounds3D(std::span<const Aevrynd::Vertex3D> vertices,
                                       CharacterBindBounds3D& output) noexcept {
  output={};if(vertices.empty())return false;
  CharacterBindBounds3D b{{vertices.front().x,vertices.front().y,vertices.front().z},{vertices.front().x,vertices.front().y,vertices.front().z}};
  for(const auto& v:vertices){
    if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))return false;
    b.minimum.x=(std::min)(b.minimum.x,v.x);b.minimum.y=(std::min)(b.minimum.y,v.y);b.minimum.z=(std::min)(b.minimum.z,v.z);
    b.maximum.x=(std::max)(b.maximum.x,v.x);b.maximum.y=(std::max)(b.maximum.y,v.y);b.maximum.z=(std::max)(b.maximum.z,v.z);
  }
  if(!ValidCharacterBindBounds3D(b))return false;output=b;return true;
}

struct CharacterPresentationScale3D {
  CharacterBindBounds3D bindBounds{};
  float bindHeight{},standingHeight{},uniformScale{};
  // union 最低點對齊 feet；不改骨架、inverse bind 或各件局部座標。
  float modelOriginWorldYOffset{};
  [[nodiscard]] bool Valid() const noexcept {
    return ValidCharacterBindBounds3D(bindBounds)&&std::isfinite(bindHeight)&&bindHeight>=.25f&&bindHeight<=4.f&&
        std::isfinite(standingHeight)&&standingHeight>=1.f&&standingHeight<=2.5f&&
        std::isfinite(uniformScale)&&uniformScale>=.25f&&uniformScale<=4.f&&
        std::isfinite(modelOriginWorldYOffset)&&bindHeight==bindBounds.maximum.y-bindBounds.minimum.y&&
        uniformScale==standingHeight/bindHeight&&modelOriginWorldYOffset==-bindBounds.minimum.y*uniformScale;
  }
  [[nodiscard]] Aevrynd::Vec3 ScaleVector() const noexcept {return {uniformScale,uniformScale,uniformScale};}
};

inline bool BuildCharacterPresentationScale3D(const CharacterBindBounds3D& bounds,
    float standingHeight,CharacterPresentationScale3D& output) noexcept {
  output={};if(!ValidCharacterBindBounds3D(bounds))return false;
  CharacterPresentationScale3D value;value.bindBounds=bounds;value.bindHeight=bounds.maximum.y-bounds.minimum.y;
  value.standingHeight=standingHeight;
  if(value.bindHeight<.25f||!std::isfinite(standingHeight))return false;
  value.uniformScale=standingHeight/value.bindHeight;
  value.modelOriginWorldYOffset=-bounds.minimum.y*value.uniformScale;
  if(!value.Valid())return false;output=value;return true;
}

// 四件共同組成實際外觀外框；不能把 upper/hair 各自拉到站高，否則衣物與骨架會分離。
inline bool BuildCandidate7PresentationScale3D(const std::array<CharacterBindBounds3D,4>& components,
    float standingHeight,CharacterPresentationScale3D& output) noexcept {
  output={};CharacterBindBounds3D combined=components[0];
  for(const auto& b:components){
    if(!ValidCharacterBindBounds3D(b))return false;
    combined.minimum.x=(std::min)(combined.minimum.x,b.minimum.x);combined.minimum.y=(std::min)(combined.minimum.y,b.minimum.y);combined.minimum.z=(std::min)(combined.minimum.z,b.minimum.z);
    combined.maximum.x=(std::max)(combined.maximum.x,b.maximum.x);combined.maximum.y=(std::max)(combined.maximum.y,b.maximum.y);combined.maximum.z=(std::max)(combined.maximum.z,b.maximum.z);
  }
  return BuildCharacterPresentationScale3D(combined,standingHeight,output);
}

inline bool BuildCharacterPresentationWorld3D(const CharacterPresentationScale3D& presentation,
    Aevrynd::Vec3 feet,float yaw,Aevrynd::Matrix4& world) noexcept {
  world={};if(!presentation.Valid()||!std::isfinite(feet.x)||!std::isfinite(feet.y)||!std::isfinite(feet.z)||!std::isfinite(yaw))return false;
  feet.y+=presentation.modelOriginWorldYOffset;
  world=Aevrynd::CreateTransformMatrix(feet,{0,yaw,0},presentation.ScaleVector());
  for(const auto& row:world.values)for(float v:row)if(!std::isfinite(v)){world={};return false;}
  return true;
}

inline bool BuildCandidate7PresentationWorlds3D(const CharacterPresentationScale3D& presentation,
    Aevrynd::Vec3 feet,float yaw,std::array<Aevrynd::Matrix4,4>& worlds) noexcept {
  worlds={};Aevrynd::Matrix4 common;
  if(!BuildCharacterPresentationWorld3D(presentation,feet,yaw,common))return false;
  worlds.fill(common);return true;
}

// 足部求解必須配對此逆矩陣，不能繼續沿用只反轉 yaw/translation 的舊公式。
inline bool BuildCharacterPresentationWorldInverse3D(const CharacterPresentationScale3D& presentation,
    Aevrynd::Vec3 feet,float yaw,Aevrynd::Matrix4& inverse) noexcept {
  inverse={};Aevrynd::Matrix4 world;
  if(!BuildCharacterPresentationWorld3D(presentation,feet,yaw,world))return false;
  feet.y+=presentation.modelOriginWorldYOffset;
  const float reciprocal=1.f/presentation.uniformScale;
  inverse=Aevrynd::CreateScaleMatrix({reciprocal,reciprocal,reciprocal})*
          Aevrynd::CreateRotationMatrix({0,-yaw,0})*Aevrynd::CreateTranslationMatrix(feet*-1.f);
  for(const auto& row:inverse.values)for(float v:row)if(!std::isfinite(v)){inverse={};return false;}
  return true;
}
} // namespace GameplayLab
