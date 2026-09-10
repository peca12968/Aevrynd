#include "GameplayCandidate7Package3D.h"
#include <iostream>
using namespace Aevrynd;
using namespace GameplayLab;
int main(int argc,char** argv){
    if(argc!=3)return 2;
    auto body=LoadSkinnedModelAssetFileDetailed(argv[1]),lower=LoadSkinnedModelAssetFileDetailed(argv[2]);
    if(!body||!lower)return 3;
    HumanoidSkeletonMap3D map;if(!ResolveCharacterRig3D(body.asset,map))return 4;
    try{
        const auto settings=BuildCandidate7FootProfile3D(body.asset.model,lower.asset.model,map);
        AnimationPose3D bind;for(const auto& j:body.asset.model.skeleton.joints)bind.localTransforms.push_back(j.bindTransform);
        bind.globalMatrices.resize(53);BuildAnimationPoseGlobalMatrices3D(body.asset.model.skeleton,bind);
        for(unsigned side=0;side<2;++side){
            const auto& m=bind.globalMatrices[side?51:47];
            auto angle=[&](Vec3 axis){auto q=m*Vec4{axis.x,axis.y,axis.z,0};return std::acos(Clamp(Normalize(Vec3{q.x,q.y,q.z}).y,-1.f,1.f))*180.f/3.14159265359f;};
            const float old=angle({0,1,0}),corrected=angle(settings.feet[side].soleUpLocal);
            std::cout<<"side="<<side<<" oldAxisErrorDegrees="<<old<<" profileErrorDegrees="<<corrected<<" offsetMeters="<<settings.feet[side].soleOffset<<'\n';
            if(old<100||corrected>2)return 10;
            const auto a=settings.feet[side].soleForwardLocal;const auto q=m*Vec4{a.x,a.y,a.z,0};
            Vec3 expected=TransformPoint(bind.globalMatrices[side?52:48],{})-TransformPoint(m,{});expected.y=0;
            if(Dot(Normalize(Vec3{q.x,q.y,q.z}),Normalize(expected))<.9999f||std::abs(q.y)>1e-4f)return 12;
        }
        unsigned rejected=0;
        // 每個負例均呼叫同一 production helper，不以 Configure 的表面成功冒充有效 bind。
        for(unsigned mutation=0;mutation<6;++mutation){
            auto b=body.asset.model,l=lower.asset.model;
            auto mapping=map;
            if(mutation==0)b.inverseBindMatrices[47][0][0]=std::numeric_limits<float>::quiet_NaN();
            if(mutation==1)b.skeleton.joints[47].bindTransform.scale={0,0,0};
            if(mutation==2)b.skeleton.joints[48].parent=51;
            if(mutation==3)b.skeleton.joints[48].bindTransform.translation={};
            if(mutation==4)for(auto& v:l.bindMesh.vertices)v.y+=1;
            if(mutation==5)for(auto pair:std::array<std::pair<HumanoidJoint3D,HumanoidJoint3D>,3>{{{HumanoidJoint3D::LeftUpperLeg,HumanoidJoint3D::RightUpperLeg},{HumanoidJoint3D::LeftLowerLeg,HumanoidJoint3D::RightLowerLeg},{HumanoidJoint3D::LeftFoot,HumanoidJoint3D::RightFoot}}})std::swap(mapping.joints[std::size_t(pair.first)],mapping.joints[std::size_t(pair.second)]);
            try{(void)BuildCandidate7FootProfile3D(b,l,mapping);}catch(const std::exception&){++rejected;}
        }
        std::cout<<"hostileRejected="<<rejected<<"/6\n";
        return rejected==6?0:11;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 20;}
}
