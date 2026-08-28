#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace Aevrynd::Tests {
struct HzbScreenBounds3D {int left{},top{},right{},bottom{};float nearestDepth{};uint64_t identity{};bool nearPlaneIntersection{};};
struct HzbOcclusionSettings3D {bool enabled{true};uint32_t minimumCandidates{2048};uint32_t minimumOccluderPixels{4096};float depthBias{.0005f};};
struct HzbOcclusionStats3D {uint32_t candidates{},occluded{},failOpen{},mipCount{};uint64_t identityHash{};bool resourcesResident{},historyValid{};};

class CurrentFrameHzbSoftwareReference3D {
public:
 CurrentFrameHzbSoftwareReference3D(uint32_t width,uint32_t height):width_(width),height_(height),depth_(size_t(width)*height,1.f){}
 void Clear(){std::ranges::fill(depth_,1.f);}
 void Rasterize(const HzbScreenBounds3D& bounds,float depth){
  // CPU 金標準使用保守矩形；未覆蓋像素保留遠平面，禁止部分覆蓋誤剔除。
  const int left=(std::max)(0,bounds.left),top=(std::max)(0,bounds.top),right=(std::min)(int(width_),bounds.right),bottom=(std::min)(int(height_),bounds.bottom);
  if(!std::isfinite(depth)||depth<0.f||depth>1.f)return;
  for(int y=top;y<bottom;++y)for(int x=left;x<right;++x)depth_[size_t(y)*width_+size_t(x)]=(std::min)(depth_[size_t(y)*width_+size_t(x)],depth);
 }
 bool GoldOccluded(const HzbScreenBounds3D& candidate,float bias=.0005f)const{return Test(candidate,bias,false);}
 bool HzbOccluded(const HzbScreenBounds3D& candidate,float bias=.0005f)const{return Test(candidate,bias,true);}
 uint32_t Width()const{return width_;}uint32_t Height()const{return height_;}
private:
 bool Test(const HzbScreenBounds3D& c,float bias,bool conservative)const{
  if(c.nearPlaneIntersection||!std::isfinite(c.nearestDepth)||c.nearestDepth<0.f||c.nearestDepth>1.f||c.left<0||c.top<0||c.right>int(width_)||c.bottom>int(height_)||c.left>=c.right||c.top>=c.bottom)return false;
  float maximum=0.f;
  for(int y=c.top;y<c.bottom;++y)for(int x=c.left;x<c.right;++x)maximum=(std::max)(maximum,depth_[size_t(y)*width_+size_t(x)]);
  // HZB 的 max reduction 等價於要求整個投影範圍皆有更近深度。
  if(conservative){const int area=(c.right-c.left)*(c.bottom-c.top);bias+=(area<=4?.001f:0.f);}
  return c.nearestDepth>maximum+bias;
 }
 uint32_t width_{},height_{};std::vector<float> depth_;
};

inline uint64_t FoldHzbIdentity3D(uint64_t hash,uint64_t identity){hash^=identity;return hash*1099511628211ull;}
inline bool HzbHistoryMustFailOpen3D(bool enabled,bool pure2D,bool resized,bool trimmed,bool cameraTeleport,bool msaaDepthResolved){return !enabled||pure2D||resized||trimmed||cameraTeleport||!msaaDepthResolved;}
inline size_t HzbHalfResolutionResidentBytes3D(uint32_t width,uint32_t height,uint32_t logicalMipCount,uint32_t slots){size_t pixels=size_t(width)*height;for(uint32_t logicalMip=1;logicalMip<logicalMipCount;++logicalMip)pixels+=size_t((std::max)(1u,width>>logicalMip))*(std::max)(1u,height>>logicalMip);return pixels*sizeof(float)*slots;}
inline bool HzbHalfResolutionPhysicalMapping3D(uint32_t width,uint32_t height,uint32_t logicalMipCount){if(!width||!height||logicalMipCount<2)return false;const uint32_t physicalWidth=(std::max)(1u,width/2),physicalHeight=(std::max)(1u,height/2);for(uint32_t logicalMip=1;logicalMip<logicalMipCount;++logicalMip){const uint32_t physicalMip=logicalMip-1;if((std::max)(1u,physicalWidth>>physicalMip)!=(std::max)(1u,width>>logicalMip)||(std::max)(1u,physicalHeight>>physicalMip)!=(std::max)(1u,height>>logicalMip))return false;}return true;}
inline bool HzbFourMipBatchMatchesSequential3D(uint32_t width,uint32_t height){
 if(!width||!height)return false;std::vector<std::vector<float>> sequential,batch;std::vector<std::pair<uint32_t,uint32_t>> sizes;std::vector<float> base(size_t(width)*height);for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x)base[size_t(y)*width+x]=float((x*17+y*31)%101)/100.f;sequential.push_back(base);sizes.push_back({width,height});
 while(width>1||height>1){const auto [sourceWidth,sourceHeight]=sizes.back();width=(std::max)(1u,sourceWidth/2);height=(std::max)(1u,sourceHeight/2);std::vector<float> output(size_t(width)*height);for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x){float maximum=0;for(uint32_t sy=0;sy<2;++sy)for(uint32_t sx=0;sx<2;++sx)maximum=(std::max)(maximum,sequential.back()[size_t((std::min)(sourceHeight-1,y*2+sy))*sourceWidth+(std::min)(sourceWidth-1,x*2+sx)]);output[size_t(y)*width+x]=maximum;}sequential.push_back(std::move(output));sizes.push_back({width,height});}
 batch.push_back(sequential.front());for(size_t first=1;first<sequential.size();first+=4){const size_t sourceMip=first-1;const auto [sourceWidth,sourceHeight]=sizes[sourceMip];for(size_t outputIndex=0;outputIndex<4&&first+outputIndex<sequential.size();++outputIndex){const uint32_t shift=uint32_t(outputIndex+1),block=1u<<shift,destinationWidth=(std::max)(1u,sourceWidth>>shift),destinationHeight=(std::max)(1u,sourceHeight>>shift);std::vector<float> output(size_t(destinationWidth)*destinationHeight);for(uint32_t y=0;y<destinationHeight;++y)for(uint32_t x=0;x<destinationWidth;++x){float maximum=0;for(uint32_t sy=0;sy<block;++sy)for(uint32_t sx=0;sx<block;++sx)maximum=(std::max)(maximum,batch[sourceMip][size_t((std::min)(sourceHeight-1,y*block+sy))*sourceWidth+(std::min)(sourceWidth-1,x*block+sx)]);output[size_t(y)*destinationWidth+x]=maximum;}batch.push_back(std::move(output));}}
 return batch==sequential;
}
}
