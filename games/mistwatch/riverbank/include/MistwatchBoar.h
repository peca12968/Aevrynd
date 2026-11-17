#pragma once
#include "MistwatchEncounter.h"
#include <Aevrynd/SkinnedModel3D.h>
#include <Aevrynd/SkeletalIk3D.h>
#include <Aevrynd/HeightField3D.h>
namespace Mistwatch {
// 本作原創四足野豬原型；模型、骨架及姿態均由目前來源重建，不使用舊人偶。
struct Boar {
    Aevrynd::SkinnedModel3D model;
    Aevrynd::AnimationPose3D pose;
    float yaw{},cycle{},displacement{},brace{};
    std::array<Aevrynd::Vec3,4> footTargets{};
    std::array<bool,4> planted{};
    Aevrynd::Vec3 lastFeet{};
    bool initialized{};
    static constexpr std::array<Aevrynd::Vec3,4> restFeet{{
        {-.25f,.075f,.43f},{.25f,.075f,.43f},{-.25f,.075f,-.52f},{.25f,.075f,-.52f}}};
    Boar() : Boar(false) {}
    explicit Boar(bool continuousCoat) {
        using namespace Aevrynd;
        const std::array<Vec3,16> joints{{{0,.62f,0},{0,.75f,.49f},{0,.52f,.94f},
            {-.25f,.57f,.4f},{-.25f,.31f,.28f},restFeet[0],
            {.25f,.57f,.4f},{.25f,.31f,.28f},restFeet[1],
            {-.25f,.57f,-.49f},{-.25f,.31f,-.3f},restFeet[2],
            {.25f,.57f,-.49f},{.25f,.31f,-.3f},restFeet[3],{0,.75f,-.75f}}};
        constexpr int parents[]{-1,0,1,0,3,4,0,6,7,0,9,10,0,12,13,0};
        for(unsigned i=0;i<joints.size();++i) {
            JointTransform3D bind;bind.translation=parents[i]<0?joints[i]:joints[i]-joints[parents[i]];
            model.skeleton.joints.push_back({parents[i],bind});
            model.inverseBindMatrices.push_back(CreateTranslationMatrix(joints[i]*-1.f));
        }
        const auto sphere=Mesh3D::CreateUnitSphere(16,10),cone=Mesh3D::CreateUnitCone(10);
        const auto part=[&](unsigned joint,const Mesh3D& mesh,Vec3 center,Vec3 size,Vec3 rotation,Color color) {
            const auto base=static_cast<std::uint32_t>(model.bindMesh.vertices.size());
            const auto transform=CreateTransformMatrix(center,rotation,size),normals=CreateNormalMatrix(transform);
            for(auto vertex:mesh.vertices) {
                const auto p=TransformPoint(transform,{vertex.x,vertex.y,vertex.z});
                const auto n=UnpackNormalSnorm8(vertex.normal);
                vertex.x=p.x;vertex.y=p.y;vertex.z=p.z;
                vertex.normal=PackNormalSnorm8(TransformNormal(normals,{n.x,n.y,n.z}));
                vertex.color=PackColorRgba8(color);model.bindMesh.vertices.push_back(vertex);
                model.influences.push_back({{static_cast<std::uint16_t>(joint),0,0,0},{65535,0,0,0}});
            }
            for(auto index:mesh.indices)model.bindMesh.indices.push_back(base+index);
        };
        const Color hide{.23f,.15f,.085f,1},shoulder{.18f,.11f,.055f,1},hoof{.055f,.035f,.025f,1};
        // 沿體軸從臀部至鼻尖建立連續斷面，讓肩峰、頸部與口鼻有完整輪廓。
        struct ProfileRing {float z, centerHeight, halfWidth, halfHeight;};
        constexpr std::array profile{
            ProfileRing{-.905f,.633f,.105f,.110f}, ProfileRing{-.855f,.637f,.197f,.201f},
            ProfileRing{-.705f,.626f,.302f,.271f}, ProfileRing{-.500f,.620f,.340f,.309f},
            ProfileRing{-.250f,.634f,.363f,.332f}, ProfileRing{.000f,.670f,.389f,.356f},
            ProfileRing{.220f,.708f,.399f,.377f}, ProfileRing{.390f,.746f,.362f,.362f},
            ProfileRing{.495f,.743f,.297f,.336f}, ProfileRing{.594f,.716f,.255f,.283f},
            ProfileRing{.687f,.682f,.237f,.255f}, ProfileRing{.779f,.644f,.213f,.220f},
            ProfileRing{.871f,.583f,.182f,.181f}, ProfileRing{.963f,.541f,.163f,.151f},
            ProfileRing{1.054f,.505f,.157f,.118f}, ProfileRing{1.130f,.490f,.160f,.111f},
            ProfileRing{1.190f,.493f,.152f,.101f}, ProfileRing{1.217f,.494f,.133f,.082f}};
        constexpr unsigned sides = 36;
        constexpr float tau = 6.28318530718f;
        const auto sample = [&](std::size_t row, float angle) {
            const auto [z, center, width, height] = profile[row];
            const float vertical = std::cos(angle);
            const float narrowing = 1.f - .12f * (std::max)(0.f, -vertical);
            return Vec3{width * narrowing * std::sin(angle), center + height * vertical, z};
        };
        const auto ease = [](float value) {
            const float unit = std::clamp(value, 0.f, 1.f);
            return unit * unit * (3.f - 2.f * unit);
        };
        const auto pigment = [&](Vec3 point, float angle) {
            const float dorsal = (std::max)(0.f, std::cos(angle));
            const float coarse = std::sin(point.z * 29.f + angle * 7.f);
            const float bristle = std::sin(point.z * 211.f + angle * 35.f);
            const float patch = 1.f + .11f * coarse + .045f * bristle;
            const float muzzle = ease((point.z - .965f) / .245f);
            const Color body{(.147f - .049f * dorsal) * patch, (.103f - .039f * dorsal) * patch,
                             (.069f - .027f * dorsal) * patch, 1.f};
            constexpr Color nose{.071f,.050f,.041f,1.f};
            return Color{std::lerp(body.r,nose.r,muzzle),std::lerp(body.g,nose.g,muzzle),
                         std::lerp(body.b,nose.b,muzzle),1.f};
        };
        const auto torsoWeights = [&](float z) -> SkinInfluences3D {
            const auto neck = static_cast<std::uint16_t>(std::lround(65535.f * ease((z - .265f) / .375f)));
            const auto jaw = static_cast<std::uint16_t>(std::lround(float(neck) * ease((z - .865f) / .205f)));
            std::array<std::pair<std::uint16_t, std::uint16_t>, 3> weights{{
                {static_cast<std::uint16_t>(65535u - neck),std::uint16_t{0}},
                {static_cast<std::uint16_t>(neck - jaw),std::uint16_t{1}}, {jaw,std::uint16_t{2}}}};
            std::ranges::sort(weights, [](auto a, auto b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
            SkinInfluences3D result{};
            for (std::size_t index = 0; index < weights.size() && weights[index].first; ++index) {
                result.weights[index] = weights[index].first;
                result.joints[index] = weights[index].second;
            }
            return result;
        };
        const auto appendTorsoVertex = [&](Vec3 point, Vec3 normal, float angle) {
            model.bindMesh.vertices.push_back({point.x,point.y,point.z,PackNormalSnorm8(normal),PackColorRgba8(pigment(point,angle))});
            model.influences.push_back(torsoWeights(point.z));
        };
        if (continuousCoat) {
            for (std::size_t row = 0; row < profile.size(); ++row) {
                const auto before = row ? row - 1 : row;
                const auto after = (std::min)(row + 1, profile.size() - 1);
                for (unsigned side = 0; side < sides; ++side) {
                    const float angle = float(side) * tau / float(sides);
                    const Vec3 around = sample(row, angle + .025f) - sample(row, angle - .025f);
                    const Vec3 along = sample(after, angle) - sample(before, angle);
                    appendTorsoVertex(sample(row, angle), Normalize(Cross(along, around)), angle);
                }
            }
            const auto tailCap = static_cast<std::uint32_t>(model.bindMesh.vertices.size());
            appendTorsoVertex({0,profile.front().centerHeight,profile.front().z},{0,0,-1},0);
            const auto noseCap = tailCap + 1;
            appendTorsoVertex({0,profile.back().centerHeight,profile.back().z},{0,0,1},0);
            const auto endRing = static_cast<std::uint32_t>(profile.size() - 1);
            for (std::uint32_t side = 0; side < sides; ++side) {
                const auto next = (side + 1) % sides;
                model.bindMesh.indices.insert(model.bindMesh.indices.end(),{side,next,tailCap,endRing*sides+side,noseCap,endRing*sides+next});
                for (std::uint32_t ring = 0; ring < endRing; ++ring) {
                    const auto a = ring*sides+side, b=ring*sides+next, c=a+sides, d=b+sides;
                    model.bindMesh.indices.insert(model.bindMesh.indices.end(),{a,c,b,b,c,d});
                }
            }
            for(float sign:{-1.f,1.f}) {
                part(1,cone,{sign*.177f,.913f,.663f},{.144f,.244f,.089f},{.17f,0,-sign*.5f},shoulder);
                part(1,sphere,{sign*.184f,.733f,.806f},{.028f,.031f,.041f},{},{.023f,.015f,.009f,1});
                part(1,sphere,{sign*.197f,.740f,.806f},{.005f,.006f,.008f},{},{.52f,.42f,.32f,1});
                part(2,sphere,{sign*.077f,.515f,1.217f},{.049f,.031f,.010f},{},hoof);
                part(2,cone,{sign*.144f,.480f,1.063f},{.055f,.157f,.065f},{-.29f,0,-sign*.51f},{.59f,.51f,.36f,1});
            }
        } else {
            part(0,sphere,{0,.65f,-.15f},{.74f,.72f,1.48f},{},hide);
            part(0,sphere,{0,.74f,.28f},{.81f,.73f,.77f},{},shoulder);
            part(1,sphere,{0,.68f,.67f},{.53f,.53f,.62f},{.2f,0,0},hide);
            part(2,sphere,{0,.49f,1.01f},{.39f,.28f,.36f},{},hide);
            part(2,sphere,{0,.49f,1.18f},{.35f,.24f,.08f},{},{.105f,.065f,.045f,1});
            for(float sign:{-1.f,1.f}) {
                part(1,cone,{sign*.22f,.91f,.53f},{.2f,.35f,.12f},{.1f,0,-sign*.35f},shoulder);
                part(1,sphere,{sign*.235f,.78f,.82f},{.055f,.055f,.075f},{},{.025f,.015f,.01f,1});
                part(1,sphere,{sign*.257f,.792f,.831f},{.013f,.013f,.02f},{},{.65f,.62f,.52f,1});
                part(2,sphere,{sign*.095f,.515f,1.219f},{.062f,.046f,.014f},{},hoof);
                part(2,cone,{sign*.225f,.5f,1.04f},{.09f,.22f,.085f},{-.45f,0,-sign*.5f},{.7f,.64f,.47f,1});
            }
        }
        for(unsigned leg=0;leg<4;++leg) {
            const unsigned upper=3+leg*3;
            const auto a=joints[upper],b=joints[upper+1],c=joints[upper+2];
            part(upper,sphere,(a+b)*.5f,{.2f,.34f,.22f},{},hide);
            part(upper+1,sphere,(b+c)*.5f,{.13f,.3f,.16f},{},shoulder);
            // 分趾蹄保留兩瓣輪廓，蹄底在模型根部平面以上。
            for(float side:{-1.f,1.f})part(upper+2,sphere,c+Vec3{side*.035f,-.002f,.045f},{.067f,.14f,.19f},{},hoof);
        }
        part(15,cone,{0,.71f,-.87f},{.06f,.28f,.06f},{-1.3f,0,0},shoulder);
        if (continuousCoat) {
            const auto bristleMesh = Mesh3D::CreateUnitCone(5);
            for (std::size_t row = 2; row < 8; ++row) {
                for (int strand = 0; strand < 12; ++strand) {
                    const float sway = std::sin(float(static_cast<int>(row) * 17 + strand * 7) * 3.17f);
                    const float angle = .094f * float(strand % 3 - 1) + .038f * sway;
                    const float along = (float(strand / 3) + .45f + .19f * sway) / 4.f;
                    const auto a = sample(row, angle), b = sample(row + 1, angle);
                    const auto p = a + (b - a) * along;
                    const float height = .059f + .018f * sway - .021f * std::abs(angle);
                    const auto first = model.influences.size();
                    part(0,bristleMesh,p+Vec3{.003f*sway,.012f,0},{.010f,height,.014f},
                         {-.50f,0,.17f*float(strand % 3 - 1)},{.079f,.056f,.041f,1.f});
                    std::fill(model.influences.begin() + first, model.influences.end(), torsoWeights(p.z));
                }
            }
        } else {
            for(unsigned tuft=0;tuft<11;++tuft) {
                const float z=-.65f+float(tuft)*.09f;
                part(0,cone,{0,1.04f,z},{.04f,.12f+.025f*std::sin(float(tuft)),.04f},{-.25f,0,0},hoof);
            }
        }
        model.skeletonCompatibilityId=ComputeStructuralSkeletonCompatibilityId3D(model.skeleton,model.inverseBindMatrices);
        Update({},0,EnemyPhase::Pursuit,0,{});
    }
    bool Update(Aevrynd::Vec3 movement,float dt,EnemyPhase phase,float phaseTime,Aevrynd::Vec3 look) {
        using namespace Aevrynd;
        if(!Session::Finite(movement)||!Session::Finite(look)||!std::isfinite(dt)||dt<0||dt>.1f||
            !std::isfinite(phaseTime)||phaseTime<0||unsigned(phase)>unsigned(EnemyPhase::Defeated))return false;
        displacement=std::hypot(movement.x,movement.z);
        if(displacement>.0001f)cycle=std::fmod(cycle+displacement*6.f,6.2831853f);
        if(std::hypot(look.x,look.z)>.001f)yaw=std::atan2(look.x,look.z);
        brace=phase==EnemyPhase::Windup?std::clamp(1.f-phaseTime/Encounter::WindupDuration,0.f,1.f):0.f;
        pose.localTransforms.clear();
        for(const auto& joint:model.skeleton.joints)pose.localTransforms.push_back(joint.bindTransform);
        pose.localTransforms[0].translation.y-=.025f+.045f*brace;
        pose.localTransforms[1].rotation.x=phase==EnemyPhase::Recovery?-.13f:.12f+.36f*brace;
        pose.localTransforms[1].usesQuaternion=false;
        pose.localTransforms[2].rotation.x=-.1f*brace;
        pose.localTransforms[2].usesQuaternion=false;
        pose.localTransforms[15].rotation.z=.15f*std::sin(cycle);
        pose.localTransforms[15].usesQuaternion=false;
        BuildAnimationPoseGlobalMatrices3D(model.skeleton,pose);
        return true;
    }
    bool PlaceFeet(Aevrynd::Vec3 feet,const Aevrynd::HeightField3D& terrain) {
        using namespace Aevrynd;
        if(!Session::Finite(feet))return false;
        const auto world=CreateTransformMatrix(feet,{0,yaw,0},{1,1,1});
        const auto local=CreateRotationMatrix({0,-yaw,0})*CreateTranslationMatrix(feet*-1.f);
        // 四個真實蹄點估計身體俯仰／側傾；只靠直立軀幹加 IK 會讓坡地下坡腳超出骨長。
        std::array<float,4> ground{};
        for(unsigned leg=0;leg<4;++leg){
            const auto p=TransformPoint(world,restFeet[leg]);
            if(!terrain.Contains(p.x,p.z))return false;
            ground[leg]=terrain.SampleHeight(p.x,p.z);
            if(!std::isfinite(ground[leg]))return false;
        }
        const float front=(ground[0]+ground[1])*.5f,back=(ground[2]+ground[3])*.5f;
        const float left=(ground[0]+ground[2])*.5f,right=(ground[1]+ground[3])*.5f;
        auto& root=pose.localTransforms[0];
        root.rotation={-std::clamp(std::atan2(front-back,.95f),-.5f,.5f),0,
            std::clamp(std::atan2(right-left,.5f),-.5f,.5f)};
        root.usesQuaternion=false;
        BuildAnimationPoseGlobalMatrices3D(model.skeleton,pose);
        if(!initialized || Length(feet-lastFeet)>1.f){planted={};initialized=true;}
        lastFeet=feet;
        for(unsigned leg=0;leg<4;++leg) {
            // 前左／後右為同一對角支撐組，另兩足相差半個步態週期。
            const float phase=std::fmod(cycle+((leg==1||leg==2)?3.14159265f:0.f),6.2831853f);
            const bool moving=displacement>.0001f,support=!moving||phase<3.14159265f;
            const auto rest=TransformPoint(world,restFeet[leg]);
            if(support) {
                if(!planted[leg]||std::hypot(footTargets[leg].x-rest.x,footTargets[leg].z-rest.z)>.23f) {
                    footTargets[leg]=TransformPoint(world,restFeet[leg]+Vec3{0,0,moving?.13f:0.f});
                    planted[leg]=true;
                }
            } else {
                const float t=(phase-3.14159265f)/3.14159265f;
                planted[leg]=false;
                footTargets[leg]=TransformPoint(world,restFeet[leg]+Vec3{0,0,-.13f+.26f*t});
            }
            footTargets[leg].y=terrain.SampleHeight(footTargets[leg].x,footTargets[leg].z)+restFeet[leg].y+
                (support?0.f:.11f*std::sin(phase-3.14159265f));
        }
        // 根部仍由碰撞／地形決定；僅在骨長容許區間內調整軀幹高度，不能把 Clamped 當作接地完成。
        float heightLower=-.15f,heightUpper=.15f;
        for(unsigned leg=0;leg<4;++leg){
            const unsigned hip=3+leg*3;
            const auto target=TransformPoint(local,footTargets[leg]);
            const auto position=TransformPoint(pose.globalMatrices[hip],{});
            const float reach=Length(model.skeleton.joints[hip+1].bindTransform.translation)+
                Length(model.skeleton.joints[hip+2].bindTransform.translation)-.0001f;
            const float dx=target.x-position.x,dz=target.z-position.z;
            const float squared=reach*reach-dx*dx-dz*dz;
            if(!std::isfinite(squared)||squared<=0)return false;
            const float vertical=std::sqrt(squared),difference=target.y-position.y;
            heightLower=(std::max)(heightLower,difference-vertical);heightUpper=(std::min)(heightUpper,difference+vertical);
        }
        if(heightLower>heightUpper)return false;
        root.translation.y+=std::clamp(0.f,heightLower,heightUpper);
        BuildAnimationPoseGlobalMatrices3D(model.skeleton,pose);
        for(unsigned leg=0;leg<4;++leg){
            const unsigned upper=3+leg*3;
            // 前肢肘關節向後、後肢膝關節向前；共用引擎兩骨 IK，不直接移動蒙皮頂點。
            const Vec3 pole{restFeet[leg].x,.32f,leg<2?-.2f:.2f};
            if(!SolveTwoBoneIk3D(model.skeleton,{upper,upper+1,upper+2},
                {TransformPoint(local,footTargets[leg]),pole,1,0},pose))return false;
            const auto a=pose.localTransforms[upper].quaternion,b=pose.localTransforms[upper+1].quaternion;
            if(pose.localTransforms[upper].usesQuaternion&&pose.localTransforms[upper+1].usesQuaternion) {
                const Quaternion3D legs{
                    a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
                    a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
                const auto r=QuaternionFromEuler3D(root.rotation);
                const Quaternion3D product{
                    r.w*legs.x+r.x*legs.w+r.y*legs.z-r.z*legs.y,r.w*legs.y-r.x*legs.z+r.y*legs.w+r.z*legs.x,
                    r.w*legs.z+r.x*legs.y-r.y*legs.x+r.z*legs.w,r.w*legs.w-r.x*legs.x-r.y*legs.y-r.z*legs.z};
                auto& ankle=pose.localTransforms[upper+2];
                ankle.usesQuaternion=true;ankle.quaternion={-product.x,-product.y,-product.z,product.w};ankle.rotation={};
            }
        }
        BuildAnimationPoseGlobalMatrices3D(model.skeleton,pose);
        return true;
    }
};
}
