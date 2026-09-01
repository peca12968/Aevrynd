#ifndef AEVRYND_CHARACTER_BUILD_FINGERPRINT
#define AEVRYND_CHARACTER_BUILD_FINGERPRINT "gameplay-contact-evidence"
#endif
#include "GameplayLabGame.h"
#include <algorithm>
#include <array>
#include <cmath>

using namespace Aevrynd;

GameplayLab::GameplayGpuContactArtifact3D GameplayLab::BuildGameplayGpuContactArtifact3D(
    unsigned width,unsigned height,std::span<const std::uint32_t> objectIds,
    std::span<const float> depth,std::uint32_t playerToken,std::uint32_t terrainToken,
    const ReadbackRasterProvenance3D& provenance,std::uint64_t poseHash) noexcept {
  GameplayGpuContactArtifact3D result{};
  if(!width||!height||!playerToken||!terrainToken||playerToken==terrainToken||objectIds.size()!=std::size_t(width)*height||depth.size()!=objectIds.size()||!provenance.Complete()||!poseHash)return result;
  result.rgba.assign(objectIds.size()*4,0);for(std::size_t pixel=0;pixel<objectIds.size();++pixel)result.rgba[pixel*4+3]=255;
  unsigned minimumX=width,maximumX{},minimumY=height,maximumY{};
  for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)if(objectIds[std::size_t(y)*width+x]==playerToken){minimumX=std::min(minimumX,x);maximumX=std::max(maximumX,x);minimumY=std::min(minimumY,y);maximumY=std::max(maximumY,y);}
  if(minimumX>maximumX||minimumY>maximumY)return {};
  const unsigned middle=(minimumX+maximumX)/2;
  const auto paint=[&](unsigned x,unsigned y,std::array<std::uint8_t,3> color){const auto offset=(std::size_t(y)*width+x)*4;result.rgba[offset]=color[0];result.rgba[offset+1]=color[1];result.rgba[offset+2]=color[2];};
  const auto classifyFoot=[&](unsigned beginX,unsigned endX,bool left,std::uint32_t& gap,float& depthDelta){
    unsigned bottomY{};bool found{};for(unsigned y=minimumY;y<=maximumY;++y)for(unsigned x=beginX;x<=endX;++x)if(objectIds[std::size_t(y)*width+x]==playerToken&&(!found||y>bottomY)){bottomY=y;found=true;}if(!found)return false;
    const unsigned bandBegin=bottomY>3?bottomY-3:0;std::uint64_t xSum{};std::uint32_t samples{};
    for(unsigned y=bandBegin;y<=bottomY;++y)for(unsigned x=beginX;x<=endX;++x)if(objectIds[std::size_t(y)*width+x]==playerToken){if(!std::isfinite(depth[std::size_t(y)*width+x]))return false;paint(x,y,left?std::array<std::uint8_t,3>{255,48,48}:std::array<std::uint8_t,3>{48,96,255});++samples;xSum+=x;}
    if(!samples)return false;const unsigned meanX=unsigned(xSum/samples);unsigned anchorX=beginX,anchorDistance=width;for(unsigned x=beginX;x<=endX;++x)if(objectIds[std::size_t(bottomY)*width+x]==playerToken&&unsigned(std::abs(int(x)-int(meanX)))<anchorDistance){anchorX=x;anchorDistance=unsigned(std::abs(int(x)-int(meanX)));}if(anchorDistance==width)return false;gap=std::numeric_limits<std::uint32_t>::max();depthDelta=std::numeric_limits<float>::infinity();
    for(unsigned y=bottomY+1;y<std::min(height,bottomY+13);++y){for(unsigned x=anchorX>5?anchorX-5:0;x<=std::min(width-1,anchorX+5);++x){const auto pixel=std::size_t(y)*width+x;if(objectIds[pixel]!=terrainToken||!std::isfinite(depth[pixel]))continue;const float footDepth=depth[std::size_t(bottomY)*width+anchorX];if(!std::isfinite(footDepth))return false;const float delta=depth[pixel]-footDepth;if(!std::isfinite(delta)||delta<-.0001f||delta>.02f)continue;gap=std::min(gap,y-bottomY-1);depthDelta=std::min(depthDelta,delta);paint(x,y,{48,255,64});++result.groundSupportPixels;}}
    if(gap==std::numeric_limits<std::uint32_t>::max())return false;
    for(unsigned y=bottomY+1;y<std::min(height,bottomY+7);++y)for(unsigned x=anchorX>8?anchorX-8:0;x<=std::min(width-1,anchorX+8);++x){const auto pixel=std::size_t(y)*width+x;if(objectIds[pixel]!=terrainToken||!std::isfinite(depth[pixel]))continue;const auto offset=pixel*4;if(result.rgba[offset]||result.rgba[offset+1]||result.rgba[offset+2])continue;paint(x,y,{255,208,48});++result.contactShadowPixels;}
    if(left)result.leftFootPixels=samples;else result.rightFootPixels=samples;return true;
  };
  if(!classifyFoot(minimumX,middle,true,result.leftGapPixels,result.leftDepthDelta)||!classifyFoot(std::min(maximumX,middle+1),maximumX,false,result.rightGapPixels,result.rightDepthDelta))return {};
  result.frameSerial=provenance.frameSerial;result.poseHash=poseHash;if(!result.Complete(width,height))return {};return result;
}
