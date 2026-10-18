#pragma once
#include "MistwatchExplorer.h"
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <vector>

namespace Mistwatch::ExplorerArtDiagnostic {
using Aevrynd::Vec3;using Edge=std::array<std::uint32_t,2>;
struct DirectedUse {std::size_t face{};std::uint32_t from{},to{};};
inline Vec3 Position(const Aevrynd::Vertex3D& v){return {v.x,v.y,v.z};}
inline Vec3 PackedNormal(const Aevrynd::Vertex3D& v){const auto n=Aevrynd::UnpackNormalSnorm8(v.normal);return {n.x,n.y,n.z};}
inline Vec3 Unit(Vec3 v){const float n=std::sqrt(Aevrynd::Dot(v,v));return n>0?v*(1.f/n):Vec3{};}
inline Edge Key(std::uint32_t a,std::uint32_t b){return {(std::min)(a,b),(std::max)(a,b)};}
inline void Vector(const char* name,Vec3 p){std::cout<<' '<<name<<"=("<<p.x<<','<<p.y<<','<<p.z<<')';}
inline const char* MaterialName(ExplorerArtMaterial value){switch(value){
    case ExplorerArtMaterial::Fabric:return "Fabric";case ExplorerArtMaterial::Leather:return "Leather";
    case ExplorerArtMaterial::Skin:return "Skin";case ExplorerArtMaterial::Hair:return "Hair";
    case ExplorerArtMaterial::Metal:return "Metal";default:return "Unknown";}}
inline ExplorerArtMaterial FaceMaterial(const Explorer& explorer,std::size_t face){
    const auto offset=face*3u;for(const auto& r:explorer.art.ranges)
        if(offset>=r.firstIndex&&offset-r.firstIndex<r.indexCount)return r.material;
    return ExplorerArtMaterial::Count;
}
inline void PrintNegativeFaces(const Explorer& explorer) {
    using namespace Aevrynd;
    const auto& model=explorer.model;const auto& mesh=model.bindMesh;
    const std::size_t count=mesh.indices.size()/3u;
    std::vector<Vec3> areas(count),units(count),unitSums(mesh.vertices.size()),areaSums(mesh.vertices.size());
    std::vector<std::vector<std::size_t>> vertexFaces(mesh.vertices.size());
    std::map<Edge,std::vector<DirectedUse>> edgeUses;
    for(std::size_t face=0;face<count;++face){const auto a=mesh.indices[face*3u],b=mesh.indices[face*3u+1u],c=mesh.indices[face*3u+2u];
        areas[face]=Cross(Position(mesh.vertices[b])-Position(mesh.vertices[a]),Position(mesh.vertices[c])-Position(mesh.vertices[a]));
        units[face]=Unit(areas[face]);for(const auto v:{a,b,c}){unitSums[v]=unitSums[v]+units[face];areaSums[v]=areaSums[v]+areas[face];vertexFaces[v].push_back(face);}
        for(const auto edge:std::array<Edge,3>{Edge{a,b},Edge{b,c},Edge{c,a}})edgeUses[Key(edge[0],edge[1])].push_back({face,edge[0],edge[1]});
    }
    const auto oldPrecision=std::cout.precision();std::cout<<std::setprecision(std::numeric_limits<float>::max_digits10);
    unsigned negatives{},windingMismatches{},idealNegativeFaces{},quantizationCrossings{};
    for(std::size_t face=0;face<count;++face){const auto a=mesh.indices[face*3u],b=mesh.indices[face*3u+1u],c=mesh.indices[face*3u+2u];
        const Vec3 packedSum=PackedNormal(mesh.vertices[a])+PackedNormal(mesh.vertices[b])+PackedNormal(mesh.vertices[c]);
        const float packedDot=Dot(areas[face],packedSum);if(packedDot>0)continue;
        ++negatives;const Vec3 idealSum=Unit(unitSums[a])+Unit(unitSums[b])+Unit(unitSums[c]);
        const Vec3 areaSum=Unit(areaSums[a])+Unit(areaSums[b])+Unit(areaSums[c]);
        const float idealDot=Dot(areas[face],idealSum),areaDot=Dot(areas[face],areaSum);
        idealNegativeFaces+=idealDot<=0;quantizationCrossings+=idealDot>0;
        std::cout<<"NEGATIVE_FACE id="<<face<<" firstIndex="<<face*3u<<" vertices="<<a<<','<<b<<','<<c
            <<" material="<<MaterialName(FaceMaterial(explorer,face))<<" packed_dot="<<packedDot<<" ideal_unit_average_dot="<<idealDot
            <<" area_average_dot="<<areaDot<<" packed_cosine="<<Dot(units[face],Unit(packedSum));
        Vector("area",areas[face]);Vector("face_unit",units[face]);Vector("packed_average",packedSum);Vector("ideal_average",idealSum);std::cout<<'\n';
        std::set<std::size_t> adjacent;
        for(const auto v:{a,b,c}){
            const auto& influence=model.influences[v];std::cout<<"NEGATIVE_VERTEX face="<<face<<" vertex="<<v;
            Vector("position",Position(mesh.vertices[v]));Vector("packed_normal",PackedNormal(mesh.vertices[v]));
            Vector("ideal_normal",Unit(unitSums[v]));Vector("area_normal",Unit(areaSums[v]));
            std::cout<<" own_face_dot="<<Dot(units[face],PackedNormal(mesh.vertices[v]))<<" ideal_own_face_dot="<<Dot(units[face],Unit(unitSums[v]))
                <<" unit_sum_length="<<Length(unitSums[v])<<" neighbor_count="<<vertexFaces[v].size()<<" influences=";
            for(unsigned slot=0;slot<4;++slot)std::cout<<(slot?",":"")<<influence.joints[slot]<<':'<<influence.weights[slot];
            std::cout<<" ring_joints=";bool first=true;
            for(const auto& join:explorer.art.joins)if(std::find(join.boundary.begin(),join.boundary.end(),v)!=join.boundary.end()){
                std::cout<<(first?"":",")<<join.joint;first=false;}
            if(first)std::cout<<"none";std::cout<<'\n';
            adjacent.insert(vertexFaces[v].begin(),vertexFaces[v].end());
        }
        unsigned faceWindingMismatches{};
        for(const auto edge:std::array<Edge,3>{Edge{a,b},Edge{b,c},Edge{c,a}}){const auto& uses=edgeUses.at(Key(edge[0],edge[1]));
            unsigned forward{},reverse{};for(const auto use:uses){forward+=use.from<use.to;reverse+=use.from>use.to;}
            const bool mismatch=uses.size()==2&&(forward!=1||reverse!=1);faceWindingMismatches+=mismatch;
            std::cout<<"NEGATIVE_EDGE face="<<face<<" edge="<<edge[0]<<','<<edge[1]<<" uses="<<uses.size()<<" forward="<<forward<<" reverse="<<reverse;
            for(const auto use:uses)std::cout<<" incident="<<use.face<<':'<<use.from<<"->"<<use.to;std::cout<<'\n';
        }
        windingMismatches+=faceWindingMismatches;
        for(const auto neighbor:adjacent){const auto na=mesh.indices[neighbor*3u],nb=mesh.indices[neighbor*3u+1u],nc=mesh.indices[neighbor*3u+2u];
            const Vec3 normalSum=PackedNormal(mesh.vertices[na])+PackedNormal(mesh.vertices[nb])+PackedNormal(mesh.vertices[nc]);
            std::cout<<"NEGATIVE_NEIGHBOR target="<<face<<" neighbor="<<neighbor<<" vertices="<<na<<','<<nb<<','<<nc
                <<" material="<<MaterialName(FaceMaterial(explorer,neighbor))<<" face_dot_target="<<Dot(units[face],units[neighbor])
                <<" neighbor_packed_dot="<<Dot(areas[neighbor],normalSum);Vector("area",areas[neighbor]);Vector("face_unit",units[neighbor]);std::cout<<'\n';
        }
        std::cout<<"NEGATIVE_CLASSIFICATION face="<<face<<" shared_edge_winding_mismatches="<<faceWindingMismatches
            <<" ideal_average_nonpositive="<<(idealDot<=0)<<" snorm8_sign_crossing="<<(idealDot>0)<<" geometry_changed=0\n";
    }
    // 在原normal gate失敗前列出所有joint邊，不繞過原fixture的任何Require。
    unsigned joinBadEdges{};
    for(const auto& join:explorer.art.joins)for(std::size_t i=0;i<join.boundary.size();++i){const auto a=join.boundary[i],b=join.boundary[(i+1u)%join.boundary.size()];
        const auto found=edgeUses.find(Key(a,b));unsigned forward{},reverse{},uses{};
        if(found!=edgeUses.end()){uses=static_cast<unsigned>(found->second.size());for(const auto use:found->second){forward+=use.from<use.to;reverse+=use.from>use.to;}}
        if(uses!=2||forward!=1||reverse!=1){++joinBadEdges;std::cout<<"DIAGNOSTIC_JOIN_EDGE joint="<<join.joint<<" edge="<<a<<','<<b<<" uses="<<uses<<" forward="<<forward<<" reverse="<<reverse<<'\n';}
    }
    std::cout<<"NORMAL_DIAGNOSTIC_SUMMARY negative_faces="<<negatives<<" shared_edge_winding_mismatches="<<windingMismatches
        <<" ideal_average_nonpositive_faces="<<idealNegativeFaces<<" snorm8_sign_crossings="<<quantizationCrossings
        <<" bad_joint_edges="<<joinBadEdges<<" all_original_gates_preserved=1\n";
    std::cout.precision(oldPrecision);
}
}
