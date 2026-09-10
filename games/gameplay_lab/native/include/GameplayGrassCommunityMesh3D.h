#pragma once
#include <Aevrynd/EnvironmentMeshes3D.h>
#include <array>
#include <cmath>
#include <limits>

namespace GameplayLab {
// 正式草地共用群落網格；不改 placement、身份或遊戲碰撞。
inline Aevrynd::Mesh3D CreateGameplayGrassCommunityBaseline3D(unsigned blades,unsigned segments){
  using namespace Aevrynd;
  const auto tuft=CreateGrassClumpMesh3D(blades,segments);
  constexpr std::array<Vec3,9> offsets{{
    {-.08f,0,.04f},{-1.12f,0,-.24f},{.86f,0,.34f},{-.34f,0,1.18f},
    {.28f,0,-1.26f},{1.42f,0,-.62f},{-1.38f,0,.72f},{.78f,0,1.36f},{-.72f,0,-1.48f}}};
  constexpr std::array<float,9> rotations{{.18f,1.06f,2.31f,3.54f,4.62f,5.71f,.72f,2.82f,4.08f}};
  Mesh3D result;result.vertices.reserve(tuft.vertices.size()*offsets.size());result.indices.reserve(tuft.indices.size()*offsets.size());
  for(std::size_t patch=0;patch<offsets.size();++patch){
    const auto base=std::uint32_t(result.vertices.size());const float scale=.56f+.027f*float((patch*5)%7);
    const float sine=std::sin(rotations[patch]),cosine=std::cos(rotations[patch]);
    for(auto vertex:tuft.vertices){
      const float localX=vertex.x*scale,localZ=vertex.z*scale;
      vertex.x=localX*cosine-localZ*sine+offsets[patch].x;
      vertex.y*=.78f+.027f*float((patch*3)%6);
      vertex.z=localX*sine+localZ*cosine+offsets[patch].z;
      const auto packed=UnpackNormalSnorm8(vertex.normal);
      vertex.normal=PackNormalSnorm8({packed.x*cosine-packed.z*sine,packed.y,packed.x*sine+packed.z*cosine},packed.w);
      result.vertices.push_back(vertex);
    }
    for(const auto index:tuft.indices)result.indices.push_back(base+index);
  }
  return result;
}
inline Aevrynd::Mesh3D CreateGameplayGrassCommunityMesh3D(unsigned blades,unsigned segments){
  using namespace Aevrynd;
  auto result=CreateGameplayGrassCommunityBaseline3D(blades,segments);
  // 保留各方向極值所在整片葉，讓兩級 mesh 的既有 bounds 與葉高逐位元不變。
  // 其餘葉片只平移根部：不放大葉片、不增加三角形，也不改風權重與法線。
  const unsigned stride=(std::clamp)(segments,2u,5u)*2u+2u;
  const std::size_t count=result.vertices.size()/stride;
  const auto bounds=result.Bounds();
  std::vector<bool> fixed(count,false);
  Vec3 minimum{result.vertices.front().x,result.vertices.front().y,result.vertices.front().z},maximum=minimum;
  for(std::size_t i=0;i<result.vertices.size();++i){
    const auto& vertex=result.vertices[i];
    minimum.x=(std::min)(minimum.x,vertex.x);minimum.z=(std::min)(minimum.z,vertex.z);
    maximum.x=(std::max)(maximum.x,vertex.x);maximum.z=(std::max)(maximum.z,vertex.z);
    // Bounds 的 radius 是最遠頂點，不是 AABB 對角線；必須另保留球面極值葉。
    if(Length(Vec3{vertex.x,vertex.y,vertex.z}-bounds.center)==bounds.radius)fixed[i/stride]=true;
  }
  for(unsigned axis=0;axis<2;++axis)for(bool findMaximum:{false,true}){
    std::size_t extreme{};
    for(std::size_t i=1;i<result.vertices.size();++i){
      const float a=axis?result.vertices[i].z:result.vertices[i].x;
      const float b=axis?result.vertices[extreme].z:result.vertices[extreme].x;
      if(findMaximum?a>b:a<b)extreme=i;
    }
    fixed[extreme/stride]=true;
  }
  // 固定葉是已佔用的覆蓋中心。候選仍為有限集合，但每格先做可重現抖動，
  // 再讓多數葉片聚成長度、方向與中心都不同的短群島；每第四葉回到全域補洞，
  // 避免共用 mesh 在每個 instance 重播橫跨整片的正弦弧帶。
  std::vector<Vec3> centers;centers.reserve(count);
  for(std::size_t blade=0;blade<count;++blade)if(fixed[blade]){
    const auto& a=result.vertices[blade*stride];const auto& b=result.vertices[blade*stride+1];
    centers.push_back({(a.x+b.x)*.5f,0,(a.z+b.z)*.5f});
  }
  const auto noise=[](unsigned x,unsigned z,unsigned salt){
    std::uint32_t value=x*0x9e3779b9u^z*0x85ebca6bu^salt*0xc2b2ae35u;
    value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;value^=value>>16;
    return float(value&0x00ffffffu)/float(0x01000000u);
  };
  constexpr std::array<Vec2,6> islandCenters{{
    {-.58f,-.46f},{-.08f,-.34f},{.53f,-.47f},
    {-.49f,.34f},{.08f,.48f},{.59f,.25f}}};
  constexpr std::array<float,6> islandAngles{{.24f,-.68f,.91f,-1.03f,.55f,-.19f}};
  constexpr std::array<Vec2,6> islandRadii{{
    {.34f,.105f},{.27f,.13f},{.39f,.095f},
    {.25f,.14f},{.35f,.11f},{.29f,.125f}}};
  for(std::size_t blade=0;blade<count;++blade){
    if(fixed[blade])continue;
    Vec3 target{};float best=-1.f;
    for(unsigned z=0;z<25;++z)for(unsigned x=0;x<25;++x){
      const unsigned salt=unsigned(count)*4u+unsigned(blade)*17u;
      const float u=((float(x)+.5f+(noise(x,z,salt)-.5f)*.78f)/25.f*2.f-1.f);
      const float v=((float(z)+.5f+(noise(z,x,salt+31u)-.5f)*.78f)/25.f*2.f-1.f);
      float islandDistance=(std::numeric_limits<float>::max)();
      for(std::size_t island=0;island<islandCenters.size();++island){
        const float sine=std::sin(islandAngles[island]),cosine=std::cos(islandAngles[island]);
        const float dx=u-islandCenters[island].x,dz=v-islandCenters[island].y;
        const float along=(dx*cosine+dz*sine)/islandRadii[island].x;
        const float across=(-dx*sine+dz*cosine)/islandRadii[island].y;
        islandDistance=(std::min)(islandDistance,std::sqrt(along*along+across*across));
      }
      if((blade&3u)!=3u&&islandDistance>1.f+.18f*noise(x,z,salt+73u))continue;
      const Vec3 candidate{bounds.center.x+u*bounds.extents.x*.82f,0,
                           bounds.center.z+v*bounds.extents.z*.82f};
      float distance=(std::numeric_limits<float>::max)();
      for(const auto center:centers){const auto d=candidate-center;distance=(std::min)(distance,d.x*d.x+d.z*d.z);}
      const float islandPreference=1.f-(std::min)(1.f,islandDistance);
      const float score=distance*(.9f+.1f*noise(x,z,salt+109u))+islandPreference*.0024f;
      if(score>best){best=score;target=candidate;}
    }
    const auto first=blade*stride;
    float minX=result.vertices[first].x,maxX=minX,minZ=result.vertices[first].z,maxZ=minZ;
    for(unsigned v=1;v<stride;++v){const auto& p=result.vertices[first+v];minX=(std::min)(minX,p.x);maxX=(std::max)(maxX,p.x);minZ=(std::min)(minZ,p.z);maxZ=(std::max)(maxZ,p.z);}
    const float rootX=(minX+maxX)*.5f;
    const float rootZ=(minZ+maxZ)*.5f;
    // 移動整片葉的投影中心，維持葉片彎曲與原有朝向。
    const float targetX=target.x;
    const float targetZ=target.z;
    float dx=(std::clamp)(targetX-rootX,minimum.x-minX,maximum.x-maxX);
    float dz=(std::clamp)(targetZ-rootZ,minimum.z-minZ,maximum.z-maxZ);
    // 以實際浮點加法後的頂點驗證 AABB 與球界；收縮整葉平移，不夾扁個別頂點。
    const auto containedAt=[&](float fraction){
      bool contained=true;
      for(unsigned vertex=0;vertex<stride;++vertex){const auto& p=result.vertices[first+vertex];const Vec3 moved{p.x+dx*fraction,p.y,p.z+dz*fraction};
        contained&=moved.x>=minimum.x&&moved.x<=maximum.x&&moved.z>=minimum.z&&moved.z<=maximum.z&&Length(moved-bounds.center)<=bounds.radius;
      }
      return contained;
    };
    if(!containedAt(1.f)){
      float safe=0.f,unsafe=1.f;
      for(unsigned attempt=0;attempt<24;++attempt){const float middle=(safe+unsafe)*.5f;if(containedAt(middle))safe=middle;else unsafe=middle;}
      dx*=safe;dz*=safe;
    }
    for(unsigned vertex=0;vertex<stride;++vertex){result.vertices[first+vertex].x+=dx;result.vertices[first+vertex].z+=dz;}
    centers.push_back({rootX+dx,0,rootZ+dz});
  }
  return result;
}
} // namespace GameplayLab
