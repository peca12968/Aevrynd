#pragma once
#include "MistwatchRootSupport.h"
#include <array>
#include <limits>
#include <set>

namespace Mistwatch::FirRootContact {
inline constexpr std::string_view Profile="fir-source-bottom-loop-contact-v1";
inline constexpr std::uint64_t SourceFingerprint=9544599023395000417ull;
inline constexpr unsigned MaximumIterations=4096;
inline constexpr double ResidualTolerance=1.e-8;
// 明確的候選資源／變形上限，超出即回報失敗，不偷偷擴大容差。
inline constexpr double MaximumWorldDisplacement=1.0;
enum class Result : std::uint8_t { Unproven,Convergence,Displacement,Geometry,Coverage,RingGap,LowerGap,AllGap,Safe };
struct Counts {
    unsigned sourceVertices{},sourceIndices{},welded{},bottomRing{},topRing{},freeNodes{},fixedNodes{};
    unsigned boundaryEdges{},bottomVertexReferences{},lowerFaces{},affectedFaces{},affectedNormals{},iterations{};
    unsigned footprintFans{},coveredFans{},ringSegments{},ringIntersections{},lowerIntersections{};
    unsigned allFaces{},allIntersections{};
};
struct Candidate {
    Aevrynd::Mesh3D mesh;
    bool derived{},proofComplete{},safe{};
    Result result{Result::Unproven};std::string failure;
    Counts counts;
    std::uint64_t sourceFingerprint{},derivedFingerprint{},surfaceFingerprint{},provenanceFingerprint{};
    double minimumRingGap{std::numeric_limits<double>::quiet_NaN()};
    double maximumRingGap{std::numeric_limits<double>::quiet_NaN()};
    double minimumLowerGap{std::numeric_limits<double>::quiet_NaN()};
    double minimumAllGap{std::numeric_limits<double>::quiet_NaN()};
    double maxDisplacement{},harmonicResidual{std::numeric_limits<double>::infinity()};
};
namespace Detail {
using namespace Aevrynd;
namespace D=::Mistwatch::LandscapeSpeciesDetail;
namespace S=::Mistwatch::RootSupportDetail;
inline void Need(bool ok,const char* reason){if(!ok)throw std::invalid_argument(reason);}
inline bool Same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
using Edge=std::pair<unsigned,unsigned>;
struct Topology {
    std::vector<unsigned> welded,representatives;
    std::vector<std::set<unsigned>> adjacency;
    std::vector<bool> bottom;
    std::vector<Edge> bottomEdges;
};
inline Topology SourceTopology(const Mesh3D& source,Counts& counts,std::uint64_t& provenance){
    Need(source.vertices.size()==222&&source.indices.size()==279,"FIR_ROOT_SOURCE_COUNTS");
    D::ValidateMesh(source,false);
    Need(S::MeshFingerprint(source)==SourceFingerprint,"FIR_ROOT_SOURCE_SEAL");
    Topology t;std::map<std::array<std::uint32_t,3>,unsigned> unique;
    for(unsigned i=0;i<source.vertices.size();++i){const auto& v=source.vertices[i];
        const std::array<std::uint32_t,3> key{v.x==0?0:std::bit_cast<std::uint32_t>(v.x),
            v.y==0?0:std::bit_cast<std::uint32_t>(v.y),v.z==0?0:std::bit_cast<std::uint32_t>(v.z)};
        const auto [it,added]=unique.emplace(key,static_cast<unsigned>(t.representatives.size()));
        if(added)t.representatives.push_back(i);t.welded.push_back(it->second);
    }
    Need(t.representatives.size()==56,"FIR_ROOT_WELDED_COUNT");t.adjacency.resize(56);
    std::map<Edge,unsigned> edges;
    for(unsigned f=0;f<source.indices.size();f+=3)for(unsigned e=0;e<3;++e){
        auto a=t.welded[source.indices[f+e]],b=t.welded[source.indices[f+(e+1)%3]];
        Need(a!=b,"FIR_ROOT_COLLAPSED_EDGE");t.adjacency[a].insert(b);t.adjacency[b].insert(a);
        if(a>b)std::swap(a,b);++edges[{a,b}];
    }
    std::vector<std::set<unsigned>> boundary(56);
    for(const auto& [edge,count]:edges){Need(count<=2,"FIR_ROOT_NONMANIFOLD");if(count!=1)continue;
        boundary[edge.first].insert(edge.second);boundary[edge.second].insert(edge.first);++counts.boundaryEdges;}
    Need(counts.boundaryEdges==19,"FIR_ROOT_BOUNDARY_COUNT");
    for(const auto& adjacent:boundary)Need(adjacent.empty()||adjacent.size()==2,"FIR_ROOT_BOUNDARY_NOT_CLOSED");
    unsigned minimum{};for(unsigned i=1;i<56;++i)
        if(source.vertices[t.representatives[i]].y<source.vertices[t.representatives[minimum]].y)minimum=i;
    Need(!boundary[minimum].empty(),"FIR_ROOT_MINIMUM_NOT_BOUNDARY");
    t.bottom.assign(56,false);std::vector<unsigned> pending{minimum};
    while(!pending.empty()){const auto i=pending.back();pending.pop_back();if(t.bottom[i])continue;
        t.bottom[i]=true;for(auto j:boundary[i])pending.push_back(j);}
    std::vector<bool> top(56,false);unsigned topStart=56;
    for(unsigned i=0;i<56;++i)if(!boundary[i].empty()&&!t.bottom[i]){topStart=i;break;}
    Need(topStart<56,"FIR_ROOT_TOP_MISSING");pending.push_back(topStart);
    while(!pending.empty()){const auto i=pending.back();pending.pop_back();if(top[i])continue;
        top[i]=true;for(auto j:boundary[i])pending.push_back(j);}
    for(unsigned i=0;i<56;++i){const auto y=source.vertices[t.representatives[i]].y;
        if(t.bottom[i]){++counts.bottomRing;Need(y<.35f&&!top[i],"FIR_ROOT_BOTTOM_DOMAIN");}
        if(top[i]){++counts.topRing;Need(y>13.f,"FIR_ROOT_TOP_DOMAIN");}
        Need(boundary[i].empty()||t.bottom[i]||top[i],"FIR_ROOT_EXTRA_BOUNDARY");
        if(!t.bottom[i]&&y<.35f)++counts.freeNodes;
    }
    Need(counts.bottomRing==16&&counts.topRing==3&&counts.freeNodes==15,"FIR_ROOT_COMPONENT_COUNTS");
    counts.sourceVertices=222;counts.sourceIndices=279;counts.welded=56;counts.fixedNodes=41;
    provenance=14695981039346656037ull;
    // 記錄封印來源的完整底環邊；不以最終 gap 或水平高度裁掉困難邊。
    for(const auto& [edge,count]:edges)if(count==1&&t.bottom[edge.first]){
        Need(t.bottom[edge.second],"FIR_ROOT_SPLIT_BOTTOM");t.bottomEdges.push_back(edge);
        for(auto node:{edge.first,edge.second}){const auto index=t.representatives[node];D::Word(provenance,index);
            const auto& v=source.vertices[index];for(float value:{v.x,v.y,v.z})D::Word(provenance,std::bit_cast<std::uint32_t>(value));}
    }
    Need(t.bottomEdges.size()==16,"FIR_ROOT_BOTTOM_EDGES");
    for(auto i:t.welded)counts.bottomVertexReferences+=t.bottom[i]?1u:0u;
    Need(counts.bottomVertexReferences==44,"FIR_ROOT_BOTTOM_REFERENCE_COUNT");
    // 每個自由連接分量都必須通往底環或固定上幹，不能讓奇異系統以零初值冒充收斂。
    for(unsigned start=0;start<56;++start)if(!t.bottom[start]&&source.vertices[t.representatives[start]].y<.35f){
        std::array<bool,56> seen{};pending={start};bool anchored{};
        while(!pending.empty()){const auto i=pending.back();pending.pop_back();if(seen[i])continue;seen[i]=true;
            if(t.bottom[i]||source.vertices[t.representatives[i]].y>=.35f){anchored=true;continue;}
            for(auto j:t.adjacency[i])pending.push_back(j);}
        Need(anchored,"FIR_ROOT_UNANCHORED_FREE_COMPONENT");
    }
    return t;
}
inline std::vector<D::P> AreaDomain(const std::vector<D::P>& points){
    if(points.size()<3)return{};bool hasArea{};
    for(std::size_t j=1;j+1<points.size();++j)hasArea|=D::Area(points[0],points[j],points[j+1]).Sign()!=0;
    // CoplanarIntersection 回傳全部極值點而非排序多邊形，必須先以 exact hull 排序。
    return hasArea?D::Hull(points):std::vector<D::P>{};
}
inline D::R PolygonArea(const std::vector<D::P>& points){
    const auto polygon=AreaDomain(points);D::R area;
    for(std::size_t j=1;j+1<polygon.size();++j)area=area+D::Abs(D::Area(polygon[0],polygon[j],polygon[j+1]));
    return area;
}
inline bool CoveredHull(const std::vector<D::P>& points,const D::Surface& surface,Counts& counts){
    const auto hull=D::Hull(points);
    for(unsigned i=1;i+1<hull.size();++i){++counts.footprintFans;const D::T fan{hull[0],hull[i],hull[i+1]};
        const auto expected=D::Abs(D::Area(fan[0],fan[1],fan[2]));D::R covered;
        std::vector<D::T> pieces;
        for(auto f:surface.Query(fan)){const auto polygon=AreaDomain(D::PlanarIntersection(fan,surface.Face(f)));
            const auto area=PolygonArea(polygon);if(area.Sign()==0)continue;covered=covered+area;
            for(std::size_t j=1;j+1<polygon.size();++j){const D::T piece{polygon[0],polygon[j],polygon[j+1]};
                if(D::Area(piece[0],piece[1],piece[2]).Sign()==0)continue;
                // 面積總和不能讓重疊區補償缺口，因此另拒絕任何正面積重疊。
                for(const auto& prior:pieces)if(PolygonArea(D::PlanarIntersection(piece,prior)).Sign()!=0)return false;
                pieces.push_back(piece);}
        }
        if(Compare(covered,expected)!=0)return false;++counts.coveredFans;
    }
    return counts.footprintFans>0&&counts.footprintFans==counts.coveredFans;
}
}
// safe 僅證明此來源底環接觸與完整網格不穿入指定地表；不是森林坡度、其他物件碰撞、通路或美術通過。
// 原 broadleaf 的 .97/.10 門完全不變；森林 layout 的坡度资格由呼叫者另外驗證。
inline Candidate Derive(const Aevrynd::Mesh3D& rawSource,const Aevrynd::Transform3D& pose,
    const Aevrynd::Mesh3D& actualLandscape){
    using namespace Aevrynd;namespace D=LandscapeSpeciesDetail;namespace S=RootSupportDetail;namespace F=Detail;
    Candidate out;out.sourceFingerprint=S::MeshFingerprint(rawSource);
    const auto topology=F::SourceTopology(rawSource,out.counts,out.provenanceFingerprint);
    S::ValidatePose(pose);D::ValidateMesh(actualLandscape,true);out.surfaceFingerprint=S::MeshFingerprint(actualLandscape);
    out.mesh=rawSource;
    const auto fail=[&](Result result,const char* message){out.result=result;out.failure=message;
        out.safe=false;out.derivedFingerprint=S::MeshFingerprint(out.mesh);return out;};
    const D::Surface surface(actualLandscape);const auto matrix=pose.Matrix();
    std::array<double,56> displacement{},ground{},minimum{};std::array<bool,56> free{};
    for(unsigned i=0;i<56;++i){const auto& v=rawSource.vertices[topology.representatives[i]];
        const auto world=TransformPoint(matrix,{v.x,v.y,v.z});F::Need(D::Finite(world),"FIR_ROOT_WORLD_NONFINITE");
        ground[i]=(surface.At(world).Approximate()-double(pose.position.y))/double(pose.scale.x);
        minimum[i]=ground[i]-double(v.y);F::Need(std::isfinite(minimum[i]),"FIR_ROOT_GROUND_NONFINITE");
        free[i]=!topology.bottom[i]&&v.y<.35f;
        if(topology.bottom[i])displacement[i]=minimum[i];else if(free[i])displacement[i]=(std::max)(0.0,minimum[i]);
    }
    // 單位邊權圖上的投影 Gauss-Seidel；底環與 y>=.35 原頂點是不可漂移的 Dirichlet 邊界。
    const auto projected=[&](unsigned i){double sum{};for(auto j:topology.adjacency[i])sum+=displacement[j];
        F::Need(!topology.adjacency[i].empty(),"FIR_ROOT_ISOLATED_NODE");
        return (std::max)(minimum[i],sum/double(topology.adjacency[i].size()));};
    for(unsigned iteration=0;iteration<MaximumIterations;++iteration){
        for(unsigned i=0;i<56;++i)if(free[i])displacement[i]=projected(i);
        out.harmonicResidual=0;
        for(unsigned i=0;i<56;++i){if(!std::isfinite(displacement[i]))return fail(Result::Convergence,"FIR_ROOT_SOLVER_NONFINITE");
            if(std::abs(displacement[i])*double(pose.scale.x)>MaximumWorldDisplacement)
                return fail(Result::Displacement,"FIR_ROOT_DISPLACEMENT_LIMIT");
            if(free[i])out.harmonicResidual=(std::max)(out.harmonicResidual,std::abs(displacement[i]-projected(i)));}
        out.counts.iterations=iteration+1;if(out.harmonicResidual<=ResidualTolerance)break;
    }
    if(out.harmonicResidual>ResidualTolerance)return fail(Result::Convergence,"FIR_ROOT_SOLVER_NOT_CONVERGED");
    std::array<float,56> finalY{};
    for(unsigned i=0;i<56;++i){const float original=rawSource.vertices[topology.representatives[i]].y;
        finalY[i]=topology.bottom[i]?static_cast<float>(ground[i]):free[i]?static_cast<float>(double(original)+displacement[i]):original;
        if(!std::isfinite(finalY[i]))return fail(Result::Geometry,"FIR_ROOT_DERIVED_NONFINITE");
        out.maxDisplacement=(std::max)(out.maxDisplacement,std::abs(double(finalY[i])-original)*double(pose.scale.x));}
    if(out.maxDisplacement>MaximumWorldDisplacement)return fail(Result::Displacement,"FIR_ROOT_ROUNDED_DISPLACEMENT_LIMIT");
    for(unsigned i=0;i<out.mesh.vertices.size();++i)out.mesh.vertices[i].y=finalY[topology.welded[i]];
    std::vector<Vec3> normals(222);std::vector<bool> affected(222);
    for(unsigned f=0;f<rawSource.indices.size();f+=3){const auto a=rawSource.indices[f],b=rawSource.indices[f+1],c=rawSource.indices[f+2];
        const auto before=Cross(D::Vertex(rawSource,b)-D::Vertex(rawSource,a),D::Vertex(rawSource,c)-D::Vertex(rawSource,a));
        const auto after=Cross(D::Vertex(out.mesh,b)-D::Vertex(out.mesh,a),D::Vertex(out.mesh,c)-D::Vertex(out.mesh,a));
        if(!D::Finite(after)||Length(after)<=0||Dot(before,after)<=0)return fail(Result::Geometry,"FIR_ROOT_FACE_ORIENTATION");
        normals[a]=normals[a]+after;normals[b]=normals[b]+after;normals[c]=normals[c]+after;
        if(!F::Same(rawSource.vertices[a].y,out.mesh.vertices[a].y)||!F::Same(rawSource.vertices[b].y,out.mesh.vertices[b].y)||
            !F::Same(rawSource.vertices[c].y,out.mesh.vertices[c].y)){
            affected[a]=affected[b]=affected[c]=true;++out.counts.affectedFaces;}
    }
    for(unsigned i=0;i<222;++i)if(affected[i]){
        if(!D::Finite(normals[i])||Length(normals[i])<=1.e-8f)return fail(Result::Geometry,"FIR_ROOT_NORMAL_DEGENERATE");
        out.mesh.vertices[i].normal=(PackNormalSnorm8(Normalize(normals[i]))&0x00ffffffu)|
            (rawSource.vertices[i].normal&0xff000000u);
        ++out.counts.affectedNormals;
    }
    for(unsigned i=0;i<222;++i){const auto& a=rawSource.vertices[i];const auto& b=out.mesh.vertices[i];
        F::Need(F::Same(a.x,b.x)&&F::Same(a.z,b.z)&&F::Same(a.u,b.u)&&F::Same(a.v,b.v)&&a.color==b.color&&
            (a.normal&0xff000000u)==(b.normal&0xff000000u),"FIR_ROOT_PRESERVED_FIELDS");
        F::Need(a.y<.35f||F::Same(a.y,b.y),"FIR_ROOT_UPPER_MOVED");
        F::Need(F::Same(b.y,finalY[topology.welded[i]]),"FIR_ROOT_SPLIT_WELD");}
    F::Need(rawSource.indices==out.mesh.indices,"FIR_ROOT_TOPOLOGY_CHANGED");
    out.derived=true;out.derivedFingerprint=S::MeshFingerprint(out.mesh);
    // 足域來源由原始整面決定：凡原面觸及低於 .35 的頂點，完整三角面（含上鄰點）皆驗，不以最終 gap 篩選。
    std::vector<D::T> lower,all;std::vector<D::P> support;
    for(unsigned f=0;f<rawSource.indices.size();f+=3){bool low{};
        for(unsigned j=0;j<3;++j)low|=rawSource.vertices[rawSource.indices[f+j]].y<.35f;
        const auto face=S::WorldTriangle(out.mesh,f,matrix);all.push_back(face);
        // 完整 93 面的投影都納入覆蓋證明，上幹不可因局部超出地表而只驗到部分交域。
        support.insert(support.end(),face.begin(),face.end());if(low)lower.push_back(face);}
    out.counts.lowerFaces=static_cast<unsigned>(lower.size());
    out.counts.allFaces=static_cast<unsigned>(all.size());F::Need(out.counts.allFaces==93,"FIR_ROOT_ALL_FACE_COUNT");
    if(lower.empty()||!F::CoveredHull(support,surface,out.counts))return fail(Result::Coverage,"FIR_ROOT_INCOMPLETE_FOOTPRINT");
    const auto negative=D::R::Float(-.04f),positive=D::R::Float(.04f);D::Range ringRange,lowerRange;
    for(const auto& edge:topology.bottomEdges){
        const auto a=D::Exact::ToExact(TransformPoint(matrix,D::Vertex(out.mesh,topology.representatives[edge.first])));
        const auto b=D::Exact::ToExact(TransformPoint(matrix,D::Vertex(out.mesh,topology.representatives[edge.second])));
        const auto range=D::Overlay(D::T{a,b,b},surface);++out.counts.ringSegments;out.counts.ringIntersections+=range.points;
        if(!range.points)return fail(Result::Coverage,"FIR_ROOT_RING_NO_OVERLAY");
        D::Record(ringRange,range.minimum);D::Record(ringRange,range.maximum);
    }
    out.minimumRingGap=ringRange.minimum.Approximate();out.maximumRingGap=ringRange.maximum.Approximate();
    if(Compare(ringRange.minimum,negative)<0||Compare(ringRange.maximum,positive)>0)return fail(Result::RingGap,"FIR_ROOT_RING_GAP");
    for(const auto& face:lower){const auto range=D::Overlay(face,surface);out.counts.lowerIntersections+=range.points;
        if(!range.points)return fail(Result::Coverage,"FIR_ROOT_LOWER_NO_OVERLAY");D::Record(lowerRange,range.minimum);}
    out.minimumLowerGap=lowerRange.minimum.Approximate();
    if(Compare(lowerRange.minimum,negative)<0)return fail(Result::LowerGap,"FIR_ROOT_LOWER_PENETRATION");
    // 高於原始 .35 的位置不准移動，但仍逐整面驗穿地；不能因無法修復而略過。
    D::Range allRange;
    for(const auto& face:all){const auto range=D::Overlay(face,surface);out.counts.allIntersections+=range.points;
        if(!range.points)return fail(Result::Coverage,"FIR_ROOT_ALL_NO_OVERLAY");D::Record(allRange,range.minimum);}
    out.minimumAllGap=allRange.minimum.Approximate();out.proofComplete=true;
    if(Compare(allRange.minimum,negative)<0)return fail(Result::AllGap,"FIR_ROOT_ALL_PENETRATION");
    out.result=Result::Safe;out.safe=true;return out;
}
}
