#pragma once
#include "Mesh3D.h"
#include <string>
#include <unordered_set>

namespace Aevrynd {
struct CharacterShapeDelta3D {
    std::string name;
    std::vector<Vec3> offsets;
    std::vector<uint32_t> topology;
    float minimumWeight{-1.f},maximumWeight{1.f};
};
struct CharacterShapeWeight3D {std::string name;float value{};};
struct CharacterShapeResult3D {
    Mesh3D mesh;
    // 此核心重建法線但不輸出切線；呼叫端必須重建切線後才可使用法線貼圖。
    bool requiresTangentRebuild{true};
};

// baseline 必須保留為原始休息網格，不得傳入上次輸出；所有失敗保持 output 不變。
inline bool ApplyCharacterShape3D(const Mesh3D& baseline,
    std::span<const CharacterShapeDelta3D> targets,std::span<const CharacterShapeWeight3D> weights,
    CharacterShapeResult3D& output,std::string* error=nullptr){
    auto fail=[&](const char* text){if(error)*error=text;return false;};
    auto finite=[](Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    if(&baseline==&output.mesh)return fail("baseline_aliases_output");
    if(!baseline.Valid()||baseline.vertices.size()>100000||baseline.indices.size()>300000||targets.size()>128||weights.size()>128)return fail("invalid_mesh_or_limits");
    for(const auto& v:baseline.vertices)if(!finite({v.x,v.y,v.z})||!std::isfinite(v.u)||!std::isfinite(v.v))return fail("nonfinite_baseline");
    std::unordered_set<std::string> names;
    for(const auto& target:targets){
        if(target.name.empty()||target.name.size()>128||!names.insert(target.name).second)return fail("invalid_target_name");
        if(target.offsets.size()!=baseline.vertices.size()||target.topology!=baseline.indices)return fail("wrong_topology");
        if(!std::isfinite(target.minimumWeight)||!std::isfinite(target.maximumWeight)||target.minimumWeight>0||target.maximumWeight<0||target.minimumWeight>target.maximumWeight)return fail("invalid_weight_bounds");
        for(auto delta:target.offsets)if(!finite(delta))return fail("nonfinite_delta");
    }
    names.clear();
    for(const auto& weight:weights){
        const auto target=std::find_if(targets.begin(),targets.end(),[&](const auto& t){return t.name==weight.name;});
        if(!names.insert(weight.name).second||target==targets.end()||!std::isfinite(weight.value)||weight.value<target->minimumWeight||weight.value>target->maximumWeight)return fail("invalid_weight");
    }
    CharacterShapeResult3D candidate;
    candidate.mesh=baseline;
    std::vector<CharacterShapeWeight3D> ordered(weights.begin(),weights.end());
    std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.name<b.name;});
    for(std::size_t i=0;i<candidate.mesh.vertices.size();++i){
        auto& v=candidate.mesh.vertices[i];double x=v.x,y=v.y,z=v.z;
        for(const auto& weight:ordered){const auto target=std::find_if(targets.begin(),targets.end(),[&](const auto& t){return t.name==weight.name;});const auto d=target->offsets[i];x+=double(d.x)*weight.value;y+=double(d.y)*weight.value;z+=double(d.z)*weight.value;}
        v.x=static_cast<float>(x);v.y=static_cast<float>(y);v.z=static_cast<float>(z);if(!finite({v.x,v.y,v.z}))return fail("position_overflow");
    }
    std::vector<Vec3> normals(candidate.mesh.vertices.size());
    std::vector<bool> changed(candidate.mesh.vertices.size());
    // 每面以休息與變形後的切平面建立逆轉置傳輸，保留 authored 法線偏差。
    // 不焊接位置相同的頂點；硬邊維持各自法線。厚度方向採單位法線。
    for(std::size_t i=0;i<candidate.mesh.indices.size();i+=3){
        const auto a=candidate.mesh.indices[i],b=candidate.mesh.indices[i+1],c=candidate.mesh.indices[i+2];
        auto point=[&](uint32_t index){const auto& v=candidate.mesh.vertices[index];return Vec3{v.x,v.y,v.z};};
        const auto normal=Cross(point(b)-point(a),point(c)-point(a));
        if(!finite(normal)||Length(normal)<=1e-12f)return fail("degenerate_triangle");
        auto rest=[&](uint32_t index){const auto& v=baseline.vertices[index];return Vec3{v.x,v.y,v.z};};
        const auto u=rest(b)-rest(a),v=rest(c)-rest(a),cross=Cross(u,v);
        const float area=Length(cross);if(!std::isfinite(area)||area<=1e-12f)return fail("degenerate_baseline_triangle");
        // 面積已經通過退化檢查，不可再套一般 Normalize 的較大長度門檻。
        const auto U=point(b)-point(a),V=point(c)-point(a),N=normal*(1.f/Length(normal)),n=cross*(1.f/area);
        const bool same=U.x==u.x&&U.y==u.y&&U.z==u.z&&V.x==v.x&&V.y==v.y&&V.z==v.z;
        const float determinant=Dot(U,Cross(V,N));if(!std::isfinite(determinant)||std::abs(determinant)<=1e-12f)return fail("singular_deformation");
        for(auto index:{a,b,c}){
            const auto packed=UnpackNormalSnorm8(baseline.vertices[index].normal);Vec3 authored{packed.x,packed.y,packed.z};
            if(Length(authored)<=1e-8f)authored=n;
            const auto transported=same?authored:(Cross(V,N)*Dot(u,authored)+Cross(N,U)*Dot(v,authored)+Cross(U,V)*Dot(n,authored))*(1.f/determinant);
            if(!finite(transported))return fail("normal_overflow");
            normals[index]=normals[index]+transported*area;changed[index]=changed[index]||!same;
        }
    }
    for(std::size_t i=0;i<normals.size();++i){if(!changed[i])continue;const float length=Length(normals[i]);if(!finite(normals[i])||!std::isfinite(length)||length<=1e-12f)return fail("invalid_vertex_normal");const auto authored=UnpackNormalSnorm8(baseline.vertices[i].normal);candidate.mesh.vertices[i].normal=PackNormalSnorm8(normals[i]*(1.f/length),authored.w);}
    // 全部歸零時保留資產原有法線，避免重設外觀後光照仍與原始模型不同。
    if(std::all_of(weights.begin(),weights.end(),[](const auto& weight){return weight.value==0.f;})){
        for(std::size_t i=0;i<candidate.mesh.vertices.size();++i)
            candidate.mesh.vertices[i].normal=baseline.vertices[i].normal;
    }
    output=std::move(candidate);
    if(error)error->clear();
    return true;
}
}
