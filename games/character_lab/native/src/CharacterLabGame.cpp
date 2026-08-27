#include "CharacterLabGame.h"
#include "CharacterLabRules.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <Aevrynd/SkinnedMaterialSidecar3D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <iterator>
#include <stdexcept>

using namespace Aevrynd;
namespace {
constexpr Color Cyan{.12f,.92f,.82f,1},Gold{1.f,.72f,.2f,1},Ink{.72f,.86f,.88f,1},Coral{1.f,.35f,.25f,1};
constexpr std::array<const wchar_t*,5> CharacterCaptureNames={L"front-pair.png",L"female-profile.png",L"back-pair.png",L"male-profile.png",L"portrait-pair.png"};

Surface3D CharacterSurface(std::string_view name){
    auto has=[&](std::string_view value){return name.find(value)!=std::string_view::npos;};
    if(has("Metal")||has("Gold")||has("Buckle"))return{.22f,.82f,0,.96f};
    if(has("Eye")||has("Iris"))return{.16f,0,0,1};
    if(has("Hair"))return{.31f,0,0,.94f};
    if(has("Skin")||has("Face"))return{.43f,0,0,1};
    if(has("Lip"))return{.36f,0,0,1};
    if(has("Leather")||has("Boot"))return{.48f,0,0,.96f};
    return{.68f,0,0,1};
}

std::vector<Material3D> CharacterMaterials(const SkinnedModelAssetData& asset){
    std::vector<Material3D> result;result.reserve(asset.materialSlotNames.size());
    for(std::size_t slot=0;slot<asset.materialSlotNames.size();++slot){
        Material3D material;material.useUv=false;
        if(asset.materials.empty())material.surface=CharacterSurface(asset.materialSlotNames[slot]);
        else{const SkinnedMaterialDefinition3D& source=asset.materials[slot];material.tint=source.baseColorFactor;material.surface={source.roughnessFactor,source.metallicFactor,Clamp((std::max)({source.emissiveFactor.x,source.emissiveFactor.y,source.emissiveFactor.z})*source.emissiveStrength,0.f,1.f),source.occlusionStrength};}
        result.push_back(material);
    }
    return result;
}

std::vector<Material3D> LoadCharacterMaterials(Engine& engine,MaterialLibrary3D& library,const SkinnedModelAssetData& asset,const std::filesystem::path& materialRoot,const std::filesystem::path& sidecar){
    const SkinnedMaterialSidecarResult3D parsed=LoadSkinnedMaterialSidecarFile3D(sidecar,asset.materialSlotNames,materialRoot);
    if(!parsed)throw std::runtime_error("Character material sidecar failed: "+parsed.error.message);
    std::vector<MaterialAssetDefinition3D> definitions=parsed.materials;
    const std::vector<Material3D> sourceMaterials=CharacterMaterials(asset);
    for(std::size_t index=0;index<definitions.size();++index){const float textureScale=definitions[index].material.textureScale;const bool useUv=definitions[index].material.useUv,castsShadow=definitions[index].material.castsShadow;const CharacterShadingModel3D shadingModel=definitions[index].material.shadingModel;definitions[index].material=sourceMaterials[index];definitions[index].material.textureScale=textureScale;definitions[index].material.useUv=useUv;definitions[index].material.castsShadow=castsShadow;definitions[index].material.shadingModel=shadingModel;}
    if(!library.Initialize(engine,materialRoot)||!library.LoadAll(definitions))throw std::runtime_error("Character PBR texture batch loading failed");
    std::vector<Material3D> result;result.reserve(asset.materialSlotNames.size());for(const std::string& name:asset.materialSlotNames){const Material3D* material=library.Find(name);if(!material)throw std::runtime_error("Character PBR material binding failed: "+name);result.push_back(*material);}return result;
}

void AppendSkinnedPart(SkinnedModel3D& model,const Mesh3D& source,Vec3 position,Vec3 scale,std::uint16_t joint,Color color){
    const Matrix4 transform=CreateTransformMatrix(position,{},scale),normalTransform=CreateNormalMatrix(transform);const std::uint32_t base=std::uint32_t(model.bindMesh.vertices.size());model.bindMesh.vertices.reserve(model.bindMesh.vertices.size()+source.vertices.size());model.influences.reserve(model.influences.size()+source.vertices.size());
    for(const Vertex3D& vertex:source.vertices){const Vec3 point=TransformPoint(transform,{vertex.x,vertex.y,vertex.z});const Vec4 packed=UnpackNormalSnorm8(vertex.normal);const Vec3 normal=TransformNormal(normalTransform,{packed.x,packed.y,packed.z});model.bindMesh.vertices.push_back({point.x,point.y,point.z,PackNormalSnorm8(normal,packed.w),PackColorRgba8(color),vertex.u,vertex.v});SkinInfluences3D influences{};influences.joints[0]=joint;influences.weights[0]=65535;model.influences.push_back(influences);}
    model.bindMesh.indices.reserve(model.bindMesh.indices.size()+source.indices.size());for(std::uint32_t index:source.indices)model.bindMesh.indices.push_back(base+index);
}

SkinnedModel3D CreateCharacterModel(){
    SkinnedModel3D model;model.skeleton.joints={
        {-1,{{0,1.f,0},{},{1,1,1}}},
        {0,{{0,.55f,0},{},{1,1,1}}},
        {1,{{0,.65f,0},{},{1,1,1}}},
        {1,{{-.55f,.25f,0},{},{1,1,1}}},
        {1,{{.55f,.25f,0},{},{1,1,1}}},
        {0,{{-.22f,-.1f,0},{},{1,1,1}}},
        {0,{{.22f,-.1f,0},{},{1,1,1}}}};
    const std::array<Vec3,7> bindPositions={Vec3{0,1,0},Vec3{0,1.55f,0},Vec3{0,2.2f,0},Vec3{-.55f,1.8f,0},Vec3{.55f,1.8f,0},Vec3{-.22f,.9f,0},Vec3{.22f,.9f,0}};model.inverseBindMatrices.reserve(bindPositions.size());for(Vec3 position:bindPositions)model.inverseBindMatrices.push_back(CreateTranslationMatrix(position*-1.f));
    const Mesh3D sphere=Mesh3D::CreateUnitSphere(18,12);AppendSkinnedPart(model,sphere,{0,1.02f,0},{.72f,.58f,.52f},0,{.09f,.28f,.36f,1});AppendSkinnedPart(model,sphere,{0,1.58f,0},{.9f,1.02f,.58f},1,{.08f,.55f,.72f,1});AppendSkinnedPart(model,sphere,{0,2.22f,0},{.56f,.62f,.55f},2,{.72f,.56f,.42f,1});
    AppendSkinnedPart(model,sphere,{-.55f,1.4f,0},{.3f,.88f,.32f},3,{.08f,.46f,.62f,1});AppendSkinnedPart(model,sphere,{-.55f,.94f,0},{.27f,.28f,.3f},3,{.72f,.56f,.42f,1});AppendSkinnedPart(model,sphere,{.55f,1.4f,0},{.3f,.88f,.32f},4,{.08f,.46f,.62f,1});AppendSkinnedPart(model,sphere,{.55f,.94f,0},{.27f,.28f,.3f},4,{.72f,.56f,.42f,1});
    AppendSkinnedPart(model,sphere,{-.22f,.45f,0},{.38f,.92f,.42f},5,{.055f,.16f,.22f,1});AppendSkinnedPart(model,sphere,{-.22f,.02f,.13f},{.42f,.2f,.72f},5,{.035f,.08f,.11f,1});AppendSkinnedPart(model,sphere,{.22f,.45f,0},{.38f,.92f,.42f},6,{.055f,.16f,.22f,1});AppendSkinnedPart(model,sphere,{.22f,.02f,.13f},{.42f,.2f,.72f},6,{.035f,.08f,.11f,1});return model;
}

AnimationClip3D CreatePresentationClip(const SkinnedModel3D& model,float phase){
    AnimationClip3D clip;clip.duration=2.4f;clip.looping=true;if(model.skeleton.joints.size()<2)return clip;
    JointTransform3D root=model.skeleton.joints[0].bindTransform,raised=root;raised.translation.y+=.018f;
    JointTransform3D hips=model.skeleton.joints[1].bindTransform,left=hips,right=hips;left.translation.x-=.012f;right.translation.x+=.012f;
    clip.tracks.push_back({0,{{0,root},{.6f+phase,raised},{1.2f+phase,root},{1.8f+phase,raised},{2.4f,root}}});
    clip.tracks.push_back({1,{{0,hips},{.6f,left},{1.2f,hips},{1.8f,right},{2.4f,hips}}});return clip;
}
}

Aevrynd::ReusableMemoryStats CharacterLabGame::GetReusableMemoryStats()const{const ReusableMemoryStats collision=collisionWorld_.MemoryStats(),scene=scene_.MemoryStats(),skinning=skinningWorkspace_.MemoryStats();return{collision.retainedCapacityBytes+scene.retainedCapacityBytes+skinning.retainedCapacityBytes,collision.reclaimableCapacityBytes+scene.reclaimableCapacityBytes+skinning.reclaimableCapacityBytes,lastReusableMemoryReleasedBytes_,reusableMemoryTrimCount_};}
std::size_t CharacterLabGame::TrimReusableMemory(){const std::size_t before=GetReusableMemoryStats().retainedCapacityBytes;collisionWorld_.TrimUnusedMemory();scene_.TrimUnusedMemory();skinningWorkspace_.TrimUnusedMemory();const std::size_t after=GetReusableMemoryStats().retainedCapacityBytes;lastReusableMemoryReleasedBytes_=before>after?before-after:0;++reusableMemoryTrimCount_;return lastReusableMemoryReleasedBytes_;}

void CharacterLabGame::OnStart(Engine& engine){
    const BoxCollider3D boxes[]={{{0,-.5f,0},{14,.5f,14}},{{-13.5f,2,0},{.5f,2,14}},{{13.5f,2,0},{.5f,2,14}},{{0,2,-13.5f},{14,2,.5f}},{{0,2,13.5f},{14,2,.5f}},{{0,1,2},{2,1,.5f}},{{-5,1,-2},{1,1,2}},{{5,.75f,4},{1.5f,.75f,1.5f}}};for(std::size_t index=0;index<std::size(boxes);++index)collisionWorld_.CreateBox({boxes[index],1,CollisionLayerAll3D,index,false});controller_.Reset({0,0,-8});boxMesh_=engine.CreateMeshResource3D(Mesh3D::CreateUnitBox());scene_.SetBoxMesh(boxMesh_);scene_.Reserve({0,32,8,4,0});
    const std::filesystem::path modelRoot=engine.ExecutableDirectory()/"assets"/"model-packs",materialRoot=engine.ExecutableDirectory()/"assets"/"material-packs"/"original";SkinnedModelAssetDetailedResult female=LoadSkinnedModelAssetFileDetailed(modelRoot/"realistic-female"/"realistic-female.aeskin"),male=LoadSkinnedModelAssetFileDetailed(modelRoot/"realistic-male"/"realistic-male.aeskin");if(!female)throw std::runtime_error("Female character asset loading failed: "+female.error);if(!male)throw std::runtime_error("Male character asset loading failed: "+male.error);femaleMaterials_=LoadCharacterMaterials(engine,femaleMaterialLibrary_,female.asset,materialRoot,materialRoot/"female.materials.json");maleMaterials_=LoadCharacterMaterials(engine,maleMaterialLibrary_,male.asset,materialRoot,materialRoot/"male.materials.json");femaleModel_=std::move(female.asset.model);femaleSubmeshes_=std::move(female.asset.submeshes);maleModel_=std::move(male.asset.model);maleSubmeshes_=std::move(male.asset.submeshes);gpuFemale_=engine.CreateSkinnedMeshResource3D(femaleModel_);gpuMale_=engine.CreateSkinnedMeshResource3D(maleModel_);if(!gpuFemale_||!gpuMale_)throw std::runtime_error("GPU character creation failed");femaleClip_=female.asset.clips.empty()?CreatePresentationClip(femaleModel_,0):std::move(female.asset.clips.front().clip);femaleClip_.looping=true;maleClip_=male.asset.clips.empty()?CreatePresentationClip(maleModel_,.3f):std::move(male.asset.clips.front().clip);maleClip_.looping=true;femalePlayer_.Play(femaleClip_,0,true);malePlayer_.Play(maleClip_,0,true);jumpSound_=engine.Audio().CreateSound(GenerateTone(520,.08f,.14f));if(!captureDirectory_.empty()){std::filesystem::create_directories(captureDirectory_);for(const wchar_t* name:CharacterCaptureNames){std::error_code error;std::filesystem::remove(captureDirectory_/name,error);}automatic_=false;firstPerson_=false;}if(engine.DLSSOn())engine.ToggleDLSS();
}

void CharacterLabGame::OnStop(Engine& engine){scene_.SetBoxMesh({});engine.DestroySkinnedMesh3D(gpuFemale_.handle);engine.DestroySkinnedMesh3D(gpuMale_.handle);femaleMaterialLibrary_.Shutdown();maleMaterialLibrary_.Shutdown();engine.DestroyMesh3D(boxMesh_.handle);engine.Audio().DestroySound(jumpSound_);gpuFemale_={};gpuMale_={};boxMesh_={};jumpSound_={};femaleMaterials_.clear();maleMaterials_.clear();}

void CharacterLabGame::OnUpdate(Engine& engine,float deltaTime){
    Input& input=engine.GetInput();if(input.Pressed(VK_ESCAPE))engine.Quit();if(input.Pressed(VK_SPACE))automatic_=!automatic_;if(input.Pressed(VK_F1)){firstPerson_=true;automatic_=false;++modeSwitches_;}if(input.Pressed(VK_F2)){firstPerson_=false;automatic_=false;++modeSwitches_;}if(input.Pressed(VK_F3)){femaleActive_=true;automatic_=false;}if(input.Pressed(VK_F4)){femaleActive_=false;automatic_=false;}
    CharacterControl3D control{};bool requestedJump=false;if(automatic_){const int phase=CharacterLabRules::AutomaticPhase(engine.Time());if(phase!=automaticPhase_){automaticPhase_=phase;firstPerson_=CharacterLabRules::FirstPerson(phase);++modeSwitches_;}control.movement=CharacterLabRules::Movement(phase);yaw_=std::atan2(control.movement.x,control.movement.z);const int jumpCycle=CharacterLabRules::JumpCycle(engine.Time());requestedJump=jumpCycle!=lastJumpCycle_&&controller_.Grounded();if(requestedJump)lastJumpCycle_=jumpCycle;}else{yaw_+=(float(input.Down(VK_RIGHT))-float(input.Down(VK_LEFT))-input.MouseDelta().x*.025f)*deltaTime*2.2f;pitch_=Clamp(pitch_+(float(input.Down(VK_UP))-float(input.Down(VK_DOWN))+input.MouseDelta().y*.02f)*deltaTime*1.8f,-.45f,.8f);const Vec3 forward{std::sin(yaw_),0,std::cos(yaw_)},right{std::cos(yaw_),0,-std::sin(yaw_)};control.movement=forward*(float(input.Down('W'))-float(input.Down('S'))+input.GamepadLeftY())+right*(float(input.Down('D'))-float(input.Down('A'))+input.GamepadLeftX());requestedJump=input.Pressed(VK_RETURN)||input.GamepadPressed(XINPUT_GAMEPAD_A);}control.jump=requestedJump;const uint64_t previousJumps=controller_.Stats().jumps;controller_.Step(control,collisionWorld_,deltaTime);if(controller_.Stats().jumps>previousJumps)engine.Audio().Play(jumpSound_);
    femalePlayer_.Update(deltaTime);malePlayer_.Update(deltaTime);if(automatic_)femaleActive_=int(engine.Time()/6)&1?false:true;if(!captureDirectory_.empty()){const RendererStats renderer=engine.GetRendererStats();if(capturePending_&&(renderer.successfulFrameCaptures>captureSuccessfulBaseline_||renderer.rejectedFrameCaptures>captureRejectedBaseline_)){if(renderer.successfulFrameCaptures>captureSuccessfulBaseline_)++captureIndex_;capturePending_=false;nextCaptureTime_=engine.Time()+.35;}if(captureIndex_==CharacterCaptureNames.size()){bool ready=true;for(const wchar_t* name:CharacterCaptureNames)ready&=std::filesystem::is_regular_file(captureDirectory_/name);if(ready){if(!renderer.lastSubmittedTexturedSkinnedInstances||!renderer.lastSubmittedSkinShadedInstances||!renderer.lastSubmittedHairShadedInstances||renderer.lastSkinnedMaterialDescriptorBinds!=renderer.lastSubmittedTexturedSkinnedInstances||renderer.missingSkinnedTextureBindings)throw std::runtime_error("Automated character PBR, skin, and hair shading validation failed");engine.Quit();return;}}if(!capturePending_&&captureIndex_<CharacterCaptureNames.size()&&engine.Time()>=nextCaptureTime_){captureSuccessfulBaseline_=renderer.successfulFrameCaptures;captureRejectedBaseline_=renderer.rejectedFrameCaptures;capturePending_=engine.CaptureNextFrame3D(captureDirectory_/CharacterCaptureNames[captureIndex_]);nextCaptureTime_=engine.Time()+.4;}}const auto stats=controller_.Stats();const RendererStats renderer=engine.GetRendererStats();engine.SetTitle(std::format(L"Aevrynd Character Lab | {} | HERO {} | POS {:.1f} {:.1f} {:.1f} | GROUND {} | STEP {} COLL {} JUMP {} LAND {} | GPU SKIN {} PBR {} SSS {} HAIR {} BIND {} | JOINTS {} | SHADOW {} | CAM BLOCK {}",firstPerson_?L"FIRST PERSON":L"THIRD PERSON",femaleActive_?L"FEMALE":L"MALE",controller_.Position().x,controller_.Position().y,controller_.Position().z,controller_.Grounded()?L"YES":L"NO",stats.steps,stats.collisions,stats.jumps,stats.landings,renderer.lastSubmittedSkinnedInstances,renderer.lastSubmittedTexturedSkinnedInstances,renderer.lastSubmittedSkinShadedInstances,renderer.lastSubmittedHairShadedInstances,renderer.lastSkinnedMaterialDescriptorBinds,renderer.lastUploadedSkinJoints,sceneStats_.shadowInstances,cameraObstructions_));
}

void CharacterLabGame::BuildEnvironment(){
    scene_.Clear();scene_.MaterialBox({0,-.5f,0},{28,1,28},{{.055f,.065f,.075f,1},{.88f,0,0,.8f},false});scene_.Box({-13.5f,2,0},{1,4,28},{.075f,.085f,.105f,1});scene_.Box({13.5f,2,0},{1,4,28},{.075f,.085f,.105f,1});scene_.Box({0,2,-13.5f},{28,4,1},{.055f,.065f,.085f,1});scene_.Box({0,2,13.5f},{28,4,1},{.055f,.065f,.085f,1});scene_.MaterialBox({-1.05f,.035f,-8},{1.65f,.07f,1.55f},{{.11f,.12f,.14f,1},{.3f,.02f,0,.78f},false});scene_.MaterialBox({1.05f,.035f,-8},{1.65f,.07f,1.55f},{{.11f,.12f,.14f,1},{.3f,.02f,0,.78f},false});scene_.Box({0,1.45f,1.8f},{5.8f,2.9f,.18f},{.13f,.075f,.055f,1});scene_.Box({-5,.8f,3.5f},{1.2f,1.6f,1.2f},{.10f,.13f,.17f,1});scene_.Box({5,.8f,3.5f},{1.2f,1.6f,1.2f},{.17f,.12f,.08f,1});for(int index=-10;index<=10;index+=2){scene_.Line({float(index),.012f,-12},{float(index),.012f,12},1,{.12f,.13f,.15f,.45f});scene_.Line({-12,.012f,float(index)},{12,.012f,float(index)},1,{.12f,.13f,.15f,.45f});}}

void CharacterLabGame::OnRender(Engine& engine,float){
    BuildEnvironment();const Vec3 feet=controller_.Position();Vec3 focus=feet+Vec3{0,1.25f,0};float cameraYaw=yaw_,cameraDistance=5.8f;if(!captureDirectory_.empty()&&captureIndex_<CharacterCaptureNames.size()){constexpr std::array<float,5> angles={0.f,1.5707963f,3.1415926f,-1.5707963f,0.f},distances={5.8f,3.2f,5.8f,3.2f,3.5f};cameraYaw=angles[captureIndex_];cameraDistance=distances[captureIndex_];if(captureIndex_==1)focus=focus+Vec3{-1.05f,.18f,0};else if(captureIndex_==3)focus=focus+Vec3{1.05f,.18f,0};else if(captureIndex_==4)focus.y+=.28f;}if(firstPerson_)camera_=MakeFirstPersonCamera(feet+Vec3{0,1.65f,0},yaw_,pitch_);else{Camera3D desired=MakeThirdPersonCamera(focus,cameraYaw,.22f,cameraDistance);camera_=ConstrainThirdPersonCamera(desired,focus,collisionWorld_,.25f);if(Length(camera_.position-focus)+.01f<Length(desired.position-focus))++cameraObstructions_;}
    aimHit_=scene_.Raycast(ScreenPointToRay(camera_,{1280,720},{640,360}),40,&queryStats_);if(aimHit_){scene_.Line(aimHit_.position-Vec3{.25f,0,0},aimHit_.position+Vec3{.25f,0,0},5,Gold);scene_.Line(aimHit_.position-Vec3{0,.25f,0},aimHit_.position+Vec3{0,.25f,0},5,Gold);scene_.Line(aimHit_.position,aimHit_.position+aimHit_.normal,3,Cyan);}
    DrawList& draw=engine.Draw();engine.SetClearColor({.025f,.035f,.05f,1});draw.SetDirectionalLight({{.38f,-1,.28f},.64f,{1,.95f,.88f,1},1.55f});draw.SetAtmosphere({{.045f,.06f,.085f,.56f},camera_.position,34,110,true});draw.AddPointLight({feet+Vec3{2.4f,2.7f,-2.8f},10,{1.f,.78f,.65f,1},3.2f});draw.AddPointLight({feet+Vec3{-2.3f,2.2f,-1.4f},9,{.48f,.68f,1,1},2.25f});draw.AddPointLight({{5,3,4},11,{1,.58f,.28f,1},1.7f});sceneStats_=scene_.Submit(draw,camera_,{1280,720},{true,true,{true,.025f,.24f,45.f,128},{80.f,.25f}});scene_.Clear();if(!firstPerson_){femalePlayer_.EvaluateInto(femaleModel_.skeleton,femalePose_);malePlayer_.EvaluateInto(maleModel_.skeleton,malePose_);const SkinPaletteRange3D femalePalette=AddSkinPaletteFromPose(draw,femaleModel_,femalePose_,skinningWorkspace_),malePalette=AddSkinPaletteFromPose(draw,maleModel_,malePose_,skinningWorkspace_);const Vec3 right{std::cos(yaw_),0,-std::sin(yaw_)},femalePosition=feet-right*1.05f,malePosition=feet+right*1.05f;const Matrix4 femaleTransform=CreateTransformMatrix(femalePosition,{0,yaw_,0},{.92f,1.10f,.92f}),maleTransform=CreateTransformMatrix(malePosition,{0,yaw_,0},{.98f,1,.98f});const bool showFemale=captureDirectory_.empty()||captureIndex_!=3,showMale=captureDirectory_.empty()||captureIndex_!=1;if(showFemale&&femalePalette)SubmitSkinnedRenderable3D(draw,gpuFemale_.handle,femalePalette,femaleTransform,std::uint32_t(femaleModel_.bindMesh.indices.size()),femaleSubmeshes_,femaleMaterials_);if(showMale&&malePalette)SubmitSkinnedRenderable3D(draw,gpuMale_.handle,malePalette,maleTransform,std::uint32_t(maleModel_.bindMesh.indices.size()),maleSubmeshes_,maleMaterials_);}draw.Quad({640,32},{1280,64},{.01f,.025f,.035f,.92f});draw.Text({28,16},firstPerson_?"CHARACTER LAB  FIRST PERSON":"REALISTIC CHARACTER SHOWCASE",2.8f,firstPerson_?Gold:Cyan);draw.Text({820,17},femaleActive_?"FEMALE HERO  +  MALE COMPANION":"MALE HERO  +  FEMALE COMPANION",1.75f,Gold);draw.Line({628,360},{652,360},2,Cyan);draw.Line({640,348},{640,372},2,Cyan);const auto stats=controller_.Stats();const RendererStats renderer=engine.GetRendererStats();draw.Quad({640,688},{1280,64},{.01f,.025f,.035f,.92f});draw.Text({24,672},std::format("GROUND {} COLL {} JUMP {} LAND {} MODE {} GPU {} PBR {} SSS {} HAIR {} BIND {} JOINTS {} SHADOW {}",controller_.Grounded()?"YES":"NO",stats.collisions,stats.jumps,stats.landings,modeSwitches_,renderer.lastSubmittedSkinnedInstances,renderer.lastSubmittedTexturedSkinnedInstances,renderer.lastSubmittedSkinShadedInstances,renderer.lastSubmittedHairShadedInstances,renderer.lastSkinnedMaterialDescriptorBinds,renderer.lastUploadedSkinJoints,sceneStats_.shadowInstances),1.32f,Cyan);draw.Text({1040,672},automatic_?"AUTO":"F1 F2 F3 F4",1.35f,Gold);
}
