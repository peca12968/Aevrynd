#pragma once
#include "MistwatchCoreHerbClusters.h"

namespace Mistwatch::RouteGrassAuthor {
using namespace Aevrynd;
inline constexpr std::string_view Profile="complete-route-half-step-24m-v1";
inline constexpr unsigned Target=128,Rows=168,Columns=48,MaximumProposals=Rows*Columns*2;
inline constexpr std::array<unsigned,Target> Ordinals{
    1538,2784,5188,6244,9910,11140,12964,15360,97,3171,5761,6335,9139,11425,12195,14883,
    1346,2592,5442,10592,13166,15072,1,3267,5571,9237,11713,12103,15075,10,2786,5348,
    11564,13170,14882,1187,3459,4035,11617,14031,14787,1260,2498,5564,11468,13268,15842,1333,
    3175,4227,11905,13607,16035,258,2690,11568,13556,15170,1047,2955,5667,11521,13041,14789,
    1508,2694,11968,13982,15554,1143,2387,4515,11235,12945,14695,938,2502,12066,13330,15362,
    2481,4325,11427,12755,14601,270,2794,12068,13332,15652,2579,5573,11621,12567,14775,658,
    3694,10150,13526,15940,2197,4231,11141,13623,15263,466,3646,11516,13334,15460,2389,4135,
    12005,564,11612,13624,15078,2485,4423,11813,756,11710,15270,2103,4039,11431,854,2295
};
struct Proposal {Vec3 root;unsigned ordinal{},bin{},variant{};float scale{};};
inline std::array<Proposal,Target> Create(const HeightField3D& terrain){
    std::array<Proposal,Target> result;std::array<bool,Target> written{};std::uint32_t seed=0x52534331u;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    // 固定原16128格的序列；不靠Evidence清單或相機動態尋位。
    for(unsigned ordinal=0;ordinal<MaximumProposals;++ordinal){
        const unsigned row=ordinal/(Columns*2),column=(ordinal%(Columns*2))/2;
        const float side=(ordinal%2)==0?-1.f:1.f;
        Vec3 root{side*(2.4f+.45f*float(column))+(random()-.5f)*.18f,0,
            -6.f+.525f*float(row)+(random()-.5f)*.18f};
        root.y=terrain.SampleHeight(root.x,root.z);const unsigned variant=(std::min)(unsigned(random()*5.f),4u);
        for(unsigned slot=0;slot<Target;++slot)if(Ordinals[slot]==ordinal){
            result[slot]={root,ordinal,row*8/Rows+(side>0?8u:0u),variant,ForestCoverScale(root)};written[slot]=true;
        }
    }
    std::array<unsigned,2> sides{};std::array<unsigned,16> bins{};
    for(unsigned i=0;i<Target;++i){CoreHerbClusters::Detail::Need(written[i],"ROUTE_GRASS_AUTHOR_INCOMPLETE");
        const auto& p=result[i];CoreHerbClusters::Detail::Need(Session::Finite(p.root)&&p.scale>0&&p.variant<5,"ROUTE_GRASS_AUTHOR_INVALID");
        for(unsigned j=0;j<i;++j)CoreHerbClusters::Detail::Need(p.ordinal!=result[j].ordinal,"ROUTE_GRASS_AUTHOR_DUPLICATE");
        ++sides[p.bin/8];++bins[p.bin];
    }
    CoreHerbClusters::Detail::Need(sides[0]==64&&sides[1]==64,"ROUTE_GRASS_AUTHOR_SIDE_SHORTFALL");
    for(unsigned side=0;side<2;++side){unsigned occupied{};
        for(unsigned bin=side*8;bin<side*8+8;++bin)occupied+=bins[bin]>0;
        CoreHerbClusters::Detail::Need(occupied>=4,"ROUTE_GRASS_AUTHOR_LONGITUDINAL_SHORTFALL");
    }
    return result;
}
}
