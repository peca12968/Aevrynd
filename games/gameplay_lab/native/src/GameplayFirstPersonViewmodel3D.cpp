#include "GameplayFirstPersonViewmodel3D.h"
#include <algorithm>
#include <cmath>
#include <limits>

using namespace Aevrynd;
namespace GameplayLab {
GameplayViewmodelGeometryDiagnostic3D MeasureGameplayViewmodelGeometry3D(
    const Camera3D& camera, std::uint32_t viewportWidth, std::uint32_t viewportHeight,
    const SkinnedModelAssetData& character, const HumanoidSkeletonMap3D& rig,
    const AnimationPose3D& pose, const CharacterEquipmentAsset3D& weapon,
    CharacterEquipmentShape3D shape, const Matrix4& armsWorld, const Matrix4& weaponWorld) {
    GameplayViewmodelGeometryDiagnostic3D result{};
    const auto finiteVector=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    const auto finiteMatrix=[](const Matrix4& matrix){for(const auto& row:matrix.values)for(float v:row)if(!std::isfinite(v))return false;return true;};
    if(!viewportWidth||!viewportHeight||viewportWidth>16384||viewportHeight>16384||
       !finiteVector(camera.position)||!finiteVector(camera.target)||!finiteVector(camera.up)||
       Length(camera.target-camera.position)<.0001f||Length(Cross(camera.up,camera.target-camera.position))<.0001f||
       !std::isfinite(camera.projection.nearPlane)||camera.projection.nearPlane<=0||
       !std::isfinite(camera.projection.farPlane)||camera.projection.farPlane<=camera.projection.nearPlane||
       !finiteMatrix(armsWorld)||!finiteMatrix(weaponWorld)||!character.model.Valid()||
       character.jointNames.size()!=character.model.skeleton.joints.size()||
       !rig.Valid(character.model.skeleton,character.model.skeletonCompatibilityId)||
       pose.globalMatrices.size()!=character.model.skeleton.joints.size()||
       pose.localTransforms.size()!=character.model.skeleton.joints.size())return result;
    if(camera.projection.mode==ProjectionMode::Perspective){
        if(!std::isfinite(camera.projection.verticalFieldOfView)||camera.projection.verticalFieldOfView<=0||camera.projection.verticalFieldOfView>=3.14159265f)return result;
    }else if(camera.projection.mode!=ProjectionMode::Orthographic||!std::isfinite(camera.projection.orthographicHeight)||camera.projection.orthographicHeight<=0)return result;
    for(const auto& matrix:pose.globalMatrices)if(!finiteMatrix(matrix))return result;
    for(const auto& transform:pose.localTransforms)if(!finiteMatrix(transform.Matrix()))return result;
    const Matrix4 view=CreateViewMatrix(camera);
    const auto landmark=[&](Vec3 world){
        GameplayViewmodelLandmark3D point{};
        if(!finiteVector(world))return point;
        point.cameraMeters=TransformPoint(view,world);
        if(!finiteVector(point.cameraMeters)||point.cameraMeters.z<=camera.projection.nearPlane)return GameplayViewmodelLandmark3D{};
        const auto projected=ProjectToViewport(world,camera,{static_cast<float>(viewportWidth),static_cast<float>(viewportHeight)});
        point.viewportPixels=projected.position;
        point.available=std::isfinite(projected.position.x)&&std::isfinite(projected.position.y);
        return point;
    };
    const std::array<HumanoidJoint3D,2> upper{HumanoidJoint3D::LeftUpperArm,HumanoidJoint3D::RightUpperArm};
    const std::array<HumanoidJoint3D,2> lower{HumanoidJoint3D::LeftLowerArm,HumanoidJoint3D::RightLowerArm};
    const std::array<HumanoidJoint3D,2> hand{HumanoidJoint3D::LeftHand,HumanoidJoint3D::RightHand};
    for(std::size_t side=0;side<2;++side){
        const auto jointPoint=[&](HumanoidJoint3D joint){return landmark(TransformPoint(armsWorld*pose.globalMatrices[rig.Joint(joint)],{}));};
        result.shoulders[side]=jointPoint(upper[side]);result.elbows[side]=jointPoint(lower[side]);result.wrists[side]=jointPoint(hand[side]);
        if(!result.shoulders[side].available||!result.elbows[side].available||!result.wrists[side].available)return {};
        const std::string name=side?"RightFingers":"LeftFingers";
        const auto found=std::find(character.jointNames.begin(),character.jointNames.end(),name);
        if(found!=character.jointNames.end()&&std::count(character.jointNames.begin(),character.jointNames.end(),name)==1){
            const auto index=std::size_t(found-character.jointNames.begin());
            if(index>=pose.globalMatrices.size()||character.model.skeleton.joints[index].parent!=int(rig.Joint(hand[side])))return {};
            result.fingerRoots[side]=landmark(TransformPoint(armsWorld*pose.globalMatrices[index],{}));
        }
    }
    result.valid=true;
    if(shape!=CharacterEquipmentShape3D::Sword)return result;
    // 與正式工廠逐頂點核對，避免把任意材質槽或包圍盒誤認為握把／刀尖。
    CharacterEquipmentAsset3D expected{};
    if(!CreateCharacterEquipmentAsset3D(character,rig,shape,expected)||!weapon.model.Valid()||
       weapon.model.skeletonCompatibilityId!=expected.model.skeletonCompatibilityId||
       weapon.model.bindMesh.indices!=expected.model.bindMesh.indices||
       weapon.model.bindMesh.vertices.size()!=expected.model.bindMesh.vertices.size()||
       weapon.model.influences.size()!=expected.model.influences.size()||
       weapon.submeshes.size()!=expected.submeshes.size()||
       weapon.model.skeleton.joints.size()!=expected.model.skeleton.joints.size()||
       weapon.model.inverseBindMatrices.size()!=expected.model.inverseBindMatrices.size())return {};
    for(std::size_t index=0;index<weapon.model.inverseBindMatrices.size();++index){
        const auto& joint=weapon.model.skeleton.joints[index];const auto& expectedJoint=expected.model.skeleton.joints[index];
        if(joint.parent!=expectedJoint.parent)return {};
        const auto bind=joint.bindTransform.Matrix(),expectedBind=expectedJoint.bindTransform.Matrix();
        for(std::size_t row=0;row<4;++row)for(std::size_t column=0;column<4;++column)
            if(bind[row][column]!=expectedBind[row][column]||weapon.model.inverseBindMatrices[index][row][column]!=expected.model.inverseBindMatrices[index][row][column])return {};
    }
    for(std::size_t index=0;index<weapon.submeshes.size();++index){const auto& a=weapon.submeshes[index];const auto& b=expected.submeshes[index];if(a.firstIndex!=b.firstIndex||a.indexCount!=b.indexCount||a.materialSlot!=b.materialSlot||a.flags!=b.flags)return {};}
    for(std::size_t index=0;index<weapon.model.bindMesh.vertices.size();++index){
        const auto& a=weapon.model.bindMesh.vertices[index];const auto& b=expected.model.bindMesh.vertices[index];
        if(a.x!=b.x||a.y!=b.y||a.z!=b.z||weapon.model.influences[index].joints!=expected.model.influences[index].joints||weapon.model.influences[index].weights!=expected.model.influences[index].weights)return {};
    }
    SkinningWorkspace3D workspace{};Mesh3D posed{};
    if(!SkinModelInto(weapon.model,pose,workspace,posed))return {};
    std::vector<bool> selected(posed.vertices.size());
    Vec3 grip{};
    for(const auto& range:weapon.submeshes)if(range.materialSlot==1)
        for(std::size_t offset=range.firstIndex;offset<std::size_t(range.firstIndex)+range.indexCount;++offset){
            const auto index=posed.indices[offset];if(selected[index])continue;selected[index]=true;
            const auto& v=posed.vertices[index];grip=grip+Vec3{v.x,v.y,v.z};++result.gripVertexCount;
        }
    if(!result.gripVertexCount)return {};
    result.actualGrip=landmark(TransformPoint(weaponWorld,grip*(1.f/float(result.gripVertexCount))));
    Vec3 apex{};
    if(weapon.swordGeometry.tipVertexIndices.empty())return {};
    for(const auto index:weapon.swordGeometry.tipVertexIndices){if(index>=posed.vertices.size())return {};const auto& vertex=posed.vertices[index];apex=apex+Vec3{vertex.x,vertex.y,vertex.z};}
    apex=apex*(1.f/static_cast<float>(weapon.swordGeometry.tipVertexIndices.size()));
    // 刀尖由正式幾何契約列出的全部尖端頂點求平均，不依賴舊金字塔固定索引。
    result.bladeApex=landmark(TransformPoint(weaponWorld,apex));
    result.declaredGrip=landmark(TransformPoint(weaponWorld*pose.globalMatrices[rig.Joint(HumanoidJoint3D::RightHand)],{0,.105f,.02f}));
    if(!result.actualGrip.available||!result.declaredGrip.available||!result.bladeApex.available)return {};
    result.wristToActualGripMeters=Length(result.wrists[1].cameraMeters-result.actualGrip.cameraMeters);
    result.actualGripToDeclaredSocketMeters=Length(result.actualGrip.cameraMeters-result.declaredGrip.cameraMeters);
    if(!std::isfinite(result.wristToActualGripMeters)||!std::isfinite(result.actualGripToDeclaredSocketMeters))return {};
    result.generatedSwordSupported=true;
    return result;
}
namespace {
constexpr float Width=1280.f,Height=720.f;
bool InverseAffine(const Matrix4& source,Matrix4& inverse)noexcept{
    const double a=source[0][0],b=source[0][1],c=source[0][2],d=source[1][0],e=source[1][1],f=source[1][2],g=source[2][0],h=source[2][1],i=source[2][2],det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);if(!std::isfinite(det)||std::abs(det)<1e-12)return false;
    inverse=Matrix4::Identity();inverse[0][0]=float((e*i-f*h)/det);inverse[0][1]=float((c*h-b*i)/det);inverse[0][2]=float((b*f-c*e)/det);inverse[1][0]=float((f*g-d*i)/det);inverse[1][1]=float((a*i-c*g)/det);inverse[1][2]=float((c*d-a*f)/det);inverse[2][0]=float((d*h-e*g)/det);inverse[2][1]=float((b*g-a*h)/det);inverse[2][2]=float((a*e-b*d)/det);const float tx=source[0][3],ty=source[1][3],tz=source[2][3];inverse[0][3]=-(inverse[0][0]*tx+inverse[0][1]*ty+inverse[0][2]*tz);inverse[1][3]=-(inverse[1][0]*tx+inverse[1][1]*ty+inverse[1][2]*tz);inverse[2][3]=-(inverse[2][0]*tx+inverse[2][1]*ty+inverse[2][2]*tz);return true;
}
}

GameplayFirstPersonViewmodelLayout3D GameplayFirstPersonViewmodelLayout(GameplayViewmodelSex3D sex)noexcept{
    // 真實劍身比舊合成包絡窄；放大實際武器以維持可讀輪廓，手臂比例保持不變。
    if(sex==GameplayViewmodelSex3D::Female)return {{.63f,-.10f,.92f},{-.12f,.08f,2.96f},.264f,.60f,-.54f,.505f,2.1f,.75f};
    return {{.6165f,-.11f,.96f},{-.11f,.40f,2.98f},.222f,.66f,-.54f,.50f,2.25f,.82f};
}
Matrix4 GameplayFirstPersonDesiredGripTransform3D(const GameplayFirstPersonViewmodelLayout3D& layout)noexcept{
    return CreateTransformMatrix(layout.gripTranslation,layout.gripRotation,{layout.weaponScale,layout.weaponScale,layout.weaponScale});
}
bool ComposeGameplayFirstPersonWeaponWorld3D(const Camera3D& camera,const Matrix4& modelHandSocket,const GameplayFirstPersonViewmodelLayout3D& layout,bool arms,Matrix4& weaponWorld)noexcept{
    const Vec3 forward=Normalize(camera.target-camera.position),right=Normalize(Cross(camera.up,forward)),up=Normalize(Cross(forward,right));if(Length(forward)<.001f||Length(right)<.001f||Length(up)<.001f)return false;
    Matrix4 cameraWorld=Matrix4::Identity();cameraWorld[0][0]=right.x;cameraWorld[1][0]=right.y;cameraWorld[2][0]=right.z;cameraWorld[0][1]=up.x;cameraWorld[1][1]=up.y;cameraWorld[2][1]=up.z;cameraWorld[0][2]=forward.x;cameraWorld[1][2]=forward.y;cameraWorld[2][2]=forward.z;cameraWorld[0][3]=camera.position.x;cameraWorld[1][3]=camera.position.y;cameraWorld[2][3]=camera.position.z;
    const Matrix4 modelGripSocket=modelHandSocket*CreateTranslationMatrix({0,.105f,.02f});Matrix4 inverse{};if(!InverseAffine(modelGripSocket,inverse))return false;const float scale=arms?layout.armsScale:layout.weaponScale;weaponWorld=cameraWorld*CreateTransformMatrix(layout.gripTranslation,layout.gripRotation,{scale,scale,scale})*inverse;return true;
}
GameplayFirstPersonWeaponProjectionGeometry3D BuildGameplayFirstPersonWeaponProjectionGeometry3D(const CharacterEquipmentAsset3D& equipment){
    GameplayFirstPersonWeaponProjectionGeometry3D result;
    if(!ValidateCharacterSwordGeometry3D(equipment))return result;
    const auto joint=equipment.model.influences.front().joints[0];
    const auto& inverse=equipment.model.inverseBindMatrices[joint];
    result.vertices_.reserve(equipment.model.bindMesh.vertices.size());
    for(const auto& vertex:equipment.model.bindMesh.vertices){
        const Vec3 point=TransformPoint(inverse,{vertex.x,vertex.y,vertex.z});
        if(!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z))return {};
        result.vertices_.push_back(point);
    }
    result.sword_=equipment.swordGeometry;result.valid_=true;return result;
}

GameplayFirstPersonViewmodelProjection3D EvaluateGameplayFirstPersonViewmodelProjection3D(
    const Camera3D&,const Matrix4&,const GameplayFirstPersonViewmodelLayout3D&)noexcept{
    // 舊介面沒有實際武器，無法誠實提供幾何證據；保留編譯相容但拒絕驗收。
    return {};
}

GameplayFirstPersonViewmodelProjection3D EvaluateGameplayFirstPersonViewmodelProjection3D(
    const Camera3D& camera,const Matrix4& handSocketWorld,const GameplayFirstPersonViewmodelLayout3D& layout,
    const GameplayFirstPersonWeaponProjectionGeometry3D& geometry,std::uint32_t viewportWidth,std::uint32_t viewportHeight)noexcept{
    GameplayFirstPersonViewmodelProjection3D result{};
    const auto finite=[](Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);};
    if(!geometry.Valid()||geometry.HandLocalVertices().empty()||!viewportWidth||!viewportHeight||viewportWidth>16384||viewportHeight>16384||
       !finite(camera.position)||!finite(camera.target)||!finite(camera.up)||Length(camera.target-camera.position)<.0001f||
       Length(Cross(camera.up,camera.target-camera.position))<.0001f||camera.projection.mode!=ProjectionMode::Perspective||
       !std::isfinite(camera.projection.verticalFieldOfView)||camera.projection.verticalFieldOfView<=0||camera.projection.verticalFieldOfView>=3.13f||
       !std::isfinite(camera.projection.nearPlane)||camera.projection.nearPlane<=0||!std::isfinite(camera.projection.farPlane)||
       camera.projection.farPlane<=camera.projection.nearPlane||!std::isfinite(layout.armsMinimumX)||!std::isfinite(layout.armsMaximumX)||
       layout.armsMaximumX<=layout.armsMinimumX)return result;
    for(const auto& row:handSocketWorld.values)for(float value:row)if(!std::isfinite(value))return result;
    const float width=static_cast<float>(viewportWidth),height=static_cast<float>(viewportHeight);
    const Matrix4 view=CreateViewMatrix(camera);
    float minX=std::numeric_limits<float>::max(),minY=minX,maxX=-minX,maxY=-minX,minDepth=minX;
    const auto project=[&](Vec3 local,Vec2& pixels,float& depth){
        const Vec3 world=TransformPoint(handSocketWorld,local),eye=TransformPoint(view,world);
        if(!finite(world)||!finite(eye)||eye.z<=0)return false;
        pixels=ProjectToViewport(world,camera,{width,height}).position;depth=eye.z;
        return std::isfinite(pixels.x)&&std::isfinite(pixels.y);
    };
    for(Vec3 vertex:geometry.HandLocalVertices()){
        Vec2 pixels{};float depth{};if(!project(vertex,pixels,depth))return {};
        minX=(std::min)(minX,pixels.x);maxX=(std::max)(maxX,pixels.x);minY=(std::min)(minY,pixels.y);maxY=(std::max)(maxY,pixels.y);minDepth=(std::min)(minDepth,depth);
    }
    // 此欄位仍是實際頂點投影包圍矩形面積，不宣稱是像素遮罩覆蓋率。
    result.weaponCoverage=(maxX-minX)*(maxY-minY)/(width*height);result.minimumDepth=minDepth;
    result.weaponMinimumX=minX;result.weaponMinimumY=minY;result.weaponMaximumX=maxX;result.weaponMaximumY=maxY;
    Vec2 grip{},tip{};float gripDepth{},tipDepth{};
    if(!project(geometry.Sword().grip,grip,gripDepth)||!project(geometry.Sword().bladeTip,tip,tipDepth))return {};
    result.gripX=grip.x;result.gripY=grip.y;result.bladeY=tip.y;
    const float sx=width/Width,sy=height/Height;
    result.centerExcluded=!(minX<=width*.5f&&maxX>=width*.5f&&minY<=height*.5f&&maxY>=height*.5f);
    result.nearClipClear=minDepth>camera.projection.nearPlane+.015f;
    const bool topHud=minY<66*sy&&maxY>0,paletteHud=minX<372*sx&&maxX>0&&minY<height&&maxY>470*sy,
        rightHud=minX<1272*sx&&maxX>778*sx&&minY<638*sy&&maxY>512*sy;
    result.hudClear=!(topHud||paletteHud||rightHud);
    result.gripEdgeAnchored=grip.x>=1040*sx&&grip.x<=1264*sx&&grip.y>=80*sy&&grip.y<=510*sy;
    result.bladeAboveGrip=tip.y+80*sy<grip.y;
    result.weaponMarginClear=minX>=16*sx&&maxX<=width-16*sx&&minY>=16*sy&&maxY<=height-16*sy;
    result.actualWeaponGeometryMeasured=true;
    // 沒有實際蒙皮手臂頂點時只保留武器證據；舊合成包絡不得讓完整門檻通過。
    result.actualWeaponGeometryMeasured=true;return result;
}
GameplayFirstPersonViewmodelProjection3D EvaluateGameplayFirstPersonViewmodelProjection3D(
    const Camera3D& camera,const Matrix4& handSocketWorld,const GameplayFirstPersonViewmodelLayout3D& layout,
    const GameplayFirstPersonWeaponProjectionGeometry3D& geometry,std::span<const Vertex3D> posedArms,
    const Matrix4& armsWorld,std::uint32_t viewportWidth,std::uint32_t viewportHeight)noexcept{
    auto result=EvaluateGameplayFirstPersonViewmodelProjection3D(camera,handSocketWorld,layout,geometry,viewportWidth,viewportHeight);
    if(!result.actualWeaponGeometryMeasured||posedArms.empty())return result;
    for(const auto& row:armsWorld.values)for(float value:row)if(!std::isfinite(value))return result;
    const float width=static_cast<float>(viewportWidth),height=static_cast<float>(viewportHeight),sx=width/Width,sy=height/Height;
    const Matrix4 view=CreateViewMatrix(camera);
    float minX=std::numeric_limits<float>::max(),minY=minX,maxX=-minX,maxY=-minX;
    float visibleMinX=width,visibleMinY=height,visibleMaxX{},visibleMaxY{};
    std::uint32_t visibleVertices{};bool unsafeHudOverlap{};
    for(const auto& vertex:posedArms){
        const Vec3 world=TransformPoint(armsWorld,{vertex.x,vertex.y,vertex.z}),eye=TransformPoint(view,world);
        if(!std::isfinite(world.x)||!std::isfinite(world.y)||!std::isfinite(world.z)||!std::isfinite(eye.z))return result;
        // 肩部裁切幾何可位於相機後方；它不會產生像素，也不能使可見手臂證據失效。
        if(eye.z<=0)continue;
        const Vec2 pixel=ProjectToViewport(world,camera,{width,height}).position;
        if(!std::isfinite(pixel.x)||!std::isfinite(pixel.y))return result;
        minX=(std::min)(minX,pixel.x);minY=(std::min)(minY,pixel.y);maxX=(std::max)(maxX,pixel.x);maxY=(std::max)(maxY,pixel.y);
        if(pixel.x>=0&&pixel.x<width&&pixel.y>=0&&pixel.y<height){
            if(eye.z<=camera.projection.nearPlane+.015f)return result;
            ++visibleVertices;visibleMinX=(std::min)(visibleMinX,pixel.x);visibleMinY=(std::min)(visibleMinY,pixel.y);
            visibleMaxX=(std::max)(visibleMaxX,pixel.x);visibleMaxY=(std::max)(visibleMaxY,pixel.y);
            const bool topHud=pixel.y<66*sy,paletteHud=pixel.x<372*sx&&pixel.y>470*sy;
            unsafeHudOverlap=unsafeHudOverlap||topHud||paletteHud;
        }
    }
    result.armsMinimumX=minX;result.armsMinimumY=minY;result.armsMaximumX=maxX;result.armsMaximumY=maxY;
    // 第一人稱手臂必須從畫面邊緣進入，不能要求裁切邊界全部落在安全框內；
    // 以實際可見頂點驗證右側構圖與 HUD 淨空，原始全域包圍框仍保留作診斷。
    result.armsMarginClear=visibleVertices>=3&&!unsafeHudOverlap&&visibleMaxX>=width*.65f&&
        visibleMinX<visibleMaxX&&visibleMinY<visibleMaxY;
    return result;
}
bool GameplayFirstPersonViewmodelProjection3D::Complete()const noexcept{
    return actualWeaponGeometryMeasured&&std::isfinite(weaponCoverage)&&weaponCoverage>=.02f&&weaponCoverage<=.06f&&std::isfinite(minimumDepth)&&centerExcluded&&nearClipClear&&hudClear&&gripEdgeAnchored&&bladeAboveGrip&&weaponMarginClear&&armsMarginClear;
}
}
