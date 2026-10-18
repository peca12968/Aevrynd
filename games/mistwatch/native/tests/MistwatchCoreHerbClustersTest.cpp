#include "MistwatchCoreHerbClusters.h"
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <iostream>
#include <limits>

namespace {
using namespace Aevrynd;
namespace H=Mistwatch::CoreHerbClusters;
unsigned checks{},negatives{};
void Need(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool SameDouble(double a,double b){return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);}
template<class F>void Reject(F&& action,const char* why){try{action();}
    catch(const std::invalid_argument& e){Need(std::string_view(e.what())==why,"wrong rejection");++negatives;return;}
    throw std::runtime_error("missing rejection");}
void Word(std::uint64_t& h,std::uint32_t w){for(unsigned i=0;i<4;++i){h^=(w>>(8*i))&255u;h*=1099511628211ull;}}
// 分開計算固定counter/hash契約，不從Propose取回值充當自己的預期值。
float ReferenceUnit(unsigned cluster,unsigned attempt,unsigned dimension){
    std::uint32_t word=0x43485231u;
    word^=(cluster+1u)*0x9e3779b9u;word^=(attempt+1u)*0x85ebca6bu;word^=(dimension+1u)*0xc2b2ae35u;
    word=(word^(word>>16))*0x7feb352du;word=(word^(word>>15))*0x846ca68bu;word^=word>>16;
    return static_cast<float>(word/256u)/16777216.f;
}
bool SameProposal(const H::Detail::Proposal& a,const H::Detail::Proposal& b){
    return a.variantSlot==b.variantSlot&&a.angularCell==b.angularCell&&a.radialCell==b.radialCell&&
        Same(a.height,b.height)&&Same(a.yaw,b.yaw)&&SameDouble(a.angularUnit,b.angularUnit)&&
        SameDouble(a.radialUnit,b.radialUnit)&&SameDouble(a.angle,b.angle)&&SameDouble(a.distance,b.distance);
}
void ProposalCoverage(){
    Need(H::SamplingProfile=="stratified-polar-512-v1"&&H::AngularStrata==32&&H::RadialStrata==16&&
        H::ClusterCount==12&&H::PerCluster==16&&H::Target==192&&H::AttemptsPerCluster==512&&H::MaximumProposals==6144,
        "sampling changed original population or budget");
    Need(H::WindRadius==.19f&&H::WindUp==.011f,"sampling shrank original wind envelope");
    std::array<std::array<std::uint64_t,5>,12> dimensionSeals{};
    for(unsigned cluster=0;cluster<12;++cluster){
        std::array<unsigned,512> cells{};std::array<unsigned,32> angular{};std::array<unsigned,16> radial{};
        std::array<unsigned,2> variants{};std::array<H::Detail::Proposal,512> forward{};
        dimensionSeals[cluster].fill(14695981039346656037ull);
        for(unsigned attempt=0;attempt<512;++attempt){
            const auto p=H::Detail::Propose(cluster,attempt);forward[attempt]=p;
            unsigned reversed{};for(unsigned bit=0;bit<4;++bit)reversed=(reversed<<1)|((attempt/32u>>bit)&1u);
            const auto expectedAngular=((attempt%32u)*(2u*cluster+1u)+cluster*7u)%32u;
            const auto expectedRadial=(reversed+cluster*3u)%16u;
            Need(p.angularCell==expectedAngular&&p.radialCell==expectedRadial,"stratum permutation contract");
            Need(p.angularCell<32&&p.radialCell<16,"stratum out of bounds");
            ++cells[p.angularCell*16+p.radialCell];++angular[p.angularCell];++radial[p.radialCell];
            std::array<float,5> unit{};
            for(unsigned dimension=0;dimension<5;++dimension){
                unit[dimension]=H::Detail::ProposalUnit(cluster,attempt,dimension);
                Need(std::isfinite(unit[dimension])&&unit[dimension]>=0&&unit[dimension]<1&&
                    Same(unit[dimension],ReferenceUnit(cluster,attempt,dimension)),"independent proposal dimension counter/hash");
                Word(dimensionSeals[cluster][dimension],std::bit_cast<std::uint32_t>(unit[dimension]));
            }
            const double angularUnit=(double(expectedAngular)+.01+.98*double(unit[0]))/32.;
            const double radialUnit=(double(expectedRadial)+.01+.98*double(unit[1]))/16.;
            Need(SameDouble(p.angularUnit,angularUnit)&&SameDouble(p.radialUnit,radialUnit),"position used wrong jitter dimension");
            Need(p.angularUnit>double(p.angularCell)/32&&p.angularUnit<double(p.angularCell+1)/32&&
                p.radialUnit>double(p.radialCell)/16&&p.radialUnit<double(p.radialCell+1)/16,"jitter escaped cell interior");
            Need(static_cast<unsigned>(p.angularUnit*32)==p.angularCell&&static_cast<unsigned>(p.radialUnit*16)==p.radialCell,
                "authored unit does not occupy declared cell");
            Need(SameDouble(p.angle,6.2831853071795864769*angularUnit)&&SameDouble(p.distance,6.*std::sqrt(radialUnit))&&
                p.distance>0&&p.distance<6,"equal-area radial or angular geometry changed");
            const unsigned slot=static_cast<unsigned>(unit[2]>=.5f);
            Need(p.variantSlot==slot,"variant reused position dimension");++variants[slot];
            const float height=slot==0?.3f+.35f*unit[3]:.18f+.17f*unit[3];
            Need(Same(p.height,height)&&p.height>=(slot==0?.3f:.18f)&&p.height<=(slot==0?.65f:.35f),
                "height used wrong dimension or shrank source range");
            Need(Same(p.yaw,6.28318530718f*unit[4])&&p.yaw>=0&&p.yaw<6.28318530718f,
                "yaw used wrong dimension or escaped full turn");
        }
        for(auto count:cells)Need(count==1,"512 proposals did not cover each cell exactly once");
        for(auto count:angular)Need(count==16,"angular strata population");
        for(auto count:radial)Need(count==32,"equal-area radial strata population");
        Need(variants[0]>0&&variants[1]>0,"proposal stream lost a source variant");
        // 反序及另一個完整置換查詢不得推進共用亂數狀態；等同拒絕任意前面提案後再查同筆。
        for(unsigned n=0;n<512;++n){
            Need(SameProposal(forward[511-n],H::Detail::Propose(cluster,511-n)),"reverse query changed proposal bits");
            const unsigned shuffled=(n*193u+37u)%512u;
            Need(SameProposal(forward[shuffled],H::Detail::Propose(cluster,shuffled)),"shuffled query changed proposal bits");
        }
        for(unsigned a=0;a<5;++a)for(unsigned b=a+1;b<5;++b)
            Need(dimensionSeals[cluster][a]!=dimensionSeals[cluster][b],"two complete dimension streams aliased");
    }
    for(unsigned a=0;a<12;++a)for(unsigned b=a+1;b<12;++b)for(unsigned dimension=0;dimension<5;++dimension)
        Need(dimensionSeals[a][dimension]!=dimensionSeals[b][dimension],"cluster streams aliased");
    for(unsigned cluster:{12u,(std::numeric_limits<unsigned>::max)()}){
        Reject([&]{(void)H::Detail::Propose(cluster,0);},"CORE_HERB_PROPOSAL_INDEX");
        Reject([&]{(void)H::Detail::ProposalUnit(cluster,0,0);},"CORE_HERB_PROPOSAL_INDEX");
    }
    for(unsigned attempt:{512u,(std::numeric_limits<unsigned>::max)()}){
        Reject([&]{(void)H::Detail::Propose(0,attempt);},"CORE_HERB_PROPOSAL_INDEX");
        Reject([&]{(void)H::Detail::ProposalUnit(0,attempt,0);},"CORE_HERB_PROPOSAL_INDEX");
    }
    for(unsigned dimension:{5u,(std::numeric_limits<unsigned>::max)()})
        Reject([&]{(void)H::Detail::ProposalUnit(0,0,dimension);},"CORE_HERB_PROPOSAL_INDEX");
    // 空Plan的獨立word序列確認schema=3包含作者布局與每群方向，不接受舊schema。
    std::uint64_t emptySeal=14695981039346656037ull;Word(emptySeal,3);
    for(unsigned i=0;i<12;++i)Word(emptySeal,0); // source、terrain/world/existing與author各兩word。
    for(unsigned i=0;i<4;++i)Word(emptySeal,0); // grouped/proposals/failed/complete。
    Word(emptySeal,1);Word(emptySeal,0);Word(emptySeal,0); // root wind profile及兩個計數。
    for(unsigned i=0;i<8+12*(7+8)+1;++i)Word(emptySeal,0); // rejections、十二cluster含方向、plant count。
    Need(H::Detail::Fingerprint(H::Plan{})==emptySeal,"proposal fingerprint schema not version three");
    std::cout<<"CORE_HERB_PROPOSAL profile="<<H::SamplingProfile
        <<" clusters=12 proposals_per_cluster=512 cells_per_cluster=512 exact_once=1 reordered_bits=1 separated_dimension_streams=5\n";
}
std::uint64_t Seal(const Mesh3D& mesh){std::uint64_t h=14695981039346656037ull;
    Word(h,static_cast<unsigned>(mesh.vertices.size()));Word(h,static_cast<unsigned>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})Word(h,std::bit_cast<std::uint32_t>(f));Word(h,v.normal);Word(h,v.color);}
    for(auto i:mesh.indices)Word(h,i);return h;}
void Resolver(){for(bool capture:{false,true})for(bool replay:{false,true}){
    Need(!H::Resolve(capture,replay,L"")&&!H::Resolve(capture,replay,L"original"),"default adoption");
    if(capture!=replay)Need(H::Resolve(capture,replay,L"core-herb-clusters-v1"),"explicit context");
    else Reject([&]{(void)H::Resolve(capture,replay,L"core-herb-clusters-v1");},"CORE_HERB_REQUIRED_CONTEXT");
    for(auto text:{L"Core-herb-clusters-v1",L"core-herb-clusters-v2",L" core-herb-clusters-v1",L"core-herb-clusters-v1 ",L"1"})
        Reject([&]{(void)H::Resolve(capture,replay,text);},"CORE_HERB_PROFILE");
}}
void Accounting(const H::Plan& p){
    unsigned rejected{},count{},attempted{},failed{};std::array<unsigned,2> variants{},sides{};
    unsigned pinned{},changedWind{};
    std::array<unsigned,static_cast<unsigned>(H::Rejection::Count)> patchRejected{};
    for(auto n:p.rejected)rejected+=n;
    for(unsigned i=0;i<H::ClusterCount;++i){const auto& cluster=p.clusters[i];
        Need(cluster.first==count&&cluster.count<=16&&cluster.proposals<=H::AttemptsPerCluster,"cluster bounds");
        unsigned rejectedHere{};for(unsigned reason=0;reason<patchRejected.size();++reason){
            rejectedHere+=cluster.rejected[reason];patchRejected[reason]+=cluster.rejected[reason];}
        Need(cluster.proposals==cluster.count+rejectedHere,"cluster rejection accounting");
        for(unsigned j=0;j<cluster.count;++j)Need(p.plants[count+j].cluster==i,"cluster contiguous domain");
        count+=cluster.count;attempted+=cluster.proposals;failed+=cluster.count!=16;
    }
    for(const auto& plant:p.plants){Need(plant.variant==2||plant.variant==3,"variant");++variants[plant.variant-2];++sides[plant.root.x<0?0:1];
        Need(plant.rootWind==H::RootWind::PinnedV1,"plant wind profile");pinned+=plant.pinnedVertices;changedWind+=plant.changedWindVertices;}
    Need(p.proposals==attempted&&p.proposals<=H::MaximumProposals&&p.proposals==p.plants.size()+rejected,"complete rejection accounting");
    Need(patchRejected==p.rejected,"cluster rejection sums differ from plan");
    Need(p.rootWind==H::RootWind::PinnedV1&&p.pinnedVertices==pinned&&p.changedWindVertices==changedWind,"wind author counters");
    Need(count==p.plants.size()&&p.failedClusters==failed&&p.variants==variants&&p.sides==sides,"plan counters");
    Need(p.complete==(failed==0&&count==192&&sides[0]==96&&sides[1]==96),"shortfall hidden");
    Need(!p.wholeSupportProven&&!p.groupedWorldVerified,"fixture falsely claims full world/support");
}
void Diagnostics(std::string_view fixture,const H::Plan& p){
    constexpr std::array<std::string_view,8> names{"domain","route","terrain","world","vegetation","spacing","conform","support_selection"};
    for(unsigned i=0;i<H::ClusterCount;++i){const auto& c=p.clusters[i];
        std::cout<<"CORE_HERB_PATCH fixture="<<fixture<<" cluster="<<i<<" x="<<c.centre.x<<" z="<<c.centre.z
            <<" accepted="<<c.count<<" proposals="<<c.proposals;
        for(unsigned reason=0;reason<names.size();++reason)std::cout<<' '<<names[reason]<<'='<<c.rejected[reason];
        std::cout<<'\n';}
    std::cout.flush();
}
// 與作者 helper 分開計算，含負值保留及量化後真正零根風的邊界。
std::uint32_t ExpectedWind(std::uint32_t packed,double height){
    const auto original=packed&0xff000000u;
    const double inverse=double(static_cast<std::int8_t>(static_cast<std::uint8_t>(packed>>24)))/127.;
    if(inverse<0||height>=double(.35f))return original;
    const double t=std::clamp((height-double(.035f))/double(.35f-.035f),0.,1.);
    // 原始 SNORM 先解成 float，與渲染器所讀取的權重一致。
    const double authored=1.-(1.-double(std::clamp(static_cast<float>(inverse),0.f,1.f)))*(3.*t*t-2.*t*t*t);
    const float value=static_cast<float>(authored);
    const auto quantized=static_cast<std::uint32_t>(static_cast<std::uint8_t>(static_cast<std::int8_t>(value*127.f+.5f)));
    return quantized<<24;
}
void WindBoundaries(){
    Need(H::RootWindProfile=="root-pinned-v1","root wind author identity");
    for(unsigned byte=0;byte<256;++byte)for(double height:{0.,double(.035f),.1,.2,double(.35f),.7}){
        const auto packed=(byte<<24)|0x007f0000u;
        Need(H::Detail::RootWindBits(packed,height)==ExpectedWind(packed,height),"root wind boundary formula");
        if(byte<=127&&height<=double(.035f))Need(H::Detail::RootWindBits(packed,height)==0x7f000000u,"pinned root has wind");
        if(byte>=128||height>=double(.35f))Need(H::Detail::RootWindBits(packed,height)==(packed&0xff000000u),"preserved wind bits");
    }
}
std::uint64_t ReferenceLayoutSeal(const H::AuthorLayout& author){
    std::uint64_t seal=14695981039346656037ull;Word(seal,0x43484c31u);
    for(unsigned cluster=0;cluster<12;++cluster){const auto p=author.centres[cluster];
        for(float value:{p.x,p.y,p.z,author.rotationOffsets[cluster]})Word(seal,std::bit_cast<std::uint32_t>(value));}
    return seal;
}
void LayoutBoundaries(const H::Sources& sources,const HeightField3D& flat,
    std::span<const BoxCollider3D> before,std::span<const BoxCollider3D> after){
    const std::array<Vec3,12> native8{{{-12.f,0,-2.f},{-13.5f,0,6.f},{-19.f,0,23.f},
        {-8.f,0,49.f},{-16.5f,0,68.f},{-17.f,0,79.f},{15.f,0,-2.f},{14.f,0,11.f},{18.4f,0,24.f},
        {14.f,0,54.f},{18.f,0,65.f},{19.2f,0,78.f}}};
    // 獨立Golden來自world5/FinalLocalLayouts.csv的fine-0-0-3；僅CPU配置足額，非支撐／原生／美術通過。
    const std::array<Vec3,12> golden{{{-10.5f,0,0.f},{-10.5f,0,6.f},{-19.f,0,23.f},
        {-8.f,0,49.f},{-16.5f,0,68.f},{-19.25f,0,77.75f},{17.5f,0,-.5f},{14.f,0,11.f},{18.4f,0,24.f},
        {17.f,0,54.f},{18.f,0,65.f},{19.2f,0,78.5f}}};
    const std::array<float,12> goldenOffsets{{.5235987755982988f,0,0,0,1.5707963267948966f,
        -.916297857297023f,0,0,0,0,0,-.5235987755982988f}};
    for(unsigned i=0;i<12;++i){const auto a=H::DefaultLayout.centres[i],b=golden[i];
        Need(Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z)&&Same(H::DefaultLayout.rotationOffsets[i],goldenOffsets[i]),
            "default layout differs from exact fixed-world candidate");
        const double dx=double(a.x)-native8[i].x,dz=double(a.z)-native8[i].z;
        Need(dx*dx+dz*dz<=9.,"candidate moved farther than three metres from original native8 author centre");
        if(i==2||i==3||i==7||i==8||i==10)
            Need(Same(a.x,native8[i].x)&&Same(a.y,native8[i].y)&&Same(a.z,native8[i].z)&&
                Same(H::DefaultLayout.rotationOffsets[i],0.f),"untouched cluster author identity changed");
        if(i==4)Need(Same(a.x,native8[i].x)&&Same(a.y,native8[i].y)&&Same(a.z,native8[i].z),
            "joint spacing repair translated cluster four instead of only rotating it");
    }
    Need(H::Detail::LayoutFingerprint(H::DefaultLayout)==ReferenceLayoutSeal(H::DefaultLayout)&&
        ReferenceLayoutSeal(H::DefaultLayout)==2774174745427001144ull,"default layout independent fixed-world seal");
    // 作者入口邊界可用不等於配置成功；所有植物仍要通過原Domain／Route等實際門。
    for(float x:{-4.f,-24.f})for(float z:{-6.f,84.f})for(float offset:{-3.14159265359f,3.14159265359f}){
        auto edge=H::DefaultLayout;edge.centres[0]={x,0,z};edge.rotationOffsets[0]=offset;
        Need(H::Detail::LayoutFingerprint(edge)==ReferenceLayoutSeal(edge),"valid author boundary rejected or changed");
    }
    const auto bad=[&](auto mutate){auto author=H::DefaultLayout;mutate(author);
        Reject([&]{(void)H::Detail::LayoutFingerprint(author);},"CORE_HERB_AUTHOR_LAYOUT");
        Reject([&]{(void)H::Create(sources,flat,before,after,nullptr,{}, {}, {},author);},"CORE_HERB_AUTHOR_LAYOUT");
    };
    bad([](auto& a){a.centres[0].y=1;});
    bad([](auto& a){a.centres[0].y=std::numeric_limits<float>::quiet_NaN();});
    bad([](auto& a){a.centres[0].x=std::numeric_limits<float>::quiet_NaN();});
    bad([](auto& a){a.centres[0].z=std::numeric_limits<float>::infinity();});
    bad([](auto& a){a.centres[0].x=-3.99f;});bad([](auto& a){a.centres[0].x=-24.01f;});
    bad([](auto& a){a.centres[0].x=0;});bad([](auto& a){a.centres[0].x=12;});
    bad([](auto& a){a.centres[6].x=-15;});
    bad([](auto& a){a.centres[0].z=-6.01f;});bad([](auto& a){a.centres[0].z=84.01f;});
    bad([](auto& a){a.rotationOffsets[0]=std::numeric_limits<float>::quiet_NaN();});
    bad([](auto& a){a.rotationOffsets[0]=std::numeric_limits<float>::infinity();});
    bad([](auto& a){a.rotationOffsets[0]=std::nextafter(3.14159265359f,std::numeric_limits<float>::infinity());});
    bad([](auto& a){a.rotationOffsets[0]=std::nextafter(-3.14159265359f,-std::numeric_limits<float>::infinity());});
}
void Plants(const H::Plan& plan,const H::Sources& sources,const HeightField3D& terrain,
    std::span<const BoxCollider3D> before,std::span<const BoxCollider3D> after,std::span<const BoxCollider3D> existing={},
    const H::AuthorLayout& author=H::DefaultLayout){
    Accounting(plan);unsigned roots{},deformed{};
    Need(plan.authorFingerprint==ReferenceLayoutSeal(author),"plan author fingerprint did not bind supplied layout");
    std::array<unsigned,12> proposalCursor{};
    for(unsigned i=0;i<12;++i){const auto& cluster=plan.clusters[i];const auto centre=author.centres[i];
        Need(Same(cluster.centre.x,centre.x)&&Same(cluster.centre.z,centre.z)&&
            Same(cluster.authorRotation,float(i)*1.137f+author.rotationOffsets[i]),"cluster ignored supplied author centre/rotation");}
    for(unsigned id=0;id<plan.plants.size();++id){const auto& p=plan.plants[id];const auto& raw=*sources[p.variant-2];const auto& mesh=p.derivedMesh;
        bool proposed{};const auto& patch=plan.clusters[p.cluster];
        for(unsigned attempt=proposalCursor[p.cluster];attempt<patch.proposals;++attempt){
            const auto candidate=H::Detail::Propose(p.cluster,attempt);
            if(candidate.variantSlot+2!=p.variant||!Same(candidate.height,p.requestedHeight)||!Same(candidate.yaw,p.pose.rotation.y))continue;
            const float rotation=patch.authorRotation;
            const double dx=candidate.distance*std::cos(candidate.angle),dz=.8*candidate.distance*std::sin(candidate.angle);
            const float px=static_cast<float>(patch.centre.x+dx*std::cos(rotation)-dz*std::sin(rotation));
            const float pz=static_cast<float>(patch.centre.z+dx*std::sin(rotation)+dz*std::cos(rotation));
            proposed=Same(px,p.root.x)&&Same(pz,p.root.z);if(proposed){proposalCursor[p.cluster]=attempt+1;break;}
        }
        Need(proposed,"actual plant did not originate from ordered bounded stratified proposal prefix");
        Need(p.conformResult&&!p.wholeSupportProven,"conform/full support conflated");
        Need(p.sourceFingerprint==Seal(raw)&&p.derivedFingerprint==Seal(mesh)&&mesh.indices==raw.indices&&mesh.vertices.size()==raw.vertices.size(),"source or derived identity");
        Need(p.pose.rotation.x==0&&p.pose.rotation.z==0&&p.pose.scale.x==p.pose.scale.y&&p.pose.scale.y==p.pose.scale.z,"pose is not upright uniform");
        const auto bounds=Mistwatch::MeasureNettleAssetBounds(raw);const auto scale=p.pose.scale.x;
        Need(p.requestedHeight>=(p.variant==2?.3f:.18f)&&p.requestedHeight<=(p.variant==2?.65f:.35f)&&
            std::abs((bounds.maximumY-bounds.minimumY)*scale-p.requestedHeight)<1.e-6f,"real source height range");
        Mistwatch::CanopyVolume::PlantLocalVolume rawVolume;
        Need(Mistwatch::CanopyVolume::MeasureLocalVolume(std::span<const Vertex3D>(raw.vertices),rawVolume),"raw measured volume");
        const auto rawWind=Mistwatch::CanopyVolume::ConservativeLocalVolume(rawVolume.radius+.19f/scale,
            rawVolume.minimumY,rawVolume.maximumY+.011f/scale);
        Need(p.radius==std::nextafter(rawWind.radius*scale,std::numeric_limits<float>::infinity())&&
            double(p.radius)>=double(rawVolume.radius)*scale+.19f,"world wind and outward rounding");
        const float x=std::abs(p.root.x);Need(x-p.radius>=4&&x+p.radius<=24&&p.root.z-p.radius>=-6&&p.root.z+p.radius<=84,"whole domain");
        Need(p.root.y==terrain.SampleHeight(p.root.x,p.root.z)&&p.pose.position.x==p.root.x&&p.pose.position.z==p.root.z,"ground root moved");
        Need(Mistwatch::RoadsideCoverRouteClearance(p.root,p.radius)&&Mistwatch::ForestCoverFootprint(terrain,p.root,p.radius,{}),"route/terrain rule");
        Mistwatch::CanopyVolume::PlantLocalVolume actual;
        Need(Mistwatch::CanopyVolume::MeasureLocalVolume(std::span<const Vertex3D>(mesh.vertices),actual),"derived volume");
        const Mistwatch::CanopyVolume::PlantVolume volume{p.pose.position,Mistwatch::CanopyVolume::ConservativeLocalVolume(
            actual.radius+.19f/scale,actual.minimumY,actual.maximumY+.011f/scale),scale,p.pose.rotation};
        Need(Mistwatch::CanopyVolume::ClearsCanopyWood(volume,before)&&Mistwatch::CanopyVolume::ClearsCanopyWood(volume,after)&&
            Mistwatch::CanopyVolume::ClearsCanopyWood(volume,existing),"full supplied volume collision");
        for(unsigned j=0;j<id;++j)Need(std::hypot(double(p.root.x)-plan.plants[j].root.x,double(p.root.z)-plan.plants[j].root.z)>=
            .65*(double(p.radius)+plan.plants[j].radius),"roots stacked");
        const auto matrix=p.pose.Matrix();unsigned sampled{},moved{},pinned{},changedWind{};std::vector<bool> changed(raw.vertices.size()),affected(raw.vertices.size());
        struct N{double x{},y{},z{};};std::vector<N> normals(raw.vertices.size());
        for(unsigned i=0;i<raw.vertices.size();++i){const auto& a=raw.vertices[i];const auto& b=mesh.vertices[i];
            Need(Same(a.x,b.x)&&Same(a.z,b.z)&&Same(a.u,b.u)&&Same(a.v,b.v)&&a.color==b.color,"position/material changed");
            const auto world=TransformPoint(matrix,{a.x,a.y,a.z});const double height=(double(a.y)-bounds.minimumY)*scale;
            const auto windBits=ExpectedWind(a.normal,height);
            Need((b.normal&0xff000000u)==windBits,"source-relative root wind formula");
            if(UnpackNormalSnorm8(a.normal).w>=0&&height<=double(.035f)){
                ++pinned;Need(UnpackNormalSnorm8(b.normal).w==1.f,"source low band not fully pinned");}
            changedWind+=windBits!=(a.normal&0xff000000u);
            double expected=a.y;
            if(height<double(.35f)){++sampled;const double delta=double(terrain.SampleHeight(world.x,world.z))-terrain.SampleHeight(p.root.x,p.root.z);
                const double t=std::clamp((height-double(.035f))/double(.35f-.035f),0.,1.);
                expected+=delta*(1.-t*t*(3.-2.*t))/scale;
            }
            Need(Same(b.y,static_cast<float>(expected)),"individual vertex conform formula");
            if(height>=double(.35f))Need(Same(a.y,b.y),"upper plant lifted");
            changed[i]=!Same(a.y,b.y);moved+=changed[i]?1u:0u;
            if(Same(a.y,bounds.minimumY)){const auto final=TransformPoint(matrix,{b.x,b.y,b.z});
                Need(std::abs(double(final.y)-terrain.SampleHeight(final.x,final.z))<=.0001,"source minimum root gap");++roots;}
        }
        unsigned affectedFaces{};
        for(unsigned f=0;f<raw.indices.size();f+=3){const auto ia=raw.indices[f],ib=raw.indices[f+1],ic=raw.indices[f+2];
            const auto cross=[](const Vertex3D& a,const Vertex3D& b,const Vertex3D& c){
                const double x=double(b.x)-a.x,y=double(b.y)-a.y,z=double(b.z)-a.z;
                const double u=double(c.x)-a.x,v=double(c.y)-a.y,w=double(c.z)-a.z;return N{y*w-z*v,z*u-x*w,x*v-y*u};};
            const auto n=cross(mesh.vertices[ia],mesh.vertices[ib],mesh.vertices[ic]),old=cross(raw.vertices[ia],raw.vertices[ib],raw.vertices[ic]);
            Need(n.x*n.x+n.y*n.y+n.z*n.z>0&&n.x*old.x+n.y*old.y+n.z*old.z>0,"face degenerate/flipped");
            for(auto i:{ia,ib,ic}){normals[i].x+=n.x;normals[i].y+=n.y;normals[i].z+=n.z;}
            if(changed[ia]||changed[ib]||changed[ic]){++affectedFaces;affected[ia]=affected[ib]=affected[ic]=true;}
        }
        unsigned rebuilt{};for(unsigned i=0;i<raw.vertices.size();++i){if(!affected[i]){Need((raw.vertices[i].normal&0xffffffu)==(mesh.vertices[i].normal&0xffffffu),"unaffected normal XYZ changed");continue;}
            ++rebuilt;const auto& n=normals[i];const double length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
            const auto expected=PackNormalSnorm8({float(n.x/length),float(n.y/length),float(n.z/length)})&0xffffffu;
            Need((mesh.vertices[i].normal&0xffffffu)==expected,"adjacent normal XYZ formula");}
        Need(p.pinnedVertices==pinned&&p.changedWindVertices==changedWind&&pinned>0,"per-plant wind counters");
        Need(p.conformCounters.sampledVertices==sampled&&p.conformCounters.movedVertices==moved&&
            p.conformCounters.affectedFaces==affectedFaces&&p.conformCounters.rebuiltNormals==rebuilt&&
            p.conformCounters.maximumGroundDelta<=double(.24f),"conform counters");deformed+=moved>0;
    }
    Need(plan.plants.empty()||roots>0,"no real source feet checked");
    std::cout<<"CORE_HERB_GEOMETRY plants="<<plan.plants.size()<<" root_vertices="<<roots<<" deformed="<<deformed
        <<" full_triangle_support=unproven wind_support=unproven\n";
}
}
int main(int argc,char** argv){try{
    Resolver();WindBoundaries();ProposalCoverage();Need(argc==3&&std::string_view(argv[1])=="--runtime-assets","usage --runtime-assets ABSOLUTE_DIR");
    const std::filesystem::path assets(argv[2]);Need(assets.is_absolute(),"absolute assets");
    std::array<Mesh3D,2> meshes;std::array<std::uint64_t,2> original{};
    for(unsigned slot=0;slot<2;++slot){auto loaded=LoadAevryndMeshFile(assets/"mistwatch-nettle"/"meshes"/Mistwatch::NettleMeshName(slot+2));
        Need(bool(loaded),"real nettle unavailable");meshes[slot]=std::move(loaded.mesh);Mistwatch::NormalizeNettleAsset(meshes[slot],slot+2);
        original[slot]=Seal(meshes[slot]);Need(original[slot]==H::SourceFingerprints[slot],"real normalized asset seal");}
    const H::Sources sources{&meshes[0],&meshes[1]};auto terrain=Mistwatch::CreateTerrain();
    // 真資產／真核心地形；這兩個遠方 box 只是清除碰撞測試夾具，不冒充完整遊戲世界。
    const std::array<BoxCollider3D,1> before{{{{500,0,500},{1,1,1}}}},after{{{{501,0,500},{1,1,1}}}};
    const auto plan=H::Create(sources,terrain,before,after),repeat=H::Create(sources,terrain,before,after);
    Plants(plan,sources,terrain,before,after);
    Need(plan.fingerprint==repeat.fingerprint&&plan.plants.size()==repeat.plants.size()&&plan.rejected==repeat.rejected,"deterministic actual terrain plan");
    for(unsigned i=0;i<plan.plants.size();++i)Need(Seal(plan.plants[i].derivedMesh)==Seal(repeat.plants[i].derivedMesh),"deterministic derived asset");
    // 平坦／真核心地形的無阻擋來源夾具均須足額；這仍不是完整遊戲碰撞／接地證明。
    auto flat=terrain;flat.Fill(0);const auto full=H::Create(sources,flat,before,after);
    Diagnostics("canonical-terrain",plan);Diagnostics("flat",full);
    Plants(full,sources,flat,before,after);Need(full.complete&&full.plants.size()==192,"flat bounded cluster shortfall");
    Need(plan.complete&&plan.plants.size()==192,"canonical terrain bounded cluster shortfall");
    LayoutBoundaries(sources,flat,before,after);
    const auto explicitDefault=H::Create(sources,flat,before,after,nullptr,{}, {}, {},H::DefaultLayout);
    Accounting(explicitDefault);Need(explicitDefault.fingerprint==full.fingerprint&&explicitDefault.authorFingerprint==full.authorFingerprint,
        "explicit default layout changed previous implicit default");
    auto custom=H::DefaultLayout;custom.centres[0].x+=.125f;custom.centres[6].z+=.125f;
    custom.rotationOffsets[0]=.2f;custom.rotationOffsets[6]=-.15f;
    const auto customSeal=ReferenceLayoutSeal(custom);
    const auto authored=H::Create(sources,flat,before,after,nullptr,{}, {}, {},custom);
    const auto authoredRepeat=H::Create(sources,flat,before,after,nullptr,{}, {}, {},custom);
    Plants(authored,sources,flat,before,after,{},custom);
    Need(authored.complete&&authored.plants.size()==192&&authored.sides[0]==96&&authored.sides[1]==96,
        "custom flat author fixture lowered original population target");
    Need(customSeal==ReferenceLayoutSeal(custom)&&authored.authorFingerprint==customSeal&&customSeal!=full.authorFingerprint&&
        authored.fingerprint!=full.fingerprint,"custom layout not bound or supplied author mutated");
    Need(authored.fingerprint==authoredRepeat.fingerprint&&authored.authorFingerprint==authoredRepeat.authorFingerprint&&
        authored.rejected==authoredRepeat.rejected&&authored.plants.size()==authoredRepeat.plants.size(),"custom layout repeat plan changed");
    bool customGeometryChanged{};
    for(unsigned i=0;i<authored.plants.size();++i){const auto& a=authored.plants[i];const auto& b=authoredRepeat.plants[i];
        Need(Same(a.root.x,b.root.x)&&Same(a.root.y,b.root.y)&&Same(a.root.z,b.root.z)&&Same(a.pose.rotation.y,b.pose.rotation.y)&&
            Same(a.pose.scale.x,b.pose.scale.x)&&a.variant==b.variant&&Seal(a.derivedMesh)==Seal(b.derivedMesh),"custom layout repeat actual geometry bits");
        customGeometryChanged|=!Same(a.root.x,full.plants[i].root.x)||!Same(a.root.z,full.plants[i].root.z);
    }
    Need(customGeometryChanged,"custom author changed receipt only instead of plant positions");
    const auto allowAll=H::Create(sources,flat,before,after,nullptr,{}, {},[](const H::Plant&){return true;});
    Accounting(allowAll);Need(allowAll.fingerprint==full.fingerprint,"optional selector changed default selection");
    unsigned filteredCalls{};
    const auto onlyNarrow=H::Create(sources,flat,before,after,nullptr,{}, {},[&](const H::Plant& p){
        ++filteredCalls;Need(p.sourceFingerprint==original[p.variant-2]&&p.derivedFingerprint==Seal(p.derivedMesh)&&
            p.conformResult&&!p.wholeSupportProven,"selector observed unfinished candidate");return p.variant==2;});
    Accounting(onlyNarrow);Need(!onlyNarrow.plants.empty()&&onlyNarrow.variants[1]==0&&
        filteredCalls==onlyNarrow.plants.size()+onlyNarrow.rejected[static_cast<unsigned>(H::Rejection::SupportSelection)],"known selector ignored");
    unsigned deniedCalls{};
    const auto denied=H::Create(sources,flat,before,after,nullptr,{}, {},[&](const H::Plant&){++deniedCalls;return false;});
    Accounting(denied);Need(denied.plants.empty()&&!denied.complete&&denied.failedClusters==12&&denied.proposals==H::MaximumProposals&&
        deniedCalls>0&&deniedCalls==denied.rejected[static_cast<unsigned>(H::Rejection::SupportSelection)],"selection failure fabricated full population");
    const auto& first=full.plants.front();const std::array<BoxCollider3D,1> blocking{{{first.root+Vec3{0,.3f,0},{.12f,.3f,.12f}}}};
    const auto blockedAfter=H::Create(sources,flat,before,blocking),blockedBefore=H::Create(sources,flat,blocking,after);
    Accounting(blockedAfter);Accounting(blockedBefore);
    Need(blockedAfter.rejected[static_cast<unsigned>(H::Rejection::World)]>0&&blockedBefore.rejected[static_cast<unsigned>(H::Rejection::World)]>0,"one bridge state ignored");
    const auto retained=H::Create(sources,flat,before,after,nullptr,blocking);Accounting(retained);
    Need(retained.rejected[static_cast<unsigned>(H::Rejection::Vegetation)]>0,"retained hard vegetation ignored");
    H::Sources wrong=sources;wrong[0]=nullptr;Reject([&]{(void)H::Create(wrong,flat,before,after);},"CORE_HERB_SOURCE_TOPOLOGY");
    std::swap(wrong[0],wrong[1]);Reject([&]{(void)H::Create(wrong,flat,before,after);},"CORE_HERB_SOURCE_TOPOLOGY");
    auto changed=meshes[0];changed.vertices[0].color^=1;wrong={&changed,&meshes[1]};
    Reject([&]{(void)H::Create(wrong,flat,before,after);},"CORE_HERB_SOURCE_IDENTITY");
    changed=meshes[0];Mistwatch::NormalizeNettleAsset(changed,2);
    Reject([&]{(void)H::Create(wrong,flat,before,after);},"CORE_HERB_SOURCE_IDENTITY");
    auto badTerrain=flat;badTerrain.SetHeight(0,0,std::numeric_limits<float>::quiet_NaN());
    Reject([&]{(void)H::Create(sources,badTerrain,before,after);},"CORE_HERB_TERRAIN_FINITE");
    const HeightField3D small;Reject([&]{(void)H::Create(sources,small,before,after);},"CORE_HERB_TERRAIN_LAYOUT");
    auto badBox=before;badBox[0].extents.x=0;
    Reject([&]{(void)H::Create(sources,flat,badBox,after);},"CORE_HERB_WORLD_INVALID");
    Reject([&]{(void)H::Create(sources,flat,before,after,nullptr,badBox);},"CORE_HERB_EXISTING_INVALID");
    Reject([&]{(void)H::Create(sources,flat,{},after);},"CORE_HERB_WORLD_STATES");
    const auto missing=H::Create(sources,flat,before,after,nullptr,{},[](float,float)->std::optional<double>{return {};});
    Accounting(missing);Need(!missing.complete&&missing.plants.empty()&&missing.proposals==H::MaximumProposals,"missing surface fabricated");
    for(unsigned slot=0;slot<2;++slot)Need(Seal(meshes[slot])==original[slot],"original source mutated");
    std::cout<<"CORE_HERB_CPU checks="<<checks<<" negatives="<<negatives<<" actual_count="<<plan.plants.size()
        <<" actual_complete="<<plan.complete<<" flat_count="<<full.plants.size()<<" proposals="<<plan.proposals
        <<" fingerprint="<<plan.fingerprint<<" source2="<<original[0]<<" source3="<<original[1]
        <<" root_wind="<<H::RootWindProfile<<" pinned_vertices="<<plan.pinnedVertices<<" changed_wind_vertices="<<plan.changedWindVertices
        <<" sampling_profile="<<H::SamplingProfile
        <<" author_fingerprint="<<plan.authorFingerprint<<" author_schema=3"
        <<" supplied_boxes_fixture_only=1 whole_support_proven=0 native_passed=0 art_passed=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"CORE_HERB_FAIL "<<e.what()<<'\n';return 1;}}
