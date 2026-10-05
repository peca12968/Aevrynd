#pragma once
#include "CollisionWorld3D.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <vector>

namespace Aevrynd::MeshCollisionGridSolid3D {
using Cell = std::array<std::int32_t,3>;
struct CellBox {Cell minimum{},maximum{};};
struct GridFill {
    std::vector<Cell> occupied;
    std::size_t surfaceCount{},interiorCount{},domainCount{},exteriorCount{};
};
// 這是保守碰撞格的封閉空間填充，不宣稱開放來源網格具有可靠的幾何體積。
inline GridFill FillEnclosed(std::span<const Cell> input,std::size_t maximumDomainCells) {
    GridFill result;
    if(input.empty())return result;
    if(input.size()>maximumDomainCells||maximumDomainCells==0)
        throw std::invalid_argument("grid_input_budget");
    Cell low=input.front(),high=low;
    std::set<Cell> surface;
    for(const auto& cell:input){
        surface.insert(cell);
        for(unsigned axis=0;axis<3;++axis){low[axis]=(std::min)(low[axis],cell[axis]);high[axis]=(std::max)(high[axis],cell[axis]);}
    }
    std::array<std::size_t,3> width{};
    std::size_t volume=1;
    for(unsigned axis=0;axis<3;++axis){
        if(low[axis]==(std::numeric_limits<std::int32_t>::min)()||high[axis]==(std::numeric_limits<std::int32_t>::max)())
            throw std::invalid_argument("grid_padding_overflow");
        --low[axis];++high[axis];
        const auto extent=std::uint64_t(std::int64_t(high[axis])-low[axis]+1);
        if(extent>maximumDomainCells||volume>maximumDomainCells/extent)throw std::invalid_argument("grid_domain_budget");
        width[axis]=std::size_t(extent);volume*=width[axis];
    }
    const auto index=[&](const Cell& c){
        return (std::size_t(std::int64_t(c[2])-low[2])*width[1]+std::size_t(std::int64_t(c[1])-low[1]))*width[0]+
            std::size_t(std::int64_t(c[0])-low[0]);
    };
    const auto coordinate=[&](std::size_t i){
        const auto x=i%width[0];i/=width[0];const auto y=i%width[1];i/=width[1];
        return Cell{std::int32_t(std::int64_t(low[0])+std::int64_t(x)),std::int32_t(std::int64_t(low[1])+std::int64_t(y)),std::int32_t(std::int64_t(low[2])+std::int64_t(i))};
    };
    std::vector<std::uint8_t> state(volume);
    for(const auto& c:surface)state[index(c)]=1;
    std::vector<std::size_t> queue;
    queue.reserve(volume);queue.push_back(0);state[0]=2;
    // 六鄰接：封閉格面不能被斜角路徑穿越；格網造成的額外封閉區仍須實測淨空。
    for(std::size_t cursor=0;cursor<queue.size();++cursor){
        const auto c=coordinate(queue[cursor]);
        for(unsigned axis=0;axis<3;++axis)for(int sign:{-1,1}){
            if((sign<0&&c[axis]==low[axis])||(sign>0&&c[axis]==high[axis]))continue;
            auto next=c;next[axis]+=sign;const auto i=index(next);
            if(state[i]==0){state[i]=2;queue.push_back(i);}
        }
    }
    result.surfaceCount=surface.size();result.domainCount=volume;result.exteriorCount=queue.size();
    for(std::size_t i=0;i<volume;++i)if(state[i]!=2){result.occupied.push_back(coordinate(i));result.interiorCount+=state[i]==0;}
    std::sort(result.occupied.begin(),result.occupied.end());return result;
}
// 只合併完整佔據的相鄰格，絕不把空氣間隙橋接成更大的碰撞盒。
inline std::vector<CellBox> PackExact(std::span<const Cell> input,std::size_t maximumBoxes,std::size_t maximumChecks=10000000) {
    std::set<Cell> remaining(input.begin(),input.end());
    if(remaining.size()!=input.size())throw std::invalid_argument("packing_duplicate_cells");
    std::vector<CellBox> result;
    std::size_t checks{};
    const auto checked=[&]{if(checks>=maximumChecks)throw std::invalid_argument("packing_check_budget");++checks;};
    while(!remaining.empty()){
        if(result.size()>=maximumBoxes)throw std::invalid_argument("packing_box_budget");
        const auto low=*remaining.begin();auto high=low;
        for(unsigned axis=0;axis<3;++axis){
            while(high[axis]!=(std::numeric_limits<std::int32_t>::max)()){
                const auto candidate=high[axis]+1;bool complete=true;
                Cell c=low;c[axis]=candidate;
                const unsigned a=(axis+1)%3,b=(axis+2)%3;
                for(std::int64_t first=low[a];first<=high[a]&&complete;++first)
                    for(std::int64_t second=low[b];second<=high[b];++second){
                        c[a]=std::int32_t(first);c[b]=std::int32_t(second);
                        checked();
                        if(!remaining.contains(c)){complete=false;break;}
                    }
                if(!complete)break;high[axis]=candidate;
            }
        }
        for(std::int64_t x=low[0];x<=high[0];++x)for(std::int64_t y=low[1];y<=high[1];++y)
            for(std::int64_t z=low[2];z<=high[2];++z){checked();remaining.erase(Cell{std::int32_t(x),std::int32_t(y),std::int32_t(z)});}
        result.push_back({low,high});
    }
    return result;
}
// 整數格界先向外轉成float，再形成實際碰撞消費端的center/extents。
inline BoxCollider3D ToCollider(CellBox box,double spacing) {
    if(!std::isfinite(spacing)||spacing<=0||spacing>1)throw std::invalid_argument("collider_grid_spacing");
    float center[3],extent[3];
    for(unsigned axis=0;axis<3;++axis){
        if(box.minimum[axis]>box.maximum[axis])throw std::invalid_argument("collider_grid_order");
        const double low=double(box.minimum[axis])*spacing,high=(double(box.maximum[axis])+1)*spacing;
        center[axis]=float((low+high)*.5);
        float lowFloat=float(low),highFloat=float(high);
        if(double(lowFloat)>low)lowFloat=std::nextafter(lowFloat,-std::numeric_limits<float>::infinity());
        if(double(highFloat)<high)highFloat=std::nextafter(highFloat,std::numeric_limits<float>::infinity());
        const double half=(std::max)(double(center[axis])-double(lowFloat),double(highFloat)-double(center[axis]));
        extent[axis]=std::nextafter(float(half),std::numeric_limits<float>::infinity());
        if(!std::isfinite(center[axis])||!std::isfinite(extent[axis])||extent[axis]<=0)
            throw std::invalid_argument("collider_grid_nonfinite_or_collapsed");
        if(double(center[axis]-extent[axis])>low||double(center[axis]+extent[axis])<high)
            throw std::invalid_argument("collider_grid_not_enclosed");
    }
    return {{center[0],center[1],center[2]},{extent[0],extent[1],extent[2]}};
}
}
