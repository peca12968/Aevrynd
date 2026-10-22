#pragma once
#include "MistwatchWestCliffRelief.h"
#include "MistwatchValleyContact.h"

namespace Mistwatch::ValleyTerrain {
inline constexpr std::string_view Profile="south-valley-contact-supports-v7";
// 此候選另立世界身分；原世界指紋只識別輸入，不能當作衍生世界的驗收。
inline constexpr std::uint32_t StudyEpoch=0x564c5937u;
struct Result {
    Aevrynd::HeightField3D terrain;std::size_t changed{};float maximumRise{};unsigned solveIterations{};float solveResidual{};
    std::vector<unsigned> supportGroups;std::vector<float> supportShifts;unsigned raisedGroups{};
};
inline float Smooth(float t){t=std::clamp(t,0.f,1.f);return t*t*(3.f-2.f*t);}
inline float TerraceSection(float x){
    // 手工橫斷面：路緣、低緩林床、岩石坡肩及外側回接，不以整座土丘縮高取代構圖。
    constexpr std::array<float,8> stations{3.5f,5.f,6.f,9.f,12.f,15.f,17.f,19.f};
    constexpr std::array<float,8> heights{0.f,.35f,.72f,.80f,1.75f,2.05f,1.10f,0.f};
    if(x<=stations.front()||x>=stations.back())return 0.f;
    for(std::size_t i=1;i<stations.size();++i)if(x<=stations[i])
        return std::lerp(heights[i-1],heights[i],Smooth((x-stations[i-1])/(stations[i]-stations[i-1])));
    throw std::invalid_argument("VALLEY_TERRACE_SECTION");
}
inline float TargetRise(float x,float z,std::span<const Aevrynd::BoxCollider3D> pads,bool terrace=false){
    if(!std::isfinite(x)||!std::isfinite(z))throw std::invalid_argument("VALLEY_NONFINITE");
    // 南側谷地雙肩，與河谷、核心邊界保持完整分離；中央主路不抬高。
    float mask=Smooth((std::abs(x)-3.5f)/3.f)*Smooth((19.f-std::abs(x))/4.f)*
        Smooth((z+12.f)/5.f)*Smooth((27.f-z)/5.f);
    const auto path=[&](Aevrynd::Vec3 p){
        const float outsideX=(std::max)({0.f,(std::min)(0.f,p.x)-1.8f-x,x-(std::max)(0.f,p.x)-1.8f});
        const float outsideZ=(std::max)(0.f,std::abs(z-p.z)-1.8f);
        mask*=Smooth((std::max)(outsideX,outsideZ)/2.5f);
        mask*=Smooth((std::hypot(x-p.x,z-p.z)-3.f)/2.5f);
    };
    for(const auto p:Session::supplies)path(p);
    for(const auto p:Session::repairSites)path(p);
    for(const auto& b:pads){
        if(!Session::Finite(b.center)||!Session::Finite(b.extents)||b.extents.x<0||b.extents.z<0)
            throw std::invalid_argument("VALLEY_INVALID_PAD");
        const float dx=(std::max)(0.f,std::abs(x-b.center.x)-b.extents.x-1.f);
        const float dz=(std::max)(0.f,std::abs(z-b.center.z)-b.extents.z-1.f);
        mask*=Smooth((std::max)(dx,dz)/2.5f);
    }
    const float original=2.2f*mask*(.72f+.28f*std::cos(z*.16f+x*.12f));
    if(!terrace||x>=0.f)return original;
    // 南側左岸沿路八公尺林床，首尾平順接回原世界；完整任務及支撐面門仍由下方求解器執行。
    const float along=Smooth((z+10.f)/4.f)*Smooth((14.f-z)/6.f);
    const float cross=Smooth((std::abs(x)-3.5f)/3.f)*Smooth((19.f-std::abs(x))/4.f);
    const float protections=cross>0.f?mask/cross:0.f;
    return std::lerp(original,TerraceSection(-x)*protections,along);
}
inline Result Build(const Aevrynd::HeightField3D& original,std::span<const Aevrynd::BoxCollider3D> pads,
    std::span<const Aevrynd::BoxCollider3D> supports={},std::span<const SupportSurface> surfaces={},bool terrace=false){
    if(!WestCliffRelief::SameExact(original,CreateTerrain()))throw std::invalid_argument("VALLEY_INPUT_IDENTITY");
    Result r{original};
    if(supports.size()>256)throw std::invalid_argument("VALLEY_SUPPORT_BUDGET");
    if(supports.size()!=surfaces.size())throw std::invalid_argument("VALLEY_ACTUAL_CONTACT_SURFACES_REQUIRED");
    const unsigned width=original.Settings().width,depth=original.Settings().depth;
    std::vector<float> target(original.SampleCount()),delta(original.SampleCount());
    std::vector<std::size_t> freeSamples;
    std::vector<unsigned> parent(supports.size());
    for(unsigned i=0;i<parent.size();++i)parent[i]=i;
    const auto find=[&](unsigned i){while(parent[i]!=i)i=parent[i];return i;};
    const auto join=[&](unsigned a,unsigned b){a=find(a);b=find(b);parent[(std::max)(a,b)]=(std::min)(a,b);};
    const unsigned noOwner=unsigned(supports.size());
    std::vector<unsigned> owners(original.SampleCount(),noOwner);
    for(const auto& p:supports)if(!Session::Finite(p.center)||!Session::Finite(p.extents)||p.extents.x<0||p.extents.z<0)
        throw std::invalid_argument("VALLEY_INVALID_SUPPORT");
    for(unsigned z=0;z<depth;++z)for(unsigned x=0;x<width;++x){
        const auto p=original.SamplePosition(x,z);const auto i=std::size_t(z)*width+x;
        target[i]=TargetRise(p.x,p.z,pads,terrace);
    }
    for(unsigned s=0;s<surfaces.size();++s){
        if(surfaces[s].cells.empty()||!surfaces[s].contactCells)throw std::invalid_argument("VALLEY_EMPTY_CONTACT_SURFACE");
        for(const auto& c:surfaces[s].cells){
            if(c.x+1>=width||c.z+1>=depth||!std::isfinite(c.minimumGap))throw std::invalid_argument("VALLEY_INVALID_CONTACT_CELL");
            if(c.minimumGap>.15f)continue;
            for(unsigned dz=0;dz<2;++dz)for(unsigned dx=0;dx<2;++dx){const auto i=std::size_t(c.z+dz)*width+c.x+dx;
                if(owners[i]!=noOwner)join(owners[i],s);else owners[i]=s;
            }
        }
    }
    std::vector<double> sums(supports.size());std::vector<unsigned> counts(supports.size());std::vector<bool> pinned(supports.size());
    for(std::size_t i=0;i<owners.size();++i)if(owners[i]!=noOwner){const auto group=find(owners[i]);
        sums[group]+=target[i];++counts[group];pinned[group]=pinned[group]||target[i]==0;
    }
    r.supportGroups.resize(supports.size());r.supportShifts.resize(supports.size());
    for(unsigned s=0;s<supports.size();++s){const auto group=find(s);
        if(!counts[group])throw std::invalid_argument("VALLEY_SUPPORT_OUTSIDE_FIELD");
        r.supportGroups[s]=group;r.supportShifts[s]=pinned[group]?0.f:float(sums[group]/double(counts[group]));
        if(group==s&&r.supportShifts[s]>0)++r.raisedGroups;
    }
    // 每個真實面格的最小舊gap提供保守上界；高於接地帶的幾何沒有被省略。
    // 若oldGap>=2cm則保留至少2cm，否則不增加原穿透。格內delta為三角線性插值。
    std::vector<float> upper(original.SampleCount(),2.2f);
    for(unsigned s=0;s<surfaces.size();++s)for(const auto& c:surfaces[s].cells){
        const float cap=r.supportShifts[s]+(std::max)(0.f,c.minimumGap-.02f);
        for(unsigned dz=0;dz<2;++dz)for(unsigned dx=0;dx<2;++dx){const auto i=std::size_t(c.z+dz)*width+c.x+dx;
            upper[i]=(std::min)(upper[i],cap);
        }
    }
    // 相連支撐區共用一個平移量；碰到任務／道路硬邊界的整組固定，其餘隨完整谷地抬升。
    for(unsigned z=0;z<depth;++z)for(unsigned x=0;x<width;++x){const auto i=std::size_t(z)*width+x;
        if(owners[i]!=noOwner){delta[i]=r.supportShifts[owners[i]];
            if(delta[i]>upper[i]+1.e-6f)throw std::invalid_argument("VALLEY_CONTACT_FIXED_CLEARANCE_CONFLICT");continue;}
        const bool fixed=target[i]==0||x==0||z==0||x+1==width||z+1==depth;
        if(!fixed){delta[i]=(std::min)(target[i],upper[i]);freeSamples.push_back(i);}
    }
    if(!supports.empty()){
        // 求解帶整組支撐平移邊界的screened-Poisson場，不強迫所有地物回到原來的高度。
        // 固定3公尺的藝術平滑尺度；不調坡度／接觸門，也不做參數掃描。
        constexpr float inverseLengthSquared=1.f/9.f;
        const float weight=1.f/(original.Settings().cellSize*original.Settings().cellSize);
        const float denominator=inverseLengthSquared+4.f*weight;
        bool converged=false;
        for(unsigned iteration=0;iteration<2048;++iteration){
            float maximumChange{};
            for(const auto i:freeSamples){
                const float value=(std::min)(upper[i],(inverseLengthSquared*target[i]+weight*(delta[i-1]+delta[i+1]+delta[i-width]+delta[i+width]))/denominator);
                if(!std::isfinite(value)||value<0||value>2.2f)throw std::invalid_argument("VALLEY_SOLVE_RANGE");
                maximumChange=(std::max)(maximumChange,std::abs(value-delta[i]));delta[i]=value;
            }
            r.solveIterations=iteration+1;
            if(maximumChange<=1.e-6f){converged=true;break;}
        }
        if(!converged)throw std::invalid_argument("VALLEY_SOLVE_NOT_CONVERGED");
        for(const auto i:freeSamples){
            const float fixedPoint=(std::min)(upper[i],(inverseLengthSquared*target[i]+weight*(delta[i-1]+delta[i+1]+delta[i-width]+delta[i+width]))/denominator);
            r.solveResidual=(std::max)(r.solveResidual,std::abs(delta[i]-fixedPoint));
        }
        if(r.solveResidual>1.e-6f)throw std::invalid_argument("VALLEY_SOLVE_RESIDUAL");
    }
    for(unsigned z=0;z<original.Settings().depth;++z)for(unsigned x=0;x<original.Settings().width;++x){
        const auto p=original.SamplePosition(x,z);const float rise=delta[std::size_t(z)*width+x];
        if(rise==0)continue;
        const float y=p.y+rise;
        if(!std::isfinite(y)||y<p.y||rise>2.2f)throw std::invalid_argument("VALLEY_HEIGHT_RANGE");
        r.terrain.SetHeight(x,z,y);++r.changed;r.maximumRise=(std::max)(r.maximumRise,rise);
    }
    r.terrain.RecalculateRange();
    if(!r.changed||r.maximumRise<.5f)throw std::invalid_argument("VALLEY_NO_VISIBLE_RELIEF");
    return r;
}
inline float Delta(const Aevrynd::HeightField3D& a,const Aevrynd::HeightField3D& b,Aevrynd::Vec3 p){
    if(!Session::Finite(p))throw std::invalid_argument("VALLEY_NONFINITE_ROOT");
    if(!a.Contains(p.x,p.z))return 0.f;
    return b.SampleHeight(p.x,p.z)-a.SampleHeight(p.x,p.z);
}
inline Aevrynd::Mesh3D Warp(const Aevrynd::Mesh3D& source,const Aevrynd::HeightField3D& a,
    const Aevrynd::HeightField3D& b,bool ground){
    if(!source.Valid()||!WestCliffRelief::SameLayout(a,b))throw std::invalid_argument("VALLEY_MESH_LAYOUT");
    auto r=source;
    for(auto& v:r.vertices){
        const float dy=Delta(a,b,{v.x,v.y,v.z});
        if(dy==0.f)continue;
        v.y+=dy;
        if(ground){v.normal=Aevrynd::PackNormalSnorm8(b.SampleNormal(v.x,v.z));
            v.color=GroundLayerHeightColor(v.color,v.y,b);}
    }
    // 道路及林地皆由同一個實際高度場衍生，維持UV、材質RGB與三角面拓撲。
    if(ground)for(auto& v:r.vertices)if(a.Contains(v.x,v.z))
        v.normal=Aevrynd::PackNormalSnorm8(b.SampleNormal(v.x,v.z));
    return r;
}
}
