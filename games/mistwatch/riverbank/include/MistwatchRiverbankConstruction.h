#pragma once
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <span>
#include <stdexcept>
#include <vector>
namespace Mistwatch::RiverbankConstruction {
struct Guard {float x,z,ex,ez;};
struct Result {Aevrynd::HeightField3D field;std::vector<Guard> guards;unsigned changed{},protectedSamples{};float maximumChange{};};
inline float Smooth(float v){const auto t=std::clamp(v,0.f,1.f);return t*t*(3.f-2.f*t);}
inline float Domain(float x,float z){
    return Smooth((std::abs(x)-2.5f)/2.5f)*Smooth((18.f-std::abs(x))/3.f)*Smooth((z-27.f)/2.f)*Smooth((45.f-z)/2.f);
}
inline bool Relevant(const Guard& g){return std::abs(g.x)-g.ex<20.f&&g.z+g.ez>25.f&&g.z-g.ez<47.f;}
inline Result Build(const Aevrynd::HeightField3D& source,std::span<const Aevrynd::BoxCollider3D> solids,std::span<const Guard> roots){
    const auto& settings=source.Settings();
    if(settings.width!=201||settings.depth!=241||settings.cellSize!=.5f||settings.origin.x!=-50||settings.origin.z!=-20||solids.size()!=47014)
        throw std::invalid_argument("RIVERBANK_ACTUAL_COMPLETE_SOURCE_REQUIRED");
    Result out{source};
    for(const auto& g:roots){
        if(!std::isfinite(g.x)||!std::isfinite(g.z)||!std::isfinite(g.ex)||!std::isfinite(g.ez)||g.ex<=0||g.ez<=0)
            throw std::invalid_argument("RIVERBANK_INVALID_ACTUAL_ROOT_DOMAIN");
        if(Relevant(g))out.guards.push_back({g.x,g.z,g.ex+.75f,g.ez+.75f});
    }
    for(const auto& box:solids){
        const Guard g{box.center.x,box.center.z,box.extents.x,box.extents.z};
        if(!Relevant(g)||!source.Contains(g.x,g.z))continue;
        float low=source.SampleHeight(g.x,g.z),high=low;
        for(float dx:{-g.ex,0.f,g.ex})for(float dz:{-g.ez,0.f,g.ez}){
            if(!source.Contains(g.x+dx,g.z+dz))continue;
            const float y=source.SampleHeight(g.x+dx,g.z+dz);low=(std::min)(low,y);high=(std::max)(high,y);
        }
        if(box.center.y-box.extents.y<=high+.15f&&box.center.y+box.extents.y>=low-.15f)
            out.guards.push_back({g.x,g.z,g.ex+.75f,g.ez+.75f});
    }
    for(unsigned z=0;z<settings.depth;++z)for(unsigned x=0;x<settings.width;++x){
        const auto p=source.SamplePosition(x,z);float mask=Domain(p.x,p.z);if(mask==0)continue;
        float protection=1;
        for(const auto& g:out.guards){
            const auto distance=(std::max)({0.f,std::abs(p.x-g.x)-g.ex,std::abs(p.z-g.z)-g.ez});
            protection=(std::min)(protection,Smooth(distance/1.25f));if(protection==0)break;
        }
        if(protection==0){++out.protectedSamples;continue;}
        // A coherent bend along the channel, not per-vertex random height noise.
        const float shift=1.45f*std::sin(p.x*.37f)+.45f*std::sin(p.x*.81f);
        const float target=source.SampleHeight(p.x,p.z-shift);
        const float value=std::lerp(p.y,target,mask*protection);
        if(!std::isfinite(value)||std::abs(value-p.y)>4.5f)throw std::invalid_argument("RIVERBANK_FINITE_BOUNDED_CONSTRUCTION");
        if(std::bit_cast<unsigned>(value)==std::bit_cast<unsigned>(p.y))continue;
        if(!out.field.SetHeight(x,z,value))throw std::runtime_error("RIVERBANK_REAL_HEIGHTFIELD_EDIT_FAILED");
        ++out.changed;out.maximumChange=(std::max)(out.maximumChange,std::abs(value-p.y));
    }
    out.field.RecalculateRange();
    if(out.changed<200||out.maximumChange<.5f||out.field.MinimumHeight()!=source.MinimumHeight()||out.field.MaximumHeight()!=source.MaximumHeight())
        throw std::invalid_argument("RIVERBANK_INEFFECTIVE_OR_CHANGED_WORLD_EXTREMA");
    return out;
}
}
