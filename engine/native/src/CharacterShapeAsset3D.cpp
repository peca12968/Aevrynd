#include <Aevrynd/CharacterShapeAsset3D.h>
#include <Aevrynd/SkinnedModelAsset.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <bit>
#include <fstream>
#include <windows.h>
namespace Aevrynd {
bool LoadCharacterShapeAsset3D(std::span<const std::byte> source,std::span<const std::byte> baselineAsset,CharacterShapeAsset3D& output,std::string* error){
    auto fail=[&](const char* message){if(error)*error=message;return false;};
    constexpr size_t maximum=64u*1024u*1024u;
    if(source.size()<52||source.size()>maximum||baselineAsset.empty()||baselineAsset.size()>maximum)return fail("input_size");
    size_t cursor=0;
    auto u32=[&](){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(std::to_integer<uint8_t>(source[cursor++]))<<(i*8);return v;};
    if(u32()!=0x534d4341u||u32()!=1)return fail("magic_version");
    std::array<std::byte,32> digest{};
    if(!WorldDeltaSha2563D(baselineAsset,digest)||!std::equal(digest.begin(),digest.end(),source.begin()+8))return fail("baseline_hash");
    cursor=40;const auto vertices=u32(),indices=u32(),count=u32();
    if(!vertices||vertices>100000||!indices||indices>300000||indices%3||!count||count>128)return fail("count_limits");
    const uint64_t minimum=52ull+uint64_t(indices)*4+uint64_t(count)*(11ull+uint64_t(vertices)*12);
    if(minimum>source.size())return fail("truncated");
    auto baseline=LoadSkinnedModelAssetDetailed(baselineAsset);
    if(!baseline||baseline.asset.model.bindMesh.vertices.size()!=vertices||baseline.asset.model.bindMesh.indices.size()!=indices)return fail("baseline_geometry");
    std::vector<uint32_t> topology;topology.reserve(indices);
    for(uint32_t i=0;i<indices;++i){const auto index=u32();if(index>=vertices||index!=baseline.asset.model.bindMesh.indices[i])return fail("topology");topology.push_back(index);}
    CharacterShapeAsset3D candidate;std::unordered_set<std::string> names;
    for(uint32_t i=0;i<count;++i){
        if(source.size()-cursor<2)return fail("truncated_name");
        const auto low=std::to_integer<uint8_t>(source[cursor++]);const auto high=std::to_integer<uint8_t>(source[cursor++]);const size_t length=low+size_t(high)*256;
        if(!length||length>128||source.size()-cursor<length+8+size_t(vertices)*12)return fail("target_size");
        CharacterShapeDelta3D target;target.name.assign(reinterpret_cast<const char*>(source.data()+cursor),length);cursor+=length;
        if(target.name.find('\0')!=std::string::npos||MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,target.name.data(),static_cast<int>(length),nullptr,0)==0||!names.insert(target.name).second)return fail("target_name");
        target.minimumWeight=std::bit_cast<float>(u32());target.maximumWeight=std::bit_cast<float>(u32());
        if(!std::isfinite(target.minimumWeight)||!std::isfinite(target.maximumWeight)||target.minimumWeight>0||target.maximumWeight<0||target.minimumWeight>target.maximumWeight)return fail("bounds");
        target.topology=topology;target.offsets.reserve(vertices);
        for(uint32_t j=0;j<vertices;++j){Vec3 delta;delta.x=std::bit_cast<float>(u32());delta.y=std::bit_cast<float>(u32());delta.z=std::bit_cast<float>(u32());if(!std::isfinite(delta.x)||!std::isfinite(delta.y)||!std::isfinite(delta.z))return fail("nonfinite_delta");target.offsets.push_back(delta);}
        candidate.targets.push_back(std::move(target));
    }
    if(cursor!=source.size())return fail("trailing_bytes");
    output=std::move(candidate);if(error)error->clear();return true;
}
bool LoadCharacterShapeAssetFile3D(const std::filesystem::path& path,std::span<const std::byte> baseline,CharacterShapeAsset3D& output,std::string* error){
    std::ifstream file(path,std::ios::binary|std::ios::ate);const auto size=file.tellg();
    if(!file||size<52||size>64*1024*1024){if(error)*error="file_size";return false;}
    std::vector<std::byte> bytes(static_cast<size_t>(size));file.seekg(0);if(!file.read(reinterpret_cast<char*>(bytes.data()),size)||file.peek()!=std::char_traits<char>::eof()){if(error)*error="file_read";return false;}
    return LoadCharacterShapeAsset3D(bytes,baseline,output,error);
}
}
