#include <Aevrynd/SkinnedModel3D.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

namespace Aevrynd {
namespace {
bool Finite(float value){return std::isfinite(value);}
bool FiniteMatrix(const Matrix4& matrix){for(std::size_t row=0;row<4;++row)for(std::size_t column=0;column<4;++column)if(!Finite(matrix[row][column]))return false;return true;}
bool FiniteTransform(const JointTransform3D& transform){return Finite(transform.translation.x)&&Finite(transform.translation.y)&&Finite(transform.translation.z)&&Finite(transform.rotation.x)&&Finite(transform.rotation.y)&&Finite(transform.rotation.z)&&Finite(transform.scale.x)&&Finite(transform.scale.y)&&Finite(transform.scale.z)&&transform.scale.x!=0.f&&transform.scale.y!=0.f&&transform.scale.z!=0.f;}
bool FiniteVertex(const Vertex3D& vertex){return Finite(vertex.x)&&Finite(vertex.y)&&Finite(vertex.z)&&Finite(vertex.u)&&Finite(vertex.v);}
}

SkeletonCompatibilityId3D ComputeSkeletonCompatibilityId3D(std::span<const std::string> jointNames,
    const Skeleton3D& skeleton,std::span<const Matrix4> inverseBindMatrices,SkeletonCompatibilityProvenance3D provenance){
    const bool structural=provenance==SkeletonCompatibilityProvenance3D::Structural;
    if(skeleton.joints.empty()||(!structural&&(jointNames.empty()||jointNames.size()!=skeleton.joints.size()))||inverseBindMatrices.size()!=skeleton.joints.size()||!skeleton.Valid())return{};
    std::uint64_t hash=14695981039346656037ull;
    auto mixByte=[&](std::uint8_t value){hash=(hash^value)*1099511628211ull;};
    auto mixU64=[&](std::uint64_t value){for(unsigned shift=0;shift<64;shift+=8)mixByte(std::uint8_t(value>>shift));};
    // 版本標籤可讓未來調整量化契約時明確失配，而不是靜默誤綁。
    for(const char value:"AevryndSkeletonCompatibility3D/v2")mixByte(std::uint8_t(value));
    mixByte(std::uint8_t(provenance));mixU64(skeleton.joints.size());
    auto mixQuantized=[&](float value){if(!std::isfinite(value)||std::abs(value)>1.0e9f)return false;mixU64(std::uint64_t(std::llround(double(value)*1000000.0)));return true;};
    for(std::size_t index=0;index<skeleton.joints.size();++index){
        if(!structural){if(jointNames[index].empty())return{};mixU64(jointNames[index].size());for(unsigned char value:jointNames[index])mixByte(value);}
        const SkeletonJoint3D& joint=skeleton.joints[index];mixU64(std::uint64_t(std::int64_t(joint.parent)));
        const Quaternion3D rotation=joint.bindTransform.usesQuaternion?NormalizeQuaternion3D(joint.bindTransform.quaternion):QuaternionFromEuler3D(joint.bindTransform.rotation);
        Quaternion3D canonical=rotation;if(canonical.w<0.f||(canonical.w==0.f&&(canonical.z<0.f||(canonical.z==0.f&&(canonical.y<0.f||(canonical.y==0.f&&canonical.x<0.f))))))canonical={-canonical.x,-canonical.y,-canonical.z,-canonical.w};
        for(float value:{joint.bindTransform.translation.x,joint.bindTransform.translation.y,joint.bindTransform.translation.z,canonical.x,canonical.y,canonical.z,canonical.w,joint.bindTransform.scale.x,joint.bindTransform.scale.y,joint.bindTransform.scale.z})if(!mixQuantized(value))return{};
        for(const auto& row:inverseBindMatrices[index].values)for(float value:row){
            if(!std::isfinite(value)||std::abs(value)>1.0e9f)return{};
            // 微米級量化會正規化 -0 與無意義的序列化浮點雜訊。
            const std::int64_t quantized=std::llround(double(value)*1000000.0);
            mixU64(std::uint64_t(quantized));
        }
    }
    if(!hash)hash=1;
    return{hash};
}

SkeletonCompatibilityId3D ComputeStructuralSkeletonCompatibilityId3D(const Skeleton3D& skeleton,
    std::span<const Matrix4> inverseBindMatrices){return ComputeSkeletonCompatibilityId3D({},skeleton,inverseBindMatrices,SkeletonCompatibilityProvenance3D::Structural);}

bool SkinnedModel3D::Valid(const SkinnedModelLimits3D& limits)const{
    if(!bindMesh.Valid()||bindMesh.vertices.size()>limits.maximumVertices||bindMesh.indices.size()>limits.maximumIndices||!skeleton.Valid()||skeleton.joints.size()>limits.maximumJoints||influences.size()!=bindMesh.vertices.size()||inverseBindMatrices.size()!=skeleton.joints.size()||(!tangents.empty()&&tangents.size()!=bindMesh.vertices.size()))return false;
    if(bindMesh.vertices.size()>(std::numeric_limits<std::size_t>::max)()/sizeof(Vertex3D)||bindMesh.indices.size()>(std::numeric_limits<std::size_t>::max)()/sizeof(std::uint32_t)||skeleton.joints.size()>(std::numeric_limits<std::size_t>::max)()/(sizeof(Matrix4)*2))return false;
    const std::size_t vertexBytes=bindMesh.vertices.size()*sizeof(Vertex3D),indexBytes=bindMesh.indices.size()*sizeof(std::uint32_t),matrixBytes=skeleton.joints.size()*sizeof(Matrix4)*2,tangentBytes=tangents.size()*sizeof(std::uint32_t);if(vertexBytes>limits.maximumWorkspaceBytes||indexBytes>limits.maximumWorkspaceBytes-vertexBytes||matrixBytes>limits.maximumWorkspaceBytes-vertexBytes-indexBytes||tangentBytes>limits.maximumWorkspaceBytes-vertexBytes-indexBytes-matrixBytes)return false;
    for(const Vertex3D& vertex:bindMesh.vertices)if(!FiniteVertex(vertex))return false;
    for(const SkeletonJoint3D& joint:skeleton.joints)if(!FiniteTransform(joint.bindTransform))return false;
    for(const Matrix4& inverse:inverseBindMatrices)if(!FiniteMatrix(inverse))return false;
    for(const std::uint32_t packed:tangents){const Vec4 tangent=UnpackNormalSnorm8(packed);const float length=Length(Vec3{tangent.x,tangent.y,tangent.z});if(!Finite(length)||length<.8f||length>1.2f||(std::uint8_t(packed>>24)!=0x7fu&&std::uint8_t(packed>>24)!=0x81u))return false;}
    for(const SkinInfluences3D& influence:influences){std::uint32_t total{};for(std::size_t slot=0;slot<4;++slot){total+=influence.weights[slot];if(influence.weights[slot]&&influence.joints[slot]>=skeleton.joints.size())return false;if(!influence.weights[slot]&&influence.joints[slot])return false;if(slot&&influence.weights[slot]>influence.weights[slot-1])return false;}if(total!=65535u)return false;}
    return true;
}

ReusableMemoryStats SkinningWorkspace3D::MemoryStats()const{
    const std::size_t bytes=scratch_.vertices.capacity()*sizeof(Vertex3D)+scratch_.indices.capacity()*sizeof(std::uint32_t)+(skinMatrices_.capacity()+normalMatrices_.capacity())*sizeof(Matrix4)+paletteJoints_.capacity()*sizeof(SkinPaletteJoint3D);
    // 工作內容只服務下一次蒙皮，可在記憶體壓力下全部回收。
    return{bytes,bytes,lastReleasedBytes_,trimCount_};
}

std::size_t SkinningWorkspace3D::TrimUnusedMemory(){
    const std::size_t before=MemoryStats().retainedCapacityBytes;scratch_.vertices.clear();scratch_.indices.clear();skinMatrices_.clear();normalMatrices_.clear();paletteJoints_.clear();std::vector<Vertex3D>{}.swap(scratch_.vertices);std::vector<std::uint32_t>{}.swap(scratch_.indices);std::vector<Matrix4>{}.swap(skinMatrices_);std::vector<Matrix4>{}.swap(normalMatrices_);std::vector<SkinPaletteJoint3D>{}.swap(paletteJoints_);const std::size_t after=MemoryStats().retainedCapacityBytes;lastReleasedBytes_=before>after?before-after:0;++trimCount_;return lastReleasedBytes_;
}

bool SkinModelInto(const SkinnedModel3D& model,const AnimationPose3D& pose,SkinningWorkspace3D& workspace,Mesh3D& output,SkinningStats3D* stats,const SkinnedModelLimits3D& limits){
    if(stats)*stats={};if(!model.Valid(limits)||pose.globalMatrices.size()!=model.skeleton.joints.size())return false;for(const Matrix4& matrix:pose.globalMatrices)if(!FiniteMatrix(matrix))return false;
    try{
        Mesh3D& scratch=workspace.scratch_;scratch.vertices.clear();scratch.indices.clear();scratch.vertices.reserve(model.bindMesh.vertices.size());scratch.indices.assign(model.bindMesh.indices.begin(),model.bindMesh.indices.end());
        workspace.skinMatrices_.clear();workspace.normalMatrices_.clear();workspace.skinMatrices_.reserve(model.skeleton.joints.size());workspace.normalMatrices_.reserve(model.skeleton.joints.size());for(std::size_t joint=0;joint<model.skeleton.joints.size();++joint){const Matrix4 skin=pose.globalMatrices[joint]*model.inverseBindMatrices[joint];if(!FiniteMatrix(skin))return false;workspace.skinMatrices_.push_back(skin);const Matrix4 normal=CreateNormalMatrix(skin);if(!FiniteMatrix(normal))return false;workspace.normalMatrices_.push_back(normal);}
        std::size_t activeInfluences{};for(std::size_t vertexIndex=0;vertexIndex<model.bindMesh.vertices.size();++vertexIndex){const Vertex3D& source=model.bindMesh.vertices[vertexIndex];const SkinInfluences3D& influence=model.influences[vertexIndex];const Vec3 sourcePosition{source.x,source.y,source.z};const Vec4 unpacked=UnpackNormalSnorm8(source.normal);const Vec3 sourceNormal{unpacked.x,unpacked.y,unpacked.z};Vec3 position{},normal{};for(std::size_t slot=0;slot<4;++slot){if(!influence.weights[slot])continue;++activeInfluences;const float weight=float(influence.weights[slot])/65535.f;const std::size_t jointIndex=influence.joints[slot];position=position+TransformPoint(workspace.skinMatrices_[jointIndex],sourcePosition)*weight;normal=normal+TransformNormal(workspace.normalMatrices_[jointIndex],sourceNormal)*weight;}normal=Normalize(normal);if(!Finite(position.x)||!Finite(position.y)||!Finite(position.z)||!Finite(normal.x)||!Finite(normal.y)||!Finite(normal.z)||Length(normal)<.0001f)return false;Vertex3D skinned=source;skinned.x=position.x;skinned.y=position.y;skinned.z=position.z;skinned.normal=(source.normal&0xff000000u)|(PackNormalSnorm8(normal)&0x00ffffffu);scratch.vertices.push_back(skinned);}
        output.vertices.swap(scratch.vertices);output.indices.swap(scratch.indices);output.InvalidateRaycastIndex();scratch.InvalidateRaycastIndex();if(stats)*stats={output.vertices.size(),activeInfluences,output.vertices.size()*sizeof(Vertex3D)+output.indices.size()*sizeof(std::uint32_t)};return true;
    }catch(const std::bad_alloc&){return false;}
}

bool BuildGpuSkinnedVertices(const SkinnedModel3D& model,std::vector<SkinnedVertex3D>& output,const SkinnedModelLimits3D& limits){
    if(!model.Valid(limits))return false;
    try{
        std::vector<std::uint32_t> generatedTangents;
        const std::vector<std::uint32_t>* tangents=&model.tangents;
        if(tangents->empty()){
            const std::size_t vertexCount=model.bindMesh.vertices.size();
            constexpr std::size_t BytesPerGeneratedVertex=sizeof(Vec3)*2+sizeof(std::uint32_t)+sizeof(SkinnedVertex3D);if(vertexCount>(std::numeric_limits<std::size_t>::max)()/BytesPerGeneratedVertex||vertexCount*BytesPerGeneratedVertex>limits.maximumWorkspaceBytes)return false;
            std::vector<Vec3> tangentSums(vertexCount),bitangentSums(vertexCount);
            for(std::size_t offset=0;offset<model.bindMesh.indices.size();offset+=3){const std::uint32_t ia=model.bindMesh.indices[offset],ib=model.bindMesh.indices[offset+1],ic=model.bindMesh.indices[offset+2];const Vertex3D& a=model.bindMesh.vertices[ia];const Vertex3D& b=model.bindMesh.vertices[ib];const Vertex3D& c=model.bindMesh.vertices[ic];const Vec3 edge1{b.x-a.x,b.y-a.y,b.z-a.z},edge2{c.x-a.x,c.y-a.y,c.z-a.z};const float du1=b.u-a.u,dv1=b.v-a.v,du2=c.u-a.u,dv2=c.v-a.v,determinant=du1*dv2-du2*dv1;if(!Finite(determinant)||std::abs(determinant)<1e-12f)continue;const float inverse=1.f/determinant;const Vec3 tangent=(edge1*dv2-edge2*dv1)*inverse,bitangent=(edge2*du1-edge1*du2)*inverse;if(!Finite(tangent.x)||!Finite(tangent.y)||!Finite(tangent.z)||!Finite(bitangent.x)||!Finite(bitangent.y)||!Finite(bitangent.z))continue;for(const std::uint32_t vertex:{ia,ib,ic}){tangentSums[vertex]=tangentSums[vertex]+tangent;bitangentSums[vertex]=bitangentSums[vertex]+bitangent;}}
            generatedTangents.reserve(vertexCount);for(std::size_t index=0;index<vertexCount;++index){const Vec4 packedNormal=UnpackNormalSnorm8(model.bindMesh.vertices[index].normal);Vec3 normal{packedNormal.x,packedNormal.y,packedNormal.z};normal=Length(normal)>=.5f?Normalize(normal):Vec3{0,1,0};Vec3 tangent=tangentSums[index]-normal*Dot(normal,tangentSums[index]);if(Length(tangent)<1e-6f){const Vec3 axis=std::abs(normal.y)<.9f?Vec3{0,1,0}:Vec3{1,0,0};tangent=Cross(axis,normal);}tangent=Normalize(tangent);const float handedness=Length(bitangentSums[index])>=1e-6f&&Dot(Cross(normal,tangent),bitangentSums[index])<0.f?-1.f:1.f;generatedTangents.push_back(PackNormalSnorm8(tangent,handedness));}tangents=&generatedTangents;
        }
        std::vector<SkinnedVertex3D> temporary;temporary.reserve(model.bindMesh.vertices.size());
        for(std::size_t index=0;index<model.bindMesh.vertices.size();++index){const Vertex3D& vertex=model.bindMesh.vertices[index];const SkinInfluences3D& influence=model.influences[index];temporary.push_back({vertex.x,vertex.y,vertex.z,vertex.normal,vertex.color,vertex.u,vertex.v,(*tangents)[index],influence.joints,influence.weights});}
        output.swap(temporary);return true;
    }catch(const std::bad_alloc&){return false;}
}

SkinPaletteRange3D AddSkinPaletteFromPose(DrawList& draw,const SkinnedModel3D& model,const AnimationPose3D& pose,SkinningWorkspace3D& workspace,const SkinnedModelLimits3D& limits){
    if(!model.Valid(limits)||pose.globalMatrices.size()!=model.skeleton.joints.size())return{};for(const Matrix4& matrix:pose.globalMatrices)if(!FiniteMatrix(matrix))return{};
    try{
        workspace.paletteJoints_.clear();workspace.paletteJoints_.reserve(model.skeleton.joints.size());
        for(std::size_t joint=0;joint<model.skeleton.joints.size();++joint){const Matrix4 skin=pose.globalMatrices[joint]*model.inverseBindMatrices[joint],normal=CreateNormalMatrix(skin);if(!FiniteMatrix(skin)||!FiniteMatrix(normal))return{};SkinPaletteJoint3D palette{};palette.skin=skin;for(std::size_t row=0;row<3;++row)palette.normalRows[row]={normal[row][0],normal[row][1],normal[row][2],0};workspace.paletteJoints_.push_back(palette);}
        const SkeletonCompatibilityId3D compatibility=model.skeletonCompatibilityId?model.skeletonCompatibilityId:ComputeStructuralSkeletonCompatibilityId3D(model.skeleton,model.inverseBindMatrices);
        return draw.AddSkinPalette(workspace.paletteJoints_,compatibility);
    }catch(const std::bad_alloc&){return{};}
}
}
