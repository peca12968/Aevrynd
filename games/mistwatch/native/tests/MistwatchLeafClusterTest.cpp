#include "MistwatchLeafCluster.h"
#include "Aevrynd/MeshLoader.h"
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {
unsigned rejections{};
void Need(bool valid,const char* why){if(!valid)throw std::runtime_error(why);}
void Reject(const Aevrynd::Mesh3D& candidate,const Aevrynd::Mesh3D& original,
    const char* caseName) {
    bool rejected{};
    try{Mistwatch::ValidateLeafCluster(candidate,original);}
    catch(const std::runtime_error& e){
        Need(std::string_view(e.what()).starts_with("LEAF_CLUSTER_"),
            "Unexpected failure did not identify the asset gate");
        rejected=true;
    }
    if(!rejected)throw std::runtime_error(std::string("Accepted mutation: ")+caseName);
    ++rejections;
}
}
int main(int argc,char** argv)try {
    Need(argc==2,"Usage: Aevrynd-Mistwatch-LeafClusterTest <runtime-assets>");
    const std::filesystem::path assets=argv[1];
    auto source=Aevrynd::LoadAevryndMeshFile(assets/"mistwatch-canopy-preview"/"meshes"/
        "island-tree-01-lod1-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh");
    auto candidate=Aevrynd::LoadAevryndMeshFile(assets/"mistwatch-leaf-clusters"/"leaves-v1.aemesh");
    if(!source)throw std::runtime_error("Source load: "+source.error);
    if(!candidate)throw std::runtime_error("Candidate load: "+candidate.error);
    // 真正載入固定資產，測試不呼叫內部 hash helper 來製造預期指紋。
    Mistwatch::ValidateLeafCluster(candidate.mesh,source.mesh);
    const auto badCandidate=[&](const char* name,auto mutate){
        auto changed=candidate.mesh;mutate(changed);Reject(changed,source.mesh,name);
    };
    const auto badSource=[&](const char* name,auto mutate){
        auto changed=source.mesh;mutate(changed);Reject(candidate.mesh,changed,name);
    };
    const auto next=[](float value){return std::nextafter(value,
        (std::numeric_limits<float>::infinity)());};
    // 起點、中點、尾端都改動，避免只抽樣頭尾便通過。
    for(const auto i:{std::size_t{0},candidate.mesh.vertices.size()/2u,candidate.mesh.vertices.size()-1u}){
        badCandidate("position-x",[&](auto& m){m.vertices[i].x=next(m.vertices[i].x);});
        badCandidate("position-y",[&](auto& m){m.vertices[i].y=next(m.vertices[i].y);});
        badCandidate("position-z",[&](auto& m){m.vertices[i].z=next(m.vertices[i].z);});
        badCandidate("uv-u",[&](auto& m){m.vertices[i].u=next(m.vertices[i].u);});
        badCandidate("uv-v",[&](auto& m){m.vertices[i].v=next(m.vertices[i].v);});
        badCandidate("normal",[&](auto& m){m.vertices[i].normal^=1u;});
        badCandidate("color",[&](auto& m){m.vertices[i].color^=1u;});
        badCandidate("wind-w",[&](auto& m){m.vertices[i].normal^=0x01000000u;});
    }
    for(const auto i:{std::size_t{0},candidate.mesh.indices.size()/2u,candidate.mesh.indices.size()-1u})
        badCandidate("valid-index-change",[&](auto& m){m.indices[i]=(m.indices[i]+1u)%
            static_cast<std::uint32_t>(m.vertices.size());});
    badCandidate("missing-face",[](auto& m){m.indices.resize(m.indices.size()-3u);});
    badCandidate("missing-vertex",[](auto& m){m.vertices.pop_back();});
    badCandidate("nan-position",[](auto& m){m.vertices[1].x=(std::numeric_limits<float>::quiet_NaN)();});
    badCandidate("nan-uv",[](auto& m){m.vertices[1].v=(std::numeric_limits<float>::quiet_NaN)();});
    badCandidate("infinite-position",[](auto& m){m.vertices[1].y=(std::numeric_limits<float>::infinity)();});
    badCandidate("out-of-range-index",[](auto& m){m.indices[1]=static_cast<std::uint32_t>(m.vertices.size());});
    badCandidate("outside-source-aabb",[](auto& m){m.vertices[1].z=1000000.f;});
    badSource("source-position",[&](auto& m){m.vertices[m.vertices.size()/2u].x=next(m.vertices[m.vertices.size()/2u].x);});
    badSource("source-uv",[&](auto& m){m.vertices.back().u=next(m.vertices.back().u);});
    badSource("source-normal",[](auto& m){m.vertices[0].normal^=1u;});
    badSource("source-color",[](auto& m){m.vertices.back().color^=1u;});
    badSource("source-index",[](auto& m){m.indices.back()=(m.indices.back()+1u)%static_cast<std::uint32_t>(m.vertices.size());});
    badSource("source-missing-face",[](auto& m){m.indices.resize(m.indices.size()-3u);});
    badSource("source-wind",[](auto& m){m.vertices[0].normal^=0x01000000u;});
    badSource("source-nan",[](auto& m){m.vertices[0].u=(std::numeric_limits<float>::quiet_NaN)();});
    Reject(source.mesh,source.mesh,"original-is-not-candidate");
    Reject(candidate.mesh,candidate.mesh,"candidate-is-not-original");
    Need(rejections==44u,"Negative control coverage unexpectedly changed");
    Mistwatch::ValidateLeafCluster(candidate.mesh,source.mesh);
    std::cout<<"leaf_cluster_asset_verified; negative_controls="<<rejections
        <<"; candidate_vertices="<<candidate.mesh.vertices.size()
        <<"; candidate_triangles="<<candidate.mesh.indices.size()/3u<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
