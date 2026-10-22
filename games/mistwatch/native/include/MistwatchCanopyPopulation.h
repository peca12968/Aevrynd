#pragma once
#include "MistwatchShrubCanopy.h"
#include <bit>

namespace Mistwatch {
using namespace Aevrynd;
struct CanopyGeometry {
    Vec3 minimum{10000,10000,10000},maximum{-10000,-10000,-10000};
    Vec3 lowerMin{10000,10000,10000},lowerMax{-10000,-10000,-10000};
    float radius{},rootRadius{},rootMinimum{};
    unsigned triangles{},crossingEdges{},lowerFaces{};
};
inline void Expand(Vec3& low,Vec3& high,Vec3 p) {
    low={(std::min)(low.x,p.x),(std::min)(low.y,p.y),(std::min)(low.z,p.z)};
    high={(std::max)(high.x,p.x),(std::max)(high.y,p.y),(std::max)(high.z,p.z)};
}
inline CanopyGeometry MeasureCanopy(const std::array<Mesh3D,3>& meshes) {
    CanopyGeometry result;
    for(const auto& mesh:meshes){
        if(!mesh.Valid()||mesh.vertices.size()>600000||mesh.indices.size()>1800000)
            throw std::invalid_argument("Invalid bounded canopy mesh");
        result.triangles+=unsigned(mesh.indices.size()/3);
        for(const auto& v:mesh.vertices){
            const Vec3 p{v.x,v.y,v.z};if(!Session::Finite(p)||!std::isfinite(v.u)||!std::isfinite(v.v))
                throw std::invalid_argument("Nonfinite canopy mesh");
            Expand(result.minimum,result.maximum,p);result.radius=(std::max)(result.radius,std::hypot(v.x,v.z));
        }
        // 完整三角面截高，包含跨高度邊，不只選低頂點。
        for(std::size_t face=0;face<mesh.indices.size();face+=3){
            std::array<Vec3,3> points;
            for(unsigned i=0;i<3;++i){const auto& v=mesh.vertices[mesh.indices[face+i]];points[i]={v.x,v.y,v.z};}
            bool below=false;
            for(unsigned i=0;i<3;++i){const auto a=points[i],b=points[(i+1)%3];
                if(a.y<=1.85f){Expand(result.lowerMin,result.lowerMax,a);below=true;}
                if((a.y<=1.85f)!=(b.y<=1.85f)){
                    Expand(result.lowerMin,result.lowerMax,a+(b-a)*((1.85f-a.y)/(b.y-a.y)));++result.crossingEdges;below=true;
                }
            }
            result.lowerFaces+=below;
        }
    }
    result.rootMinimum=result.minimum.y;
    for(const auto& v:meshes[0].vertices)if(v.y<=result.rootMinimum+.08f)
        result.rootRadius=(std::max)(result.rootRadius,std::hypot(v.x,v.z));
    if(result.radius<=0||result.radius>4||result.rootRadius<=0||result.rootRadius>1.2f||result.lowerFaces==0)
        throw std::invalid_argument("Canopy authored envelope invalid");
    return result;
}
struct CanopyPlant {Transform3D pose;BoxCollider3D lower;float crownRadius{},rootRadius{};Mesh3D rootedTrunk;};
struct CanopyPopulation {
    std::vector<CanopyPlant> plants;
    unsigned attempts{},clearanceRejected{},rootRejected{},spacingRejected{},meshRejected{};
    std::uint64_t fingerprint{14695981039346656037ull},groundChecks{};
    bool reachedBudget{};
};
inline Mesh3D RootCanopyToTerrain(const Mesh3D& original,const CanopyGeometry& geometry,
    const Transform3D& pose,const HeightField3D& terrain) {
    auto result=original;const auto matrix=pose.Matrix();
    const float central=terrain.SampleHeight(pose.position.x,pose.position.z);
    // 只把原根基0.35m內連續貼坡，上方幹枝與原葉完全不動；不是整棵壓平。
    for(auto& v:result.vertices){
        const float t=std::clamp((v.y-geometry.rootMinimum)/.35f,0.f,1.f);
        const float weight=1.f-t*t*(3.f-2.f*t);
        if(weight<=0)continue;
        const auto world=TransformPoint(matrix,{v.x,v.y,v.z});
        v.y+=(terrain.SampleHeight(world.x,world.z)-central)*weight/pose.scale.x;
    }
    // 修改根面的幾何後重新累積法線；不沿用變形前的法線假稱材質正確。
    std::vector<Vec3> normals(result.vertices.size());
    for(std::size_t face=0;face<result.indices.size();face+=3){
        const auto a=result.indices[face],b=result.indices[face+1],c=result.indices[face+2];
        const auto point=[&](unsigned i){const auto& v=result.vertices[i];return Vec3{v.x,v.y,v.z};};
        const auto n=Cross(point(b)-point(a),point(c)-point(a));
        normals[a]=normals[a]+n;normals[b]=normals[b]+n;normals[c]=normals[c]+n;
    }
    for(std::size_t i=0;i<result.vertices.size();++i)if(original.vertices[i].y-geometry.rootMinimum<.35f&&Length(normals[i])>1.e-8f)
        result.vertices[i].normal=PackNormalSnorm8(Normalize(normals[i]),UnpackNormalSnorm8(original.vertices[i].normal).w);
    return result;
}
inline CanopyPopulation CreateCanopyPopulation(const std::array<Mesh3D,3>& meshes,const CanopyGeometry& geometry,
    const HeightField3D& terrain,std::span<const BoxCollider3D> solids,const ForestUnderstory& understory,
    const ForestCoverResult& grass,const FernColonyResult& colonies,const std::array<FernColonyAsset,4>& ferns,
    const ShrubCanopyResult& shrubs,unsigned target=4,const EntryLayout& layout=ActiveEntryLayout) {
    if(target>8||solids.size()>10000||grass.plants.size()>10000||colonies.plants.size()>1000||shrubs.plants.size()>128)
        throw std::invalid_argument("Invalid canopy bounded input");
    const auto measured=MeasureCanopy(meshes);
    const auto same=[](Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};
    if(measured.triangles!=geometry.triangles||measured.radius!=geometry.radius||measured.rootRadius!=geometry.rootRadius||
        measured.rootMinimum!=geometry.rootMinimum||measured.crossingEdges!=geometry.crossingEdges||
        measured.lowerFaces!=geometry.lowerFaces||!same(measured.minimum,geometry.minimum)||!same(measured.maximum,geometry.maximum)||
        !same(measured.lowerMin,geometry.lowerMin)||!same(measured.lowerMax,geometry.lowerMax))
        throw std::invalid_argument("Stale canopy geometry");
    for(const auto& b:solids)if(!Session::Finite(b.center)||!Session::Finite(b.extents)||b.extents.x<0||b.extents.y<0||b.extents.z<0)
        throw std::invalid_argument("Invalid canopy existing collider");
    const auto validPlant=[&](const Transform3D& p,float radius){
        return Session::Finite(p.position)&&Session::Finite(p.rotation)&&Session::Finite(p.scale)&&
            p.scale.x>0&&p.scale.x==p.scale.y&&p.scale.x==p.scale.z&&p.rotation.x==0&&p.rotation.z==0&&
            std::isfinite(radius)&&radius>0&&radius<=4&&terrain.Contains(p.position.x,p.position.z);
    };
    for(const auto& p:grass.plants)if(!validPlant(p.transform,p.radius))throw std::invalid_argument("Invalid canopy existing grass");
    for(const auto& p:colonies.plants)if(p.variant>=4||!validPlant(p.transform,p.radius))throw std::invalid_argument("Invalid canopy existing colony");
    for(const auto& p:shrubs.plants)if(p.variant>=4||!validPlant(p.pose,p.radius))throw std::invalid_argument("Invalid canopy existing shrub");
    for(std::size_t i=0;i<understory.ferns.size();++i)if(!validPlant(understory.ferns[i],ferns[i%4].radius*understory.ferns[i].scale.x))
        throw std::invalid_argument("Invalid canopy existing fern");
    CanopyPopulation result;std::uint32_t seed=0x43414e50u;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    // V57平面根基拒絕所有落點；本候選改成有真正根部貼坡網格，沿用森林真面坡度/高度差門檻。
    ForestCoverSettings baseSettings;
    for(unsigned attempt=0;attempt<24000&&result.plants.size()<target;++attempt){
        ++result.attempts;const bool prioritizeStart=attempt<16000;
        const float x=(attempt%2?1.f:-1.f)*(7.f+(prioritizeStart?19.f:29.f)*random()),z=(prioritizeStart?-3.f:-8.f)+(prioritizeStart?29.f:94.f)*random();
        const float scale=1.f+.15f*random(),yaw=random()*6.2831853f;
        const Vec3 root{x,terrain.SampleHeight(x,z),z};const float radius=geometry.radius*scale,baseRadius=geometry.rootRadius*scale;
        if(!ForestCoverClearance(root,radius,solids,layout)||!terrain.Contains(x-radius,z-radius)||!terrain.Contains(x+radius,z+radius)){
            ++result.clearanceRejected;continue;
        }
        if(!ForestCoverFootprint(terrain,root,baseRadius,baseSettings)){++result.rootRejected;continue;}
        bool occupied=false;
        // 原低植被保留完整根基間隔；後續新地被仍須避讓完整樹冠碰撞投影。
        for(const auto& g:grass.plants)occupied|=std::hypot(x-g.transform.position.x,z-g.transform.position.z)<baseRadius+g.radius+.03f;
        for(const auto& f:colonies.plants)occupied|=std::hypot(x-f.transform.position.x,z-f.transform.position.z)<baseRadius+f.radius+.03f;
        for(std::size_t i=0;i<understory.ferns.size();++i){const auto& f=understory.ferns[i];
            occupied|=std::hypot(x-f.position.x,z-f.position.z)<baseRadius+ferns[i%4].radius*f.scale.x+.03f;
        }
        for(const auto& p:shrubs.plants)occupied|=std::hypot(x-p.pose.position.x,z-p.pose.position.z)<baseRadius+p.radius+.03f;
        for(const auto& p:result.plants)occupied|=std::hypot(x-p.pose.position.x,z-p.pose.position.z)<radius+p.crownRadius+.5f;
        if(occupied){++result.spacingRejected;continue;}
        const Transform3D pose{{x,root.y-geometry.rootMinimum*scale,z},{0,yaw,0},{scale,scale,scale}};
        auto rooted=RootCanopyToTerrain(meshes[0],geometry,pose,terrain);
        // 旋轉後保守盒含空角，不能用真頂點冠幅冒充最終實體盒的淨空。
        Vec3 minimum=geometry.minimum,maximum=geometry.maximum;
        for(const auto& v:rooted.vertices)Expand(minimum,maximum,{v.x,v.y,v.z});
        const auto centre=(minimum+maximum)*.5f,extents=(maximum-minimum)*.5f;
        const auto local=MeshBounds3D{centre,Length(extents),extents};const auto bounds=TransformBounds(local,pose);
        const BoxCollider3D collider{bounds.center,bounds.extents};
        if(!ForestCoverClearance(collider.center,(std::max)(collider.extents.x,collider.extents.z),solids,layout)){
            ++result.clearanceRejected;continue;
        }
        for(const auto& p:result.plants)occupied|=
            std::abs(p.lower.center.x-collider.center.x)<p.lower.extents.x+collider.extents.x+.03f&&
            std::abs(p.lower.center.z-collider.center.z)<p.lower.extents.z+collider.extents.z+.03f;
        if(occupied){++result.spacingRejected;continue;}
        const auto matrix=pose.Matrix();bool buried=false;
        for(unsigned slot=0;slot<3&&!buried;++slot){const auto& mesh=slot==0?rooted:meshes[slot];
            const auto safe=[&](Vec3 local){const auto p=TransformPoint(matrix,local);++result.groundChecks;
                return terrain.Contains(p.x,p.z)&&p.y-terrain.SampleHeight(p.x,p.z)>=(slot==1?.04f:-.04f);
            };
            for(const auto& v:mesh.vertices)if(!safe({v.x,v.y,v.z})){buried=true;break;}
            if(buried)break;
            for(std::size_t face=0;face<mesh.indices.size();face+=3){
                const auto& a=mesh.vertices[mesh.indices[face]],b=mesh.vertices[mesh.indices[face+1]],c=mesh.vertices[mesh.indices[face+2]];
                for(const auto w:std::array<Vec3,4>{{{1.f/3,1.f/3,1.f/3},{.8f,.1f,.1f},{.1f,.8f,.1f},{.1f,.1f,.8f}}}){
                    if(!safe(Vec3{a.x,a.y,a.z}*w.x+Vec3{b.x,b.y,b.z}*w.y+Vec3{c.x,c.y,c.z}*w.z)){buried=true;break;}
                }
                if(buried)break;
            }
        }
        if(buried){++result.meshRejected;continue;}
        // 完整三槽包絡包含上層枝與跳躍身體，不只截取站姿1.85m。
        result.plants.push_back({pose,collider,radius,baseRadius,std::move(rooted)});
        for(float value:{x,pose.position.y,z,yaw,scale,radius,baseRadius}){result.fingerprint^=std::bit_cast<std::uint32_t>(value);result.fingerprint*=1099511628211ull;}
    }
    result.reachedBudget=result.plants.size()==target;return result;
}
}
