#pragma once
#include "MistwatchLandscapeStandFill.h"

namespace Mistwatch::NorthWatershed {
// 北方流域完整重建至網格遠端邊界；不再把凹谷接回造成背牆的舊遠山。
// 寬峰與鞍部使用連續橫斷面，保留南側可玩邊界，不製造窄三角尖峰。
inline constexpr std::array<float,9> Stations{-200,-150,-100,-55,-10,35,80,135,200};
inline constexpr std::array<float,9> CrestHeight{32,42,29,43,24,39,46,31,40};
inline constexpr std::array<float,9> CrestZ{198,184,214,180,216,194,182,214,192};
template<std::size_t N> inline float Section(float value,const std::array<float,N>& xs,const std::array<float,N>& ys){
    if(!std::isfinite(value))throw std::invalid_argument("NORTH_SECTION_NONFINITE");
    if(value<=xs.front())return ys.front();if(value>=xs.back())return ys.back();
    for(std::size_t i=1;i<N;++i)if(value<=xs[i]){
        const float t=(value-xs[i-1])/(xs[i]-xs[i-1]);return std::lerp(ys[i-1],ys[i],t*t*(3-2*t));
    }
    throw std::invalid_argument("NORTH_SECTION_DOMAIN");
}
inline float Weight(float x,float z){
    if(!std::isfinite(x)||!std::isfinite(z))throw std::invalid_argument("NORTH_WEIGHT_NONFINITE");
    return z<=100?0:RiverSmooth((z-100.f)/12.f);
}
inline float Height(const Aevrynd::HeightField3D& core,float x,float z){
    const float original=LandscapeHeight(core,x,z),weight=Weight(x,z);if(weight==0)return original;
    const float crest=Section(x,Stations,CrestHeight),ridgeZ=Section(x,Stations,CrestZ);
    const float foot=LandscapeHeight(core,x,100.f),along=(z-100.f)/(ridgeZ-100.f);
    // 前方緩腳、岩坡折面、山頂及背坡是同一橫斷面；不是把整座山縮高或加隨機噪音。
    constexpr std::array<float,7> T{0,.28f,.56f,.78f,1.f,1.25f,2.f};
    constexpr std::array<float,7> Y{0,.06f,.32f,.70f,1.f,.79f,.64f};
    const float authored=foot+(crest-foot)*Section(along,T,Y);
    return std::lerp(original,authored,weight);
}
inline Aevrynd::Vec2 Cell(float v,float lo,float hi){
    if(v<lo)return {lo+std::floor((v-lo)/6.f)*6.f,6.f};
    if(v>=hi)return {hi+std::floor((v-hi)/6.f)*6.f,6.f};
    return {lo+std::floor((v-lo)/.5f)*.5f,.5f};
}
inline float SurfaceHeight(const Aevrynd::HeightField3D& core,float x,float z){
    const auto cx=Cell(x,-50,50),cz=Cell(z,-20,100);
    if(Weight(cx.x,cz.x)==0&&Weight(cx.x+cx.y,cz.x)==0&&Weight(cx.x,cz.x+cz.y)==0&&Weight(cx.x+cx.y,cz.x+cz.y)==0)
        return LandscapeSurfaceHeight(core,x,z);
    const float tx=(x-cx.x)/cx.y,tz=(z-cz.x)/cz.y;
    const float a=Height(core,cx.x,cz.x),b=Height(core,cx.x+cx.y,cz.x),c=Height(core,cx.x,cz.x+cz.y),d=Height(core,cx.x+cx.y,cz.x+cz.y);
    return tx+tz<=1?a+(b-a)*tx+(c-a)*tz:d+(c-d)*(1-tx)+(b-d)*(1-tz);
}
inline Aevrynd::Vec3 Normal(const Aevrynd::HeightField3D& core,float x,float z){
    if(Weight(x-.5f,z)==0&&Weight(x+.5f,z)==0&&Weight(x,z-.5f)==0&&Weight(x,z+.5f)==0)
        return LandscapeNormal(core,x,z);
    return Aevrynd::Normalize({SurfaceHeight(core,x-.5f,z)-SurfaceHeight(core,x+.5f,z),1,
        SurfaceHeight(core,x,z-.5f)-SurfaceHeight(core,x,z+.5f)});
}
struct Result {
    Aevrynd::Mesh3D mesh;LandscapeStandFill::Plan plan;std::vector<Aevrynd::Transform3D> base;
    std::size_t vertices{},shifted{},relocated{},inspected{};float maximumChange{},maximumRootError{};
};
inline Result Create(const Aevrynd::Mesh3D& source,const Aevrynd::HeightField3D& core,
    const std::vector<Aevrynd::Transform3D>& base,const LandscapeStandFill::Plan& plan,const Aevrynd::MeshBounds3D& bounds){
    using namespace Aevrynd;namespace D=LandscapeStandFill::Detail;
    if(!source.Valid()||base.size()!=520||plan.combined.size()!=616||plan.addedBands.size()!=96||!plan.complete)
        throw std::invalid_argument("NORTH_COMPLETE616_SOURCE");
    for(std::size_t i=0;i<base.size();++i)if(!D::Same(base[i],plan.combined[i]))throw std::invalid_argument("NORTH_BASE_PREFIX");
    Result r{source,plan,base};
    for(auto& v:r.mesh.vertices){
        if(Weight(v.x,v.z)==0)continue;
        if(core.Contains(v.x,v.z))throw std::runtime_error("NORTH_PLAYABLE_CORE_PROTECTED");
        const float before=v.y;v.y=Height(core,v.x,v.z);const auto n=Normal(core,v.x,v.z);v.normal=PackNormalSnorm8(n);
        const float rock=std::clamp((.82f-n.y)/.35f,0.f,1.f);const Color green{.38f,.47f,.30f,1},stone{.50f,.48f,.43f,1};
        v.color=GroundLayerHeightColor(PackColorRgba8({std::lerp(green.r,stone.r,rock),std::lerp(green.g,stone.g,rock),std::lerp(green.b,stone.b,rock),1}),v.y,core);
        const auto c=ComposeBankFloorCoverage(v.x,v.z,n,RiverSedimentWeight(v.x,v.z),ForestFloorMaskSelector::ForestWood,BankCoverageSelector::Patchy);
        Vec2 uv{};if(!TryEncodeTerrainLayerCoverage3D(c.soil,c.rock,uv))throw std::runtime_error("NORTH_COVERAGE");
        v.u=uv.x;v.v=uv.y;++r.vertices;r.maximumChange=(std::max)(r.maximumChange,std::abs(v.y-before));
    }
    std::vector<bool> placed(616,false);
    const auto valid=[&](std::size_t i,Transform3D p){
        const float x=p.position.x,z=p.position.z;
        if(x< -150||x>150||z< -110||z>190||(x>=-53&&x<=53&&z>=-23&&z<=103)||Normal(core,x,z).y<.78f)return false;
        if(i>=520){
            const auto& band=LandscapeStandFill::Bands.at(plan.addedBands.at(i-520));
            if(x<band.minX||x>band.maxX||z<band.minZ||z>band.maxZ)return false;
            const auto b=TransformBounds(bounds,p);const auto lo=b.center-b.extents,hi=b.center+b.extents;
            if(!(hi.x< -53||lo.x>53||hi.z< -23||lo.z>103)||lo.x< -200||hi.x>200||lo.z< -170||hi.z>250)return false;
        }
        for(std::size_t j=0;j<r.plan.combined.size();++j)if(i!=j&&placed[j]&&std::hypot(double(x)-r.plan.combined[j].position.x,double(z)-r.plan.combined[j].position.z)<5.)return false;
        return true;
    };
    // 先保留新山形上仍可用的原位置；失效位置不繼續佔住其他樹的規劃空間。
    for(std::size_t i=0;i<r.plan.combined.size();++i){
        auto& p=r.plan.combined[i];
        p.position.y=SurfaceHeight(core,p.position.x,p.position.z);
        placed[i]=valid(i,p);
    }
    // 有固定帶域的96棵優先，再處理全外環帶的基礎樹；不用擴大局部搬移半徑湊數。
    for(std::size_t order=0;order<616;++order){
        const std::size_t i=order<96?520+order:order-96;
        if(placed[i])continue;
        auto& p=r.plan.combined[i];bool found=false;
        if(i>=520){
            const auto& band=LandscapeStandFill::Bands.at(plan.addedBands.at(i-520));
            for(unsigned z=0;z<LandscapeStandFill::Grid&&!found;++z)for(unsigned x=0;x<LandscapeStandFill::Grid;++x){
                auto trial=plan.combined[i];
                trial.position.x=std::lerp(band.minX,band.maxX,(float(x)+.5f)/float(LandscapeStandFill::Grid));
                trial.position.z=std::lerp(band.minZ,band.maxZ,(float(z)+.5f)/float(LandscapeStandFill::Grid));
                trial.position.y=SurfaceHeight(core,trial.position.x,trial.position.z);
                if(valid(i,trial)){p=trial;found=true;break;}
            }
        }else{
            std::uint32_t seed=0x52494447u;
            const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
            for(unsigned attempt=0;attempt<9000;++attempt){
                auto trial=plan.combined[i];trial.position.x=-150+300*random();trial.position.z=-110+300*random();
                const float patch=.55f+.25f*std::sin(trial.position.x*.06f-trial.position.z*.04f);
                if(random()>patch)continue;
                trial.position.y=SurfaceHeight(core,trial.position.x,trial.position.z);
                if(valid(i,trial)){p=trial;found=true;break;}
            }
        }
        if(!found)throw std::runtime_error("NORTH_JOINT616_NO_VALID_PLACEMENT");
        placed[i]=true;++r.relocated;
    }
    for(std::size_t i=0;i<r.plan.combined.size();++i){
        const auto& p=r.plan.combined[i];const auto& before=plan.combined[i];
        ++r.inspected;r.shifted+=!D::Same(p,before);
        r.maximumRootError=(std::max)(r.maximumRootError,std::abs(p.position.y-SurfaceHeight(core,p.position.x,p.position.z)));
        if(p.scale.x!=before.scale.x||p.scale.y!=before.scale.y||p.scale.z!=before.scale.z||p.rotation.x!=before.rotation.x||p.rotation.y!=before.rotation.y||p.rotation.z!=before.rotation.z)
            throw std::runtime_error("NORTH_TREE_SHAPE_CHANGED");
    }
    // 全族群最終再驗；包含額外96棵而非只有520棵基礎樹。
    for(std::size_t i=0;i<r.plan.combined.size();++i)if(!valid(i,r.plan.combined[i]))throw std::runtime_error("NORTH_FINAL616_PLACEMENT");
    std::copy_n(r.plan.combined.begin(),520,r.base.begin());
    r.plan.fingerprint=D::Fingerprint(r.plan.combined);for(auto b:r.plan.addedBands)D::Word(r.plan.fingerprint,b);
    if(!r.vertices||r.mesh.indices!=source.indices||r.maximumRootError!=0)throw std::runtime_error("NORTH_RESULT_IDENTITY");
    return r;
}
}
