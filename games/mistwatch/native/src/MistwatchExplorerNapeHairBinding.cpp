#include "MistwatchExplorerNapeHairBinding.h"
#include "MistwatchExplorer.h"
#include "MistwatchExplorerNapeHairAuthor.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace Mistwatch::ExplorerNapeHairBinding {
namespace {
using namespace Aevrynd;
void Require(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
bool Same(float a,float b) {
    return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);
}
bool Same(Vec3 a,Vec3 b) {
    return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);
}
bool Same(const JointTransform3D& a,const JointTransform3D& b) {
    return Same(a.translation,b.translation)&&Same(a.rotation,b.rotation)&&Same(a.scale,b.scale)&&
        Same(a.quaternion.x,b.quaternion.x)&&Same(a.quaternion.y,b.quaternion.y)&&
        Same(a.quaternion.z,b.quaternion.z)&&Same(a.quaternion.w,b.quaternion.w)&&
        a.usesQuaternion==b.usesQuaternion;
}
bool Same(const Matrix4& a,const Matrix4& b) {
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)
        if(!Same(a.values[row][column],b.values[row][column]))return false;
    return true;
}
void SourceRig(const Explorer& explorer) {
    // 與未修改 Explorer() 的十五個 bind 位置與父關係逐字相同。
    constexpr std::array<Vec3,15> joints{{{0,.9f,0},{0,1.2f,0},{0,1.58f,0},
        {-.24f,1.4f,0},{-.28f,1.1f,0},{-.28f,.84f,0},
        {.24f,1.4f,0},{.28f,1.1f,0},{.28f,.84f,0},
        {-.12f,.88f,0},{-.12f,.48f,0},{-.12f,.1f,0},
        {.12f,.88f,0},{.12f,.48f,0},{.12f,.1f,0}}};
    constexpr int parents[]{-1,0,1,1,3,4,1,6,7,0,9,10,0,12,13};
    const auto& model=explorer.model;
    Require(model.skeleton.joints.size()==15&&model.inverseBindMatrices.size()==15&&
        explorer.pose.localTransforms.size()==15&&explorer.pose.globalMatrices.size()==15,
        "Nape binding requires original fifteen-joint rig and existing pose");
    for(unsigned i=0;i<joints.size();++i) {
        JointTransform3D expected;
        expected.translation=parents[i]<0?joints[i]:joints[i]-joints[parents[i]];
        Require(model.skeleton.joints[i].parent==parents[i]&&
            Same(model.skeleton.joints[i].bindTransform,expected)&&
            Same(model.inverseBindMatrices[i],CreateTranslationMatrix(joints[i]*-1.f)),
            "Nape binding source bind rig differs from original Explorer");
    }
    Require(model.skeletonCompatibilityId==ComputeStructuralSkeletonCompatibilityId3D(
        model.skeleton,model.inverseBindMatrices),"Nape binding source structural compatibility differs");
    for(const auto& transform:explorer.pose.localTransforms) {
        const std::array<float,13> values{{transform.translation.x,transform.translation.y,transform.translation.z,
            transform.rotation.x,transform.rotation.y,transform.rotation.z,transform.scale.x,transform.scale.y,
            transform.scale.z,transform.quaternion.x,transform.quaternion.y,transform.quaternion.z,transform.quaternion.w}};
        for(float value:values)Require(std::isfinite(value),"Nape binding nonfinite existing local pose");
    }
    for(const auto& matrix:explorer.pose.globalMatrices)for(const auto& row:matrix.values)
        for(float value:row)Require(std::isfinite(value),"Nape binding nonfinite existing global pose");
}
}

void Apply(Explorer& explorer) {
    SourceRig(explorer);
    // 唯一幾何作者為封存的純 V1150 API；這個 TU 單獨使用 /fp:precise。
    auto result=ExplorerNapeHairAuthor::Author(explorer.model,explorer.art);
    Require(result.model.skeletonCompatibilityId==explorer.model.skeletonCompatibilityId&&
        result.model.skeleton.joints.size()==15&&result.model.inverseBindMatrices.size()==15,
        "Nape binding output skeleton compatibility changed");
    for(unsigned i=0;i<15;++i)Require(
        result.model.skeleton.joints[i].parent==explorer.model.skeleton.joints[i].parent&&
        Same(result.model.skeleton.joints[i].bindTransform,explorer.model.skeleton.joints[i].bindTransform)&&
        Same(result.model.inverseBindMatrices[i],explorer.model.inverseBindMatrices[i]),
        "Nape binding output rig raw fields changed");
    static_assert(std::is_nothrow_swappable_v<SkinnedModel3D> &&
        std::is_nothrow_swappable_v<ExplorerArtReceipt>);
    // 先完成全部拒絕檢查，再無失敗點交換；pose/IK/session 與其餘 Explorer 欄位不寫。
    using std::swap;
    swap(explorer.model,result.model);
    swap(explorer.art,result.art);
}
}
