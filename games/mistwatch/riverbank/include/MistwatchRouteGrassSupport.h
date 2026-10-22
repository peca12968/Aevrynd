#pragma once
#include "MistwatchCoreHerbSupport.h"
#include "MistwatchScannedGrass.h"

// 五種原草的來源根帶契約；算術維持CoreHerb既有門檻，兩nettle入口不放寬。
// 全面CPU證據仍不代表GPU、美術或正式發布接受。
namespace Mistwatch::RouteGrassSupport {
using namespace Aevrynd;
namespace D=LandscapeSpeciesDetail;
namespace O=OuterUnderstory;
inline constexpr std::string_view Profile="route-grass-source-foot-root-pinned-v1";
enum class Source : std::uint8_t { Grass0,Grass1,Grass2,Grass3,Grass4 };
inline constexpr std::array<std::uint64_t,5> SourceFingerprints{
    13676700745932842533ull,6708166984977766172ull,9741578749886758716ull,
    15330444761625695554ull,14455864278497895329ull};
inline unsigned Slot(Source source){
    const auto value=static_cast<unsigned>(source);
    D::Need(value<5,"ROUTE_GRASS_SUPPORT_SOURCE_SLOT");return value;
}
using Proof=CoreHerbSupport::Proof;
using Timings=CoreHerbSupport::Timings;
namespace Detail {
using Tagged=CoreHerbSupport::Detail::Tagged;
using PhaseTiming=CoreHerbSupport::Detail::PhaseTiming;
using CoreHerbSupport::Detail::Foot;
using CoreHerbSupport::Detail::FootPrepared;
}
// 擁有地表副本並從真位元計算身分，不接受 caller 任意標籤；Surface 的引用不會懸空。
// 這只封存提供的 CPU 網格，仍不代表它等於真正 GPU 提交的網格。
class Ground final {
    static Mesh3D ValidatedCopy(const Mesh3D& mesh){D::ValidateMesh(mesh,true);return mesh;}
    const Mesh3D mesh_;
    const D::Surface surface_;
    const std::uint64_t fingerprint_;
public:
    explicit Ground(const Mesh3D& mesh):mesh_(ValidatedCopy(mesh)),surface_(mesh_),
        fingerprint_(CoreHerbClusters::Detail::MeshFingerprint(mesh_)){}
    Ground(const Ground&)=delete;
    Ground& operator=(const Ground&)=delete;
    Ground(Ground&&)=delete;
    Ground& operator=(Ground&&)=delete;
    const D::Surface& Surface()const noexcept{return surface_;}
    std::uint64_t Fingerprint()const noexcept{return fingerprint_;}
};
inline Proof Check(Source source,const Ground& supportGround,const Mesh3D& raw,const Mesh3D& derived,
                   const Transform3D& pose,Vec3 root,float radius,bool planeBounds=true,
                   Timings* timings=nullptr,bool footprintBroadPhase=true,bool reuseVertices=true){
    Detail::PhaseTiming timing(timings);
    const auto slot=Slot(source);const auto& surface=supportGround.Surface();
    const auto surfaceFingerprint=supportGround.Fingerprint();
    Proof out;auto& proof=out.contact;
    D::Need(raw.vertices.size()==GrassVertexCounts[slot]&&raw.indices.size()==3u*GrassTriangleCounts[slot],
        "ROUTE_GRASS_SUPPORT_SOURCE_TOPOLOGY");
    D::ValidateMesh(raw,false);D::ValidateMesh(derived,false);
    out.sourceFingerprint=CoreHerbClusters::Detail::MeshFingerprint(raw);
    D::Need(out.sourceFingerprint==SourceFingerprints[slot],
        "ROUTE_GRASS_SUPPORT_SOURCE_IDENTITY");
    out.derivedFingerprint=CoreHerbClusters::Detail::MeshFingerprint(derived);out.surfaceFingerprint=surfaceFingerprint;
    D::Need(surfaceFingerprint!=0&&raw.indices==derived.indices&&raw.vertices.size()==derived.vertices.size()&&
        Session::Finite(root)&&Session::Finite(pose.position)&&Session::Finite(pose.rotation)&&Session::Finite(pose.scale)&&
        pose.rotation.x==0&&pose.rotation.z==0&&pose.scale.x>0&&pose.scale.x==pose.scale.y&&pose.scale.x==pose.scale.z&&
        std::isfinite(radius)&&radius>0&&radius<=4,"ROUTE_GRASS_SUPPORT_INPUT");
    float minimumY=raw.vertices.front().y;for(const auto& v:raw.vertices)minimumY=(std::min)(minimumY,v.y);
    const auto matrix=pose.Matrix();std::vector<Detail::Tagged> points;points.reserve(raw.vertices.size());
    const auto same=[](float a,float b){return std::bit_cast<unsigned>(a)==std::bit_cast<unsigned>(b);};
    for(unsigned i=0;i<raw.vertices.size();++i){const auto& a=raw.vertices[i];const auto& b=derived.vertices[i];
        D::Need(same(a.x,b.x)&&same(a.z,b.z)&&same(a.u,b.u)&&same(a.v,b.v)&&a.color==b.color,
            "ROUTE_GRASS_SUPPORT_MATERIAL_OR_XZ_CHANGED");
        const auto n=UnpackNormalSnorm8(b.normal);const float wind=n.w>=0?1.f-std::clamp(n.w,0.f,1.f):0;
        const double height=(double(a.y)-minimumY)*pose.scale.x;
        D::Need((b.normal&0xff000000u)==CoreHerbClusters::Detail::RootWindBits(a.normal,height),
            "ROUTE_GRASS_SUPPORT_WIND_IDENTITY");
        if(height<=double(.035f)){D::Need(wind==0,"ROUTE_GRASS_SUPPORT_ROOT_WIND_NOT_PINNED");++out.windPinnedVertices;}
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
        D::Need(Dot(before,after)>0,"ROUTE_GRASS_SUPPORT_FACE_ORIENTATION");
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
    D::Need(out.allFaces==derived.indices.size()/3,"ROUTE_GRASS_SUPPORT_INCOMPLETE_ALL_FACES");
    proof.minimumGapBound=static_cast<float>(lower.minimum.Approximate());proof.maximumFootGapBound=static_cast<float>(footRange.maximum.Approximate());
    proof.safe=true;return out;
}
}
