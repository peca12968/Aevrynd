#pragma once
#include "MistwatchRouteGrassPlan.h"
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <filesystem>
#include <fstream>

namespace Mistwatch::RouteGrassCook {
namespace D=CoreHerbClusters::Detail;
inline constexpr std::size_t MaximumBytes=64u*1024u*1024u;
inline constexpr unsigned Magic=0x52474341u,Schema=1;
class Bytes final {
    std::vector<std::byte> data_;
public:
    void Word(std::uint32_t value){D::Need(data_.size()<=MaximumBytes-4,"ROUTE_GRASS_COOK_BUDGET");
        for(unsigned b=0;b<4;++b)data_.push_back(std::byte((value>>(8*b))&255u));}
    void Wide(std::uint64_t value){Word(static_cast<std::uint32_t>(value));Word(static_cast<std::uint32_t>(value>>32));}
    void Float(float value){D::Need(std::isfinite(value),"ROUTE_GRASS_COOK_NONFINITE");Word(std::bit_cast<std::uint32_t>(value));}
    void Double(double value){D::Need(std::isfinite(value),"ROUTE_GRASS_COOK_NONFINITE");Wide(std::bit_cast<std::uint64_t>(value));}
    void Digest(const std::array<std::byte,32>& value){for(unsigned i=0;i<8;++i){std::uint32_t w{};
        for(unsigned b=0;b<4;++b)w|=std::uint32_t(std::to_integer<unsigned>(value[i*4+b]))<<(8*b);Word(w);}}
    std::span<const std::byte> Data()const noexcept{return data_;}
    void Mesh(const Aevrynd::Mesh3D& mesh){LandscapeSpeciesDetail::ValidateMesh(mesh,false);
        D::Need(mesh.vertices.size()<=1000000&&mesh.indices.size()<=3000000,"ROUTE_GRASS_COOK_MESH_BUDGET");
        Word(0x334d4541u);Word(1);Word(static_cast<unsigned>(mesh.vertices.size()));Word(static_cast<unsigned>(mesh.indices.size()));
        for(const auto& v:mesh.vertices){Float(v.x);Float(v.y);Float(v.z);Word(v.normal);Word(v.color);Float(v.u);Float(v.v);}
        for(auto index:mesh.indices)Word(index);
    }
};
inline std::array<std::byte,32> Hash(std::span<const std::byte> bytes){std::array<std::byte,32> result{};
    D::Need(Aevrynd::WorldDeltaSha2563D(bytes,result),"ROUTE_GRASS_COOK_SHA256");return result;}
inline std::string Hex(const std::array<std::byte,32>& value){constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(64);
    for(auto b:value){const unsigned v=std::to_integer<unsigned>(b);out.push_back(digits[v>>4]);out.push_back(digits[v&15]);}return out;}
inline std::array<std::byte,32> ParseDigest(std::string_view text){
    D::Need(text.size()==64,"ROUTE_GRASS_COOK_BUILD_SHA256");std::array<std::byte,32> out;
    const auto nibble=[](char c)->unsigned {if(c>='0'&&c<='9')return unsigned(c-'0');if(c>='a'&&c<='f')return unsigned(c-'a'+10);
        throw std::invalid_argument("ROUTE_GRASS_COOK_BUILD_SHA256");};
    for(unsigned i=0;i<32;++i)out[i]=std::byte((nibble(text[i*2])<<4)|nibble(text[i*2+1]));return out;
}
inline Bytes Encode(const RouteGrassPlan::Plan& plan,const std::array<std::byte,32>& build){
    D::Need(plan.instances.size()==RouteGrassAuthor::Target&&plan.surfaceFingerprint==plan.context.words[5]&&plan.fingerprint!=0,
        "ROUTE_GRASS_COOK_INCOMPLETE");
    (void)RouteGrassContext::Fingerprint(plan.context);Bytes out;out.Word(Magic);out.Word(Schema);
    out.Word(RouteGrassAuthor::Target);out.Word(RouteGrassContext::Schema);out.Digest(build);
    for(auto value:plan.context.words)out.Wide(value);out.Wide(plan.fingerprint);out.Wide(plan.surfaceFingerprint);
    for(unsigned i=0;i<RouteGrassAuthor::Target;++i){const auto& a=plan.instances[i];const auto& p=a.plant;const auto& q=a.proof;const auto& c=q.contact;
        D::Need(a.ordinal==RouteGrassAuthor::Ordinals[i]&&a.bin<16&&p.variant<5&&q.contact.safe&&q.sourceFootPieces&&q.windPinnedVertices&&
            q.sourceFingerprint==RouteGrassSupport::SourceFingerprints[p.variant]&&q.surfaceFingerprint==plan.surfaceFingerprint&&
            p.sourceFingerprint==q.sourceFingerprint&&p.derivedFingerprint==q.derivedFingerprint&&
            D::MeshFingerprint(p.derivedMesh)==q.derivedFingerprint&&q.allFaces==p.derivedMesh.indices.size()/3,
            "ROUTE_GRASS_COOK_PROOF_IDENTITY");
        out.Word(a.ordinal);out.Word(a.bin);out.Word(p.variant);out.Word(static_cast<unsigned>(p.rootWind));
        out.Wide(q.sourceFingerprint);out.Wide(q.derivedFingerprint);out.Wide(q.surfaceFingerprint);
        for(float f:{p.pose.position.x,p.pose.position.y,p.pose.position.z,p.pose.rotation.x,p.pose.rotation.y,p.pose.rotation.z,
            p.pose.scale.x,p.pose.scale.y,p.pose.scale.z,p.root.x,p.root.y,p.root.z,p.radius,p.requestedHeight})out.Float(f);
        const auto& stats=p.conformCounters;out.Word(stats.sampledVertices);out.Word(stats.movedVertices);out.Word(stats.affectedFaces);out.Word(stats.rebuiltNormals);
        out.Float(stats.sourceMinimumY);out.Float(stats.sourceMaximumY);out.Double(stats.maximumGroundDelta);
        out.Word(p.pinnedVertices);out.Word(p.changedWindVertices);out.Word(p.conformResult?1u:0u);out.Word(p.wholeSupportProven?1u:0u);
        out.Word(q.allFaces);out.Word(q.sourceFootPieces);out.Word(q.windPinnedVertices);
        out.Word(c.safe?1u:0u);out.Word(static_cast<unsigned>(c.rejection));out.Word(c.restingFootObserved?1u:0u);out.Word(c.restingFootAboveLimit?1u:0u);
        for(float f:{c.maximumRestingFootGap,c.minimumNormalY,c.maximumHeightDelta,c.minimumGapBound,c.maximumFootGapBound})out.Float(f);
        out.Word(c.footprintFaces);out.Word(c.evaluatedFaces);out.Word(c.skippedFaces);out.Word(c.footPieces);
        for(auto count:{c.cachedGroundFaces,c.aabbRejected,c.exactOverlayFaces,c.groundHeightPlanes,c.sourceHeightPlanes,c.cachedHeightEvaluations})out.Wide(count);
        out.Mesh(p.derivedMesh);
    }return out;
}
// 只在新的隔離資產建置目錄寫入；不覆蓋使用者或既有驗收產物。
inline void WriteFresh(const std::filesystem::path& path,std::span<const std::byte> bytes){
    D::Need(path.is_absolute()&&!std::filesystem::exists(path)&&!bytes.empty()&&bytes.size()<=MaximumBytes,"ROUTE_GRASS_COOK_OUTPUT");
    std::ofstream file(path,std::ios::binary);D::Need(bool(file),"ROUTE_GRASS_COOK_OPEN");
    file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));file.close();
    D::Need(bool(file),"ROUTE_GRASS_COOK_WRITE");
}
}
