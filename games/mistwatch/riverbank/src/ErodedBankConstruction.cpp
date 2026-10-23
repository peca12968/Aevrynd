#include "MistwatchErodedBankConstruction.h"
namespace Mistwatch::ErodedBank {
Result Build(const Aevrynd::HeightField3D& original,const Aevrynd::HeightField3D& coherent,
                    std::span<const RiverbankConstruction::Guard> guards) {
    const auto& s=coherent.Settings();
    if(s.width!=201||s.depth!=241||s.cellSize!=.5f||s.origin.x!=-50||s.origin.z!=-20||
       original.Settings().width!=s.width||original.Settings().depth!=s.depth||guards.size()<100)
        throw std::invalid_argument("BANK_SLOPE_REQUIRES_COMPLETE_CURRENT_SOURCE");
    for(const auto& g:guards)
        if(!std::isfinite(g.x)||!std::isfinite(g.z)||!std::isfinite(g.ex)||!std::isfinite(g.ez)||g.ex<=0||g.ez<=0)
            throw std::invalid_argument("BANK_SLOPE_INVALID_PROTECTED_DOMAIN");
    Result out{coherent};
    for(unsigned iz=0;iz<s.depth;++iz)for(unsigned ix=0;ix<s.width;++ix) {
        const auto p=coherent.SamplePosition(ix,iz);float weight=Domain(p.x,p.z);
        if(weight==0||p.y<=FootHeight)continue;
        float protection=1.f;
        for(const auto& g:guards){
            const float d=(std::max)({0.f,std::abs(p.x-g.x)-g.ex,std::abs(p.z-g.z)-g.ez});
            protection=(std::min)(protection,RiverbankConstruction::Smooth(d/1.25f));
            if(protection==0)break;
        }
        if(protection==0){++out.protectedSamples;continue;}
        weight*=protection;
        const float center=RiverCenterZ(p.x),side=p.z>=center?1.f:-1.f,r=std::abs(p.z-center);
        const float spread=side<0?2.2f+.35f*std::sin(p.x*.13f):1.8f+.35f*std::cos(p.x*.11f);
        // The failed first construction anchored at-3.5, moving most of its
        // widening underwater. This construction fixes the ACTUAL waterline
        // foot; only dry upper-bank points move outward. No underwater sample
        // is lowered and no width/roughness/color parameter sweep is used.
        const auto mapped=[&](float radius){
            const float z=center+side*radius;
            if(!coherent.Contains(p.x,z))throw std::invalid_argument("BANK_SLOPE_SAMPLE_OUTSIDE_FIELD");
            const float h=coherent.SampleHeight(p.x,z);
            return radius+spread*RiverbankConstruction::Smooth((h-FootHeight)/(-FootHeight));
        };
        float low=(std::max)(0.f,r-spread),high=r;
        if(mapped(low)>r+.00001f||mapped(high)<r-.00001f)
            throw std::invalid_argument("BANK_SLOPE_CONSTRUCTION_NOT_BRACKETED");
        for(unsigned step=0;step<30;++step){const float mid=(low+high)*.5f;if(mapped(mid)<r)low=mid;else high=mid;}
        const float target=(std::min)(p.y,coherent.SampleHeight(p.x,center+side*(low+high)*.5f));
        const float value=std::lerp(p.y,target,weight);
        if(!std::isfinite(value)||value>p.y||std::abs(value-original.Height(ix,iz))>4.5f)
            throw std::invalid_argument("BANK_SLOPE_ORIGINAL_FINITE_4_5M_BOUND");
        if(std::bit_cast<unsigned>(value)==std::bit_cast<unsigned>(p.y))continue;
        if(!out.field.SetHeight(ix,iz,value))throw std::runtime_error("BANK_SLOPE_HEIGHTFIELD_EDIT_FAILED");
        ++out.changed;out.maximumLowering=(std::max)(out.maximumLowering,p.y-value);
    }
    out.field.RecalculateRange();
    if(out.changed<80||out.maximumLowering<.25f||out.field.MinimumHeight()!=coherent.MinimumHeight()||out.field.MaximumHeight()!=coherent.MaximumHeight())
        throw std::invalid_argument("BANK_SLOPE_INEFFECTIVE_OR_WORLD_EXTREMA_CHANGED");
    return out;
}
}
