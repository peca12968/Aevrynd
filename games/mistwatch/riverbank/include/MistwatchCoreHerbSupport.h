#pragma once
#include "MistwatchOuterUnderstory.h"
#include "MistwatchCoreHerbClusters.h"
#include <chrono>

namespace Mistwatch::CoreHerbSupport {
using namespace Aevrynd;
namespace D=LandscapeSpeciesDetail;
namespace O=OuterUnderstory;
inline constexpr std::string_view Profile="source-foot-root-pinned-v1";
struct Proof {
    O::SupportProof contact;
    std::uint64_t sourceFingerprint{},derivedFingerprint{},surfaceFingerprint{};
    unsigned sourceFootPieces{},allFaces{},windPinnedVertices{};
};
// 只供明確CPU診斷；未傳入時不讀時計，也不改幾何判定或期限。
struct Timings {std::array<std::uint64_t,5> nanoseconds{};};
namespace Detail {
class PhaseTiming {
    Timings* target_;unsigned phase_{};std::chrono::steady_clock::time_point previous_;
public:
    explicit PhaseTiming(Timings* target):target_(target){if(target_){target_->nanoseconds.fill(0);previous_=std::chrono::steady_clock::now();}}
    void Advance(unsigned next){if(!target_)return;const auto now=std::chrono::steady_clock::now();
        if(phase_<5)target_->nanoseconds[phase_]+=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now-previous_).count());
        previous_=now;phase_=next;
    }
    ~PhaseTiming(){Advance(5);}
};
struct Tagged {O::Detail::WeightedPoint actual;D::R authorHeight;};
inline std::vector<Tagged> Foot(std::span<const Tagged> face,const D::R& limit){
    std::vector<Tagged> result;
    for(unsigned i=0;i<face.size();++i){const auto& a=face[i];const auto& b=face[(i+1)%face.size()];
        const auto da=a.authorHeight-limit,db=b.authorHeight-limit;
        if(da.Sign()<=0)result.push_back(a);
        if((da.Sign()<=0)!=(db.Sign()<=0)){const auto t=da/(da-db);
            result.push_back({{D::Exact::Interpolate(a.actual.point,b.actual.point,t),a.actual.wind+(b.actual.wind-a.actual.wind)*t},limit});}
    }
    return result;
}
// 每個source頂點只算一次精確根帶距離；仍依原邊方向／次序裁切，保留邊界和退化片段。
inline std::vector<Tagged> FootPrepared(std::span<const Tagged> face,std::span<const D::R> deltas,
                                      std::span<const int> signs,const D::R& limit){
    D::Need(face.size()==deltas.size()&&face.size()==signs.size(),"CORE_HERB_FOOT_PREPARED_SIZE");
    std::vector<Tagged> result;
    for(unsigned i=0;i<face.size();++i){const auto next=(i+1)%face.size();
        const auto& a=face[i];const auto& b=face[next];
        if(signs[i]<=0)result.push_back(a);
        if((signs[i]<=0)!=(signs[next]<=0)){const auto t=deltas[i]/(deltas[i]-deltas[next]);
            result.push_back({{D::Exact::Interpolate(a.actual.point,b.actual.point,t),a.actual.wind+(b.actual.wind-a.actual.wind)*t},limit});}
    }
    return result;
}
}
// 坡面形變後，根部薄層依未變動的原來源高度定義；不能把較高的葉片因低坡世界Y較小而稱為根。
// 原 OuterUnderstory 的水平世界高度裁切政策不變。此獨立政策保留同一整面穿地、35mm根帶、40mm間隙與風擺上限。
inline Proof Check(const D::Surface& surface,const Mesh3D& raw,const Mesh3D& derived,
                   const Transform3D& pose,Vec3 root,float radius,std::uint64_t surfaceFingerprint,bool planeBounds=true,
                   Timings* timings=nullptr,bool footprintBroadPhase=true,bool reuseVertices=true){
    Detail::PhaseTiming timing(timings);
    Proof out;auto& proof=out.contact;
    D::ValidateMesh(raw,false);D::ValidateMesh(derived,false);
    out.sourceFingerprint=CoreHerbClusters::Detail::MeshFingerprint(raw);
    D::Need(out.sourceFingerprint==CoreHerbClusters::SourceFingerprints[0]||out.sourceFingerprint==CoreHerbClusters::SourceFingerprints[1],
        "CORE_HERB_SUPPORT_SOURCE_IDENTITY");
    out.derivedFingerprint=CoreHerbClusters::Detail::MeshFingerprint(derived);out.surfaceFingerprint=surfaceFingerprint;
    D::Need(surfaceFingerprint!=0&&raw.indices==derived.indices&&raw.vertices.size()==derived.vertices.size()&&
        Session::Finite(root)&&Session::Finite(pose.position)&&Session::Finite(pose.rotation)&&Session::Finite(pose.scale)&&
        pose.rotation.x==0&&pose.rotation.z==0&&pose.scale.x>0&&pose.scale.x==pose.scale.y&&pose.scale.x==pose.scale.z&&
        std::isfinite(radius)&&radius>0&&radius<=4,"CORE_HERB_SUPPORT_INPUT");
    float minimumY=raw.vertices.front().y;for(const auto& v:raw.vertices)minimumY=(std::min)(minimumY,v.y);
    const auto matrix=pose.Matrix();std::vector<Detail::Tagged> points;points.reserve(raw.vertices.size());
    const auto same=[](float a,float b){return std::bit_cast<unsigned>(a)==std::bit_cast<unsigned>(b);};
    for(unsigned i=0;i<raw.vertices.size();++i){const auto& a=raw.vertices[i];const auto& b=derived.vertices[i];
        D::Need(same(a.x,b.x)&&same(a.z,b.z)&&same(a.u,b.u)&&same(a.v,b.v)&&a.color==b.color,
            "CORE_HERB_SUPPORT_MATERIAL_OR_XZ_CHANGED");
        const auto n=UnpackNormalSnorm8(b.normal);const float wind=n.w>=0?1.f-std::clamp(n.w,0.f,1.f):0;
        const double height=(double(a.y)-minimumY)*pose.scale.x;
        D::Need((b.normal&0xff000000u)==CoreHerbClusters::Detail::RootWindBits(a.normal,height),
            "CORE_HERB_SUPPORT_WIND_IDENTITY");
        if(height<=double(.035f)){D::Need(wind==0,"CORE_HERB_SUPPORT_ROOT_WIND_NOT_PINNED");++out.windPinnedVertices;}
        points.push_back({{D::Exact::ToExact(TransformPoint(matrix,{b.x,b.y,b.z})),D::R::Float(wind)},
            (D::R::Float(a.y)-D::R::Float(minimumY))*D::R::Float(pose.scale.x)});
    }
    timing.Advance(1);
    const std::array<D::P,4> square{D::Exact::ToExact(Vec3{root.x-radius,root.y,root.z-radius}),
        D::Exact::ToExact(Vec3{root.x+radius,root.y,root.z-radius}),D::Exact::ToExact(Vec3{root.x+radius,root.y,root.z+radius}),
        D::Exact::ToExact(Vec3{root.x-radius,root.y,root.z+radius})};
    D::Range heights;double maxGradient{};const auto rootHeight=D::R::Float(root.y);
    for(unsigned half=0;half<2;++half){const D::T face{square[0],square[half+1],square[half+2]};
        const auto footprintBounds=O::Detail::GroundCache::ProjectedBounds(face);
        const auto expected=D::Abs(D::Area(face[0],face[1],face[2]));D::R covered;
        for(auto id:surface.Query(face)){const auto ground=surface.Face(id);
            if(footprintBroadPhase&&!O::Detail::GroundCache::Overlaps(footprintBounds,O::Detail::GroundCache::ProjectedBounds(ground)))continue;
            const auto intersection=D::PlanarIntersection(face,ground);
            if(intersection.empty())continue;
            for(const auto& p:intersection){const auto y=D::Height(ground,p);D::Record(heights,y);
                const auto delta=D::Abs(y-rootHeight);proof.maximumHeightDelta=(std::max)(proof.maximumHeightDelta,static_cast<float>(delta.Approximate()));
                if(Compare(delta,D::R::Float(O::HeightLimit))>0){proof.rejection=O::Reject::Height;return out;}}
            bool area{};for(unsigned j=1;j+1<intersection.size();++j)area|=D::Area(intersection[0],intersection[j],intersection[j+1]).Sign()!=0;
            if(!area)continue;const auto hull=D::Hull(intersection);
            for(unsigned j=1;j+1<hull.size();++j)covered=covered+D::Abs(D::Area(hull[0],hull[j],hull[j+1]));
            const float n=surface.NormalY(id);proof.minimumNormalY=(std::min)(proof.minimumNormalY,n);++proof.footprintFaces;
            if(!std::isfinite(n)||n<O::SlopeLimit){proof.rejection=O::Reject::Slope;return out;}
            maxGradient=(std::max)(maxGradient,std::sqrt((std::max)(0.,1.-double(n)*n))/n);
        }
        if(Compare(covered,expected)!=0){proof.rejection=O::Reject::Support;return out;}
    }
    if(!heights.points||!proof.footprintFaces)return out;
    timing.Advance(2);
    O::Detail::GroundCache cache(surface,square,proof,footprintBroadPhase);
    for(const auto& p:points)if(!cache.Covers(p.actual.point))return out;
    const auto down=D::R::Float(-static_cast<float>(maxGradient*O::WindRadius+.000001));
    const auto up=D::R::Float(static_cast<float>(maxGradient*O::WindRadius+O::WindUp+.000001));
    const auto penetrate=D::R::Float(-O::ContactLimit),floatLimit=D::R::Float(O::ContactLimit);
    const auto footLimit=D::R::Float(.035f);
    std::vector<D::P> lowVertices;std::vector<D::R> footDeltas;std::vector<int> footSigns;
    if(reuseVertices){lowVertices.reserve(points.size());footDeltas.reserve(points.size());footSigns.reserve(points.size());
        for(const auto& p:points){auto low=p.actual.point;low[1]=low[1]+down*p.actual.wind;lowVertices.push_back(std::move(low));
            footDeltas.push_back(p.authorHeight-footLimit);footSigns.push_back(footDeltas.back().Sign());}
    }
    D::Range lower,footRange;
    for(unsigned f=0;f<raw.indices.size();f+=3){timing.Advance(3);const std::array<Detail::Tagged,3> tagged{
        points[raw.indices[f]],points[raw.indices[f+1]],points[raw.indices[f+2]]};
        const auto a=raw.indices[f],b=raw.indices[f+1],c=raw.indices[f+2];
        const auto before=Cross(D::Vertex(raw,b)-D::Vertex(raw,a),D::Vertex(raw,c)-D::Vertex(raw,a));
        const auto after=Cross(D::Vertex(derived,b)-D::Vertex(derived,a),D::Vertex(derived,c)-D::Vertex(derived,a));
        D::Need(Dot(before,after)>0,"CORE_HERB_SUPPORT_FACE_ORIENTATION");
        const std::array<O::Detail::WeightedPoint,3> actual{tagged[0].actual,tagged[1].actual,tagged[2].actual};
        const auto low=reuseVertices?D::T{lowVertices[a],lowVertices[b],lowVertices[c]}:O::Detail::Shift(actual,down);
        auto y=low[0][1];for(unsigned j=1;j<3;++j)if(Compare(low[j][1],y)<0)y=low[j][1];
        const auto bound=y-heights.maximum;++out.allFaces;
        if(Compare(bound,penetrate)>=0){D::Record(lower,bound);++proof.skippedFaces;}
        else{auto range=planeBounds?cache.ConservativeRange(low):D::Range{};
            if(range.points&&Compare(range.minimum,penetrate)>=0){D::Record(lower,range.minimum);++proof.skippedFaces;}
            else{range=cache.Overlay(low);++proof.evaluatedFaces;
            if(!range.points)return out;
            if(Compare(range.minimum,penetrate)<0){proof.minimumGapBound=static_cast<float>(range.minimum.Approximate());return out;}
            D::Record(lower,range.minimum);
            }
        }
        timing.Advance(4);
        if(reuseVertices&&footSigns[a]>0&&footSigns[b]>0&&footSigns[c]>0)continue;
        const auto clipped=reuseVertices?Detail::FootPrepared(tagged,
            std::array<D::R,3>{footDeltas[a],footDeltas[b],footDeltas[c]},
            std::array<int,3>{footSigns[a],footSigns[b],footSigns[c]},footLimit):Detail::Foot(tagged,footLimit);
        if(clipped.empty())continue;
        const auto piece=[&](const std::array<O::Detail::WeightedPoint,3>& tri){
            const auto shifted=O::Detail::Shift(tri,up);
            auto range=planeBounds?cache.ConservativeRange(shifted):D::Range{};
            if(!range.points||Compare(range.maximum,floatLimit)>0)range=cache.Overlay(shifted);
            ++proof.footPieces;++out.sourceFootPieces;
            if(!range.points)return false;
            if(Compare(range.maximum,floatLimit)>0){proof.maximumFootGapBound=static_cast<float>(range.maximum.Approximate());return false;}
            D::Record(footRange,range.maximum);return true;
        };
        if(clipped.size()==1){if(!piece({clipped[0].actual,clipped[0].actual,clipped[0].actual})){proof.rejection=O::Reject::Foot;return out;}}
        else if(clipped.size()==2){if(!piece({clipped[0].actual,clipped[1].actual,clipped[1].actual})){proof.rejection=O::Reject::Foot;return out;}}
        else for(unsigned j=1;j+1<clipped.size();++j)if(!piece({clipped[0].actual,clipped[j].actual,clipped[j+1].actual})){
            proof.rejection=O::Reject::Foot;return out;}
    }
    timing.Advance(5);
    if(!lower.points||!footRange.points||!out.windPinnedVertices){proof.rejection=O::Reject::Foot;return out;}
    D::Need(out.allFaces==derived.indices.size()/3,"CORE_HERB_SUPPORT_INCOMPLETE_ALL_FACES");
    proof.minimumGapBound=static_cast<float>(lower.minimum.Approximate());proof.maximumFootGapBound=static_cast<float>(footRange.maximum.Approximate());
    proof.safe=true;return out;
}
}
