#pragma once
#include "MistwatchLandscapeSpecies.h"
#include <string>
#include <string_view>

namespace Mistwatch {
inline constexpr std::string_view RootSupportProfile="source-material-foot-v1";
enum class RootSupportResult : std::uint8_t {Safe,Slope,Ground};
struct RootSupportProof {
    std::string_view profile{RootSupportProfile};RootSupportResult result{RootSupportResult::Ground};
    float minimumLowerGap{},maximumFootGap{};
    unsigned sourceFootFaces{},sourceFootTriangles{},sourceFootSegments{},sourceFootPoints{},sourceRimSegments{},sourceNonmanifoldEdges{},lowerFaces{};
    unsigned evaluatedIntersections{},skippedLowerFaces{};
    std::uint64_t sourceMeshFingerprint{},derivedMeshFingerprint{},sourceFootFingerprint{};
};
namespace RootSupportDetail {
using namespace Aevrynd;namespace D=::Mistwatch::LandscapeSpeciesDetail;namespace Exact=D::Exact;
using R=D::R;using P=D::P;using T=D::T;
struct FootPolygon {unsigned sourceFace{};std::vector<P> barycentric;};
struct RimEdge {unsigned a{},b{};};
inline void Need(bool value,const char* label){if(!value)throw std::invalid_argument(label);}
inline bool SameWord(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline std::uint64_t MeshFingerprint(const Mesh3D& mesh){std::uint64_t h=14695981039346656037ull;
    D::Word(h,static_cast<unsigned>(mesh.vertices.size()));D::Word(h,static_cast<unsigned>(mesh.indices.size()));
    for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z,v.u,v.v})D::Word(h,std::bit_cast<std::uint32_t>(f));D::Word(h,v.normal);D::Word(h,v.color);}
    for(const auto i:mesh.indices)D::Word(h,i);return h;
}
inline void ValidatePose(const Transform3D& pose){Need(D::Finite(pose.position)&&D::Finite(pose.rotation)&&D::Finite(pose.scale)&&
    pose.rotation.x==0&&pose.rotation.z==0&&pose.scale.x>0&&pose.scale.x==pose.scale.y&&pose.scale.x==pose.scale.z,"ROOT_SUPPORT_POSE");}
inline void ValidateInput(const Mesh3D& source,const Mesh3D& final,const Transform3D& pose){
    ValidatePose(pose);D::ValidateMesh(source,false);D::ValidateMesh(final,false);
    Need(source.vertices.size()==final.vertices.size()&&source.indices==final.indices,"ROOT_SUPPORT_TOPOLOGY");
    float lowest=10000;std::map<std::array<std::uint32_t,3>,std::uint32_t> weldedY;
    for(unsigned i=0;i<source.vertices.size();++i){const auto& a=source.vertices[i];const auto& b=final.vertices[i];lowest=(std::min)(lowest,a.y);
        Need(SameWord(a.x,b.x)&&SameWord(a.z,b.z),"ROOT_SUPPORT_XZ");
        Need(SameWord(a.u,b.u)&&SameWord(a.v,b.v)&&a.color==b.color,"ROOT_SUPPORT_MATERIAL_IDENTITY");
        Need((a.normal&0xff000000u)==(b.normal&0xff000000u),"ROOT_SUPPORT_NORMAL_HANDEDNESS");
        if(a.y>=.35f)Need(SameWord(a.y,b.y),"ROOT_SUPPORT_UPPER_POSITION");
        const std::array<std::uint32_t,3> key{a.x==0?0:std::bit_cast<std::uint32_t>(a.x),a.y==0?0:std::bit_cast<std::uint32_t>(a.y),a.z==0?0:std::bit_cast<std::uint32_t>(a.z)};
        const auto y=b.y==0?0:std::bit_cast<std::uint32_t>(b.y);const auto [it,inserted]=weldedY.emplace(key,y);
        Need(inserted||it->second==y,"ROOT_SUPPORT_SPLIT_WELD");}
    Need(lowest==0.f,"ROOT_SUPPORT_SOURCE_MINIMUM");
    for(unsigned f=0;f<source.indices.size();f+=3){const auto a=source.indices[f],b=source.indices[f+1],c=source.indices[f+2];
        const auto before=Cross(D::Vertex(source,b)-D::Vertex(source,a),D::Vertex(source,c)-D::Vertex(source,a));
        const auto after=Cross(D::Vertex(final,b)-D::Vertex(final,a),D::Vertex(final,c)-D::Vertex(final,a));
        Need(Dot(before,after)>0,"ROOT_SUPPORT_FACE_ORIENTATION");}
}
inline P Weighted(const T& triangle,const P& weights){P out;for(unsigned axis=0;axis<3;++axis)
    for(unsigned i=0;i<3;++i)out[axis]=out[axis]+triangle[i][axis]*weights[i];return out;}
inline P MapFootPoint(const T& original,const T& final,const P& weights,const R& cut){
    R sum;for(const auto& weight:weights){Need(weight.Sign()>=0&&Compare(weight,R(1))<=0,"ROOT_SUPPORT_FOOT_WEIGHTS");sum=sum+weight;}
    Need(Compare(sum,R(1))==0,"ROOT_SUPPORT_FOOT_WEIGHTS");
    Need(Compare(Weighted(original,weights)[1],cut)<=0,"ROOT_SUPPORT_FOOT_DOMAIN");return Weighted(final,weights);
}
// 先在封印原面的材質參數域裁帶；不能使用最終高度或 gap 選足帶。
inline std::vector<P> ClipSourceFoot(const T& source,const R& cut){
    const std::array<P,3> weights{{P{{R(1),R{},R{}}},P{{R{},R(1),R{}}},P{{R{},R{},R(1)}}}};
    std::vector<P> out;
    for(unsigned i=0;i<3;++i){const auto j=(i+1)%3;const auto da=source[i][1]-cut,db=source[j][1]-cut;
        if(da.Sign()<=0)Exact::Append(out,weights[i]);
        if((da.Sign()<=0)!=(db.Sign()<=0))Exact::Append(out,Exact::Interpolate(weights[i],weights[j],da/(da-db)));}
    return out;
}
inline std::vector<FootPolygon> BuildFootProvenance(const Mesh3D& source,const Transform3D& pose){
    ValidatePose(pose);D::ValidateMesh(source,false);float low=10000;for(const auto& v:source.vertices)low=(std::min)(low,v.y);
    Need(low==0.f,"ROOT_SUPPORT_SOURCE_MINIMUM");const R cut=R::Float(.035f)/R::Float(pose.scale.x);std::vector<FootPolygon> out;
    for(unsigned face=0;face<source.indices.size();face+=3){auto polygon=ClipSourceFoot(D::Triangle(source,face),cut);
        if(!polygon.empty())out.push_back({face,std::move(polygon)});}
    Need(!out.empty(),"ROOT_SUPPORT_EMPTY_FOOT");return out;
}
inline void ValidateFootProvenance(const Mesh3D& source,const Transform3D& pose,std::span<const FootPolygon> actual){
    const auto expected=BuildFootProvenance(source,pose);Need(actual.size()==expected.size(),"ROOT_SUPPORT_FOOT_PROVENANCE");
    for(unsigned i=0;i<expected.size();++i){Need(actual[i].sourceFace==expected[i].sourceFace&&actual[i].barycentric.size()==expected[i].barycentric.size(),"ROOT_SUPPORT_FOOT_PROVENANCE");
        for(unsigned j=0;j<expected[i].barycentric.size();++j)Need(Exact::Equal(actual[i].barycentric[j],expected[i].barycentric[j]),"ROOT_SUPPORT_FOOT_PROVENANCE");}
}
// UV 接縫頂點以真位置焊接，只證明完整閉合底輪廓，不宣稱生成實心封底。
inline std::vector<RimEdge> SourceRim(const Mesh3D& source,const Transform3D& pose,unsigned* nonmanifold=nullptr){
    using Key=std::array<std::uint32_t,3>;std::map<Key,unsigned> welded;std::vector<unsigned> ids(source.vertices.size()),representatives;
    for(unsigned i=0;i<source.vertices.size();++i){const auto& v=source.vertices[i];const Key key{
        v.x==0?0:std::bit_cast<std::uint32_t>(v.x),v.y==0?0:std::bit_cast<std::uint32_t>(v.y),v.z==0?0:std::bit_cast<std::uint32_t>(v.z)};
        auto [it,added]=welded.emplace(key,static_cast<unsigned>(representatives.size()));if(added)representatives.push_back(i);ids[i]=it->second;}
    std::map<std::pair<unsigned,unsigned>,unsigned> edges;
    for(unsigned f=0;f<source.indices.size();f+=3)for(unsigned e=0;e<3;++e){auto a=ids[source.indices[f+e]],b=ids[source.indices[f+(e+1)%3]];
        Need(a!=b,"ROOT_SUPPORT_RIM_DEGENERATE_EDGE");if(a>b)std::swap(a,b);++edges[{a,b}];}
    // 來源可能有封印的交叉枝幹；只驗真 boundary 閉環，不假稱全來源水密。
    if(nonmanifold){*nonmanifold=0;for(const auto& [edge,count]:edges)if(count>2)++*nonmanifold;}
    const R cut=R::Float(.035f)/R::Float(pose.scale.x);std::vector<unsigned> degree(representatives.size());std::vector<RimEdge> rim;
    for(const auto& [edge,count]:edges)if(count==1){const auto a=representatives[edge.first],b=representatives[edge.second];
        const bool inA=Compare(R::Float(source.vertices[a].y),cut)<=0,inB=Compare(R::Float(source.vertices[b].y),cut)<=0;
        Need(inA==inB,"ROOT_SUPPORT_RIM_CROSSES_FOOT");if(inA){++degree[edge.first];++degree[edge.second];rim.push_back({a,b});}}
    Need(!rim.empty(),"ROOT_SUPPORT_EMPTY_RIM");for(const auto d:degree)Need(d==0||d==2,"ROOT_SUPPORT_RIM_NOT_CLOSED");return rim;
}
inline T WorldTriangle(const Mesh3D& mesh,unsigned face,const Matrix4& matrix){T out;for(unsigned i=0;i<3;++i){
    const auto point=TransformPoint(matrix,D::Vertex(mesh,mesh.indices[face+i]));Need(D::Finite(point),"ROOT_SUPPORT_WORLD_NONFINITE");out[i]=Exact::ToExact(point);}return out;}
inline void RationalWord(std::uint64_t& h,const R& value){D::Word(h,static_cast<unsigned>(value.numerator.Sign()+1));
    D::Word(h,static_cast<unsigned>(value.numerator.words.size()));for(const auto w:value.numerator.words)D::Word(h,w);
    D::Word(h,static_cast<unsigned>(value.denominator.words.size()));for(const auto w:value.denominator.words)D::Word(h,w);}
}
inline RootSupportProof RootSupportGround(const Aevrynd::Transform3D& pose,const Aevrynd::Mesh3D& originalSourceTrunk,
    const Aevrynd::Mesh3D& finalDerivedTrunk,const LandscapeSpeciesDetail::Surface& actualSurface,bool optimize=true){
    using namespace Aevrynd;namespace S=RootSupportDetail;namespace D=LandscapeSpeciesDetail;
    // 只補失敗階段；不把算術未解決轉為幾何判退或通過。
    std::string stage="validate-input";try {
    S::ValidateInput(originalSourceTrunk,finalDerivedTrunk,pose);const auto provenance=S::BuildFootProvenance(originalSourceTrunk,pose);
    S::ValidateFootProvenance(originalSourceTrunk,pose,provenance);RootSupportProof proof;const auto rim=S::SourceRim(originalSourceTrunk,pose,&proof.sourceNonmanifoldEdges);
    proof.sourceMeshFingerprint=S::MeshFingerprint(originalSourceTrunk);proof.derivedMeshFingerprint=S::MeshFingerprint(finalDerivedTrunk);
    proof.sourceFootFaces=static_cast<unsigned>(provenance.size());proof.sourceRimSegments=static_cast<unsigned>(rim.size());proof.sourceFootFingerprint=14695981039346656037ull;
    const auto matrix=pose.Matrix();const auto cut=D::R::Float(.035f)/D::R::Float(pose.scale.x);
    std::vector<D::T> footFaces,lowerFaces,rimSegments;std::vector<D::P> support;
    for(const auto& record:provenance){const auto source=D::Triangle(originalSourceTrunk,record.sourceFace),final=S::WorldTriangle(finalDerivedTrunk,record.sourceFace,matrix);
        stage="map-source-foot face="+std::to_string(record.sourceFace);
        D::Word(proof.sourceFootFingerprint,record.sourceFace);D::Word(proof.sourceFootFingerprint,static_cast<unsigned>(record.barycentric.size()));std::vector<D::P> polygon;
        for(const auto& weights:record.barycentric){for(const auto& w:weights)S::RationalWord(proof.sourceFootFingerprint,w);
            polygon.push_back(S::MapFootPoint(source,final,weights,cut));support.push_back(polygon.back());}
        if(polygon.size()==1){++proof.sourceFootPoints;footFaces.push_back({polygon[0],polygon[0],polygon[0]});}
        else if(polygon.size()==2){++proof.sourceFootSegments;footFaces.push_back({polygon[0],polygon[1],polygon[1]});}
        else for(unsigned i=1;i+1<polygon.size();++i){++proof.sourceFootTriangles;footFaces.push_back({polygon[0],polygon[i],polygon[i+1]});}}
    const auto top=D::R::Float(pose.position.y+2.f*pose.scale.x);
    for(unsigned face=0;face<finalDerivedTrunk.indices.size();face+=3){stage="lower-support face="+std::to_string(face);
        const auto polygon=D::ClipHeight(S::WorldTriangle(finalDerivedTrunk,face,matrix),top);
        support.insert(support.end(),polygon.begin(),polygon.end());for(unsigned i=1;i+1<polygon.size();++i)lowerFaces.push_back({polygon[0],polygon[i],polygon[i+1]});}
    proof.lowerFaces=static_cast<unsigned>(lowerFaces.size());if(footFaces.empty()||lowerFaces.empty()||support.size()<3)return proof;
    for(const auto edge:rim){const auto a=TransformPoint(matrix,D::Vertex(finalDerivedTrunk,edge.a)),b=TransformPoint(matrix,D::Vertex(finalDerivedTrunk,edge.b));
        S::Need(D::Finite(a)&&D::Finite(b),"ROOT_SUPPORT_WORLD_NONFINITE");const auto pa=D::Exact::ToExact(a),pb=D::Exact::ToExact(b);rimSegments.push_back({pa,pb,pb});}
    // 完整足帶和完整最終下幹的凸包地表支撐先行；閉集交域不以 gap 篩選。
    stage="support-hull";const auto hull=D::Hull(std::move(support));const auto root=D::R::Float(pose.position.y);D::Range groundHeights;
    for(unsigned i=1;i+1<hull.size();++i){stage="footprint-expected fan="+std::to_string(i);const D::T footprint{hull[0],hull[i],hull[i+1]};const auto expected=D::Abs(D::Area(footprint[0],footprint[1],footprint[2]));D::R covered;
        for(const auto face:actualSurface.Query(footprint)){const auto ground=actualSurface.Face(face);
            stage="footprint-intersection fan="+std::to_string(i)+" ground="+std::to_string(face);
            const auto polygon=D::PlanarIntersection(footprint,ground);if(polygon.empty())continue;
            stage="footprint-height fan="+std::to_string(i)+" ground="+std::to_string(face);
            for(const auto& p:polygon){const auto height=D::Height(ground,p);if(Compare(D::Abs(height-root),D::R::Float(.10f))>0)return proof;D::Record(groundHeights,height);}
            stage="footprint-area fan="+std::to_string(i)+" ground="+std::to_string(face);
            bool hasArea{};for(unsigned j=1;j+1<polygon.size();++j)hasArea|=D::Area(polygon[0],polygon[j],polygon[j+1]).Sign()!=0;
            if(hasArea){const auto domain=D::Hull(polygon);D::R area;for(unsigned j=1;j+1<domain.size();++j)area=area+D::Abs(D::Area(domain[0],domain[j],domain[j+1]));
                if(area.Sign()){if(actualSurface.NormalY(face)<.97f){proof.result=RootSupportResult::Slope;return proof;}covered=covered+area;}}}
        stage="footprint-coverage fan="+std::to_string(i);if(Compare(expected,covered)!=0)return proof;}
    if(!groundHeights.points)return proof;const auto penetration=D::R::Float(-.04f),floating=D::R::Float(.04f);D::Range low,foot;
    stage="lower-overlay";for(const auto& face:lowerFaces){if(optimize&&low.points){auto minY=face[0][1];for(unsigned i=1;i<3;++i)if(Compare(face[i][1],minY)<0)minY=face[i][1];
            if(Compare(minY-groundHeights.maximum,low.minimum)>=0){++proof.skippedLowerFaces;continue;}}
        const auto range=D::Overlay(face,actualSurface);if(!range.points)return proof;proof.evaluatedIntersections+=range.points;
        if(Compare(range.minimum,penetration)<0)return proof;D::Record(low,range.minimum);}
    // 原來源足帶全三角面以及原底輪廓全線段皆不剪枝、不刪空面。
    stage="foot-and-rim-overlay";for(const auto* collection:{&footFaces,&rimSegments})for(const auto& face:*collection){const auto range=D::Overlay(face,actualSurface);if(!range.points)return proof;
        proof.evaluatedIntersections+=range.points;if(Compare(range.minimum,penetration)<0||Compare(range.maximum,floating)>0)return proof;
        D::Record(foot,range.maximum);}
    if(!low.points||!foot.points)return proof;proof.minimumLowerGap=static_cast<float>(low.minimum.Approximate());proof.maximumFootGap=static_cast<float>(foot.maximum.Approximate());
    proof.result=RootSupportResult::Safe;return proof;
    }catch(const std::runtime_error& error){throw std::runtime_error("ROOT_SUPPORT_STAGE "+stage+": "+error.what());}
}
}
