#pragma once
#include "MistwatchBoulder.h"
#include "MistwatchFallenLogAssets.h"
#include <algorithm>
#include <limits>

namespace Mistwatch::BoulderDetail {
inline constexpr const char* MeshName="boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh";
inline constexpr const char* MeshSha="1005cca1811d45922b9daddcb1c6c6e155e09f569df8efe5de093468b11c25d6";
inline constexpr std::size_t MeshBytes=3071924,VertexCount=81373,IndexCount=198366;
// 獨立量測 LOD0 的原始來源；不更改 LOD2 碰撞與舊驗收界限。
inline constexpr Aevrynd::Vec3 ExpectedMinimum{-.3452860713f,.00223008008f,-.4959508181f};
inline constexpr Aevrynd::Vec3 ExpectedMaximum{.3469667733f,.5484802723f,.5000547171f};
inline Aevrynd::MeshBounds3D ExpectedBounds() {
    return {(ExpectedMinimum+ExpectedMaximum)*.5f,.5580717f,(ExpectedMaximum-ExpectedMinimum)*.5f};
}
inline float OriginalEnvelopeProtrusion(const Aevrynd::Mesh3D& mesh) {
    const auto old=BoulderBounds();const auto low=old.center-old.extents,high=old.center+old.extents;
    float result{};
    for(const auto& v:mesh.vertices){
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))return std::numeric_limits<float>::infinity();
        result=(std::max)({result,low.x-v.x,v.x-high.x,low.y-v.y,v.y-high.y,low.z-v.z,v.z-high.z});
    }
    return result;
}
inline bool ExactRawMesh(const Aevrynd::Mesh3D& mesh,std::span<const std::byte> raw) {
    static_assert(sizeof(Aevrynd::Vertex3D)==28&&std::endian::native==std::endian::little);
    if(raw.size()!=MeshBytes||mesh.vertices.size()!=VertexCount||mesh.indices.size()!=IndexCount||
       !mesh.Valid()||!FallenLogAssets::ExactHash(raw,MeshSha))return false;
    std::array<std::uint32_t,4> header{};std::memcpy(header.data(),raw.data(),16);
    if(header!=std::array<std::uint32_t,4>{0x334d4541u,1u,81373u,198366u})return false;
    const auto vertexBytes=VertexCount*sizeof(Aevrynd::Vertex3D);
    if(std::memcmp(raw.data()+16,mesh.vertices.data(),vertexBytes)||
       std::memcmp(raw.data()+16+vertexBytes,mesh.indices.data(),IndexCount*4))return false;
    for(const auto& v:mesh.vertices)if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||
        !std::isfinite(v.u)||!std::isfinite(v.v))return false;
    return true;
}
inline bool ExactRootedBounds(const Aevrynd::Mesh3D& mesh) {
    if(!mesh.Valid())return false;
    const auto actual=mesh.Bounds(),expected=ExpectedBounds();
    const auto boundNear=[](float a,float b){return std::isfinite(a)&&std::abs(a-b)<=2.e-7f;};
    if(!boundNear(actual.center.x,expected.center.x)||!boundNear(actual.center.y,expected.center.y)||
       !boundNear(actual.center.z,expected.center.z)||!boundNear(actual.extents.x,expected.extents.x)||
       !boundNear(actual.extents.y,expected.extents.y)||!boundNear(actual.extents.z,expected.extents.z)||
       !boundNear(actual.radius,expected.radius))return false;
    // 此值是被保留且明示的原始外凸，不等同通過舊碰撞 strict-fit。
    return boundNear(OriginalEnvelopeProtrusion(mesh),.0000547170639f);
}
// 在同一唯讀鎖存活期間核對雜湊、loader 原始資料與新包圍盒。
inline Aevrynd::Mesh3D LoadBoulderDetailMesh(const std::filesystem::path& path) {
    struct File {HANDLE handle{INVALID_HANDLE_VALUE};~File(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}};
    File file{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    LARGE_INTEGER length{};
    if(file.handle==INVALID_HANDLE_VALUE||!GetFileSizeEx(file.handle,&length)||
       length.QuadPart!=static_cast<LONGLONG>(MeshBytes))throw std::runtime_error("Boulder detail raw source unavailable");
    std::vector<std::byte> raw(MeshBytes);DWORD read{};
    if(!ReadFile(file.handle,raw.data(),static_cast<DWORD>(raw.size()),&read,nullptr)||read!=raw.size())
        throw std::runtime_error("Boulder detail source read incomplete");
    auto loaded=Aevrynd::LoadAevryndMeshFile(path);
    if(!loaded||!ExactRawMesh(loaded.mesh,raw))throw std::runtime_error("Boulder detail geometry integrity mismatch");
    RootBoulderMesh(loaded.mesh);
    if(!ExactRootedBounds(loaded.mesh))throw std::runtime_error("Boulder detail rooted bounds mismatch");
    return std::move(loaded.mesh);
}
}
