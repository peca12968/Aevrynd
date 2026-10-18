#define Mistwatch MistwatchV282Reference
#include "MistwatchExplorerArtMeshV282Reference.h"
#undef Mistwatch
#include "MistwatchExplorer.h"
#include "MistwatchExplorerFaceMesh.h"
#include <bit>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

namespace {
using namespace Aevrynd;using Edge=std::array<std::uint32_t,2>;
unsigned checks{};
void Require(bool value,const std::string& name){++checks;if(!value)throw std::runtime_error(name);}
bool SameFloat(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool SameVertex(const Vertex3D& a,const Vertex3D& b){return SameFloat(a.x,b.x)&&SameFloat(a.y,b.y)&&SameFloat(a.z,b.z)&&
    a.normal==b.normal&&a.color==b.color&&SameFloat(a.u,b.u)&&SameFloat(a.v,b.v);}
Vec3 Position(const Vertex3D& v){return {v.x,v.y,v.z};}
Vec3 Normal(const Vertex3D& v){const auto n=UnpackNormalSnorm8(v.normal);return {n.x,n.y,n.z};}
struct EdgeUse {unsigned count{},forward{},reverse{};};
std::map<Edge,EdgeUse> Edges(const Mesh3D& mesh){std::map<Edge,EdgeUse> result;
    for(std::size_t i=0;i<mesh.indices.size();i+=3){const auto a=mesh.indices[i],b=mesh.indices[i+1u],c=mesh.indices[i+2u];
        for(auto edge:std::array<Edge,3>{Edge{a,b},Edge{b,c},Edge{c,a}}){const bool forward=edge[0]<edge[1];if(!forward)std::swap(edge[0],edge[1]);
            auto& e=result[edge];++e.count;if(forward)++e.forward;else ++e.reverse;}}return result;}
void SharedRing(const Mistwatch::ExplorerArtAuthor::Ring& ring,const std::map<Edge,EdgeUse>& edges,const char* name){
    Require(ring.size()==Mistwatch::ExplorerArtAuthor::FaceAuthor::EyeSides,std::string(name)+" ring size");
    for(std::size_t i=0;i<ring.size();++i){Edge edge{ring[i],ring[(i+1u)%ring.size()]};if(edge[0]>edge[1])std::swap(edge[0],edge[1]);
        const auto found=edges.find(edge);Require(found!=edges.end()&&found->second.count==2&&found->second.forward==1&&found->second.reverse==1,std::string(name)+" shared winding");}}
void Geometry(const SkinnedModel3D& model,const Mistwatch::ExplorerArtAuthor::ExplorerFaceAudit& audit){
    using namespace Mistwatch::ExplorerArtAuthor;
    Require(model.Valid(),"face-only existing model.Valid");const auto& mesh=model.bindMesh;
    Require(mesh.vertices.size()<3500&&mesh.indices.size()/3u<6200,"finite authored face budget");
    Require(audit.mainSurface.size()>1800&&audit.mainSurface.size()<2100,"continuous authored head surface inventory");
    for(const auto& influence:model.influences)Require(influence.joints[0]==2&&influence.weights[0]==65535&&
        influence.weights[1]==0&&influence.weights[2]==0&&influence.weights[3]==0,"head joint2 rigid contract");
    const auto edges=Edges(mesh);
    for(unsigned side=0;side<2;++side){SharedRing(audit.socketBoundary[side],edges,"socket");SharedRing(audit.apertureBoundary[side],edges,"lens lid");
        Require(audit.eyeLens[side].size()==1u+5u*FaceAuthor::EyeSides&&audit.eyelids[side].size()==3u*FaceAuthor::EyeSides,"curved eye / lid inventory");
        float minimum=100,maximum=-100;for(const auto i:audit.eyeLens[side]){minimum=(std::min)(minimum,mesh.vertices[i].z);maximum=(std::max)(maximum,mesh.vertices[i].z);}
        Require(maximum-minimum>.003f,"eye lens is a flat patch");
        const auto centre=audit.eyeLens[side].front();for(const auto i:audit.apertureBoundary[side])Require(mesh.vertices[centre].z>mesh.vertices[i].z,"iris front curvature absent");
        const auto first=audit.apertureBoundary[side][0],middle=audit.apertureBoundary[side][FaceAuthor::EyeSides/4u];
        Require(std::abs(mesh.vertices[first].y-mesh.vertices[middle].y)>.002f,"eyelid contour is straight");
        Require(audit.earFront[side].size()==121,"helix / concha inventory");
        const Vec3 centreEar{side?.0875f:-.0875f,1.651f,-.011f},outward{side?.72f:-.72f,0,.69f};
        float low=100,high=-100;for(const auto i:audit.earFront[side]){const float depth=Dot(Position(mesh.vertices[i])-centreEar,outward);low=(std::min)(low,depth);high=(std::max)(high,depth);}
        Require(high-low>.005f,"ear concha and helix have no depth");
    }
    Require(audit.eyebrow.size()==126,"curved brow strips inventory");
    float minNormal=100;unsigned bad{};
    for(std::size_t i=0;i<mesh.indices.size();i+=3){const auto a=mesh.indices[i],b=mesh.indices[i+1u],c=mesh.indices[i+2u];
        const auto area=Cross(Position(mesh.vertices[b])-Position(mesh.vertices[a]),Position(mesh.vertices[c])-Position(mesh.vertices[a]));
        Require(Dot(area,area)>1.e-16f,"degenerate face feature triangle");
        const float direction=Dot(area,Normal(mesh.vertices[a])+Normal(mesh.vertices[b])+Normal(mesh.vertices[c]));minNormal=(std::min)(minNormal,direction);
        if(direction<=0){++bad;std::cout<<"FACE_NORMAL_DIVERGENCE face="<<i/3u<<" vertices="<<a<<','<<b<<','<<c<<" dot="<<direction<<'\n';}}
    Require(bad==0,"face-only positive normal gate");
    // 依實際main grid索引取鼻尖／鼻翼／唇，不只測位移函式的返回值。
    const auto nearest=[&](float x,float y){std::uint32_t result=audit.mainSurface.front();float best=100;
        for(const auto i:audit.mainSurface){const auto& v=mesh.vertices[i];if(v.z<0)continue;const float score=(v.x-x)*(v.x-x)+(v.y-y)*(v.y-y);
            if(score<best){best=score;result=i;}}return result;};
    const auto tip=nearest(0,1.625f),bridge=nearest(0,1.654f),wing=nearest(.0165f,1.621f),cheek=nearest(.04f,1.625f);
    Require(mesh.vertices[tip].z>mesh.vertices[bridge].z+.004f&&mesh.vertices[wing].z>mesh.vertices[cheek].z+.008f,"continuous nose tip / wing relief");
    const auto upper=nearest(0,1.595f),lower=nearest(0,1.589f),corner=nearest(.025f,1.589f);
    Require(mesh.vertices[upper].z>mesh.vertices[corner].z+.002f&&mesh.vertices[lower].z>mesh.vertices[corner].z+.002f,"lip relief absent");
    Require(mesh.vertices[upper].color!=mesh.vertices[corner].color,"lip volume has no smooth color transition");
    const auto bounds=mesh.Bounds();Require(bounds.center.y-bounds.extents.y>=1.48f&&bounds.center.y+bounds.extents.y<1.82f&&bounds.extents.x<.105f,"head size / cap envelope");
    std::cout<<"face_geometry vertices="<<mesh.vertices.size()<<" triangles="<<mesh.indices.size()/3u<<" min_normal_dot="<<minNormal<<" shared_socket_and_aperture=1\n";
}
struct BodyMap {std::vector<std::uint32_t> sourceIndices,compact;};
BodyMap WithoutHead(const SkinnedModel3D& model,const std::vector<std::uint32_t>& head){const auto absent=(std::numeric_limits<std::uint32_t>::max)();
    std::vector<bool> excluded(model.bindMesh.vertices.size());for(auto i:head)excluded.at(i)=true;BodyMap result;result.compact.resize(excluded.size(),absent);
    for(std::uint32_t i=0;i<excluded.size();++i)if(!excluded[i]){result.compact[i]=static_cast<std::uint32_t>(result.sourceIndices.size());result.sourceIndices.push_back(i);}return result;}
void BodyUnchanged(const Mistwatch::Explorer& current,SkinnedModel3D& reference,BodyMap& now,BodyMap& old){
    reference.skeleton=current.model.skeleton;reference.inverseBindMatrices=current.model.inverseBindMatrices;reference.skeletonCompatibilityId=current.model.skeletonCompatibilityId;
    const auto receipt=MistwatchV282Reference::BuildExplorerArtMesh(reference);
    now=WithoutHead(current.model,current.art.criticalVertices[2]);old=WithoutHead(reference,receipt.criticalVertices[2]);
    Require(now.sourceIndices.size()==old.sourceIndices.size(),"nonhead vertex count changed");
    for(std::size_t i=0;i<now.sourceIndices.size();++i){const auto a=now.sourceIndices[i],b=old.sourceIndices[i];
        Require(SameVertex(current.model.bindMesh.vertices[a],reference.bindMesh.vertices[b]),"body bind vertex/normal/UV/color changed at compact "+std::to_string(i));
        Require(current.model.influences[a].joints==reference.influences[b].joints&&current.model.influences[a].weights==reference.influences[b].weights,"body influences changed");}
    const auto indices=[&](const SkinnedModel3D& model,const BodyMap& map){std::vector<std::uint32_t> out;const auto absent=(std::numeric_limits<std::uint32_t>::max)();
        for(std::size_t i=0;i<model.bindMesh.indices.size();i+=3){const auto a=map.compact[model.bindMesh.indices[i]],b=map.compact[model.bindMesh.indices[i+1u]],c=map.compact[model.bindMesh.indices[i+2u]];
            if(a!=absent&&b!=absent&&c!=absent)out.insert(out.end(),{a,b,c});else Require(a==absent&&b==absent&&c==absent,"new triangle crosses body/head scope");}return out;};
    const auto actual=indices(current.model,now),expected=indices(reference,old);Require(actual==expected,"nonhead triangle indices changed");
    Require(current.art.joins.size()==receipt.joins.size()&&current.art.joins.size()==14,"original14 joins changed");
    Require(current.art.ranges.size()==5&&receipt.ranges.size()==5,"original5 material groups changed");
    std::cout<<"body_exact nonhead_vertices="<<now.sourceIndices.size()<<" nonhead_triangles="<<actual.size()/3u<<" raw_fields_and_remapped_indices_equal=1\n";
}
void Poses(Mistwatch::Explorer& explorer,const SkinnedModel3D& face,const SkinnedModel3D& reference,const BodyMap& now,const BodyMap& old){
    SkinningWorkspace3D faceWork,nowWork,oldWork;Mesh3D facePosed,nowPosed,oldPosed;
    for(unsigned action=0;action<3;++action){explorer.Update(action==1?Vec3{0,0,.12f}:Vec3{},1.f/60.f,action==2);
        Require(SkinModelInto(face,explorer.pose,faceWork,facePosed),"face pose skin rejected");
        Require(SkinModelInto(explorer.model,explorer.pose,nowWork,nowPosed)&&SkinModelInto(reference,explorer.pose,oldWork,oldPosed),"body reference pose rejected");
        for(const auto& v:facePosed.vertices)Require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&Length(Normal(v))>.97f,"face stand/walk/crouch finite normal");
        for(std::size_t i=0;i<now.sourceIndices.size();++i)Require(SameVertex(nowPosed.vertices[now.sourceIndices[i]],oldPosed.vertices[old.sourceIndices[i]]),"body posed fields changed");
        for(std::size_t i=0;i<facePosed.indices.size();i+=3){const auto a=facePosed.indices[i],b=facePosed.indices[i+1u],c=facePosed.indices[i+2u];
            const auto area=Cross(Position(facePosed.vertices[b])-Position(facePosed.vertices[a]),Position(facePosed.vertices[c])-Position(facePosed.vertices[a]));
            Require(Dot(area,Normal(facePosed.vertices[a])+Normal(facePosed.vertices[b])+Normal(facePosed.vertices[c]))>0,"posed face normal orientation");}
    }
}
}
int main(){try{Mistwatch::Explorer explorer;SkinnedModel3D face;face.skeleton=explorer.model.skeleton;face.inverseBindMatrices=explorer.model.inverseBindMatrices;face.skeletonCompatibilityId=explorer.model.skeletonCompatibilityId;
    Mistwatch::ExplorerArtAuthor::Builder builder(face);Mistwatch::ExplorerArtAuthor::ExplorerFaceAudit audit;Mistwatch::ExplorerArtAuthor::BuildExplorerFace(builder,&audit);const auto receipt=builder.Finish();
    Require(receipt.ranges.size()==5,"face author range count");Geometry(face,audit);SkinnedModel3D reference;BodyMap now,old;BodyUnchanged(explorer,reference,now,old);Poses(explorer,face,reference,now,old);
    Require(explorer.model.bindMesh.vertices.size()<10000&&explorer.model.bindMesh.indices.size()/3u<12000,"original global vertex/triangle budget");
    std::cout<<"ExplorerFaceArtCpuTest PASS checks="<<checks<<" native_image_verified=0 realism_claimed=0\n";return 0;}
    catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<" checks="<<checks<<'\n';return 1;}}
