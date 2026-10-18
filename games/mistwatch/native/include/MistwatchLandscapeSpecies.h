#pragma once
#include "MistwatchLandscape.h"
#include "MistwatchBroadleaf.h"
#include <Aevrynd/ExactTriangleContact3D.h>
#include <Aevrynd/TrianglePointBroadPhase3D.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace Mistwatch {
enum class LandscapeSpeciesProfile : std::uint8_t { Original, EdgeMixedBroadleafV1 };
enum class LandscapeTreeSpecies : std::uint8_t { Fir, Broadleaf };
struct LandscapeSpeciesInstance {
    unsigned sourceOrdinal{};
    LandscapeTreeSpecies species{LandscapeTreeSpecies::Fir};
    Aevrynd::Transform3D render{};
};
struct LandscapeSpeciesPlan {
    std::array<LandscapeSpeciesInstance,520> instances{};
    std::array<unsigned,16> selectedSourceOrdinals{};
    unsigned selected{},firstMissingSlot{~0u};
    unsigned considered{},rejectedBand{},rejectedBounds{},rejectedSlope{},rejectedGround{},rejectedOverlap{};
    unsigned groundEvaluations{},groundCacheHits{},evaluatedIntersections{},skippedLowerFaces{};
    bool reachedBudget{};
    std::uint64_t sourcePoseFingerprint{},renderSpeciesFingerprint{};
    float maximumRootMeshError{},minimumLowerFaceGap{},maximumBaseStripGap{};
};
namespace LandscapeSpeciesDetail {
using namespace Aevrynd;
namespace Exact=::Aevrynd::ExactTriangleContactDetail3D;
using R=Exact::Rational;
using P=Exact::Point;
using T=Exact::Triangle;
inline void Need(bool value,const char* label){if(!value)throw std::invalid_argument(label);}
inline bool Finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline void Word(std::uint64_t& h,std::uint32_t value){for(unsigned b=0;b<4;++b){h^=(value>>(b*8))&255u;h*=1099511628211ull;}}
inline void Pose(std::uint64_t& h,const Transform3D& p){for(float f:{p.position.x,p.position.y,p.position.z,
    p.rotation.x,p.rotation.y,p.rotation.z,p.scale.x,p.scale.y,p.scale.z})Word(h,std::bit_cast<std::uint32_t>(f));}
inline P Flat(P p){p[1]=R{};return p;}
inline auto Xz(const P& p){return Exact::Project(p,1);}
inline R Area(const P& a,const P& b,const P& c){return Exact::Orient(Xz(a),Xz(b),Xz(c));}
inline R Abs(R r){return r.Sign()<0?-r:r;}
inline Vec3 Vertex(const Mesh3D& m,unsigned i){const auto& v=m.vertices[i];return{v.x,v.y,v.z};}
inline T Triangle(const Mesh3D& m,unsigned face){return{{Exact::ToExact(Vertex(m,m.indices[face])),
    Exact::ToExact(Vertex(m,m.indices[face+1])),Exact::ToExact(Vertex(m,m.indices[face+2]))}};}
inline R Height(const T& tri,const P& point){
    const auto a=Area(tri[0],tri[1],tri[2]);Need(a.Sign()!=0,"LANDSCAPE_SPECIES_PROJECTED_SURFACE_DEGENERATE");
    return (Area(tri[1],tri[2],point)*tri[0][1]+Area(tri[2],tri[0],point)*tri[1][1]+
        Area(tri[0],tri[1],point)*tri[2][1])/a;
}
inline void ValidateMesh(const Mesh3D& mesh,bool surface){
    Need(mesh.Valid()&&mesh.vertices.size()<=120000&&mesh.indices.size()<=360000,"LANDSCAPE_SPECIES_INVALID_MESH");
    for(const auto& v:mesh.vertices)Need(Finite({v.x,v.y,v.z})&&std::abs(v.x)<=10000&&std::abs(v.y)<=10000&&
        std::abs(v.z)<=10000&&std::isfinite(v.u)&&std::isfinite(v.v),"LANDSCAPE_SPECIES_NONFINITE_MESH");
    for(unsigned f=0;f<mesh.indices.size();f+=3){
        const auto a=Vertex(mesh,mesh.indices[f]),b=Vertex(mesh,mesh.indices[f+1]),c=Vertex(mesh,mesh.indices[f+2]);
        const auto n=Cross(b-a,c-a);
        Need(Finite(n)&&Length(n)>0,"LANDSCAPE_SPECIES_DEGENERATE_FACE");
        if(surface)Need(n.y>0,"LANDSCAPE_SPECIES_SURFACE_NOT_UPWARD");
    }
}
// 使用既有 exact rational 算術；切面交點不轉回 float 再做幾何判定。
inline std::vector<P> ClipHeight(std::span<const P> polygon,const R& limit){
    std::vector<P> out;
    for(std::size_t i=0;i<polygon.size();++i){const auto& a=polygon[i];const auto& b=polygon[(i+1)%polygon.size()];
        const auto da=a[1]-limit,db=b[1]-limit;
        if(da.Sign()<=0)Exact::Append(out,a);
        if((da.Sign()<=0)!=(db.Sign()<=0))Exact::Append(out,Exact::Interpolate(a,b,da/(da-db)));
    }
    return out;
}
inline std::vector<P> Hull(std::vector<P> points){
    for(auto& p:points)p=Flat(p);
    std::sort(points.begin(),points.end(),[](const P& a,const P& b){const int x=Compare(a[0],b[0]);return x?x<0:Compare(a[2],b[2])<0;});
    points.erase(std::unique(points.begin(),points.end(),[](const P& a,const P& b){return Exact::Equal(a,b);}),points.end());
    Need(points.size()>=3,"LANDSCAPE_SPECIES_EMPTY_SUPPORT");std::vector<P> hull;
    for(const auto& p:points){while(hull.size()>=2&&Area(hull[hull.size()-2],hull.back(),p).Sign()<=0)hull.pop_back();hull.push_back(p);}
    const auto lower=hull.size();
    for(std::size_t i=points.size()-1;i>0;--i){const auto& p=points[i-1];
        while(hull.size()>lower&&Area(hull[hull.size()-2],hull.back(),p).Sign()<=0)hull.pop_back();hull.push_back(p);}
    hull.pop_back();Need(hull.size()>=3,"LANDSCAPE_SPECIES_EMPTY_SUPPORT");return hull;
}
// 真地表三角形的有限8m索引；不呼叫解析高度代替提交的網格。
class Surface {
    const Mesh3D& mesh_;
    std::map<std::pair<int,int>,std::vector<unsigned>> cells_;
    static int Cell(double x){return static_cast<int>(std::floor(x/8.0));}
public:
    explicit Surface(const Mesh3D& mesh):mesh_(mesh){
        ValidateMesh(mesh,true);
        for(unsigned f=0;f<mesh.indices.size();f+=3){const auto t=Triangle(mesh,f);const auto ids=Cells(t);
            for(const auto id:ids)cells_[id].push_back(f);}
    }
    static std::vector<std::pair<int,int>> Cells(std::span<const P> points){
        Need(!points.empty(),"LANDSCAPE_SPECIES_EMPTY_QUERY");double x0=points[0][0].Approximate(),x1=x0,z0=points[0][2].Approximate(),z1=z0;
        for(const auto& p:points){const double x=p[0].Approximate(),z=p[2].Approximate();
            x0=(std::min)(x0,x);x1=(std::max)(x1,x);z0=(std::min)(z0,z);z1=(std::max)(z1,z);}
        // 向外展開避免 rational -> double 的 broad-phase 捨入漏掉邊界接觸。
        x0=std::nextafter(x0,-std::numeric_limits<double>::infinity());x1=std::nextafter(x1,std::numeric_limits<double>::infinity());
        z0=std::nextafter(z0,-std::numeric_limits<double>::infinity());z1=std::nextafter(z1,std::numeric_limits<double>::infinity());
        const int ax=Cell(x0),bx=Cell(x1),az=Cell(z0),bz=Cell(z1);
        Need(bx-ax<=8&&bz-az<=8,"LANDSCAPE_SPECIES_QUERY_RESOURCE_BOUND");std::vector<std::pair<int,int>> ids;
        for(int z=az;z<=bz;++z)for(int x=ax;x<=bx;++x)ids.emplace_back(x,z);return ids;
    }
    std::vector<unsigned> Query(std::span<const P> points)const{
        std::vector<unsigned> faces;for(const auto id:Cells(points)){const auto it=cells_.find(id);
            if(it!=cells_.end())faces.insert(faces.end(),it->second.begin(),it->second.end());}
        std::sort(faces.begin(),faces.end());faces.erase(std::unique(faces.begin(),faces.end()),faces.end());return faces;
    }
    T Face(unsigned face)const{return Triangle(mesh_,face);}
    float NormalY(unsigned face)const{const auto a=Vertex(mesh_,mesh_.indices[face]),b=Vertex(mesh_,mesh_.indices[face+1]),c=Vertex(mesh_,mesh_.indices[face+2]);return Normalize(Cross(b-a,c-a)).y;}
    R At(Vec3 point,bool pointBounds=true)const{
        const auto p=Exact::ToExact(point);const std::array<P,1> query{p};bool found{};R height;
        for(const auto f:Query(query)){
            // Surface原本引用來源；當場讀取頂點避免呼叫者更新Y／XZ後使用陳舊包圍盒。
            // false保留原exact oracle；不改Query、三角邊／角或疊面ambiguity檢查。
            if(pointBounds&&!TrianglePointMayOverlapXZ3D(point,Vertex(mesh_,mesh_.indices[f]),
                Vertex(mesh_,mesh_.indices[f+1]),Vertex(mesh_,mesh_.indices[f+2])))continue;
            const auto tri=Face(f);const std::array<Exact::Projected,3> projected{Xz(tri[0]),Xz(tri[1]),Xz(tri[2])};
            if(!Exact::Inside(Xz(p),projected))continue;const auto h=Height(tri,p);
            if(found)Need(Compare(Abs(h-height),R::Float(.00005f))<=0,"LANDSCAPE_SPECIES_AMBIGUOUS_SURFACE");else height=h;found=true;}
        Need(found,"LANDSCAPE_SPECIES_ROOT_SURFACE_MISSING");return height;
    }
};
inline std::vector<P> PlanarIntersection(const T& a,const T& b){
    T pa,pb;for(unsigned i=0;i<3;++i){pa[i]=Flat(a[i]);pb[i]=Flat(b[i]);}
    return Exact::CoplanarIntersection(pa,pb,P{{R{},R(1),R{}}});
}
// 垂直面投影為線時仍保留整段和高度端點，不能跳過底部碰撞。
inline std::vector<P> SegmentIntersection(const P& a,const P& b,const T& ground){
    const auto direction=Area(ground[0],ground[1],ground[2]);Need(direction.Sign()!=0,"LANDSCAPE_SPECIES_PROJECTED_SURFACE_DEGENERATE");
    R lo,hi(1);const R zero,one(1);
    for(unsigned e=0;e<3;++e){auto da=Area(ground[e],ground[(e+1)%3],a),db=Area(ground[e],ground[(e+1)%3],b);
        if(direction.Sign()<0){da=-da;db=-db;}
        if(da.Sign()<0&&db.Sign()<0)return{};
        if((da.Sign()<0)!=(db.Sign()<0)){const auto t=da/(da-db);
            if(da.Sign()<0){if(Compare(t,lo)>0)lo=t;}else if(Compare(t,hi)<0)hi=t;}
    }
    if(Compare(lo,hi)>0||Compare(lo,zero)<0||Compare(hi,one)>0)return{};
    return{Exact::Interpolate(a,b,lo),Exact::Interpolate(a,b,hi)};
}
struct Range {R minimum,maximum;unsigned points{};};
inline void Record(Range& range,const R& gap){if(!range.points){range.minimum=gap;range.maximum=gap;}
    else{if(Compare(gap,range.minimum)<0)range.minimum=gap;if(Compare(gap,range.maximum)>0)range.maximum=gap;}++range.points;}
inline Range Overlay(const T& face,const Surface& surface){
    Range out;const bool flat=Area(face[0],face[1],face[2]).Sign()==0;
    for(const auto f:surface.Query(face)){const auto ground=surface.Face(f);
        if(flat){for(unsigned e=0;e<3;++e)for(const auto& p:SegmentIntersection(face[e],face[(e+1)%3],ground))Record(out,p[1]-Height(ground,p));}
        else for(const auto& p:PlanarIntersection(face,ground))Record(out,Height(face,p)-Height(ground,p));
    }
    return out;
}
inline MeshBounds3D Measure(const std::array<Mesh3D,3>& meshes){
    constexpr std::array<std::size_t,3> vertices{4306,99050,1329},indices{10704,232725,5133};
    Vec3 minimum{10000,10000,10000},maximum{-10000,-10000,-10000};
    for(unsigned slot=0;slot<3;++slot){const auto& m=meshes[slot];ValidateMesh(m,false);
        Need(m.vertices.size()==vertices[slot]&&m.indices.size()==indices[slot],"LANDSCAPE_SPECIES_SOURCE_COUNTS");
        for(const auto& v:m.vertices){minimum.x=(std::min)(minimum.x,v.x);minimum.y=(std::min)(minimum.y,v.y);minimum.z=(std::min)(minimum.z,v.z);
            maximum.x=(std::max)(maximum.x,v.x);maximum.y=(std::max)(maximum.y,v.y);maximum.z=(std::max)(maximum.z,v.z);}}
    const auto known=BroadleafBounds();Need(Length(minimum-(known.center-known.extents))<=.00001f&&
        Length(maximum-(known.center+known.extents))<=.00001f,"LANDSCAPE_SPECIES_SOURCE_BOUNDS");
    Vec3 low{10000,10000,10000},high{-10000,-10000,-10000};unsigned crossings{},points{};
    const auto& trunk=meshes[2];for(unsigned f=0;f<trunk.indices.size();f+=3){const auto tri=Triangle(trunk,f);
        for(unsigned e=0;e<3;++e)crossings+=(Compare(tri[e][1],R(2))*Compare(tri[(e+1)%3][1],R(2))<0);
        for(const auto& p:ClipHeight(tri,R(2))){const Vec3 v{static_cast<float>(p[0].Approximate()),static_cast<float>(p[1].Approximate()),static_cast<float>(p[2].Approximate())};
            low.x=(std::min)(low.x,v.x);low.y=(std::min)(low.y,v.y);low.z=(std::min)(low.z,v.z);
            high.x=(std::max)(high.x,v.x);high.y=(std::max)(high.y,v.y);high.z=(std::max)(high.z,v.z);++points;}}
    const auto lower=BroadleafLowerTrunkBounds();Need(points>100&&crossings>0&&Length(low-(lower.center-lower.extents))<=.00001f&&
        Length(high-(lower.center+lower.extents))<=.00001f,"LANDSCAPE_SPECIES_LOWER_SOURCE_BOUNDS");
    return{(minimum+maximum)*.5f,known.radius,(maximum-minimum)*.5f};
}
enum class GroundResult { Safe, Slope, Ground };
// 診斷只記實際求交量；剪枝面不冒充已生成完整交點。
struct GroundProof {GroundResult result{GroundResult::Ground};float minimumLowerGap{},maximumBaseGap{};unsigned evaluatedIntersections{},skippedLowerFaces{};};
inline GroundProof Ground(const Transform3D& pose,const Mesh3D& trunk,const Surface& surface,bool optimize=true){
    const auto matrix=pose.Matrix();std::vector<T> lowerFaces,baseFaces;std::vector<P> support;
    const R top=R::Float(pose.position.y+2.f*pose.scale.x),baseTop=R::Float(pose.position.y+.035f);
    for(unsigned f=0;f<trunk.indices.size();f+=3){T face;for(unsigned i=0;i<3;++i)face[i]=Exact::ToExact(TransformPoint(matrix,Vertex(trunk,trunk.indices[f+i])));
        const auto lower=ClipHeight(face,top),base=ClipHeight(face,baseTop);
        support.insert(support.end(),lower.begin(),lower.end());
        for(std::size_t i=1;i+1<lower.size();++i)lowerFaces.push_back({lower[0],lower[i],lower[i+1]});
        for(std::size_t i=1;i+1<base.size();++i)baseFaces.push_back({base[0],base[i],base[i+1]});}
    GroundProof proof;if(support.size()<3||lowerFaces.empty()||baseFaces.empty())return proof;
    const auto hull=Hull(std::move(support));const R root=R::Float(pose.position.y),heightLimit=R::Float(.10f);Range groundHeights;
    for(std::size_t k=1;k+1<hull.size();++k){const T footprint{hull[0],hull[k],hull[k+1]};
        const R expected=Abs(Area(footprint[0],footprint[1],footprint[2]));R covered;
        for(const auto f:surface.Query(footprint)){const auto ground=surface.Face(f);const auto polygon=PlanarIntersection(footprint,ground);
            if(polygon.empty())continue;
            // 交域點集無需排序即可核高度極值；覆蓋面積另用真實凸包。
            for(const auto& p:polygon){const auto height=Height(ground,p);
                if(Compare(Abs(height-root),heightLimit)>0)return proof;Record(groundHeights,height);}
            bool hasArea{};for(std::size_t i=1;i+1<polygon.size();++i)hasArea|=Area(polygon[0],polygon[i],polygon[i+1]).Sign()!=0;
            if(hasArea){const auto intersection=Hull(polygon);R area;
                for(std::size_t i=1;i+1<intersection.size();++i)area=area+Abs(Area(intersection[0],intersection[i],intersection[i+1]));
                if(area.Sign()){if(surface.NormalY(f)<.97f){proof.result=GroundResult::Slope;return proof;}covered=covered+area;}}
        }
        if(Compare(expected,covered)!=0)return proof;
    }
    if(!groundHeights.points)return proof;
    Range low,base;const R penetration=R::Float(-.04f),floating=R::Float(.04f);
    for(const auto& face:lowerFaces){
        // 完整腳印已通過覆蓋／坡度／高度門；真三角地表最大值給出精確下界。
        // 首面仍求交，後續面只有確定不能降低既有真 minimum 才能剪枝。
        if(optimize&&low.points){R minY=face[0][1];for(unsigned i=1;i<3;++i)if(Compare(face[i][1],minY)<0)minY=face[i][1];
            if(Compare(minY-groundHeights.maximum,low.minimum)>=0){++proof.skippedLowerFaces;continue;}}
        const auto r=Overlay(face,surface);if(!r.points)return proof;
        proof.evaluatedIntersections+=r.points;
        if(Compare(r.minimum,penetration)<0)return proof;Record(low,r.minimum);}
    for(const auto& face:baseFaces){const auto r=Overlay(face,surface);if(!r.points)return proof;
        proof.evaluatedIntersections+=r.points;
        if(Compare(r.minimum,penetration)<0||Compare(r.maximum,floating)>0)return proof;
        Record(base,r.maximum);}
    if(!low.points||!base.points)return proof;
    proof.minimumLowerGap=static_cast<float>(low.minimum.Approximate());proof.maximumBaseGap=static_cast<float>(base.maximum.Approximate());
    proof.result=GroundResult::Safe;return proof;
}
inline bool OutsideCore(const MeshBounds3D& b){return b.center.x+b.extents.x< -53.f||b.center.x-b.extents.x>53.f||
    b.center.z+b.extents.z< -23.f||b.center.z-b.extents.z>103.f;}
inline bool Overlap(const MeshBounds3D& a,const MeshBounds3D& b){return std::abs(a.center.x-b.center.x)<=a.extents.x+b.extents.x&&
    std::abs(a.center.z-b.center.z)<=a.extents.z+b.extents.z;}
}
inline std::uint64_t LandscapeSourcePoseFingerprint(std::span<const Aevrynd::Transform3D> trees){
    std::uint64_t h=14695981039346656037ull;LandscapeSpeciesDetail::Word(h,static_cast<unsigned>(trees.size()));
    for(unsigned i=0;i<trees.size();++i){LandscapeSpeciesDetail::Word(h,i);LandscapeSpeciesDetail::Pose(h,trees[i]);}return h;
}
inline LandscapeSpeciesPlan CreateLandscapeSpeciesPlan(const Aevrynd::HeightField3D& core,
    const Aevrynd::Mesh3D& landscape,std::span<const Aevrynd::Transform3D> original,
    const std::array<Aevrynd::Mesh3D,3>& broadleaf,std::uint64_t sealedSourcePoseFingerprint,
    LandscapeSpeciesProfile profile=LandscapeSpeciesProfile::Original){
    using namespace Aevrynd;using namespace LandscapeSpeciesDetail;
    Need(profile==LandscapeSpeciesProfile::Original||profile==LandscapeSpeciesProfile::EdgeMixedBroadleafV1,"LANDSCAPE_SPECIES_INVALID_PROFILE");
    Need(original.size()==520,"LANDSCAPE_SPECIES_SOURCE_POPULATION");
    for(unsigned i=0;i<original.size();++i){const auto& p=original[i];
        Need(Finite(p.position)&&Finite(p.rotation)&&Finite(p.scale)&&p.rotation.x==0&&p.rotation.z==0&&p.scale.x>=.5f&&p.scale.x<=.85f&&
            p.scale.x==p.scale.y&&p.scale.x==p.scale.z,"LANDSCAPE_SPECIES_SOURCE_POSE");
        Need(p.position.x>=-150&&p.position.x<=150&&p.position.z>=-110&&p.position.z<=190&&
            !(p.position.x>=-53&&p.position.x<=53&&p.position.z>=-23&&p.position.z<=103),"LANDSCAPE_SPECIES_SOURCE_DOMAIN");
        for(unsigned j=0;j<i;++j)Need(std::hypot(p.position.x-original[j].position.x,p.position.z-original[j].position.z)>=5.f,"LANDSCAPE_SPECIES_SOURCE_SPACING");}
    const auto fingerprint=LandscapeSourcePoseFingerprint(original);Need(fingerprint==sealedSourcePoseFingerprint,"LANDSCAPE_SPECIES_SOURCE_SEAL");
    const auto crown=Measure(broadleaf);const Surface surface(landscape);LandscapeSpeciesPlan out;out.selectedSourceOrdinals.fill(~0u);out.sourcePoseFingerprint=fingerprint;
    for(unsigned i=0;i<original.size();++i){const auto& p=original[i];
        const auto exactError=Abs(surface.At(p.position)-R::Float(p.position.y));const double error=exactError.Approximate();
        Need(Compare(exactError,R::Float(.00005f))<=0,"LANDSCAPE_SPECIES_SOURCE_ROOT_GAP");out.maximumRootMeshError=(std::max)(out.maximumRootMeshError,static_cast<float>(error));
        Need(LandscapeNormal(core,p.position.x,p.position.z).y>=.78f,"LANDSCAPE_SPECIES_SOURCE_SLOPE");out.instances[i]={i,LandscapeTreeSpecies::Fir,p};}
    if(profile==LandscapeSpeciesProfile::EdgeMixedBroadleafV1){
        std::array<bool,520> used{};std::vector<MeshBounds3D> selectedBounds;
        // 僅此呼叫的固定 ordinal／姿態／來源；不快取受已選樹影響的 overlap。
        std::array<std::optional<GroundProof>,520> groundCache;
        for(unsigned slot=0;slot<16;++slot){const bool left=slot<8;const float anchor=float(slot%8)*12.f;std::vector<unsigned> candidates;
            for(unsigned i=0;i<original.size();++i){const auto p=original[i].position;
                if(!used[i]&&(p.x<0)==left&&std::abs(p.x)>=54&&std::abs(p.x)<=90&&p.z>=-10&&p.z<=95)candidates.push_back(i);else ++out.rejectedBand;}
            std::sort(candidates.begin(),candidates.end(),[&](unsigned a,unsigned b){const auto x=original[a].position,y=original[b].position;
                const float da=std::abs(x.z-anchor),db=std::abs(y.z-anchor);if(da!=db)return da<db;
                const float ax=std::abs(x.x)-54.f,bx=std::abs(y.x)-54.f;return ax!=bx?ax<bx:a<b;});
            bool found{};
            for(const auto i:candidates){++out.considered;auto pose=original[i];const float scale=pose.scale.x*1.4f;pose.scale={scale,scale,scale};
                const auto bounds=TransformBounds(crown,pose);if(!OutsideCore(bounds)){++out.rejectedBounds;continue;}
                bool occupied{};for(const auto& b:selectedBounds)occupied|=Overlap(bounds,b);if(occupied){++out.rejectedOverlap;continue;}
                if(LandscapeNormal(core,pose.position.x,pose.position.z).y<.97f){++out.rejectedSlope;continue;}
                if(!groundCache[i]){groundCache[i]=Ground(pose,broadleaf[2],surface);++out.groundEvaluations;
                    out.evaluatedIntersections+=groundCache[i]->evaluatedIntersections;out.skippedLowerFaces+=groundCache[i]->skippedLowerFaces;}
                else ++out.groundCacheHits;
                const auto& proof=*groundCache[i];if(proof.result!=GroundResult::Safe){
                    if(proof.result==GroundResult::Slope)++out.rejectedSlope;else ++out.rejectedGround;continue;}
                out.instances[i]={i,LandscapeTreeSpecies::Broadleaf,pose};out.selectedSourceOrdinals[slot]=i;used[i]=true;selectedBounds.push_back(bounds);
                if(!out.selected){out.minimumLowerFaceGap=proof.minimumLowerGap;out.maximumBaseStripGap=proof.maximumBaseGap;}
                else{out.minimumLowerFaceGap=(std::min)(out.minimumLowerFaceGap,proof.minimumLowerGap);out.maximumBaseStripGap=(std::max)(out.maximumBaseStripGap,proof.maximumBaseGap);}
                ++out.selected;found=true;break;
            }
            if(!found&&out.firstMissingSlot==~0u)out.firstMissingSlot=slot;
        }
        out.reachedBudget=out.selected==16;
    }else out.reachedBudget=true;
    out.renderSpeciesFingerprint=14695981039346656037ull;
    for(const auto& instance:out.instances){Word(out.renderSpeciesFingerprint,instance.sourceOrdinal);
        Word(out.renderSpeciesFingerprint,static_cast<unsigned>(instance.species));Pose(out.renderSpeciesFingerprint,instance.render);}
    return out;
}
}
