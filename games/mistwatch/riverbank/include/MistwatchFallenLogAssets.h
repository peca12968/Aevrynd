#pragma once
#include <Aevrynd/MaterialLibrary3D.h>
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <Windows.h>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace Mistwatch::FallenLogAssets {
inline constexpr const char* MeshName="dead-tree-trunk-lod2-node0-mesh0-primitive0-materialnone-material-none.aemesh";
inline constexpr const char* MeshSha="b7f6a05bf928787082dea92a0051557496e45894a9c2b1a0af1e51110abd0d46";
inline constexpr std::size_t MeshBytes=434012,VertexCount=8956,IndexCount=45807;
inline constexpr std::array<const char*,3> TextureSha{
    "da9db4ede331a22f4e47ba2eab34cfcfb2c6979f6f180007e28dfc61ea23410b",
    "147d81e27afeaa1d1afc6e88d70d3814912a7d6235110ca4935edf6ffb5838fe",
    "f2f8ebd0a4c52c543047fb8131ad42391374313d595cd0901ddf5136c2b01354"};
inline bool ExactHash(std::span<const std::byte> raw,std::string_view expected) {
    std::array<std::byte,32> value{};
    if(expected.size()!=64||!Aevrynd::WorldDeltaSha2563D(raw,value))return false;
    constexpr char digits[]="0123456789abcdef";
    for(std::size_t at=0;at<value.size();++at){const auto n=std::to_integer<unsigned>(value[at]);
        if(expected[at*2]!=digits[n>>4]||expected[at*2+1]!=digits[n&15])return false;}
    return true;
}
inline bool ExactRawMesh(const Aevrynd::Mesh3D& mesh,std::span<const std::byte> raw) {
    static_assert(sizeof(Aevrynd::Vertex3D)==28&&std::endian::native==std::endian::little);
    if(raw.size()!=MeshBytes||mesh.vertices.size()!=VertexCount||mesh.indices.size()!=IndexCount||
       !mesh.Valid()||!ExactHash(raw,MeshSha))return false;
    std::array<std::uint32_t,4> header{};std::memcpy(header.data(),raw.data(),16);
    if(header!=std::array<std::uint32_t,4>{0x334d4541u,1u,8956u,45807u})return false;
    const auto vertexBytes=VertexCount*sizeof(Aevrynd::Vertex3D);
    if(std::memcmp(raw.data()+16,mesh.vertices.data(),vertexBytes)||
       std::memcmp(raw.data()+16+vertexBytes,mesh.indices.data(),IndexCount*4))return false;
    for(const auto& v:mesh.vertices)if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||
        !std::isfinite(v.u)||!std::isfinite(v.v))return false;
    return true;
}
// 同一開啟的檔案禁止寫入/刪除，雜湊與原生 loader 的兩次讀取不得改來源。
inline Aevrynd::Mesh3D LoadExactMesh(const std::filesystem::path& path) {
    struct File {HANDLE handle{INVALID_HANDLE_VALUE};~File(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}};
    File file{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    LARGE_INTEGER length{};
    if(file.handle==INVALID_HANDLE_VALUE||!GetFileSizeEx(file.handle,&length)||
        length.QuadPart!=static_cast<LONGLONG>(MeshBytes))throw std::runtime_error("Fallen log raw source unavailable");
    std::vector<std::byte> raw(MeshBytes);DWORD read{};
    if(!ReadFile(file.handle,raw.data(),static_cast<DWORD>(raw.size()),&read,nullptr)||read!=raw.size())
        throw std::runtime_error("Fallen log raw source read incomplete");
    const auto loaded=Aevrynd::LoadAevryndMeshFile(path);
    if(!loaded||!ExactRawMesh(loaded.mesh,raw))throw std::runtime_error("Fallen log geometry integrity mismatch");
    return loaded.mesh;
}
inline Aevrynd::MaterialAssetDefinition3D MaterialDefinition() {
    using namespace Aevrynd;
    MaterialTextureSetDefinition3D textures{"materials/slot-0-base.dds","materials/slot-0-arm.dds",2048,
                                          "materials/slot-0-normal.dds"};
    // LOD2 唯一 materialnone primitive 明示綁定原 glTF slot0；ARM 的 AO 為手動接線，原 glTF 沒有 occlusionTexture。
    textures.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;
    for(unsigned channel=0;channel<3;++channel)textures.integrity[channel]={5592580,TextureSha[channel]};
    Material3D material{{1,1,1,1},{1.f,0,0,1},true,{},1.f,true};
    material.doubleSided=true;
    return {"fallen-log",textures,material};
}
// 獨立資源，不替換同時用於修橋木梁的舊圓柱 handle。
class Resources {
public:
    Aevrynd::MeshResource3D mesh;
    Aevrynd::MaterialLibrary3D materials;
    Aevrynd::Material3D material;
    bool Initialize(Aevrynd::Engine& engine,const std::filesystem::path& root) {
        if(mesh||materials.Initialized())return false;
        try {
            const auto source=LoadExactMesh(root/"meshes"/MeshName);
            if(!materials.Initialize(engine,root)||!materials.Load(MaterialDefinition()))
                throw std::runtime_error("Fallen log material unavailable");
            const auto* loaded=materials.Find("fallen-log");
            if(!loaded||materials.Stats().materials!=1)throw std::runtime_error("Fallen log material identity mismatch");
            material=*loaded;mesh=engine.CreateMeshResource3D(source);
            if(!mesh)throw std::runtime_error("Fallen log GPU upload failed");
            return true;
        } catch(...) {Shutdown(engine);return false;}
    }
    unsigned Shutdown(Aevrynd::Engine& engine) {
        materials.Shutdown();unsigned destroyed{};
        if(mesh){engine.DestroyMesh3D(mesh.handle);destroyed=1;}
        mesh={};material={};return destroyed;
    }
};
}
