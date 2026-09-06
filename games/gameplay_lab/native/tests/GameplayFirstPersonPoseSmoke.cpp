#include "GameplayFirstPersonPose3D.h"
#include "GameplayClipSegmentVisibility3D.h"
#include <Aevrynd/FirstPersonArmsSkinnedMesh3D.h>
#include <Aevrynd/SkinnedModelAsset.h>
#include <filesystem>
#include <iostream>
#include <limits>
#include <algorithm>
#include <cmath>
#include <string_view>
#include <source_location>

using namespace Aevrynd;
using namespace GameplayLab;
namespace {
std::uint64_t Hash(const AnimationPose3D& pose){
    std::uint64_t hash=14695981039346656037ull;
    for(const auto& matrix:pose.globalMatrices)for(const auto& row:matrix.values)for(float value:row){
        const auto* bytes=reinterpret_cast<const unsigned char*>(&value);
        for(std::size_t index=0;index<sizeof(value);++index){hash^=bytes[index];hash*=1099511628211ull;}
    }
    return hash;
}
struct SearchCandidate {Vec3 shoulder{},grip{};unsigned id{};bool probe{};};
bool Check(const std::filesystem::path& path,const SearchCandidate* candidate=nullptr){
    unsigned diagnosticState{};float diagnosticPhase{},diagnosticBlend{};
    const auto fail=[&](const std::source_location location=std::source_location::current()){
        std::cerr<<"FIRST_DIVERGENCE asset="<<path.filename().string()<<" line="<<location.line()
                 <<" state="<<diagnosticState<<" phase="<<diagnosticPhase<<" blend="<<diagnosticBlend;
        if(candidate)std::cerr<<" candidate="<<candidate->id<<" probe="<<candidate->probe;
        std::cerr<<'\n';return false;
    };
    const auto loaded=LoadSkinnedModelAssetFileDetailed(path);if(!loaded){std::cerr<<"ASSET_DIVERGENCE "<<loaded.error<<'\n';return fail();}
    GameplayAnimation3D animation;if(!animation.Initialize(loaded.asset))return fail();
    CharacterEquipmentAsset3D sword;std::string swordError;
    if(!CreateCharacterEquipmentAsset3D(loaded.asset,animation.Rig(),CharacterEquipmentShape3D::Sword,sword,&swordError)){
        std::cerr<<"SWORD_DIVERGENCE "<<swordError<<'\n';return fail();
    }
    const auto arms=BuildFirstPersonArmsSkinnedMesh3D(loaded.asset);if(!arms)return fail();
    unsigned maximumVisibleKnownCutEdges{},maximumVisibleRetainedBoundaries{},maximumVisibleSameDirectionSeams{};
    GameplayFirstPersonPoseWorkspace3D workspace;GameplayFirstPersonPoseOutput3D result;
    std::array<Vec3,2> shoulderReference{};bool haveReference=false;
    float minimumCoverage=std::numeric_limits<float>::max(),maximumCoverage=0.f;
    float minimumTipAboveGrip=std::numeric_limits<float>::max();
    float minimumBladeViewAngle=std::numeric_limits<float>::max(),maximumRightArmReach=0.f;
    float minimumRightArmExtensionMargin=std::numeric_limits<float>::max();
    unsigned projectionSamples{},projectionLayoutFailures{},armsLayoutFailures{};
    float observedArmsMinX{},observedArmsMinY{},observedArmsMaxX{},observedArmsMaxY{};
    for(const auto state:{AnimationState3D::Idle,AnimationState3D::Walk,AnimationState3D::Run,AnimationState3D::Attack1,AnimationState3D::Attack2,AnimationState3D::Attack3}){
        for(float phase:{0.f,.1f,.25f,.5f,.75f,.9f,1.f})for(float blend:{0.f,.5f,1.f}){
            if(candidate&&candidate->probe&&(state!=AnimationState3D::Attack1||phase!=.5f||blend!=1.f))continue;
            diagnosticState=unsigned(state);diagnosticPhase=phase;diagnosticBlend=blend;
            animation.SetState(state,true);animation.SynchronizeNormalizedPhase(phase);AnimationPose3D source;
            if(!animation.EvaluateInto(source))return fail();const auto sourceHash=Hash(source);
            GameplayFirstPersonPoseInput3D input;input.character=&loaded.asset.model;input.rig=&animation.Rig();input.sourcePose=&source;input.sword=&sword.swordGeometry;
            input.state=state;input.phase=phase;input.enterBlend=blend;input.camera=MakeFirstPersonCamera({2,1,3},.6f,-.1f);
            if(candidate){input.shoulderCenter=candidate->shoulder;input.readyGrip=candidate->grip;}
            const auto status=EvaluateGameplayFirstPersonPose3D(input,workspace,result);
            if(status!=GameplayFirstPersonPoseStatus3D::Ready||!result.valid||result.palmContactVerified||result.maximumBoneLengthError>.0001f||result.gripError>.0001f||Hash(source)!=sourceHash){
                auto& failureOut=candidate?std::cout:std::cerr;
                failureOut<<"FIRST_PERSON_POSE failed asset="<<path.filename().string()<<" state="<<unsigned(state)<<" phase="<<phase<<" blend="<<blend<<" status="<<unsigned(status);
                if(candidate)failureOut<<" candidate="<<candidate->id<<" probe="<<candidate->probe<<" outputPreservedFromPriorSample=1";
                failureOut<<" grip="<<result.gripError<<'\n';
                // 失敗時記錄實際骨鏈，區分不可達與投影問題，不改動求解門檻。
                for(const auto joint:{HumanoidJoint3D::RightUpperArm,HumanoidJoint3D::RightLowerArm,HumanoidJoint3D::RightHand}){
                    const auto index=animation.Rig().Joint(joint);
                    if(index<workspace.scratch.pose.globalMatrices.size()){
                        const Vec3 position=TransformPoint(workspace.scratch.pose.globalMatrices[index],{});
                        failureOut<<"POSE_REACH joint="<<unsigned(joint)<<" position="<<position.x<<','<<position.y<<','<<position.z<<'\n';
                    }
                }
                return fail();
            }
            if(!haveReference){shoulderReference=result.shouldersCamera;haveReference=true;}
            for(std::size_t side=0;side<2;++side)if(Length(result.shouldersCamera[side]-shoulderReference[side])>.0001f)return fail();
            const auto lowerJoint=animation.Rig().Joint(HumanoidJoint3D::RightLowerArm),handJoint=animation.Rig().Joint(HumanoidJoint3D::RightHand);
            const float upperLength=Length(loaded.asset.model.skeleton.joints[lowerJoint].bindTransform.translation);
            const float lowerLength=Length(loaded.asset.model.skeleton.joints[handJoint].bindTransform.translation);
            const float rightArmReach=Length(result.wristsCamera[1]-result.shouldersCamera[1]);
            const float extensionMargin=upperLength+lowerLength-rightArmReach;
            if(!std::isfinite(rightArmReach)||!std::isfinite(extensionMargin)||extensionMargin<=.001f||
               rightArmReach<=std::abs(upperLength-lowerLength)+.001f)return fail();
            maximumRightArmReach=(std::max)(maximumRightArmReach,rightArmReach);
            minimumRightArmExtensionMargin=(std::min)(minimumRightArmExtensionMargin,extensionMargin);
            SkinningWorkspace3D skinWorkspace;Mesh3D weaponMesh;if(!SkinModelInto(sword.model,result.pose,skinWorkspace,weaponMesh))return fail();
            // 依實際皮革 submesh 索引範圍收集握柄頂點，不依賴特定幾何版本的頂點排列。
            std::vector<std::uint32_t> gripVertices;
            for(const auto& submesh:sword.submeshes){
                if(submesh.materialSlot!=1)continue;
                if(!submesh.indexCount||submesh.firstIndex>weaponMesh.indices.size()||
                   submesh.indexCount>weaponMesh.indices.size()-submesh.firstIndex)return fail();
                for(std::size_t offset=0;offset<submesh.indexCount;++offset){
                    const auto index=weaponMesh.indices[submesh.firstIndex+offset];
                    if(index>=weaponMesh.vertices.size())return fail();
                    if(std::find(gripVertices.begin(),gripVertices.end(),index)==gripVertices.end())gripVertices.push_back(index);
                }
            }
            if(gripVertices.empty())return fail();
            Vec3 center{};for(const auto index:gripVertices){const auto& vertex=weaponMesh.vertices[index];center=center+Vec3{vertex.x,vertex.y,vertex.z};}
            center=center*(1.f/static_cast<float>(gripVertices.size()));
            const Vec3 renderedGrip=TransformPoint(result.modelToWorld,center);
            const Vec3 cameraGrip=TransformPoint(CreateViewMatrix(input.camera),renderedGrip);
            if(Length(cameraGrip-result.targetGripCamera)>.0001f){
                std::cerr<<"GRIP_DIVERGENCE uniqueVertices="<<gripVertices.size()<<" meshVertices="<<weaponMesh.vertices.size()
                         <<" error="<<Length(cameraGrip-result.targetGripCamera)<<" actual="<<cameraGrip.x<<','<<cameraGrip.y<<','<<cameraGrip.z
                         <<" target="<<result.targetGripCamera.x<<','<<result.targetGripCamera.y<<','<<result.targetGripCamera.z<<'\n';
                return fail();
            }
            // 獨立投影真正已蒙皮且由索引引用的頂點；包圍矩形不是像素遮罩覆蓋率。
            const Matrix4 viewProjection=CreateViewProjectionMatrix(input.camera,1280.f/720.f);
            // 僅診斷已追溯切口；拓樸不可用時不可把零相交數宣稱為完整手臂驗收。
            SkinningWorkspace3D armsWorkspace;Mesh3D actualArms;
            if(!SkinModelInto(arms.model,result.pose,armsWorkspace,actualArms))return fail();
            if(candidate){
                float armsMinX=1280.f,armsMinY=720.f,armsMaxX=0.f,armsMaxY=0.f;
                for(const auto& vertex:actualArms.vertices){
                    const Vec3 world=TransformPoint(result.modelToWorld,{vertex.x,vertex.y,vertex.z});
                    const Vec4 clip=viewProjection*Vec4{world.x,world.y,world.z,1.f};
                    if(!std::isfinite(clip.x)||!std::isfinite(clip.y)||!std::isfinite(clip.w)||clip.w<=.00001f){++armsLayoutFailures;continue;}
                    const Vec2 pixel{(clip.x/clip.w*.5f+.5f)*1280.f,(.5f-clip.y/clip.w*.5f)*720.f};
                    armsMinX=(std::min)(armsMinX,pixel.x);armsMinY=(std::min)(armsMinY,pixel.y);
                    armsMaxX=(std::max)(armsMaxX,pixel.x);armsMaxY=(std::max)(armsMaxY,pixel.y);
                }
                const bool armsLayoutPass=armsMinX>=16.f&&armsMaxX<=1264.f&&armsMinY>=16.f&&armsMaxY<=704.f;
                observedArmsMinX=armsMinX;observedArmsMinY=armsMinY;observedArmsMaxX=armsMaxX;observedArmsMaxY=armsMaxY;
                if(!armsLayoutPass)++armsLayoutFailures;
            }
            const auto edgeVisibility=[&](const std::array<std::uint32_t,2>& compactVertices){
                std::array<Vec4,2> clip{};
                for(std::size_t endpoint=0;endpoint<2;++endpoint){
                    if(compactVertices[endpoint]>=actualArms.vertices.size())return GameplayClipSegmentVisibility3D::Invalid;
                    const auto& vertex=actualArms.vertices[compactVertices[endpoint]];
                    const Vec3 world=TransformPoint(result.modelToWorld,{vertex.x,vertex.y,vertex.z});
                    clip[endpoint]=viewProjection*Vec4{world.x,world.y,world.z,1.f};
                }
                return ClassifyGameplayClipSegmentVisibility3D(clip[0],clip[1]);
            };
            unsigned visibleKnownCutEdges{},visibleRetainedBoundaries{},visibleSameDirectionSeams{};
            for(const auto& edge:arms.cutTopology.edges){
                const auto visibility=edgeVisibility(edge.compactVertices);
                if(visibility==GameplayClipSegmentVisibility3D::Invalid)return fail();
                if(visibility==GameplayClipSegmentVisibility3D::Intersects)++visibleKnownCutEdges;
            }
            // 原始保留邊界與接縫分開量測；不能把切口零相交當作完整清單零相交。
            for(const auto& edge:arms.cutTopology.retainedBoundaries){
                const auto visibility=edgeVisibility(edge.compactVertices);
                if(visibility==GameplayClipSegmentVisibility3D::Invalid)return fail();
                if(visibility==GameplayClipSegmentVisibility3D::Intersects)++visibleRetainedBoundaries;
            }
            for(const auto& seam:arms.cutTopology.retainedSameDirectionSeams){
                bool visible=false;
                for(const auto& edge:seam.edges){
                    const auto visibility=edgeVisibility(edge.compactVertices);
                    if(visibility==GameplayClipSegmentVisibility3D::Invalid)return fail();
                    visible=visible||visibility==GameplayClipSegmentVisibility3D::Intersects;
                }
                if(visible)++visibleSameDirectionSeams;
            }
            maximumVisibleKnownCutEdges=(std::max)(maximumVisibleKnownCutEdges,visibleKnownCutEdges);
            maximumVisibleRetainedBoundaries=(std::max)(maximumVisibleRetainedBoundaries,visibleRetainedBoundaries);
            maximumVisibleSameDirectionSeams=(std::max)(maximumVisibleSameDirectionSeams,visibleSameDirectionSeams);
            const auto project=[&](Vec3 world,Vec2& pixel){
                const Vec4 clip=viewProjection*Vec4{world.x,world.y,world.z,1.f};
                if(!std::isfinite(clip.x)||!std::isfinite(clip.y)||!std::isfinite(clip.z)||!std::isfinite(clip.w)||clip.w<=.00001f)return fail();
                pixel={(clip.x/clip.w*.5f+.5f)*1280.f,(.5f-clip.y/clip.w*.5f)*720.f};
                return std::isfinite(pixel.x)&&std::isfinite(pixel.y);
            };
            float minX=std::numeric_limits<float>::max(),minY=minX,maxX=-minX,maxY=-minX,minDepth=minX;
            for(const auto index:weaponMesh.indices){
                if(index>=weaponMesh.vertices.size())return fail();
                const auto& vertex=weaponMesh.vertices[index];Vec2 pixel{};
                const Vec3 world=TransformPoint(result.modelToWorld,{vertex.x,vertex.y,vertex.z});
                if(!project(world,pixel))return fail();
                const float depth=TransformPoint(CreateViewMatrix(input.camera),world).z;
                if(!std::isfinite(depth))return fail();minDepth=(std::min)(minDepth,depth);
                minX=(std::min)(minX,pixel.x);maxX=(std::max)(maxX,pixel.x);
                minY=(std::min)(minY,pixel.y);maxY=(std::max)(maxY,pixel.y);
            }
            Vec3 actualTip{};
            for(const auto index:sword.swordGeometry.tipVertexIndices){
                if(index>=weaponMesh.vertices.size()||std::find(weaponMesh.indices.begin(),weaponMesh.indices.end(),index)==weaponMesh.indices.end())return fail();
                const auto& vertex=weaponMesh.vertices[index];actualTip=actualTip+Vec3{vertex.x,vertex.y,vertex.z};
            }
            actualTip=actualTip*(1.f/static_cast<float>(sword.swordGeometry.tipVertexIndices.size()));
            Vec2 gripPixel{},tipPixel{};
            if(!project(renderedGrip,gripPixel)||!project(TransformPoint(result.modelToWorld,actualTip),tipPixel))return fail();
            const float coverage=(maxX-minX)*(maxY-minY)/(1280.f*720.f),tipAboveGrip=gripPixel.y-tipPixel.y;
            const Vec3 renderedTip=TransformPoint(result.modelToWorld,actualTip);
            const Vec3 bladeVector=renderedTip-renderedGrip,viewVector=renderedGrip-input.camera.position;
            if(Length(bladeVector)<.0001f||Length(viewVector)<.0001f)return fail();
            const float bladeViewAngle=std::acos(Clamp(Dot(Normalize(bladeVector),Normalize(viewVector)),-1.f,1.f))*180.f/3.14159265358979323846f;
            if(!std::isfinite(coverage)||!std::isfinite(tipAboveGrip)||!std::isfinite(bladeViewAngle))return fail();
            minimumCoverage=(std::min)(minimumCoverage,coverage);maximumCoverage=(std::max)(maximumCoverage,coverage);
            minimumTipAboveGrip=(std::min)(minimumTipAboveGrip,tipAboveGrip);++projectionSamples;
            minimumBladeViewAngle=(std::min)(minimumBladeViewAngle,bladeViewAngle);
            const bool hudClear=!(minY<66.f&&maxY>0.f)&&!(minX<372.f&&maxX>0.f&&minY<720.f&&maxY>470.f)&&
                                !(minX<1272.f&&maxX>778.f&&minY<638.f&&maxY>512.f);
            const bool layoutMeasuredPass=coverage>=.02f&&coverage<=.06f&&tipAboveGrip>80.f&&
                minX>=16.f&&maxX<=1264.f&&minY>=16.f&&maxY<=704.f&&hudClear&&
                !(minX<=640.f&&maxX>=640.f&&minY<=360.f&&maxY>=360.f)&&
                gripPixel.x>=1040.f&&gripPixel.x<=1264.f&&gripPixel.y>=80.f&&gripPixel.y<=510.f&&
                minDepth>input.camera.projection.nearPlane+.015f&&tipAboveGrip>=140.f&&bladeViewAngle>=20.f;
            if(!layoutMeasuredPass)++projectionLayoutFailures;
            // CPU 真實武器投影違反門檻即失敗；仍不宣稱 GPU 像素或完整雙臂構圖通過。
            if(!candidate)std::cout<<"FIRST_PERSON_POSE_PROJECTION diagnosticOnly=1 asset="<<path.filename().string()<<" state="<<unsigned(state)
                     <<" phase="<<phase<<" blend="<<blend<<" bboxCoverage="<<coverage<<" tipAboveGripPixels="<<tipAboveGrip
                     <<" bladeViewAngleDegrees="<<bladeViewAngle<<" rightArmReach="<<rightArmReach<<" extensionMargin="<<extensionMargin
                     <<" grip="<<gripPixel.x<<','<<gripPixel.y<<" tip="<<tipPixel.x<<','<<tipPixel.y
                     <<" bbox="<<minX<<','<<minY<<','<<maxX<<','<<maxY<<" existingLayoutThresholdsMet="<<layoutMeasuredPass<<'\n';
            if(phase<1.f){
                auto adjacentInput=input;adjacentInput.phase=phase+.001f;
                GameplayFirstPersonPoseOutput3D adjacent;
                if(EvaluateGameplayFirstPersonPose3D(adjacentInput,workspace,adjacent)!=GameplayFirstPersonPoseStatus3D::Ready||
                   Length(adjacent.actualGripCamera-result.actualGripCamera)>.002f||
                   Length(adjacent.wristsCamera[0]-result.wristsCamera[0])>.002f)return fail();
            }
            const auto previousHash=Hash(result.pose);const auto previousRoot=result.modelToWorld;
            input.readyGrip={20,20,20};
            if(EvaluateGameplayFirstPersonPose3D(input,workspace,result)!=GameplayFirstPersonPoseStatus3D::Unreachable||Hash(result.pose)!=previousHash)return fail();
            for(std::size_t row=0;row<4;++row)for(std::size_t col=0;col<4;++col)if(result.modelToWorld[row][col]!=previousRoot[row][col])return fail();
            input.readyGrip={.27f,-.13f,.30f};input.phase=std::numeric_limits<float>::quiet_NaN();
            if(EvaluateGameplayFirstPersonPose3D(input,workspace,result)!=GameplayFirstPersonPoseStatus3D::InvalidInput||Hash(result.pose)!=previousHash)return fail();
            input.phase=phase;
            for(const auto badRotation:{Quaternion3D{std::numeric_limits<float>::quiet_NaN(),0,0,1},
                                         Quaternion3D{0,std::numeric_limits<float>::infinity(),0,1},Quaternion3D{0,0,0,0}}){
                auto malformedSource=source;malformedSource.localTransforms.front().usesQuaternion=true;malformedSource.localTransforms.front().quaternion=badRotation;
                input.sourcePose=&malformedSource;
                if(EvaluateGameplayFirstPersonPose3D(input,workspace,result)!=GameplayFirstPersonPoseStatus3D::InvalidInput||Hash(result.pose)!=previousHash)return fail();
                for(std::size_t row=0;row<4;++row)for(std::size_t col=0;col<4;++col)if(result.modelToWorld[row][col]!=previousRoot[row][col])return fail();
            }
            auto malformedEuler=source;malformedEuler.localTransforms.front().usesQuaternion=false;
            malformedEuler.localTransforms.front().rotation.z=std::numeric_limits<float>::infinity();input.sourcePose=&malformedEuler;
            if(EvaluateGameplayFirstPersonPose3D(input,workspace,result)!=GameplayFirstPersonPoseStatus3D::InvalidInput||Hash(result.pose)!=previousHash)return fail();
        }
    }
    if(candidate){
        const bool passed=projectionSamples==(candidate->probe?1u:126u)&&projectionLayoutFailures==0&&armsLayoutFailures==0&&maximumVisibleKnownCutEdges==0;
        // 搜尋只是 CPU 候選證據；保留原始邊界與接縫數，絕不寫回正式姿態或宣稱 GPU 通過。
        std::cout<<"POSE_SEARCH diagnosticOnly=1 candidate="<<candidate->id<<" probe="<<candidate->probe<<" asset="<<path.filename().string()
                 <<" shoulder="<<candidate->shoulder.x<<','<<candidate->shoulder.y<<','<<candidate->shoulder.z
                 <<" grip="<<candidate->grip.x<<','<<candidate->grip.y<<','<<candidate->grip.z
                 <<" samples="<<projectionSamples<<" knownCutMax="<<maximumVisibleKnownCutEdges
                 <<" retainedBoundaryMax="<<maximumVisibleRetainedBoundaries<<" seamMax="<<maximumVisibleSameDirectionSeams
                 <<" coverageMin="<<minimumCoverage<<" coverageMax="<<maximumCoverage<<" tipRiseMin="<<minimumTipAboveGrip
                 <<" angleMin="<<minimumBladeViewAngle<<" reachMax="<<maximumRightArmReach<<" extensionMarginMin="<<minimumRightArmExtensionMargin
                 <<" layoutFailures="<<projectionLayoutFailures<<" armsLayoutFailures="<<armsLayoutFailures
                 <<" arms="<<observedArmsMinX<<','<<observedArmsMinY<<'-'<<observedArmsMaxX<<','<<observedArmsMaxY
                 <<" candidatePassed="<<passed<<" fullArmsGatePassed="<<(armsLayoutFailures==0)<<"\n";
        return passed;
    }
    std::cout<<"FIRST_PERSON_POSE asset="<<path.filename().string()<<" samples=126 boneLengthPreserved=1 shoulderStable=1 actualGripMatched=1 sourceUnchanged=1 palmContactVerified=0\n";
    std::cout<<"FIRST_PERSON_POSE_PROJECTION_SUMMARY diagnosticOnly=1 asset="<<path.filename().string()<<" samples="<<projectionSamples
             <<" minimumBboxCoverage="<<minimumCoverage<<" maximumBboxCoverage="<<maximumCoverage
             <<" minimumTipAboveGripPixels="<<minimumTipAboveGrip<<" layoutFailures="<<projectionLayoutFailures
             <<" minimumBladeViewAngleDegrees="<<minimumBladeViewAngle<<" maximumRightArmReach="<<maximumRightArmReach
             <<" minimumRightArmExtensionMargin="<<minimumRightArmExtensionMargin
             <<" coverageGateMin=0.02 coverageGateMax=0.06 tipGatePixels=80 honestTipGatePixels=140 honestAngleGateDegrees=20 pixelMaskMeasured=0\n";
    std::cout<<"ARMS_CUT_PROJECTION diagnosticOnly=1 asset="<<path.filename().string()
             <<" topologyAvailable="<<arms.cutTopology.Available()<<" knownEdges="<<arms.cutTopology.edges.size()
             <<" maximumVisibleKnownCutEdges="<<maximumVisibleKnownCutEdges
             <<" boundaryInventoryAvailable="<<arms.cutTopology.BoundaryInventoryAvailable()<<" combinedOpenVertices="<<arms.cutTopology.combinedOpenVertices
             <<" retainedSourceBoundaryEdges="<<arms.cutTopology.retainedBoundaries.size()<<" maximumVisibleRetainedSourceBoundaryEdges="<<maximumVisibleRetainedBoundaries
             <<" retainedSameDirectionSeamPairs="<<arms.cutTopology.retainedSameDirectionSeams.size()<<" maximumVisibleSameDirectionSeamPairs="<<maximumVisibleSameDirectionSeams
             <<" surfaceManifold="<<arms.cutTopology.surfaceManifold<<" fullArmsGatePassed=0\n";
    // 已追溯切口必須全樣本不可見；原始保留邊界與接縫仍單獨回報，不冒充完整 GPU 驗收。
    return projectionSamples==126&&projectionLayoutFailures==0&&maximumVisibleKnownCutEdges==0;
}
}
int main(int argc,char** argv){const auto root=std::filesystem::path(AEVRYND_SOURCE_DIR)/"games/character_lab/assets/model-packs";
    if(argc==2&&(std::string_view(argv[1])=="--search-known-cut"||std::string_view(argv[1])=="--search-known-cut-local")){
        unsigned candidateId{},probePasses{},fullPasses{};
        std::vector<SearchCandidate> candidates;
        if(std::string_view(argv[1])=="--search-known-cut-local"){
            for(float shoulderX:{.04f,.08f,.12f})for(float shoulderY:{-.32f,-.27f,-.22f})
            for(float shoulderZ:{.26f,.34f,.42f,.50f,.58f})for(float gripZ:{.52f,.58f,.64f,.70f})
                candidates.push_back({{shoulderX,shoulderY,shoulderZ},{.31f/.47f*gripZ,-.055f,gripZ},++candidateId,true});
        }else{
            for(float shoulderX:{0.f,.06f,.12f})for(float shoulderY:{-.32f,-.27f,-.22f})for(float shoulderZ:{.04f,.09f,.14f})
            for(float gripZ:{.41f,.44f,.47f})for(float gripY:{-.02f,-.038f,-.055f})
                candidates.push_back({{shoulderX,shoulderY,shoulderZ},{.31f/.47f*gripZ,gripY,gripZ},++candidateId,true});
        }
        for(auto candidate:candidates){
            std::cout<<"POSE_SEARCH_BEGIN candidate="<<candidate.id<<" shoulder="<<candidate.shoulder.x<<','<<candidate.shoulder.y<<','<<candidate.shoulder.z
                     <<" grip="<<candidate.grip.x<<','<<candidate.grip.y<<','<<candidate.grip.z<<'\n';
            // 先以攻擊峰值與完整進入姿態篩除不可達候選，再以兩性完整 126 組重新驗證。
            const bool femaleProbe=Check(root/"realistic-female/realistic-female.aeskin",&candidate);
            const bool maleProbe=Check(root/"realistic-male/realistic-male.aeskin",&candidate);
            if(!femaleProbe||!maleProbe)continue;++probePasses;candidate.probe=false;
            const bool femaleFull=Check(root/"realistic-female/realistic-female.aeskin",&candidate);
            const bool maleFull=Check(root/"realistic-male/realistic-male.aeskin",&candidate);
            if(femaleFull&&maleFull)++fullPasses;
        }
        std::cout<<"POSE_SEARCH_SUMMARY diagnosticOnly=1 candidates="<<candidateId<<" probePasses="<<probePasses<<" fullPasses="<<fullPasses<<" productionConstantsChanged=0 fullArmsGatePassed=0\n";
        return fullPasses?0:1;
    }
    return Check(root/"realistic-female/realistic-female.aeskin")&&Check(root/"realistic-male/realistic-male.aeskin")?0:1;
}
