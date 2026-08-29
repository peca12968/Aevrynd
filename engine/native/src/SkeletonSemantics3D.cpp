#include <Aevrynd/SkeletonSemantics3D.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

namespace Aevrynd { namespace {
using AliasList=std::span<const std::string_view>;
constexpr std::array<std::string_view,1> RootNames{"root"};
constexpr std::array<std::string_view,2> HipsNames{"hips","pelvis"};
constexpr std::array<std::string_view,2> SpineNames{"spine","spine1"};
constexpr std::array<std::string_view,1> HeadNames{"head"};
constexpr std::array<std::string_view,2> LeftUpperLegNames{"leftupperleg","leftupleg"};
constexpr std::array<std::string_view,2> LeftLowerLegNames{"leftlowerleg","leftleg"};
constexpr std::array<std::string_view,2> LeftFootNames{"leftfoot","leftankle"};
constexpr std::array<std::string_view,2> RightUpperLegNames{"rightupperleg","rightupleg"};
constexpr std::array<std::string_view,2> RightLowerLegNames{"rightlowerleg","rightleg"};
constexpr std::array<std::string_view,2> RightFootNames{"rightfoot","rightankle"};
constexpr std::array<std::string_view,2> LeftUpperArmNames{"leftupperarm","leftarm"};
constexpr std::array<std::string_view,2> LeftLowerArmNames{"leftlowerarm","leftforearm"};
constexpr std::array<std::string_view,2> LeftHandNames{"lefthand","leftwrist"};
constexpr std::array<std::string_view,2> RightUpperArmNames{"rightupperarm","rightarm"};
constexpr std::array<std::string_view,2> RightLowerArmNames{"rightlowerarm","rightforearm"};
constexpr std::array<std::string_view,2> RightHandNames{"righthand","rightwrist"};
const std::array<AliasList,std::size_t(HumanoidJoint3D::Count)> Aliases{
    RootNames,HipsNames,SpineNames,HeadNames,LeftUpperLegNames,LeftLowerLegNames,LeftFootNames,
    RightUpperLegNames,RightLowerLegNames,RightFootNames,LeftUpperArmNames,LeftLowerArmNames,
    LeftHandNames,RightUpperArmNames,RightLowerArmNames,RightHandNames};

std::string Canonical(std::string_view value){
    const std::size_t delimiter=value.find_last_of(":|/");if(delimiter!=std::string_view::npos)value.remove_prefix(delimiter+1);
    std::string result;result.reserve(value.size());
    for(unsigned char character:value)if(std::isalnum(character))result.push_back(char(std::tolower(character)));
    return result;
}
bool IsDescendant(const Skeleton3D& skeleton,std::size_t child,std::size_t ancestor)noexcept{
    for(std::size_t count=0;count<skeleton.joints.size()&&child<skeleton.joints.size();++count){
        const int parent=skeleton.joints[child].parent;if(parent<0)return false;if(std::size_t(parent)==ancestor)return true;child=std::size_t(parent);
    }return false;
}
bool Direct(const Skeleton3D& skeleton,std::size_t child,std::size_t parent)noexcept{return child<skeleton.joints.size()&&skeleton.joints[child].parent==int(parent);}
bool Topology(const Skeleton3D& skeleton,const HumanoidSkeletonMap3D& map)noexcept{
    const auto j=[&](HumanoidJoint3D value){return map.Joint(value);};
    return IsDescendant(skeleton,j(HumanoidJoint3D::Hips),j(HumanoidJoint3D::Root))&&
        IsDescendant(skeleton,j(HumanoidJoint3D::Spine),j(HumanoidJoint3D::Hips))&&
        IsDescendant(skeleton,j(HumanoidJoint3D::Head),j(HumanoidJoint3D::Spine))&&
        IsDescendant(skeleton,j(HumanoidJoint3D::LeftUpperLeg),j(HumanoidJoint3D::Hips))&&
        Direct(skeleton,j(HumanoidJoint3D::LeftLowerLeg),j(HumanoidJoint3D::LeftUpperLeg))&&
        Direct(skeleton,j(HumanoidJoint3D::LeftFoot),j(HumanoidJoint3D::LeftLowerLeg))&&
        IsDescendant(skeleton,j(HumanoidJoint3D::RightUpperLeg),j(HumanoidJoint3D::Hips))&&
        Direct(skeleton,j(HumanoidJoint3D::RightLowerLeg),j(HumanoidJoint3D::RightUpperLeg))&&
        Direct(skeleton,j(HumanoidJoint3D::RightFoot),j(HumanoidJoint3D::RightLowerLeg))&&
        IsDescendant(skeleton,j(HumanoidJoint3D::LeftUpperArm),j(HumanoidJoint3D::Spine))&&
        Direct(skeleton,j(HumanoidJoint3D::LeftLowerArm),j(HumanoidJoint3D::LeftUpperArm))&&
        Direct(skeleton,j(HumanoidJoint3D::LeftHand),j(HumanoidJoint3D::LeftLowerArm))&&
        IsDescendant(skeleton,j(HumanoidJoint3D::RightUpperArm),j(HumanoidJoint3D::Spine))&&
        Direct(skeleton,j(HumanoidJoint3D::RightLowerArm),j(HumanoidJoint3D::RightUpperArm))&&
        Direct(skeleton,j(HumanoidJoint3D::RightHand),j(HumanoidJoint3D::RightLowerArm));
}
}

bool HumanoidSkeletonMap3D::Valid(const Skeleton3D& skeleton,SkeletonCompatibilityId3D expected)const noexcept{
    if(!compatibility||compatibility!=expected||!skeleton.Valid())return false;
    std::array<bool,256> used{};if(skeleton.joints.size()>used.size())return false;
    for(const std::uint16_t joint:joints)if(joint==InvalidJoint||joint>=skeleton.joints.size()||used[joint])return false;else used[joint]=true;
    return Topology(skeleton,*this);
}

bool ResolveHumanoidSkeletonMap3D(std::span<const std::string> names,const SkinnedModel3D& model,
    SkeletonCompatibilityId3D expectedCompatibility,HumanoidSkeletonMap3D& output,SkeletonSemanticError3D* error)noexcept{
    auto fail=[&](SkeletonSemanticError3D value){if(error)*error=value;return false;};
    const Skeleton3D& skeleton=model.skeleton;
    if(!skeleton.Valid()||skeleton.joints.size()>256)return fail(SkeletonSemanticError3D::InvalidSkeleton);
    if(!expectedCompatibility||!model.skeletonCompatibilityId)return fail(SkeletonSemanticError3D::UnknownCompatibility);
    if(names.size()!=skeleton.joints.size())return fail(SkeletonSemanticError3D::NameCountMismatch);
    if(model.inverseBindMatrices.size()!=skeleton.joints.size()||model.skeletonCompatibilityId!=expectedCompatibility||
        ComputeSkeletonCompatibilityId3D(names,skeleton,model.inverseBindMatrices)!=expectedCompatibility)return fail(SkeletonSemanticError3D::CompatibilityMismatch);
    HumanoidSkeletonMap3D candidate;candidate.compatibility=expectedCompatibility;candidate.joints.fill(HumanoidSkeletonMap3D::InvalidJoint);
    try{
        for(std::size_t semantic=0;semantic<Aliases.size();++semantic){
            std::size_t match=names.size();
            for(std::size_t joint=0;joint<names.size();++joint){const std::string canonical=Canonical(names[joint]);if(std::find(Aliases[semantic].begin(),Aliases[semantic].end(),canonical)==Aliases[semantic].end())continue;if(match!=names.size())return fail(SkeletonSemanticError3D::DuplicateJoint);match=joint;}
            if(match==names.size())return fail(SkeletonSemanticError3D::MissingJoint);candidate.joints[semantic]=std::uint16_t(match);
        }
    }catch(...){return fail(SkeletonSemanticError3D::InvalidSkeleton);}
    if(!candidate.Valid(skeleton,expectedCompatibility))return fail(SkeletonSemanticError3D::InvalidTopology);
    output=candidate;if(error)*error=SkeletonSemanticError3D::None;return true;
}
}
