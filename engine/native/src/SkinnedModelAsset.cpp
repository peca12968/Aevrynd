#include <Aevrynd/SkinnedModelAsset.h>
#include <bit>
#include <fstream>
#include <limits>
#include <new>

namespace Aevrynd {
namespace {
constexpr std::uint32_t Magic=0x4b534541u,Version=1u,HeaderBytes=36u;
constexpr std::uint64_t VertexBytes=28,JointBytes=44,InfluenceBytes=16,MatrixBytes=64;

void SetError(std::string* error,std::string value){if(error)*error=std::move(value);}
bool AddSize(std::uint64_t& total,std::uint64_t count,std::uint64_t stride){if(count&&stride>(std::numeric_limits<std::uint64_t>::max)()/count)return false;const std::uint64_t bytes=count*stride;if(bytes>(std::numeric_limits<std::uint64_t>::max)()-total)return false;total+=bytes;return true;}

class Writer {
public:
    explicit Writer(std::vector<std::byte>& bytes):bytes_(bytes){}
    void U16(std::uint16_t value){bytes_.push_back(std::byte(value&255));bytes_.push_back(std::byte(value>>8));}
    void U32(std::uint32_t value){for(unsigned shift=0;shift<32;shift+=8)bytes_.push_back(std::byte((value>>shift)&255));}
    void U64(std::uint64_t value){for(unsigned shift=0;shift<64;shift+=8)bytes_.push_back(std::byte((value>>shift)&255));}
    void I32(std::int32_t value){U32(std::bit_cast<std::uint32_t>(value));}
    void F32(float value){U32(std::bit_cast<std::uint32_t>(value));}
private:std::vector<std::byte>& bytes_;
};

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes):bytes_(bytes){}
    bool U16(std::uint16_t& value){if(bytes_.size()-offset_<2)return false;value=std::uint16_t(std::to_integer<unsigned>(bytes_[offset_]))|std::uint16_t(std::to_integer<unsigned>(bytes_[offset_+1])<<8);offset_+=2;return true;}
    bool U32(std::uint32_t& value){if(bytes_.size()-offset_<4)return false;value=0;for(unsigned shift=0;shift<32;shift+=8)value|=std::uint32_t(std::to_integer<unsigned>(bytes_[offset_++]))<<shift;return true;}
    bool U64(std::uint64_t& value){if(bytes_.size()-offset_<8)return false;value=0;for(unsigned shift=0;shift<64;shift+=8)value|=std::uint64_t(std::to_integer<unsigned>(bytes_[offset_++]))<<shift;return true;}
    bool I32(std::int32_t& value){std::uint32_t bits{};if(!U32(bits))return false;value=std::bit_cast<std::int32_t>(bits);return true;}
    bool F32(float& value){std::uint32_t bits{};if(!U32(bits))return false;value=std::bit_cast<float>(bits);return true;}
    std::size_t Offset()const{return offset_;}
private:std::span<const std::byte> bytes_;std::size_t offset_{};
};

bool ExpectedSize(std::uint32_t vertices,std::uint32_t indices,std::uint32_t joints,std::uint64_t& size){size=HeaderBytes;return AddSize(size,vertices,VertexBytes)&&AddSize(size,indices,4)&&AddSize(size,joints,JointBytes)&&AddSize(size,vertices,InfluenceBytes)&&AddSize(size,joints,MatrixBytes);}
}

bool EncodeSkinnedModelAsset(const SkinnedModel3D& model,std::vector<std::byte>& output,std::string* error,const SkinnedModelLimits3D& limits){
    SetError(error,{});if(!model.Valid(limits)){SetError(error,"Skinned model is invalid");return false;}if(model.bindMesh.vertices.size()>UINT32_MAX||model.bindMesh.indices.size()>UINT32_MAX||model.skeleton.joints.size()>UINT32_MAX){SetError(error,"Skinned model count exceeds the file format");return false;}
    const auto vertexCount=std::uint32_t(model.bindMesh.vertices.size()),indexCount=std::uint32_t(model.bindMesh.indices.size()),jointCount=std::uint32_t(model.skeleton.joints.size());std::uint64_t size{};if(!ExpectedSize(vertexCount,indexCount,jointCount,size)||size>limits.maximumWorkspaceBytes||size>SIZE_MAX){SetError(error,"Skinned model asset exceeds the size limit");return false;}
    try{
        std::vector<std::byte> temporary;temporary.reserve(std::size_t(size));Writer writer(temporary);writer.U32(Magic);writer.U32(Version);writer.U32(HeaderBytes);writer.U32(0);writer.U64(size);writer.U32(vertexCount);writer.U32(indexCount);writer.U32(jointCount);
        for(const Vertex3D& vertex:model.bindMesh.vertices){writer.F32(vertex.x);writer.F32(vertex.y);writer.F32(vertex.z);writer.U32(vertex.normal);writer.U32(vertex.color);writer.F32(vertex.u);writer.F32(vertex.v);}
        for(std::uint32_t index:model.bindMesh.indices)writer.U32(index);
        for(const SkeletonJoint3D& joint:model.skeleton.joints){const Quaternion3D rotation=joint.bindTransform.usesQuaternion?NormalizeQuaternion3D(joint.bindTransform.quaternion):QuaternionFromEuler3D(joint.bindTransform.rotation);writer.I32(joint.parent);writer.F32(joint.bindTransform.translation.x);writer.F32(joint.bindTransform.translation.y);writer.F32(joint.bindTransform.translation.z);writer.F32(rotation.x);writer.F32(rotation.y);writer.F32(rotation.z);writer.F32(rotation.w);writer.F32(joint.bindTransform.scale.x);writer.F32(joint.bindTransform.scale.y);writer.F32(joint.bindTransform.scale.z);}
        for(const SkinInfluences3D& influence:model.influences){for(std::uint16_t joint:influence.joints)writer.U16(joint);for(std::uint16_t weight:influence.weights)writer.U16(weight);}
        for(const Matrix4& matrix:model.inverseBindMatrices)for(std::size_t row=0;row<4;++row)for(std::size_t column=0;column<4;++column)writer.F32(matrix[row][column]);
        if(temporary.size()!=size){SetError(error,"Skinned model encoding length mismatch");return false;}output.swap(temporary);return true;
    }catch(const std::bad_alloc&){SetError(error,"Skinned model encoding allocation failed");return false;}
}

SkinnedModelAssetResult LoadSkinnedModelAsset(std::span<const std::byte> source,const SkinnedModelLimits3D& limits){
    auto fail=[](std::string error){return SkinnedModelAssetResult{{},std::move(error)};};if(source.size()<HeaderBytes||source.size()>limits.maximumWorkspaceBytes)return fail("Skinned model asset length is outside limits");Reader reader(source);std::uint32_t magic{},version{},headerBytes{},flags{},vertexCount{},indexCount{},jointCount{};std::uint64_t fileSize{};if(!reader.U32(magic)||!reader.U32(version)||!reader.U32(headerBytes)||!reader.U32(flags)||!reader.U64(fileSize)||!reader.U32(vertexCount)||!reader.U32(indexCount)||!reader.U32(jointCount))return fail("Skinned model header is truncated");
    if(magic!=Magic||version!=Version||headerBytes!=HeaderBytes||flags!=0)return fail("Skinned model header is unsupported");if(!vertexCount||!indexCount||indexCount%3||!jointCount||vertexCount>limits.maximumVertices||indexCount>limits.maximumIndices||jointCount>limits.maximumJoints)return fail("Skinned model counts are invalid");std::uint64_t expected{};if(!ExpectedSize(vertexCount,indexCount,jointCount,expected)||fileSize!=expected||fileSize!=source.size())return fail("Skinned model asset length is invalid");
    try{
        SkinnedModel3D model;model.bindMesh.vertices.resize(vertexCount);model.bindMesh.indices.resize(indexCount);model.skeleton.joints.resize(jointCount);model.influences.resize(vertexCount);model.inverseBindMatrices.resize(jointCount);
        for(Vertex3D& vertex:model.bindMesh.vertices)if(!reader.F32(vertex.x)||!reader.F32(vertex.y)||!reader.F32(vertex.z)||!reader.U32(vertex.normal)||!reader.U32(vertex.color)||!reader.F32(vertex.u)||!reader.F32(vertex.v))return fail("Skinned model vertices are truncated");
        for(std::uint32_t& index:model.bindMesh.indices)if(!reader.U32(index))return fail("Skinned model indices are truncated");
        for(SkeletonJoint3D& joint:model.skeleton.joints){std::int32_t parent{};Quaternion3D rotation{};if(!reader.I32(parent)||!reader.F32(joint.bindTransform.translation.x)||!reader.F32(joint.bindTransform.translation.y)||!reader.F32(joint.bindTransform.translation.z)||!reader.F32(rotation.x)||!reader.F32(rotation.y)||!reader.F32(rotation.z)||!reader.F32(rotation.w)||!reader.F32(joint.bindTransform.scale.x)||!reader.F32(joint.bindTransform.scale.y)||!reader.F32(joint.bindTransform.scale.z))return fail("Skinned model joints are truncated");const float lengthSquared=rotation.x*rotation.x+rotation.y*rotation.y+rotation.z*rotation.z+rotation.w*rotation.w;if(!std::isfinite(lengthSquared)||lengthSquared<=1e-12f)return fail("Skinned model joint quaternion is invalid");joint.parent=parent;joint.bindTransform.rotation={};joint.bindTransform.quaternion=NormalizeQuaternion3D(rotation);joint.bindTransform.usesQuaternion=true;}
        for(SkinInfluences3D& influence:model.influences){for(std::uint16_t& joint:influence.joints)if(!reader.U16(joint))return fail("Skinned model influences are truncated");for(std::uint16_t& weight:influence.weights)if(!reader.U16(weight))return fail("Skinned model influences are truncated");}
        for(Matrix4& matrix:model.inverseBindMatrices)for(std::size_t row=0;row<4;++row)for(std::size_t column=0;column<4;++column)if(!reader.F32(matrix[row][column]))return fail("Skinned model matrices are truncated");
        if(reader.Offset()!=source.size()||!model.Valid(limits))return fail("Skinned model payload is invalid");return{std::move(model),{}};
    }catch(const std::bad_alloc&){return fail("Skinned model loading allocation failed");}
}

SkinnedModelAssetResult LoadSkinnedModelAssetFile(const std::filesystem::path& path,const SkinnedModelLimits3D& limits){std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return{{},"Unable to open skinned model asset"};const std::streamoff length=file.tellg();if(length<0||std::uint64_t(length)>limits.maximumWorkspaceBytes)return{{},"Skinned model file length is outside limits"};std::vector<std::byte> bytes;try{bytes.resize(std::size_t(length));}catch(const std::bad_alloc&){return{{},"Skinned model file allocation failed"};}file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),length);if(!file&&length)return{{},"Unable to read skinned model asset"};return LoadSkinnedModelAsset(bytes,limits);}

bool SaveSkinnedModelAssetFile(const std::filesystem::path& path,const SkinnedModel3D& model,std::string* error,const SkinnedModelLimits3D& limits){std::vector<std::byte> bytes;if(!EncodeSkinnedModelAsset(model,bytes,error,limits))return false;std::ofstream file(path,std::ios::binary|std::ios::trunc);if(!file){SetError(error,"Unable to open skinned model output");return false;}file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));if(!file){SetError(error,"Unable to write skinned model output");return false;}return true;}
}
