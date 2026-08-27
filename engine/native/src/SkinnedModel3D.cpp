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

bool SkinnedModel3D::Valid(const SkinnedModelLimits3D& limits)const{
    if(!bindMesh.Valid()||bindMesh.vertices.size()>limits.maximumVertices||bindMesh.indices.size()>limits.maximumIndices||!skeleton.Valid()||skeleton.joints.size()>limits.maximumJoints||influences.size()!=bindMesh.vertices.size()||inverseBindMatrices.size()!=skeleton.joints.size())return false;
    if(bindMesh.vertices.size()>(std::numeric_limits<std::size_t>::max)()/sizeof(Vertex3D)||bindMesh.indices.size()>(std::numeric_limits<std::size_t>::max)()/sizeof(std::uint32_t)||skeleton.joints.size()>(std::numeric_limits<std::size_t>::max)()/(sizeof(Matrix4)*2))return false;
    const std::size_t vertexBytes=bindMesh.vertices.size()*sizeof(Vertex3D),indexBytes=bindMesh.indices.size()*sizeof(std::uint32_t),matrixBytes=skeleton.joints.size()*sizeof(Matrix4)*2;if(vertexBytes>limits.maximumWorkspaceBytes||indexBytes>limits.maximumWorkspaceBytes-vertexBytes||matrixBytes>limits.maximumWorkspaceBytes-vertexBytes-indexBytes)return false;
    for(const Vertex3D& vertex:bindMesh.vertices)if(!FiniteVertex(vertex))return false;
    for(const SkeletonJoint3D& joint:skeleton.joints)if(!FiniteTransform(joint.bindTransform))return false;
    for(const Matrix4& inverse:inverseBindMatrices)if(!FiniteMatrix(inverse))return false;
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
        std::vector<SkinnedVertex3D> temporary;temporary.reserve(model.bindMesh.vertices.size());
        for(std::size_t index=0;index<model.bindMesh.vertices.size();++index){const Vertex3D& vertex=model.bindMesh.vertices[index];const SkinInfluences3D& influence=model.influences[index];temporary.push_back({vertex.x,vertex.y,vertex.z,vertex.normal,vertex.color,vertex.u,vertex.v,influence.joints,influence.weights});}
        output.swap(temporary);return true;
    }catch(const std::bad_alloc&){return false;}
}

SkinPaletteRange3D AddSkinPaletteFromPose(DrawList& draw,const SkinnedModel3D& model,const AnimationPose3D& pose,SkinningWorkspace3D& workspace,const SkinnedModelLimits3D& limits){
    if(!model.Valid(limits)||pose.globalMatrices.size()!=model.skeleton.joints.size())return{};for(const Matrix4& matrix:pose.globalMatrices)if(!FiniteMatrix(matrix))return{};
    try{
        workspace.paletteJoints_.clear();workspace.paletteJoints_.reserve(model.skeleton.joints.size());
        for(std::size_t joint=0;joint<model.skeleton.joints.size();++joint){const Matrix4 skin=pose.globalMatrices[joint]*model.inverseBindMatrices[joint],normal=CreateNormalMatrix(skin);if(!FiniteMatrix(skin)||!FiniteMatrix(normal))return{};SkinPaletteJoint3D palette{};palette.skin=skin;for(std::size_t row=0;row<3;++row)palette.normalRows[row]={normal[row][0],normal[row][1],normal[row][2],0};workspace.paletteJoints_.push_back(palette);}
        return draw.AddSkinPalette(workspace.paletteJoints_);
    }catch(const std::bad_alloc&){return{};}
}
}
