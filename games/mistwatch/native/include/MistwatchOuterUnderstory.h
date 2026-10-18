#pragma once
#include "MistwatchOuterForestFloor.h"
#include "MistwatchLandscapeSpecies.h"
#include "MistwatchForestCover.h"
#include "MistwatchFern.h"
#include "MistwatchNettleAsset.h"
#include <span>
#include <map>
#include <optional>

namespace Mistwatch::OuterUnderstory {
using namespace Aevrynd;
inline constexpr std::string_view Profile="outer-understory-clusters-v1";
inline constexpr unsigned BandCount=6,ClustersPerBand=4,PlantsPerCluster=16,Target=384,MaximumProposals=24576;
namespace TerrainAware {
inline constexpr std::string_view Revision="source-footprint-ranked-proposals-v1";
struct Statistics {
    std::uint64_t indexedFaces{},indexedCells{},centreScans{},previewRoots{},rankedRoots{};
    std::uint64_t sampleQueries{},faceQueries{},missingSamples{};
    std::uint64_t footLayers{},footSummaryPoints{},footMarginQueries{};
};
}
inline constexpr std::uint64_t IdentityNamespace=0x4d574f5554455231ull;
inline constexpr float SlopeLimit=.88f,HeightLimit=.24f,ContactLimit=.04f,WindRadius=.19f,WindUp=.011f;
inline constexpr std::array<unsigned,11> TriangleCounts{714,1760,1215,2489,1664,2384,2248,784,816,3592,1088};
inline constexpr std::array<unsigned,11> VertexCounts{645,1575,1072,2197,1542,1660,1568,544,565,2736,849};
inline constexpr std::array<unsigned,16> ClusterSlots{0,1,2,3,4,0,1,2,3,4,5,6,7,8,9,10};
struct Sources {
    // 順序固定為五草、四蕨、兩寬葉；皆是主程式既有正規化後 CPU 網格。
    std::array<const Mesh3D*,11> plants{};
    std::array<std::uint64_t,11> fingerprints{};
    // 前四為原 LOD2 bark/twig/dead-branches/trunk-c，第五為 LOD1 twig。
    std::array<const Mesh3D*,5> trees{};
    std::array<std::uint64_t,5> treeFingerprints{};
};
enum class Reject : unsigned { Domain,World,Tree,Spacing,Slope,Height,Support,Foot,Count };
struct SupportProof {
    bool safe{};Reject rejection{Reject::Support};
    bool restingFootObserved{},restingFootAboveLimit{};
    float maximumRestingFootGap{};
    float minimumNormalY{1},maximumHeightDelta{},minimumGapBound{},maximumFootGapBound{};
    unsigned footprintFaces{},evaluatedFaces{},skippedFaces{},footPieces{};
    std::uint64_t cachedGroundFaces{},aabbRejected{},exactOverlayFaces{};
    std::uint64_t groundHeightPlanes{},sourceHeightPlanes{},cachedHeightEvaluations{};
};
struct Plant {
    Transform3D pose;Vec3 groundRoot{};
    unsigned slot{},band{},cluster{},anchorTree{};float radius{};
    SupportProof support;
};
struct Cluster {Vec3 centre{};unsigned band{},anchorTree{},first{},count{},proposalEnd{};};
struct Plan {
    std::string_view proposalRevision{TerrainAware::Revision};
    TerrainAware::Statistics ranking;
    std::uint64_t footHintFingerprint{};
    std::vector<Plant> plants;std::vector<Cluster> clusters;
    std::array<unsigned,6> bandPlants{},bandClusters{};
    std::array<unsigned,11> variants{};
    std::array<unsigned,11> slotProposals{},slotSupportChecks{};
    std::array<unsigned,static_cast<unsigned>(Reject::Count)> rejected{};
    unsigned proposals{},acceptedCandidates{},rolledBack{},failedClusters{},rejectedAnchors{};
    bool complete{};
    std::uint64_t poseFingerprint{},surfaceFingerprint{},fingerprint{},instanceTriangles{};
    std::array<std::uint64_t,11> sourceFingerprints{};
};
namespace Detail {
namespace D=LandscapeSpeciesDetail;
inline void Need(bool yes,const char* reason){if(!yes)throw std::invalid_argument(reason);}
inline bool Finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline std::uint64_t MeshFingerprint(const Mesh3D& mesh){return OuterForestFloor::MeshFingerprint(mesh);}
inline MeshBounds3D Union(const Sources& sources){
    Vec3 lo{10000,10000,10000},hi{-10000,-10000,-10000};
    for(unsigned slot=0;slot<5;++slot){const auto* mesh=sources.trees[slot];Need(mesh&&mesh->Valid(),"OUTER_PLANTS_TREE_SOURCE");
        Need(sources.treeFingerprints[slot]!=0&&MeshFingerprint(*mesh)==sources.treeFingerprints[slot],"OUTER_PLANTS_TREE_IDENTITY");
        for(const auto& v:mesh->vertices){Need(Finite({v.x,v.y,v.z}),"OUTER_PLANTS_TREE_FINITE");
            lo.x=(std::min)(lo.x,v.x);lo.y=(std::min)(lo.y,v.y);lo.z=(std::min)(lo.z,v.z);
            hi.x=(std::max)(hi.x,v.x);hi.y=(std::max)(hi.y,v.y);hi.z=(std::max)(hi.z,v.z);}}
    const auto e=(hi-lo)*.5f;return {(lo+hi)*.5f,Length(e),e};
}
inline std::array<CanopyVolume::PlantLocalVolume,11> ValidateSources(const Sources& sources){
    std::array<CanopyVolume::PlantLocalVolume,11> volumes{};
    for(unsigned slot=0;slot<11;++slot){const auto* mesh=sources.plants[slot];
        Need(mesh&&mesh->Valid()&&mesh->vertices.size()==VertexCounts[slot]&&mesh->indices.size()==std::size_t(TriangleCounts[slot])*3,"OUTER_PLANTS_SOURCE_TOPOLOGY");
        for(const auto& v:mesh->vertices){const auto n=UnpackNormalSnorm8(v.normal);const auto len=Length({n.x,n.y,n.z});
            Need(Finite({v.x,v.y,v.z})&&std::isfinite(v.u)&&std::isfinite(v.v)&&std::isfinite(len)&&len>.95f&&len<1.05f,"OUTER_PLANTS_SOURCE_VERTEX");}
        Need(sources.fingerprints[slot]!=0&&MeshFingerprint(*mesh)==sources.fingerprints[slot],"OUTER_PLANTS_SOURCE_IDENTITY");
        Need(CanopyVolume::MeasureLocalVolume(std::span<const Vertex3D>(mesh->vertices),volumes[slot])&&volumes[slot].radius<8.f,"OUTER_PLANTS_SOURCE_VOLUME");
    }return volumes;
}
inline void ValidateLandscape(const HeightField3D& terrain,const Mesh3D& mesh){
    const auto expected=CreateLandscapeMesh(terrain,true);
    Need(mesh.vertices.size()==expected.vertices.size()&&mesh.indices==expected.indices,"OUTER_PLANTS_SURFACE_TOPOLOGY");
    for(std::size_t i=0;i<mesh.vertices.size();++i){const auto& a=mesh.vertices[i];const auto& b=expected.vertices[i];Vec2 uv{};
        Need(OuterForestFloor::Detail::Same(a.x,b.x)&&OuterForestFloor::Detail::Same(a.y,b.y)&&OuterForestFloor::Detail::Same(a.z,b.z)&&
            a.normal==b.normal&&a.color==b.color&&TryEncodeTerrainLayerCoverage3D(a.u,a.v,uv),"OUTER_PLANTS_SURFACE_VERTEX");}
}
inline CanopyVolume::PlantVolume Volume(const Plant& p,const CanopyVolume::PlantLocalVolume& local){
    return {p.pose.position,CanopyVolume::ConservativeLocalVolume(local.radius+WindRadius/p.pose.scale.x,
        local.minimumY,local.maximumY+WindUp/p.pose.scale.x),p.pose.scale.x,p.pose.rotation};
}
inline bool Domain(const Plant& p,const CanopyVolume::PlantLocalVolume& local){
    CanopyVolume::Detail::Bounds b;if(!CanopyVolume::Detail::PlantBounds(Volume(p,local),.03f,b))return false;
    return b[0].minimum>=-200&&b[0].maximum<=200&&b[2].minimum>=-170&&b[2].maximum<=250&&
        (b[0].maximum< -53||b[0].minimum>53||b[2].maximum< -23||b[2].minimum>103);
}
inline std::vector<BoxCollider3D> TreeWood(const Sources& sources,const LandscapeStandFill::Plan& trees){
    std::vector<BoxCollider3D> result;result.reserve(1232);
    // 只取實際 bark/trunk-c 硬木，不把整個葉冠變成禁種空洞；這些盒不加入玩家碰撞。
    for(const auto& pose:trees.combined)for(unsigned slot:{0u,3u}){
        const auto bounds=TransformBounds(sources.trees[slot]->Bounds(),pose);result.push_back({bounds.center,bounds.extents});}
    Need(CanopyVolume::ValidWoodParts(result),"OUTER_PLANTS_TREE_VOLUME");return result;
}
inline bool Spacing(const Plant& p,std::span<const Plant> old){
    for(const auto& other:old)if(std::hypot(double(p.groundRoot.x)-other.groundRoot.x,double(p.groundRoot.z)-other.groundRoot.z)<
        .65*(double(p.radius)+other.radius))return false;return true;
}
struct WeightedPoint {D::P point;D::R wind;};
inline std::vector<WeightedPoint> Foot(std::span<const WeightedPoint> face,const D::R& limit){
    std::vector<WeightedPoint> out;
    for(std::size_t i=0;i<face.size();++i){const auto& a=face[i];const auto& b=face[(i+1)%face.size()];
        const auto da=a.point[1]-limit,db=b.point[1]-limit;
        if(da.Sign()<=0)out.push_back(a);
        if((da.Sign()<=0)!=(db.Sign()<=0)){const auto t=da/(da-db);
            out.push_back({D::Exact::Interpolate(a.point,b.point,t),a.wind+(b.wind-a.wind)*t});}}
    return out;
}
inline D::T Shift(std::span<const WeightedPoint,3> face,const D::R& amount){
    D::T out;for(unsigned i=0;i<3;++i){out[i]=face[i].point;out[i][1]=out[i][1]+amount*face[i].wind;}return out;
}
// 每株只建立一次真地表快取；精確包圍盒僅排除嚴格分離，邊與點接觸仍走原求交。
class GroundCache {
public:
    using Bounds=std::array<D::R,4>;
private:
    // 真 rational 三點的精確平面；不使用 double 或近似法線。
    struct HeightPlane {
        D::R x,z,constant;
        D::R At(const D::P& point)const{
            return x*point[0]+z*point[2]+constant;
        }
    };
    static HeightPlane PrepareHeight(const D::T& triangle){
        const auto ax=triangle[1][0]-triangle[0][0];
        const auto az=triangle[1][2]-triangle[0][2];
        const auto ay=triangle[1][1]-triangle[0][1];
        const auto bx=triangle[2][0]-triangle[0][0];
        const auto bz=triangle[2][2]-triangle[0][2];
        const auto by=triangle[2][1]-triangle[0][1];
        const auto determinant=ax*bz-bx*az;
        D::Need(determinant.Sign()!=0,"LANDSCAPE_SPECIES_PROJECTED_SURFACE_DEGENERATE");
        const auto x=(ay*bz-by*az)/determinant;
        const auto z=(ax*by-bx*ay)/determinant;
        return{x,z,triangle[0][1]-x*triangle[0][0]-z*triangle[0][2]};
    }
    struct Face {D::T triangle;Bounds bounds;HeightPlane height;};
    std::vector<Face> faces_;
    Bounds footprint_;
    SupportProof& proof_;
    bool footprintOnly_{};
public:
    static Bounds ProjectedBounds(std::span<const D::P> points){
        Bounds b{points[0][0],points[0][0],points[0][2],points[0][2]};
        for(const auto& p:points){if(Compare(p[0],b[0])<0)b[0]=p[0];if(Compare(p[0],b[1])>0)b[1]=p[0];
            if(Compare(p[2],b[2])<0)b[2]=p[2];if(Compare(p[2],b[3])>0)b[3]=p[2];}return b;
    }
    static bool Overlaps(const Bounds& a,const Bounds& b){
        return Compare(a[1],b[0])>=0&&Compare(b[1],a[0])>=0&&Compare(a[3],b[2])>=0&&Compare(b[3],a[2])>=0;
    }
public:
    GroundCache(const D::Surface& surface,std::span<const D::P> footprint,SupportProof& proof,bool footprintOnly=false):
        footprint_(ProjectedBounds(footprint)),proof_(proof),footprintOnly_(footprintOnly){
        const auto ids=surface.Query(footprint);faces_.reserve(ids.size());
        for(const auto id:ids){auto triangle=surface.Face(id);const auto bounds=ProjectedBounds(triangle);
            // 只有查詢已證明位於footprint內的模式可先排除严格分離；邊／點接觸保留。
            if(footprintOnly_&&!Overlaps(bounds,footprint_))continue;
            const auto height=PrepareHeight(triangle);
            ++proof_.groundHeightPlanes;
            faces_.push_back({std::move(triangle),bounds,height});}
        proof_.cachedGroundFaces=faces_.size();
    }
    bool Covers(const D::P& p)const{
        return Compare(p[0],footprint_[0])>=0&&Compare(p[0],footprint_[1])<=0&&
            Compare(p[2],footprint_[2])>=0&&Compare(p[2],footprint_[3])<=0;
    }
    // 地表平面外延的三頂點極值保守包住真交集，投影退化面亦成立；空結果不代表安全。
    // 這只供安全剪枝：界限太鬆時仍需 Overlay，不能反向以界限直接判定碰撞。
    D::Range ConservativeRange(const D::T& face)const{
        if(footprintOnly_)for(const auto& point:face)D::Need(Covers(point),"OUTER_PLANTS_CACHE_QUERY_OUTSIDE_FOOTPRINT");
        D::Range result;const auto bounds=ProjectedBounds(face);
        for(const auto& entry:faces_){
            if(!Overlaps(bounds,entry.bounds))continue;
            for(const auto& point:face)D::Record(result,point[1]-entry.height.At(point));
        }
        return result;
    }
    D::Range Overlay(const D::T& face){
        if(footprintOnly_)for(const auto& point:face)D::Need(Covers(point),"OUTER_PLANTS_CACHE_QUERY_OUTSIDE_FOOTPRINT");
        D::Range result;const auto bounds=ProjectedBounds(face);const bool flat=D::Area(face[0],face[1],face[2]).Sign()==0;
        // 投影退化的 source 面沒有單值高度平面，必須保留原線段高度。
        std::optional<HeightPlane> sourceHeight;
        if(!flat){sourceHeight=PrepareHeight(face);++proof_.sourceHeightPlanes;}
        for(const auto& entry:faces_){
            if(!Overlaps(bounds,entry.bounds)){++proof_.aabbRejected;continue;}
            ++proof_.exactOverlayFaces;const auto& ground=entry.triangle;
            if(flat){for(unsigned edge=0;edge<3;++edge)for(const auto& p:D::SegmentIntersection(face[edge],face[(edge+1)%3],ground))
                {++proof_.cachedHeightEvaluations;D::Record(result,p[1]-entry.height.At(p));}}
            else for(const auto& p:D::PlanarIntersection(face,ground)){
                proof_.cachedHeightEvaluations+=2;D::Record(result,sourceHeight->At(p)-entry.height.At(p));}
        }return result;
    }
};
inline SupportProof Support(const D::Surface& surface,const Mesh3D& source,const Plant& plant,bool useCache=true,bool diagnoseRestingFoot=false){
    SupportProof proof;const auto centre=plant.groundRoot;const float r=plant.radius;
    const std::array<D::P,4> square{D::Exact::ToExact(Vec3{centre.x-r,centre.y,centre.z-r}),
        D::Exact::ToExact(Vec3{centre.x+r,centre.y,centre.z-r}),D::Exact::ToExact(Vec3{centre.x+r,centre.y,centre.z+r}),
        D::Exact::ToExact(Vec3{centre.x-r,centre.y,centre.z+r})};
    D::Range heights;const auto root=D::R::Float(centre.y);double maxGradient{};
    for(unsigned half=0;half<2;++half){const D::T footprint{square[0],square[half+1],square[half+2]};
        const auto expected=D::Abs(D::Area(footprint[0],footprint[1],footprint[2]));D::R covered;
        for(const auto id:surface.Query(footprint)){const auto ground=surface.Face(id);const auto intersection=D::PlanarIntersection(footprint,ground);
            if(intersection.empty())continue;
            for(const auto& p:intersection){const auto y=D::Height(ground,p);D::Record(heights,y);
                proof.maximumHeightDelta=(std::max)(proof.maximumHeightDelta,static_cast<float>(D::Abs(y-root).Approximate()));
                if(Compare(D::Abs(y-root),D::R::Float(HeightLimit))>0){proof.rejection=Reject::Height;return proof;}}
            bool area=false;for(std::size_t i=1;i+1<intersection.size();++i)area|=D::Area(intersection[0],intersection[i],intersection[i+1]).Sign()!=0;
            if(!area)continue;const auto hull=D::Hull(intersection);
            for(std::size_t i=1;i+1<hull.size();++i)covered=covered+D::Abs(D::Area(hull[0],hull[i],hull[i+1]));
            ++proof.footprintFaces;const float normal=surface.NormalY(id);proof.minimumNormalY=(std::min)(proof.minimumNormalY,normal);
            if(!std::isfinite(normal)||normal<SlopeLimit){proof.rejection=Reject::Slope;return proof;}
            maxGradient=(std::max)(maxGradient,std::sqrt((std::max)(0.0,1.0-double(normal)*normal))/double(normal));
        }
        if(Compare(covered,expected)!=0){proof.rejection=Reject::Support;return proof;}
    }
    if(!heights.points||!proof.footprintFaces)return proof;
    GroundCache cache(surface,square,proof);
    const auto overlay=[&](const D::T& face){return useCache?cache.Overlay(face):D::Overlay(face,surface);};
    // 風後水平位移受 .19m*weight 限制；最大地面梯度將其轉成保守高度差。
    const auto lowerShift=D::R::Float(-static_cast<float>(maxGradient*double(WindRadius)+.000001));
    const auto upperShift=D::R::Float(static_cast<float>(maxGradient*double(WindRadius)+double(WindUp)+.000001));
    const auto penetration=D::R::Float(-ContactLimit),floating=D::R::Float(ContactLimit);
    D::Range lower,footRange;const auto matrix=plant.pose.Matrix();std::vector<WeightedPoint> points;points.reserve(source.vertices.size());
    for(const auto& v:source.vertices){const auto n=UnpackNormalSnorm8(v.normal);
        const float wind=n.w>=0?1.f-std::clamp(n.w,0.f,1.f):0.f;
        const auto point=D::Exact::ToExact(TransformPoint(matrix,{v.x,v.y,v.z}));
        if(!cache.Covers(point))return proof;
        points.push_back({point,D::R::Float(wind)});}
    const auto footTop=D::R::Float(centre.y+.035f);
    for(std::size_t f=0;f<source.indices.size();f+=3){const std::array<WeightedPoint,3> original{points[source.indices[f]],points[source.indices[f+1]],points[source.indices[f+2]]};
        const auto lowFace=Shift(original,lowerShift);auto minY=lowFace[0][1];for(unsigned j=1;j<3;++j)if(Compare(lowFace[j][1],minY)<0)minY=lowFace[j][1];
        // 這是完整footprint證明後的保守下界，不把剪枝面標成實際求交面。
        const auto bound=minY-heights.maximum;
        if(Compare(bound,penetration)>=0){D::Record(lower,bound);++proof.skippedFaces;}
        else {const auto range=overlay(lowFace);++proof.evaluatedFaces;
            if(!range.points)return proof;
            if(Compare(range.minimum,penetration)<0){proof.minimumGapBound=static_cast<float>(range.minimum.Approximate());return proof;}
            D::Record(lower,range.minimum);}
        const auto clipped=Foot(original,footTop);if(clipped.empty())continue;
        const auto piece=[&](const std::array<WeightedPoint,3>& vertices){
            const auto range=overlay(Shift(vertices,upperShift));++proof.footPieces;
            if(!range.points)return false;
            if(Compare(range.maximum,floating)>0){proof.maximumFootGapBound=static_cast<float>(range.maximum.Approximate());
                // 僅記錄原先失敗的同一塊底部幾何；不改裁切、拒絕或風擺門。
                if(diagnoseRestingFoot){const auto resting=overlay(Shift(vertices,D::R{}));
                    proof.restingFootObserved=resting.points!=0;
                    if(resting.points){proof.maximumRestingFootGap=static_cast<float>(resting.maximum.Approximate());
                        proof.restingFootAboveLimit=Compare(resting.maximum,floating)>0;}}
                return false;}
            D::Record(footRange,range.maximum);return true;};
        if(clipped.size()==1){if(!piece({clipped[0],clipped[0],clipped[0]})){proof.rejection=Reject::Foot;return proof;}}
        else if(clipped.size()==2){if(!piece({clipped[0],clipped[1],clipped[1]})){proof.rejection=Reject::Foot;return proof;}}
        else for(std::size_t j=1;j+1<clipped.size();++j)if(!piece({clipped[0],clipped[j],clipped[j+1]})){proof.rejection=Reject::Foot;return proof;}
    }
    if(!lower.points||!footRange.points){proof.rejection=Reject::Foot;return proof;}
    proof.minimumGapBound=static_cast<float>(lower.minimum.Approximate());proof.maximumFootGapBound=static_cast<float>(footRange.maximum.Approximate());
    proof.safe=true;return proof;
}
inline std::uint64_t Fingerprint(const Plan& plan){
    std::uint64_t h=14695981039346656037ull;const auto word=[&](std::uint32_t value){LandscapeStandFill::Detail::Word(h,value);};
    for(const unsigned char c:plan.proposalRevision)word(c);
    word(static_cast<std::uint32_t>(plan.footHintFingerprint));word(static_cast<std::uint32_t>(plan.footHintFingerprint>>32));
    word(static_cast<unsigned>(plan.plants.size()));
    for(const auto& p:plan.plants){LandscapeStandFill::Detail::Pose(h,p.pose);word(p.slot);word(p.band);word(p.cluster);word(p.anchorTree);
        for(float f:{p.groundRoot.x,p.groundRoot.y,p.groundRoot.z,p.radius})word(std::bit_cast<std::uint32_t>(f));}
    return h;
}
}
namespace TerrainAware {
inline constexpr unsigned CentreCount=48,PreviewRoots=32,RootCount=256;
struct Sample {double height{},dx{},dz{},normalY{};};

// 真來源的底部薄層摘要只供選址排序；不可取代完整 rational Support。
class FootHints {
public:
    static constexpr unsigned LayerCount=16;
    struct Layer {
        bool populated{};
        double minimumX{},maximumX{},minimumZ{},maximumZ{},minimumY{},maximumY{},maximumWind{};
        unsigned points{};
    };
    struct Species {
        double minimumScale{},maximumScale{},pivot{},lower{},upper{};
        std::array<Layer,LayerCount> layers{};
    };
private:
    using Point=std::array<double,4>; // x、相對作者底部的 y、z、風權重。
    std::array<Species,11> species_{};
    static std::vector<Point> Clip(std::span<const Point> input,double height,bool upper){
        std::vector<Point> out;
        for(std::size_t i=0;i<input.size();++i){const auto& a=input[i];const auto& b=input[(i+1)%input.size()];
            const bool insideA=upper?a[1]<=height:a[1]>=height;
            const bool insideB=upper?b[1]<=height:b[1]>=height;
            if(insideA)out.push_back(a);
            if(insideA!=insideB){const double t=(height-a[1])/(b[1]-a[1]);Point point{};
                for(unsigned k=0;k<4;++k)point[k]=a[k]+(b[k]-a[k])*t;
                point[1]=height;out.push_back(point);}
        }return out;
    }
    static void Record(Layer& layer,const Point& p){
        for(const double value:p)Detail::Need(std::isfinite(value),"OUTER_FOOT_HINT_POINT");
        if(!layer.populated){layer.minimumX=layer.maximumX=p[0];layer.minimumY=layer.maximumY=p[1];
            layer.minimumZ=layer.maximumZ=p[2];layer.maximumWind=p[3];layer.populated=true;}
        else {layer.minimumX=(std::min)(layer.minimumX,p[0]);layer.maximumX=(std::max)(layer.maximumX,p[0]);
            layer.minimumY=(std::min)(layer.minimumY,p[1]);layer.maximumY=(std::max)(layer.maximumY,p[1]);
            layer.minimumZ=(std::min)(layer.minimumZ,p[2]);layer.maximumZ=(std::max)(layer.maximumZ,p[2]);
            layer.maximumWind=(std::max)(layer.maximumWind,p[3]);}
        ++layer.points;
    }
public:
    FootHints(const Sources& sources,const std::array<CanopyVolume::PlantLocalVolume,11>& volumes,Statistics& stats){
        for(unsigned slot=0;slot<11;++slot){const auto* mesh=sources.plants[slot];const auto& volume=volumes[slot];
            Detail::Need(mesh&&mesh->Valid()&&mesh->indices.size()%3==0&&mesh->indices.size()/3<10000,
                         "OUTER_FOOT_HINT_SOURCE");
            auto& species=species_[slot];const double span=double(volume.maximumY)-volume.minimumY;
            Detail::Need(std::isfinite(span)&&span>0&&std::isfinite(volume.minimumY),"OUTER_FOOT_HINT_VOLUME");
            const float heightSpan=volume.maximumY-volume.minimumY;
            Detail::Need(std::isfinite(heightSpan)&&heightSpan>0,"OUTER_FOOT_HINT_VOLUME");
            species.minimumScale=slot<5?double(.25f):slot<9?double(.6f):double((slot==9?.26f:.10f)/heightSpan);
            species.maximumScale=slot<5?double(.75f):slot<9?double(.6f+.18f):double((slot==9?.35f:.16f)/heightSpan);
            species.pivot=slot>=9?double(volume.minimumY):0;
            species.lower=double(volume.minimumY)-species.pivot;
            species.upper=double(.035f)/species.minimumScale;
            Detail::Need(std::isfinite(species.minimumScale)&&species.minimumScale>0&&
                std::isfinite(species.maximumScale)&&species.maximumScale>=species.minimumScale&&
                std::isfinite(species.lower)&&std::isfinite(species.upper),"OUTER_FOOT_HINT_VOLUME");
            Detail::Need(species.lower<species.upper,"OUTER_FOOT_HINT_EMPTY_RANGE");
            for(unsigned bin=0;bin<LayerCount;++bin){const double lo=species.lower+(species.upper-species.lower)*double(bin)/LayerCount;
                const double hi=species.lower+(species.upper-species.lower)*double(bin+1)/LayerCount;
                auto& layer=species.layers[bin];
                for(std::size_t f=0;f<mesh->indices.size();f+=3){std::array<Point,3> face{};
                    for(unsigned k=0;k<3;++k){const auto& v=mesh->vertices[mesh->indices[f+k]];const auto n=UnpackNormalSnorm8(v.normal);
                        Detail::Need(Detail::Finite({v.x,v.y,v.z})&&std::isfinite(n.w),"OUTER_FOOT_HINT_POINT");
                        face[k]={v.x,double(v.y)-species.pivot,v.z,n.w>=0?double(1.f-std::clamp(n.w,0.f,1.f)):0};}
                    const auto above=Clip(face,lo,false),inside=Clip(above,hi,true);
                    for(const auto& p:inside){Record(layer,p);++stats.footSummaryPoints;}
                }
                ++stats.footLayers;
            }
            Detail::Need(std::any_of(species.layers.begin(),species.layers.end(),[](const auto& l){return l.populated;}),
                         "OUTER_FOOT_HINT_EMPTY_SOURCE");
        }
    }
    const Species& At(unsigned slot)const{Detail::Need(slot<11,"OUTER_FOOT_HINT_SLOT");return species_[slot];}
    double Gap(unsigned slot,float scale,float yaw,const Sample& plane,double gradient,double bend,Statistics& stats)const{
        const auto& species=At(slot);++stats.footMarginQueries;
        Detail::Need(std::isfinite(scale)&&scale>0&&std::isfinite(yaw)&&std::isfinite(plane.dx)&&std::isfinite(plane.dz)&&
            std::isfinite(gradient)&&gradient>=0&&std::isfinite(bend)&&bend>=0,"OUTER_FOOT_HINT_QUERY");
        // 尺度範圍外不截成較小底部；缺少摘要明確排在最後，仍由原 Support 決定。
        if(double(scale)<species.minimumScale||double(scale)>species.maximumScale)return 1e12;
        const double cosine=std::cos(double(yaw)),sine=std::sin(double(yaw));
        const double x=-plane.dx*cosine+plane.dz*sine,z=-plane.dx*sine-plane.dz*cosine;
        double maximum=-1e12;bool found=false;
        for(const auto& layer:species.layers){if(!layer.populated||double(scale)*layer.minimumY>double(.035f))continue;
            const double horizontal=x*(x>=0?layer.maximumX:layer.minimumX)+z*(z>=0?layer.maximumZ:layer.minimumZ);
            const double upperY=(std::min)(double(.035f),double(scale)*layer.maximumY);
            const double gap=upperY+double(scale)*horizontal+layer.maximumWind*(double(WindRadius)*gradient+double(WindUp)+.000001)+bend;
            maximum=(std::max)(maximum,gap);found=true;
        }return found?maximum:1e12;
    }
    std::uint64_t Fingerprint()const{
        std::uint64_t h=14695981039346656037ull;const auto word=[&](std::uint32_t value){LandscapeStandFill::Detail::Word(h,value);};
        const auto number=[&](double value){const auto bits=std::bit_cast<std::uint64_t>(value);word(static_cast<std::uint32_t>(bits));word(static_cast<std::uint32_t>(bits>>32));};
        word(LayerCount);
        for(const auto& species:species_){for(double value:{species.minimumScale,species.maximumScale,species.pivot,species.lower,species.upper})number(value);
            for(const auto& layer:species.layers){word(layer.populated?1u:0u);word(layer.points);
                for(double value:{layer.minimumX,layer.maximumX,layer.minimumZ,layer.maximumZ,layer.minimumY,layer.maximumY,layer.maximumWind})number(value);}}
        return h;
    }
};

// 索引只讀實際網格面，不用解析地形；所有 double 結果僅供排序。
class Index {
    struct Face {
        std::array<double,3> x{},y{},z{};
        double determinant{},dx{},dz{},normalY{},minX{},maxX{},minZ{},maxZ{};
    };
    std::vector<Face> faces_;
    std::map<std::pair<int,int>,std::vector<unsigned>> cells_;
    static int Cell(double v){return static_cast<int>(std::floor(v/8.0));}
public:
    explicit Index(const Aevrynd::Mesh3D& mesh,Statistics& stats){
        Detail::Need(mesh.Valid()&&mesh.indices.size()%3==0&&mesh.indices.size()/3<100000,
                     "OUTER_RANK_SURFACE_TOPOLOGY");
        faces_.reserve(mesh.indices.size()/3);
        for(std::size_t f=0;f<mesh.indices.size();f+=3){Face face;
            for(unsigned k=0;k<3;++k){const auto& v=mesh.vertices[mesh.indices[f+k]];
                Detail::Need(Detail::Finite({v.x,v.y,v.z})&&std::abs(v.x)<=1000&&std::abs(v.z)<=1000,
                             "OUTER_RANK_SURFACE_VERTEX");
                face.x[k]=v.x;face.y[k]=v.y;face.z[k]=v.z;
            }
            const double ax=face.x[1]-face.x[0],az=face.z[1]-face.z[0];
            const double bx=face.x[2]-face.x[0],bz=face.z[2]-face.z[0];
            face.determinant=ax*bz-bx*az;
            Detail::Need(std::isfinite(face.determinant)&&std::abs(face.determinant)>1e-12,
                         "OUTER_RANK_PROJECTED_FACE");
            face.dx=((face.y[1]-face.y[0])*bz-(face.y[2]-face.y[0])*az)/face.determinant;
            face.dz=(ax*(face.y[2]-face.y[0])-bx*(face.y[1]-face.y[0]))/face.determinant;
            face.normalY=1.0/std::sqrt(1.0+face.dx*face.dx+face.dz*face.dz);
            Detail::Need(std::isfinite(face.dx)&&std::isfinite(face.dz)&&std::isfinite(face.normalY),
                         "OUTER_RANK_FACE_FINITE");
            face.minX=(std::min)({face.x[0],face.x[1],face.x[2]});
            face.maxX=(std::max)({face.x[0],face.x[1],face.x[2]});
            face.minZ=(std::min)({face.z[0],face.z[1],face.z[2]});
            face.maxZ=(std::max)({face.z[0],face.z[1],face.z[2]});
            // 排序索引多收邊界格，不從精確支撐索引移除任何資料。
            const int x0=Cell(std::nextafter(face.minX,-std::numeric_limits<double>::infinity()));
            const int x1=Cell(std::nextafter(face.maxX,std::numeric_limits<double>::infinity()));
            const int z0=Cell(std::nextafter(face.minZ,-std::numeric_limits<double>::infinity()));
            const int z1=Cell(std::nextafter(face.maxZ,std::numeric_limits<double>::infinity()));
            Detail::Need(x1-x0<=8&&z1-z0<=8,"OUTER_RANK_FACE_RESOURCE_BOUND");
            const auto id=static_cast<unsigned>(faces_.size());faces_.push_back(face);
            for(int z=z0;z<=z1;++z)for(int x=x0;x<=x1;++x)cells_[{x,z}].push_back(id);
        }
        stats.indexedFaces=static_cast<std::uint64_t>(faces_.size());
        stats.indexedCells=static_cast<std::uint64_t>(cells_.size());
    }
    std::optional<Sample> At(double x,double z,Statistics& stats)const{
        ++stats.sampleQueries;
        if(!std::isfinite(x)||!std::isfinite(z)||std::abs(x)>1000||std::abs(z)>1000){++stats.missingSamples;return{};}
        const auto cell=cells_.find({Cell(x),Cell(z)});
        if(cell==cells_.end()){++stats.missingSamples;return{};}
        std::optional<Sample> result;
        for(const auto id:cell->second){++stats.faceQueries;const auto& face=faces_[id];
            if(x<face.minX-1e-10||x>face.maxX+1e-10||z<face.minZ-1e-10||z>face.maxZ+1e-10)continue;
            const double px=x-face.x[0],pz=z-face.z[0];
            const double u=(px*(face.z[2]-face.z[0])-(face.x[2]-face.x[0])*pz)/face.determinant;
            const double v=((face.x[1]-face.x[0])*pz-px*(face.z[1]-face.z[0]))/face.determinant;
            if(u< -1e-10||v< -1e-10||u+v>1.0+1e-10)continue;
            const double height=face.y[0]+face.dx*px+face.dz*pz;
            if(result&&std::abs(result->height-height)>1e-5){++stats.missingSamples;return{};}
            // 接縫可能命中兩面，採較陡面作排序，不宣稱完整 footprint 已證明。
            if(!result||face.normalY<result->normalY)result=Sample{height,face.dx,face.dz,face.normalY};
        }
        if(!result)++stats.missingSamples;return result;
    }
};

struct Root {
    Aevrynd::Vec3 position{}; // y 刻意為零，最終必須重新讀取 rational Surface.At。
    float scale{},yaw{},radius{};
    unsigned ordinal{};
    bool sampled{},suggested{};
    double score{1e12},footGap{1e12};
};
struct Centre {
    Aevrynd::Vec3 position{};
    unsigned ordinal{},anchor{},band{},worstSpeciesCapacity{},suggestedTotal{};
    double score{1e12},worstFootGap{1e12};
    bool inDomain{};
};
inline bool CentreBefore(const Centre& a,const Centre& b){
    if(a.inDomain!=b.inDomain)return a.inDomain>b.inDomain;
    if(a.worstFootGap!=b.worstFootGap)return a.worstFootGap<b.worstFootGap;
    if(a.worstSpeciesCapacity!=b.worstSpeciesCapacity)return a.worstSpeciesCapacity>b.worstSpeciesCapacity;
    if(a.suggestedTotal!=b.suggestedTotal)return a.suggestedTotal>b.suggestedTotal;
    if(a.score!=b.score)return a.score<b.score;return a.ordinal<b.ordinal;
}
inline std::uint32_t Seed(unsigned anchor,unsigned centre,unsigned root,unsigned slot){
    const auto a=LandscapeStandFill::Detail::Mix(anchor^0x4f555431u);
    return LandscapeStandFill::Detail::Mix(a^(centre*0x9e3779b9u)^(root*0x85ebca6bu)^(slot*0xc2b2ae35u));
}
inline float Unit(std::uint32_t value){return static_cast<float>(value>>8)/16777216.f;}
inline Root MakeRoot(const Centre& centre,unsigned ordinal,unsigned count,unsigned slot,
                     const CanopyVolume::PlantLocalVolume& volume){
    Detail::Need(centre.anchor>=520&&centre.anchor<616&&centre.ordinal<CentreCount&&centre.band<BandCount&&
                 Detail::Finite(centre.position)&&std::abs(centre.position.x)<=1000&&std::abs(centre.position.z)<=1000&&
                 count>0&&count<=RootCount&&ordinal<count&&slot<11&&
                 std::isfinite(volume.radius)&&volume.radius>0&&std::isfinite(volume.minimumY)&&
                 std::isfinite(volume.maximumY)&&volume.maximumY>volume.minimumY,"OUTER_RANK_ROOT_INPUT");
    // 固定等面積圓盤點，種子僅旋轉整盤；先後拒絕不改變其餘提案。
    const double rotation=double(Unit(Seed(centre.anchor,centre.ordinal,0,0)))*6.283185307179586;
    const double angle=rotation+double(ordinal)*2.399963229728653;
    const double radius=4.0*std::sqrt((double(ordinal)+.5)/double(count));
    Root root;root.ordinal=ordinal;
    root.position={centre.position.x+static_cast<float>(radius*std::cos(angle)),0,
                   centre.position.z+static_cast<float>(radius*std::sin(angle))};
    const auto seed=Seed(centre.anchor,centre.ordinal,ordinal,slot);
    const float random=Unit(seed);
    if(slot<5)root.scale=ForestCoverScale(root.position);
    else if(slot<9)root.scale=.6f+.18f*random;
    else {const float height=slot==9?.26f+.09f*random:.10f+.06f*random;
        root.scale=height/(volume.maximumY-volume.minimumY);}
    root.yaw=Unit(LandscapeStandFill::Detail::Mix(seed))*6.2831853f;
    root.radius=volume.radius*root.scale+WindRadius;
    Detail::Need(std::isfinite(root.scale)&&root.scale>0&&std::isfinite(root.radius),"OUTER_RANK_SCALE_FINITE");
    return root;
}
inline void ScoreRoot(const Index& index,Root& root,Statistics& stats,const FootHints* hints=nullptr,unsigned slot=0){
    const auto middle=index.At(root.position.x,root.position.z,stats);
    if(!middle)return;
    double normal=middle->normalY;
    double gradient=std::hypot(middle->dx,middle->dz);
    double delta=double(root.radius)*(std::abs(middle->dx)+std::abs(middle->dz));
    double bend{};
    for(const auto offset:std::array<std::array<int,2>,4>{{{{-1,-1}},{{1,-1}},{{1,1}},{{-1,1}}}}){
        const double dx=double(root.radius)*offset[0],dz=double(root.radius)*offset[1];
        const auto point=index.At(double(root.position.x)+dx,double(root.position.z)+dz,stats);
        if(!point)return;
        normal=(std::min)(normal,point->normalY);
        gradient=(std::max)(gradient,std::hypot(point->dx,point->dz));
        delta=(std::max)(delta,std::abs(point->height-middle->height));
        bend=(std::max)(bend,std::abs(point->height-middle->height-middle->dx*dx-middle->dz*dz));
    }
    root.sampled=true;
    if(hints)root.footGap=hints->Gap(slot,root.scale,root.yaw,*middle,gradient,bend,stats);
    root.suggested=normal>=double(SlopeLimit)&&delta<=double(HeightLimit);
    // 越平緩、越少跨面彎折越先試；這不是±.04的近似替代驗收。
    root.score=delta/double(HeightLimit)+bend/double(ContactLimit)+
        8.0*(std::max)(0.0,double(SlopeLimit)-normal);
}
inline std::array<Centre,CentreCount> RankCentres(const Index& index,const Aevrynd::Transform3D& tree,
    unsigned anchor,unsigned band,const std::array<CanopyVolume::PlantLocalVolume,11>& volumes,Statistics& stats,const FootHints* hints=nullptr){
    Detail::Need(anchor>=520&&anchor<616&&band<BandCount&&Detail::Finite(tree.position),"OUTER_RANK_ANCHOR");
    std::array<Centre,CentreCount> centres{};const auto& region=LandscapeStandFill::Bands[band];
    for(unsigned ordinal=0;ordinal<CentreCount;++ordinal){++stats.centreScans;
        auto& centre=centres[ordinal];centre.ordinal=ordinal;centre.anchor=anchor;centre.band=band;
        const double angle=double(ordinal%16)*6.283185307179586/16.0;
        const double distance=2.0+2.0*double(ordinal/16);
        centre.position={tree.position.x+static_cast<float>(distance*std::cos(angle)),0,
                         tree.position.z+static_cast<float>(distance*std::sin(angle))};
        centre.inDomain=centre.position.x>=region.minX&&centre.position.x<=region.maxX&&
            centre.position.z>=region.minZ&&centre.position.z<=region.maxZ&&
            (centre.position.x< -53||centre.position.x>53||centre.position.z< -23||centre.position.z>103);
        if(!centre.inDomain)continue;
        std::array<unsigned,11> suggested{};double total{};unsigned sampled{};
        std::array<std::array<double,PreviewRoots>,11> footGaps{};
        for(unsigned slot=0;slot<11;++slot)for(unsigned i=0;i<PreviewRoots;++i){
            ++stats.previewRoots;auto root=MakeRoot(centre,i,PreviewRoots,slot,volumes[slot]);ScoreRoot(index,root,stats,hints,slot);
            footGaps[slot][i]=root.footGap;
            if(root.suggested)++suggested[slot];
            if(root.sampled){total+=root.score;++sampled;}else total+=1e6;
        }
        centre.worstSpeciesCapacity=PreviewRoots;
        centre.worstFootGap=hints?-1e12:1e12;
        for(unsigned slot=0;slot<11;++slot){
            // 五種草各需兩株，其他來源各一株；不能只看第一種草的好位置數。
            centre.worstSpeciesCapacity=(std::min)(centre.worstSpeciesCapacity,suggested[slot]/(slot<5?2u:1u));
            centre.suggestedTotal+=suggested[slot];
            if(hints){auto& gaps=footGaps[slot];std::sort(gaps.begin(),gaps.end());
                centre.worstFootGap=(std::max)(centre.worstFootGap,gaps[slot<5?1u:0u]);}
        }
        centre.score=sampled?total/double(PreviewRoots*11):1e12;
    }
    std::sort(centres.begin(),centres.end(),CentreBefore);return centres;
}
class RootPool {
    std::array<std::vector<Root>,11> roots_;
    std::array<unsigned,11> used_{};
public:
    RootPool(const Index& index,const Centre& centre,const std::array<CanopyVolume::PlantLocalVolume,11>& volumes,
             Statistics& stats,const FootHints* hints=nullptr){
        for(unsigned slot=0;slot<11;++slot){auto& list=roots_[slot];list.reserve(RootCount);
            for(unsigned i=0;i<RootCount;++i){++stats.rankedRoots;auto root=MakeRoot(centre,i,RootCount,slot,volumes[slot]);
                ScoreRoot(index,root,stats,hints,slot);list.push_back(root);}
            std::sort(list.begin(),list.end(),[](const Root& a,const Root& b){
                if(a.sampled!=b.sampled)return a.sampled>b.sampled;
                if(a.footGap!=b.footGap)return a.footGap<b.footGap;
                if(a.suggested!=b.suggested)return a.suggested>b.suggested;
                if(a.score!=b.score)return a.score<b.score;return a.ordinal<b.ordinal;});
        }
    }
    std::optional<Root> Next(unsigned slot){
        Detail::Need(slot<11,"OUTER_RANK_SLOT");
        if(used_[slot]==roots_[slot].size())return{};
        return roots_[slot][used_[slot]++];
    }
};

}
inline Plan Create(const HeightField3D& terrain,const Mesh3D& landscape,const LandscapeStandFill::Plan& trees,
    const Sources& sources,std::span<const BoxCollider3D> before,std::span<const BoxCollider3D> after,unsigned proposalLimit=MaximumProposals,
    void (*progress)(const Plan&)=nullptr,void (*supportObservation)(const Plant&)=nullptr){
    namespace Q=Detail;namespace D=LandscapeSpeciesDetail;
    Q::Need(proposalLimit<=MaximumProposals,"OUTER_PLANTS_PROPOSAL_LIMIT");
    const auto& layout=terrain.Settings();
    Q::Need(layout.width==201&&layout.depth==241&&layout.cellSize==.5f&&layout.origin.x==-50.f&&layout.origin.y==0.f&&layout.origin.z==-20.f,"OUTER_PLANTS_TERRAIN_LAYOUT");
    for(unsigned z=0;z<layout.depth;++z)for(unsigned x=0;x<layout.width;++x)
        Q::Need(std::isfinite(terrain.Height(x,z)),"OUTER_PLANTS_TERRAIN_FINITE");
    const auto volumes=Q::ValidateSources(sources);const auto treeBounds=Q::Union(sources);
    OuterForestFloor::Detail::ValidatePlan(terrain,trees,treeBounds);Q::ValidateLandscape(terrain,landscape);
    Q::Need(CanopyVolume::ValidWoodParts(before)&&CanopyVolume::ValidWoodParts(after),"OUTER_PLANTS_WORLD_INPUT");
    const D::Surface surface(landscape);const auto wood=Q::TreeWood(sources,trees);
    Plan result;result.poseFingerprint=trees.fingerprint;result.surfaceFingerprint=Q::MeshFingerprint(landscape);result.sourceFingerprints=sources.fingerprints;
    const TerrainAware::Index rankingSurface(landscape,result.ranking);
    const TerrainAware::FootHints footHints(sources,volumes,result.ranking);result.footHintFingerprint=footHints.Fingerprint();
    for(unsigned band=0;band<BandCount;++band){std::vector<unsigned> order;
        for(unsigned i=0;i<96;++i)if(trees.addedBands[i]==band)order.push_back(520+i);
        Q::Need(order.size()==16,"OUTER_PLANTS_ANCHOR_COUNT");
        // 固定最遠點順序，包含失敗錨點，不依畫面或可接受數量重抽種子。
        for(std::size_t used=1;used<order.size();++used){std::size_t best=used;double largest=-1;
            for(std::size_t candidate=used;candidate<order.size();++candidate){double distance=1000000;
                for(std::size_t old=0;old<used;++old){const auto a=trees.combined[order[candidate]].position,b=trees.combined[order[old]].position;
                    distance=(std::min)(distance,std::hypot(double(a.x)-b.x,double(a.z)-b.z));}
                if(distance>largest){largest=distance;best=candidate;}}
            std::swap(order[used],order[best]);}
        for(const auto anchor:order){if(result.bandClusters[band]==ClustersPerBand||result.proposals==proposalLimit)break;
            const auto centres=TerrainAware::RankCentres(rankingSurface,trees.combined[anchor],anchor,band,volumes,result.ranking,&footHints);
            const auto& selected=centres.front();
            if(!selected.inDomain){++result.rejectedAnchors;continue;}
            auto centre=selected.position;
            centre.y=static_cast<float>(surface.At(centre).Approximate());
            TerrainAware::RootPool rootPool(rankingSurface,selected,volumes,result.ranking,&footHints);
            std::vector<Plant> pending;pending.reserve(PlantsPerCluster);
            for(unsigned attempt=0;attempt<256&&pending.size()<PlantsPerCluster&&result.proposals<proposalLimit;++attempt){
                if(progress&&result.proposals&&(result.proposals%64)==0)progress(result);
                ++result.proposals;Plant p;p.slot=ClusterSlots[pending.size()];p.band=band;p.cluster=static_cast<unsigned>(result.clusters.size());p.anchorTree=anchor;
                ++result.slotProposals[p.slot];
                const auto candidate=rootPool.Next(p.slot);Q::Need(candidate.has_value(),"OUTER_RANK_POOL_EXHAUSTED");
                p.groundRoot=candidate->position;const auto& volume=volumes[p.slot];const float scale=candidate->scale;
                p.radius=candidate->radius;p.pose={p.groundRoot,{0,candidate->yaw,0},{scale,scale,scale}};
                if(!Q::Domain(p,volume)){++result.rejected[static_cast<unsigned>(Reject::Domain)];continue;}
                p.groundRoot.y=static_cast<float>(surface.At(p.groundRoot).Approximate());p.pose.position=p.groundRoot;
                if(p.slot>=9)p.pose.position.y-=volume.minimumY*scale;
                if(!Q::Domain(p,volume)){++result.rejected[static_cast<unsigned>(Reject::Domain)];continue;}
                const auto box=Q::Volume(p,volume);
                if(!CanopyVolume::ClearsCanopyWood(box,before)||!CanopyVolume::ClearsCanopyWood(box,after)){
                    ++result.rejected[static_cast<unsigned>(Reject::World)];continue;}
                bool close=false;for(const auto& t:trees.combined)if(std::hypot(double(p.groundRoot.x)-t.position.x,double(p.groundRoot.z)-t.position.z)<double(p.radius)+.5){close=true;break;}
                if(close||!CanopyVolume::ClearsCanopyWood(box,wood)){++result.rejected[static_cast<unsigned>(Reject::Tree)];continue;}
                if(!Q::Spacing(p,result.plants)||!Q::Spacing(p,pending)){++result.rejected[static_cast<unsigned>(Reject::Spacing)];continue;}
                ++result.slotSupportChecks[p.slot];p.support=Q::Support(surface,*sources.plants[p.slot],p,true,supportObservation!=nullptr);
                if(supportObservation)supportObservation(p);
                if(!p.support.safe){++result.rejected[static_cast<unsigned>(p.support.rejection)];continue;}
                pending.push_back(p);++result.acceptedCandidates;
            }
            if(pending.size()!=PlantsPerCluster){result.rolledBack+=static_cast<unsigned>(pending.size());++result.failedClusters;continue;}
            result.clusters.push_back({centre,band,anchor,static_cast<unsigned>(result.plants.size()),PlantsPerCluster,result.proposals});
            for(const auto& p:pending){result.plants.push_back(p);++result.variants[p.slot];++result.bandPlants[band];result.instanceTriangles+=TriangleCounts[p.slot];}
            ++result.bandClusters[band];
        }
    }
    result.complete=result.plants.size()==Target&&result.clusters.size()==24;
    result.fingerprint=Q::Fingerprint(result);return result;
}
}
