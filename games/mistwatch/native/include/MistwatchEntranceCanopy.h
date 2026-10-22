#pragma once
#include "MistwatchCanopyCollision.h"
#include "MistwatchRootStemTerrain.h"
#include <functional>
#include <string>

namespace Mistwatch {
struct EntranceCanopyAnchor {float x,z,yaw,scale;};
// 僅四個作者錨點：兩側各兩株、深度錯開；不掃描、重試或縮小碰撞來湊數。
inline constexpr std::array<EntranceCanopyAnchor,4> EntranceCanopyAnchors{{
    {-13.f,2.f,.3f,1.04f},{-21.f,-5.f,2.2f,1.10f},
    {11.f,9.f,5.08f,1.05f},{17.f,24.f,.9f,1.07f}}};
inline constexpr std::size_t EntranceCanopyOriginalCount=4,EntranceCanopyTotalCount=8;
struct EntranceCanopyMetadata {
    std::uint64_t originalFingerprint{},faceChecks{},exactOverlayFaces{},woodParts{};
    unsigned appended{};
};
// 僅供呼叫端以完整真實輸入與固定內容雜湊驗證的烘焙資料；所有幾何／接地／蕨體檢查仍執行。
using EntranceCanopyQualifiedWood=std::function<std::vector<CanopyWoodProxy>(const CanopyPopulation&)>;
namespace EntranceCanopyDetail {
inline void Need(bool okay,std::size_t index,const char* gate) {
    if(!okay)throw std::runtime_error("ENTRANCE_CANOPY_FIRST_DIVERGENCE anchor="+
        std::to_string(index)+" gate="+gate);
}
inline bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
// 真三角面接地：高於整個所涵蓋地形最高格點者有解析界限；其餘做完整地形面交集。
// 這不是只驗三個頂點／幾個重心點；地形每格的真線性三角面最高點必在格點。
inline void CheckGround(const Mesh3D& mesh,const Transform3D& pose,const HeightField3D& terrain,
    float lowerGate,std::size_t anchor,EntranceCanopyMetadata& metadata,bool derivedStem=false) {
    Need(mesh.Valid(),anchor,"invalid-final-mesh");
    const auto matrix=pose.Matrix();
    std::vector<Vec3> world;world.reserve(mesh.vertices.size());
    Vec3 low{10000,10000,10000},high{-10000,-10000,-10000};
    for(const auto& vertex:mesh.vertices){const auto p=TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});
        Need(Session::Finite(p)&&terrain.Contains(p.x,p.z),anchor,"final-mesh-outside-terrain");
        world.push_back(p);Expand(low,high,p);
    }
    const auto& grid=terrain.Settings();
    const auto cell=[&](float v,float origin,unsigned limit){return std::clamp(
        int(std::floor((v-origin)/grid.cellSize)),0,int(limit)-2);};
    float highest=-(std::numeric_limits<float>::infinity)();
    for(int z=cell(low.z,grid.origin.z,grid.depth);z<=cell(high.z,grid.origin.z,grid.depth)+1;++z)
    for(int x=cell(low.x,grid.origin.x,grid.width);x<=cell(high.x,grid.origin.x,grid.width)+1;++x)
        highest=(std::max)(highest,terrain.Height(unsigned(x),unsigned(z)));
    Need(std::isfinite(highest),anchor,"nonfinite-terrain");
    for(std::size_t face=0;face<mesh.indices.size();face+=3){
        const std::array<Vec3,3> points{{world[mesh.indices[face]],world[mesh.indices[face+1]],world[mesh.indices[face+2]]}};
        ++metadata.faceChecks;
        const float minimum=(std::min)({points[0].y,points[1].y,points[2].y});
        const bool cap=derivedStem&&face/3>=5104u+152u;
        if(!cap&&minimum-highest>=lowerGate)continue;
        const auto overlay=RootGroundOverlay(points,terrain);++metadata.exactOverlayFaces;
        Need(overlay.minimum>=lowerGate,anchor,"full-triangle-ground-penetration");
        if(cap){Need(overlay.maximum<=-.005f,anchor,"root-cap-not-buried");
            Need(Cross(points[1]-points[0],points[2]-points[0]).y<0,anchor,"root-cap-not-down-facing");}
    }
}
}
inline CanopyPopulation AppendEntranceCanopy(const CanopyPopulation& original,
    const std::array<Mesh3D,3>& meshes,const CanopyGeometry& geometry,const Mesh3D& derived,
    const HeightField3D& terrain,std::span<const BoxCollider3D> solids,const ForestUnderstory& understory,
    const std::array<FernColonyAsset,4>& ferns,EntranceCanopyMetadata& metadata,const EntryLayout& layout=ActiveEntryLayout,
    const EntranceCanopyQualifiedWood& qualifiedWood={}) {
    using EntranceCanopyDetail::Need;
    Need(original.reachedBudget&&original.plants.size()==4&&
        original.fingerprint==6584434718760703877ull,0,"original-four-forestwood-identity");
    Need(derived.Valid()&&derived.vertices.size()==3608&&derived.indices.size()==5330u*3u,0,"derived-root-stem-identity");
    Need(solids.size()<=10000&&understory.ferns.size()<=10000,0,"bounded-input");
    const auto measured=MeasureCanopy(meshes);
    const auto same=EntranceCanopyDetail::Same;
    Need(measured.triangles==geometry.triangles&&measured.radius==geometry.radius&&
        measured.rootRadius==geometry.rootRadius&&measured.rootMinimum==geometry.rootMinimum&&
        measured.crossingEdges==geometry.crossingEdges&&measured.lowerFaces==geometry.lowerFaces&&
        same(measured.minimum,geometry.minimum)&&same(measured.maximum,geometry.maximum)&&
        same(measured.lowerMin,geometry.lowerMin)&&same(measured.lowerMax,geometry.lowerMax),0,"stale-geometry");
    for(const auto& box:solids)Need(Session::Finite(box.center)&&Session::Finite(box.extents)&&
        box.extents.x>=0&&box.extents.y>=0&&box.extents.z>=0,0,"invalid-existing-solid");
    for(const auto& f:ferns)Need(std::isfinite(f.radius)&&f.radius>0&&f.radius<=4&&
        std::isfinite(f.minimumY)&&std::isfinite(f.height)&&f.minimumY<f.height,0,"invalid-existing-fern-volume");
    for(const auto& f:understory.ferns)Need(Session::Finite(f.position)&&Session::Finite(f.rotation)&&
        Session::Finite(f.scale)&&f.scale.x>0&&f.scale.x==f.scale.y&&f.scale.x==f.scale.z&&
        f.rotation.x==0&&f.rotation.z==0&&terrain.Contains(f.position.x,f.position.z),0,"invalid-existing-fern-pose");
    std::uint64_t originalIdentity=14695981039346656037ull;
    for(const auto& p:original.plants){
        Need(Session::Finite(p.pose.position)&&Session::Finite(p.pose.rotation)&&Session::Finite(p.pose.scale)&&
            p.pose.rotation.x==0&&p.pose.rotation.z==0&&p.pose.scale.x>=1.f&&p.pose.scale.x<=1.15f&&
            p.pose.scale.x==p.pose.scale.y&&p.pose.scale.x==p.pose.scale.z&&
            p.crownRadius==geometry.radius*p.pose.scale.x&&p.rootRadius==geometry.rootRadius*p.pose.scale.x,
            0,"invalid-original-four-pose");
        for(float value:{p.pose.position.x,p.pose.position.y,p.pose.position.z,p.pose.rotation.y,p.pose.scale.x,
            p.crownRadius,p.rootRadius}){originalIdentity^=std::bit_cast<std::uint32_t>(value);originalIdentity*=1099511628211ull;}
    }
    Need(originalIdentity==original.fingerprint,0,"original-four-pose-hash-mismatch");
    auto result=original;EntranceCanopyMetadata checked;checked.originalFingerprint=original.fingerprint;
    const auto checkWood=[&](const CanopyWoodProxy& proxy,std::size_t index){
        Need(!proxy.parts.empty()&&proxy.spacing==.15&&CanopyVolume::ValidWoodParts(proxy.parts),index,"full-wood-grid-proxy");
        for(std::size_t i=0;i<understory.ferns.size();++i){const auto& f=ferns[i%4];
            Need(CanopyVolume::ClearsCanopyWood(understory.ferns[i],
                CanopyVolume::ConservativeLocalVolume(f.radius,f.minimumY,f.height),proxy.parts),
                index,"retained-fern-full-volume-vs-new-wood");}
        checked.woodParts+=proxy.parts.size();
    };
    ForestCoverSettings settings;
    for(std::size_t index=0;index<EntranceCanopyAnchors.size();++index){
        const auto anchor=EntranceCanopyAnchors[index];
        Need(std::isfinite(anchor.x)&&std::isfinite(anchor.z)&&std::isfinite(anchor.yaw)&&
            std::isfinite(anchor.scale)&&anchor.scale>=1.f&&anchor.scale<=1.15f,index,"invalid-authored-anchor");
        const Vec3 root{anchor.x,terrain.SampleHeight(anchor.x,anchor.z),anchor.z};
        const float radius=geometry.radius*anchor.scale,baseRadius=geometry.rootRadius*anchor.scale;
        Need(ForestCoverClearance(root,radius,solids,layout)&&terrain.Contains(root.x-radius,root.z-radius)&&
            terrain.Contains(root.x+radius,root.z+radius),index,"original-crown-world-clearance");
        Need(ForestCoverFootprint(terrain,root,baseRadius,settings),index,"original-root-footprint");
        for(std::size_t i=0;i<understory.ferns.size();++i){const auto& f=understory.ferns[i];
            Need(std::hypot(root.x-f.position.x,root.z-f.position.z)>=
                baseRadius+ferns[i%4].radius*f.scale.x+.03f,index,"original-fern-root-spacing");}
        for(const auto& plant:result.plants)Need(std::hypot(root.x-plant.pose.position.x,root.z-plant.pose.position.z)>=
            radius+plant.crownRadius+.5f,index,"original-full-crown-spacing");
        const Transform3D pose{{root.x,root.y-geometry.rootMinimum*anchor.scale,root.z},
            {0,anchor.yaw,0},{anchor.scale,anchor.scale,anchor.scale}};
        auto rooted=RootCanopyToTerrain(meshes[0],geometry,pose,terrain);
        Vec3 minimum=geometry.minimum,maximum=geometry.maximum;
        for(const auto& vertex:rooted.vertices)Expand(minimum,maximum,{vertex.x,vertex.y,vertex.z});
        const auto centre=(minimum+maximum)*.5f,extents=(maximum-minimum)*.5f;
        const MeshBounds3D local{centre,Length(extents),extents};const auto bounds=TransformBounds(local,pose);
        BoxCollider3D collider{bounds.center,bounds.extents};
        Need(ForestCoverClearance(collider.center,(std::max)(collider.extents.x,collider.extents.z),solids,layout),
            index,"original-final-envelope-clearance");
        for(const auto& plant:result.plants)Need(
            std::abs(plant.lower.center.x-collider.center.x)>=plant.lower.extents.x+collider.extents.x+.03f||
            std::abs(plant.lower.center.z-collider.center.z)>=plant.lower.extents.z+collider.extents.z+.03f,
            index,"original-final-envelope-spacing");
        // 新株真正短幹用與原四株完全相同的 helper，不能另造便宜的碰撞代理。
        const auto finalStem=PlantDerivedRootStem(derived,geometry,pose,terrain);
        const auto finalBox=ExtendRootCollision(collider,finalStem,pose);
        Need(PropClearsObjectives(finalBox,layout),index,"final-root-objective-clearance");
        EntranceCanopyDetail::CheckGround(finalStem,pose,terrain,-.04f,index,checked,true);
        EntranceCanopyDetail::CheckGround(meshes[1],pose,terrain,.04f,index,checked);
        EntranceCanopyDetail::CheckGround(meshes[2],pose,terrain,-.04f,index,checked);
        // 未烘焙路徑保持原逐株作者／檢查順序。烘焙路徑須先有完整八株真輸入才能驗證身分。
        if(!qualifiedWood)checkWood(CreateCanopyWoodProxy(finalStem,meshes[2],pose),index);
        result.plants.push_back({pose,collider,radius,baseRadius,std::move(rooted)});
        for(float value:{root.x,pose.position.y,root.z,anchor.yaw,anchor.scale,radius,baseRadius}){
            result.fingerprint^=std::bit_cast<std::uint32_t>(value);result.fingerprint*=1099511628211ull;}
        ++result.attempts;++checked.appended;
    }
    if(qualifiedWood){
        const auto proxies=qualifiedWood(result);
        Need(proxies.size()==EntranceCanopyAnchors.size(),4,"qualified-wood-group-count");
        for(std::size_t index=0;index<proxies.size();++index)checkWood(proxies[index],index);
    }
    result.groundChecks+=checked.faceChecks;
    result.reachedBudget=result.plants.size()==EntranceCanopyTotalCount;
    Need(result.reachedBudget&&checked.appended==4,4,"finite-authored-cohort-incomplete");
    metadata=checked;return result;
}
}
