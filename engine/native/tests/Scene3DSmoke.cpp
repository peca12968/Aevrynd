#include <Aevrynd/Scene3D.h>
#include <cmath>
#include <iostream>

using namespace Aevrynd;
bool Near(float left,float right,float tolerance=.001f){return std::abs(left-right)<tolerance;}

int main(){
    const Matrix4 shadowMatrix=CreateDirectionalShadowMatrix({.5f,-1.f,.25f},.1f);const Vec3 projected=TransformPoint(shadowMatrix,{2,4,6});if(!Near(projected.x,3.95f)||!Near(projected.y,.1f)||!Near(projected.z,6.975f)){std::cerr<<"Directional shadow projection failed\n";return 18;}
    Scene3D scene;
    scene.Box({0,1,6},{2,2,2},{.2f,.8f,.7f,1});
    scene.Line({-2,0,4},{2,0,4},2,{1,1,1,1});
    if(scene.TriangleCount()!=12||scene.LineCount()!=1||scene.MeshCount()!=1){std::cerr<<"Scene box topology failed\n";return 1;}
    DrawList draw;
    Camera3D camera=MakeFirstPersonCamera({0,1,0},0,0);
    Scene3DStats stats=scene.Render(draw,camera,{1280,720});
    if(stats.inputTriangles!=12||stats.visibleTriangles!=12||stats.culledTriangles!=0||stats.visibleLines!=1||draw.vertices.size()!=42){std::cerr<<"Scene projection statistics failed\n";return 2;}

    scene.Clear();draw.Clear();
    const Color nearColor{1, 0, 0, 1}, farColor{0, 0, 1, 1};
    scene.Triangle({-1, -1, 5}, {0, 1, 5}, {1, -1, 5}, nearColor);
    scene.Triangle({-1, -1, 10}, {0, 1, 10}, {1, -1, 10}, farColor);
    stats = scene.Render(draw, camera, {1280, 720});
    if (stats.visibleTriangles != 2 || draw.vertices.size() != 6 ||
        draw.vertices.front().color != PackColorRgba8(farColor)) {
      std::cerr << "Stable far-to-near sorting failed\n";
      return 3;
    }
    scene.Line({-1, 0, 4}, {1, 0, 4}, 1, {1, 1, 1, 1});
    draw.Clear();
    stats = scene.Submit(draw, camera, {1280, 720});
    if (stats.inputTriangles != 2 || stats.visibleLines != 1 ||
        draw.worldVertices.size() != 10 || draw.worldIndices.size() != 12 ||
        draw.vertices.size() != 0) {
      std::cerr << "GPU scene submission failed\n";
      return 4;
    }
    scene.Clear();
    draw.Clear();
    scene.Box({0, 1, 6}, {2, 2, 2}, {.2f, .8f, .7f, 1});
    scene.Box({1000, 1, 6}, {2, 2, 2}, {1, .2f, .2f, 1});
    stats = scene.Submit(draw, camera, {1280, 720});
    if (stats.meshInstances != 2 || stats.visibleMeshInstances != 1 ||
        stats.culledMeshInstances != 1 || stats.visibleTriangles != 12 ||
        stats.culledTriangles != 12 || draw.worldVertices.size() != 24 ||
        draw.worldIndices.size() != 36) {
      std::cerr << "Mesh frustum culling failed\n";
      return 5;
    }
    draw.Clear();
    stats = scene.Submit(draw, camera, {1280, 720}, {false});
    if (stats.visibleMeshInstances != 2 || stats.culledMeshInstances != 0 ||
        draw.worldVertices.size() != 48 || draw.worldIndices.size() != 72) {
      std::cerr << "Frustum culling override failed\n";
      return 6;
    }
    auto boxCpu = std::make_shared<Mesh3D>(Mesh3D::CreateUnitBox());
    scene.SetBoxMesh({{7},
                      boxCpu->Bounds(),
                      boxCpu->vertices.size(),
                      boxCpu->indices.size(),
                      boxCpu});
    Scene3D normalScaleScene;normalScaleScene.SetBoxMesh({{7},boxCpu->Bounds(),boxCpu->vertices.size(),boxCpu->indices.size(),boxCpu});Material3D detailedMaterial;detailedMaterial.normalScale=-1.75f;normalScaleScene.MaterialBox({0,1,6},{2,2,2},detailedMaterial);DrawList normalScaleDraw;const Scene3DStats normalScaleStats=normalScaleScene.Submit(normalScaleDraw,camera,{1280,720});if(normalScaleStats.visibleMeshInstances!=1||normalScaleDraw.meshInstances.size()!=1||normalScaleDraw.meshInstances.front().normalScale!=-1.75f){std::cerr<<"Scene material normal scale propagation failed\n";return 39;}
    draw.Clear();
    stats = scene.Submit(draw, camera, {1280, 720});
    if (stats.visibleMeshInstances != 1 || draw.meshInstances.size() != 1 ||
        draw.meshInstances.front().mesh.value != 7 ||
        !draw.worldVertices.empty() || !draw.worldIndices.empty()) {
      std::cerr << "Resident scene mesh submission failed\n";
      return 7;
    }
    draw.Clear();
    stats = scene.Submit(draw, camera, {1280, 720},
                         {true, true, {true, .025f, .25f, 100.f, 8}});
    if (stats.shadowCasters || stats.shadowInstances || stats.shadowSkipped ||
        draw.meshInstances.size() != 1 ||
        draw.meshInstances.front().layer != MeshRenderLayer3D::Opaque ||
        !draw.meshInstances.front().castsShadow ||
        draw.meshInstances.front().shadowCascadeSelection != ShadowCascadeSelection3D::Automatic) {
      std::cerr << "Directional shadow scene routing failed\n";
      return 19;
    }
    draw.SetDirectionalShadowMapEnabled(false);
    draw.Clear();
    stats = scene.Submit(draw, camera, {1280, 720},
                         {true, true, {true, .025f, .25f, 100.f, 8}});
    if (stats.shadowCasters != 1 || stats.shadowInstances != 1 ||
        stats.shadowSkipped != 0 || draw.meshInstances.size() != 2 ||
        draw.meshInstances.front().layer != MeshRenderLayer3D::Overlay ||
        draw.meshInstances.back().layer != MeshRenderLayer3D::Opaque) {
      std::cerr << "Projected shadow fallback submission failed\n";
      return 26;
    }
    draw.SetDirectionalShadowMapEnabled(true);
    Scene3D shadowOnlyScene;
    shadowOnlyScene.SetBoxMesh({{7}, boxCpu->Bounds(), boxCpu->vertices.size(),
                                boxCpu->indices.size(), boxCpu});
    shadowOnlyScene.Box({20, 1, 6}, {2, 2, 2}, {1, 1, 1, 1});
    shadowOnlyScene.Box({24, 1, 6}, {2, 2, 2}, {1, 1, 1, 1});
    Scene3DSubmissionOptions shadowOnlyOptions{
        true, true, {true, .025f, .25f, 100.f, 8}};
    shadowOnlyOptions.maximumShadowOnlyInstances = 1;
    draw.Clear();
    stats = shadowOnlyScene.Submit(draw, camera, {1280, 720},
                                   shadowOnlyOptions);
    if (stats.visibleMeshInstances || stats.shadowOnlyCandidates != 2 ||
        stats.shadowOnlyInstances != 1 ||
        stats.shadowOnlyBudgetDropped != 1 || draw.meshInstances.size() != 1 ||
        draw.meshInstances.front().layer != MeshRenderLayer3D::ShadowOnly ||
        !draw.meshInstances.front().castsShadow ||
        draw.meshInstances.front().shadowCascadeSelection != ShadowCascadeSelection3D::Automatic) {
      std::cerr << "Bounded shadow-only scene routing failed\n";
      return 33;
    }
    Scene3D farCascadeScene;
    farCascadeScene.SetBoxMesh(
        {{7}, boxCpu->Bounds(), boxCpu->vertices.size(), boxCpu->indices.size(), boxCpu});
    farCascadeScene.Box({0, 1, 50}, {2, 2, 2}, {1, 1, 1, 1});
    draw.Clear();
    stats = farCascadeScene.Submit(draw, camera, {1280, 720});
    if (draw.meshInstances.size() != 1 ||
        draw.meshInstances.front().shadowCascadeSelection != ShadowCascadeSelection3D::Automatic) {
      std::cerr << "Far cascade scene routing failed\n";
      return 36;
    }
    Scene3D distantShadowScene;
    distantShadowScene.SetBoxMesh(
        {{7}, boxCpu->Bounds(), boxCpu->vertices.size(), boxCpu->indices.size(),
         boxCpu});
    distantShadowScene.Box({0, 1, 50}, {2, 2, 2}, {1, 1, 1, 1});
    draw.Clear();
    stats = distantShadowScene.Submit(
        draw, camera, {1280, 720},
        {true, true, {true, .025f, .25f, 10.f, 8}, {100.f, 0.f}});
    if (stats.visibleMeshInstances != 1 || stats.shadowDistanceRejected != 1 ||
        draw.meshInstances.size() != 1 ||
        draw.meshInstances.front().castsShadow) {
      std::cerr << "Distant color-only shadow routing failed\n";
      return 34;
    }
    draw.Clear();
    stats = scene.Submit(draw, camera, {1280, 720}, {true, true, {}, {5.f, 0}});
    if (stats.visibleMeshInstances != 1 ||
        stats.distanceCulledMeshInstances != 1 ||
        stats.culledMeshInstances != 1) {
      std::cerr << "Scene distance visibility budget failed\n";
      return 20;
    }
    scene.Clear();scene.SetBoxMesh({});auto pyramidCpu=std::make_shared<Mesh3D>(Mesh3D::CreateUnitPyramid());MeshResource3D pyramid{{9},pyramidCpu->Bounds(),pyramidCpu->vertices.size(),pyramidCpu->indices.size(),pyramidCpu};scene.Mesh(pyramid,{{0,1,6},{0,.4f,0},{2,3,2}},{{.4f,.8f,1,1},{.2f,.8f,.1f,.7f},true,{42},.5f});draw.Clear();stats=scene.Submit(draw,camera,{1280,720});
    if(stats.meshInstances!=1||stats.visibleMeshInstances!=1||stats.inputTriangles!=6||stats.visibleTriangles!=6||draw.meshInstances.size()!=1||draw.meshInstances.front().mesh.value!=9||draw.meshInstances.front().surface!=PackSurface3D({.2f,.8f,.1f,.7f})||draw.meshInstances.front().textures.value!=42||!Near(draw.meshInstances.front().textureScale,.5f)){std::cerr<<"Generic resident mesh material submission failed\n";return 8;}
    draw.Clear();stats=scene.Submit(draw,camera,{1280,720},{true,false});if(!draw.meshInstances.empty()||draw.worldVertices.size()!=16||draw.worldIndices.size()!=18){std::cerr<<"Generic mesh CPU fallback failed\n";return 9;}
    Scene3D tinyScene;tinyScene.Mesh(pyramid,{{0,1,100},{},{.01f,.01f,.01f}});draw.Clear();stats=tinyScene.Submit(draw,camera,{1280,720},{true,true,{}, {0,1.f}});if(stats.visibleMeshInstances!=0||stats.screenSizeCulledMeshInstances!=1||!draw.meshInstances.empty()){std::cerr<<"Scene projected-size visibility budget failed\n";return 21;}
    Scene3D queryScene;queryScene.Box({0,1,6},{2,2,2},{1,1,1,1});SceneQueryStats3D queryStats{};const Ray3D sceneRay=ScreenPointToRay(camera,{1280,720},{640,360});SceneHit3D hit=queryScene.Raycast(sceneRay,100,&queryStats);if(!hit||hit.objectType!=SceneObjectType3D::Box||hit.objectIndex!=0||!Near(hit.distance,5)||!Near(hit.position.z,5)||!Near(hit.normal.z,-1)||queryStats.boundsTested!=1||queryStats.boundsRejected!=0||queryStats.trianglesTested!=12||queryScene.Raycast(sceneRay,4.9f)){std::cerr<<"Scene box raycast failed\n";return 10;}
    hit=queryScene.Raycast({{20,1,0},{0,0,1}},100,&queryStats);if(hit||queryStats.boundsTested!=1||queryStats.boundsRejected!=1||queryStats.trianglesTested!=0){std::cerr<<"Scene raycast broad phase failed\n";return 11;}
    queryScene.Clear();queryScene.Triangle({-2,-1,8},{0,2,8},{2,-1,8},{1,1,1,1});queryScene.Triangle({-1,-1,4},{0,1,4},{1,-1,4},{1,1,1,1});hit=queryScene.Raycast({{0,0,0},{0,0,4}},100,&queryStats);if(!hit||hit.objectType!=SceneObjectType3D::Triangle||hit.objectIndex!=1||!Near(hit.distance,4)||queryStats.trianglesTested!=2){std::cerr<<"Nearest normalized scene raycast failed\n";return 12;}
    queryScene.Clear();auto queryMeshCpu=std::make_shared<Mesh3D>(Mesh3D::CreateUnitBox());MeshResource3D queryMesh{{10},queryMeshCpu->Bounds(),queryMeshCpu->vertices.size(),queryMeshCpu->indices.size(),queryMeshCpu};queryScene.Mesh(queryMesh,{{0,1,7},{0,.4f,0},{2,3,2}});hit=queryScene.Raycast({{0,1,0},{0,0,1}},100,&queryStats);if(!hit||hit.objectType!=SceneObjectType3D::Mesh||hit.objectIndex!=0||hit.distance<=5||hit.distance>=7||queryStats.boundsRejected!=0){std::cerr<<"Transformed mesh raycast failed\n";return 13;}
    Scene3D lodScene;
    auto lodNearCpu = std::make_shared<Mesh3D>(Mesh3D::CreateUnitBox()),
         lodFarCpu = std::make_shared<Mesh3D>(Mesh3D::CreateUnitPyramid());
    MeshResource3D lodNear{{21},
                           lodNearCpu->Bounds(),
                           lodNearCpu->vertices.size(),
                           lodNearCpu->indices.size(),
                           lodNearCpu},
        lodFar{{22},
               lodFarCpu->Bounds(),
               lodFarCpu->vertices.size(),
               lodFarCpu->indices.size(),
               lodFarCpu};
    const RenderObjectIdentity3D boxIdentity{{0x8100000000000001ull,1},71},meshIdentity{{0x8100000000000002ull,2},73},lodIdentity{{0x8100000000000003ull,3},79};
    Scene3D identityScene;identityScene.SetBoxMesh({{7},boxCpu->Bounds(),boxCpu->vertices.size(),boxCpu->indices.size(),boxCpu});
    if(!identityScene.MaterialBox({-2,1,6},{1,1,1},{},boxIdentity)||!identityScene.Mesh(lodNear,{{0,1,6}}, {}, {},meshIdentity)){std::cerr<<"Scene identity objects were rejected\n";return 40;}
    identityScene.MeshLod(lodNear,lodFar,MeshLodPolicy3D{10},{{2,1,6}}, {}, {}, {},lodIdentity);draw.Clear();stats=identityScene.Submit(draw,camera,{1280,720});
    if(draw.meshInstances.size()!=3||std::ranges::count(draw.meshInstances,boxIdentity,&MeshInstance3D::identity)!=1||std::ranges::count(draw.meshInstances,meshIdentity,&MeshInstance3D::identity)!=1||std::ranges::count(draw.meshInstances,lodIdentity,&MeshInstance3D::identity)!=1){std::cerr<<"Scene Box/Mesh/LOD identity pass-through failed\n";return 41;}
    const auto sceneOpacityUpdate=identityScene.SetObjectOcclusionOpacity(lodIdentity,.18f);draw.Clear();identityScene.Submit(draw,camera,{1280,720});const auto fadedLod=std::ranges::find(draw.meshInstances,lodIdentity,&MeshInstance3D::identity);if(!sceneOpacityUpdate||sceneOpacityUpdate.matchedStatic!=1||fadedLod==draw.meshInstances.end()||!Near(fadedLod->occlusionOpacity,.18f)||identityScene.MaterialBox({0,1,6},{1,1,1},{},{},.5f)){std::cerr<<"Scene exact identity opacity propagation failed\n";return 42;}
    lodScene.MeshLod(lodFar, lodNear, 10, {{0, 1, 6}, {}, {2, 2, 2}});
    if (lodScene.MeshCount() != 0) {
      std::cerr << "LOD must reject a more complex far mesh\n";
      return 14;
    }
    lodScene.MeshLod(lodNear, lodFar, 10, {{0, 1, 6}, {}, {2, 2, 2}});
    draw.Clear();
    stats = lodScene.Submit(draw, camera, {1280, 720});
    if (stats.lodMeshInstances != 1 || stats.nearLodInstances != 1 ||
        stats.farLodInstances != 0 || stats.lodTrianglesSaved != 0 ||
        draw.meshInstances.size() != 1 ||
        draw.meshInstances.front().mesh.value != 21) {
      std::cerr << "Near mesh LOD selection failed\n";
      return 15;
    }
    Camera3D farCamera = MakeFirstPersonCamera({0, 1, -20}, 0, 0);
    draw.Clear();
    stats = lodScene.Submit(draw, farCamera, {1280, 720});
    if (stats.nearLodInstances != 0 || stats.farLodInstances != 1 ||
        stats.visibleTriangles != 6 || stats.culledTriangles != 6 ||
        stats.lodTrianglesSaved != 6 || draw.meshInstances.size() != 1 ||
        draw.meshInstances.front().mesh.value != 22) {
      std::cerr << "Far mesh LOD selection failed\n";
      return 16;
    }
    MeshResource3D lodFarGpuOnly = lodFar;
    lodFarGpuOnly.cpuMesh.reset();
    Scene3D cpuFallbackLodScene;
    cpuFallbackLodScene.MeshLod(lodNear, lodFarGpuOnly, 10,
                                {{0, 1, 6}, {}, {2, 2, 2}});
    draw.Clear();
    stats = cpuFallbackLodScene.Submit(draw, farCamera, {1280, 720},
                                       {true, false});
    if (stats.visibleMeshInstances != 1 || stats.cpuFallbackUnavailable ||
        draw.worldIndices.size() != lodNear.indexCount ||
        !draw.meshInstances.empty()) {
      std::cerr << "GPU-only LOD CPU fallback failed\n";
      return 35;
    }
    hit = lodScene.Raycast({{0, 1, 0}, {0, 0, 1}}, 100, &queryStats);
    if (!hit || hit.objectIndex != 0 || queryStats.trianglesTested != 12) {
      std::cerr << "LOD raycast must use detailed collision mesh\n";
      return 17;
    }
    MeshResource3D lodMiddle{{23},lodFarCpu->Bounds(),lodFarCpu->vertices.size(),9,lodFarCpu},lodLowest{{24},lodFarCpu->Bounds(),lodFarCpu->vertices.size(),3,lodFarCpu};const std::array<MeshLodLevel3D,4> chainLevels{{{lodNear,{},8,2},{lodFar,{},16,2},{lodMiddle,{},24,2},{lodLowest,{}}}};const MeshLodChainSelection3D chainNear=EvaluateMeshLodChain3D(4,0,chainLevels,MeshLodDistanceMetric3D::Center),chainMiddle=EvaluateMeshLodChain3D(20,0,chainLevels,MeshLodDistanceMetric3D::Center),chainBlend=EvaluateMeshLodChain3D(16,0,chainLevels,MeshLodDistanceMetric3D::Center),chainFar=EvaluateMeshLodChain3D(30,0,chainLevels,MeshLodDistanceMetric3D::Center);if(chainNear.primaryLevel!=0||chainMiddle.primaryLevel!=2||!chainBlend.transition||chainBlend.nearLevel!=1||chainBlend.farLevel!=2||!Near(chainBlend.farCoverage,.5f)||chainFar.primaryLevel!=3){std::cerr<<"Multi-level LOD evaluation failed\n";return 32;}Scene3D chainScene;chainScene.MeshLodChain(chainLevels,MeshLodDistanceMetric3D::Center,{{0,1,20},{},{2,2,2}});draw.Clear();stats=chainScene.Submit(draw,camera,{1280,720});if(chainScene.MeshCount()!=1||stats.selectedLodLevels[2]!=1||stats.visibleTriangles!=3||stats.lodTrianglesSaved!=9||draw.meshInstances.size()!=1||draw.meshInstances.front().mesh.value!=23){std::cerr<<"Multi-level LOD submission failed\n";return 32;}hit=chainScene.Raycast({{0,1,0},{0,0,1}},100,&queryStats);if(!hit||queryStats.trianglesTested!=12){std::cerr<<"Multi-level LOD collision must use level zero\n";return 32;}const std::array<MeshLodLevel3D,3> overlappingLevels{{{lodNear,{},10,8},{lodFar,{},12,8},{lodMiddle,{}}}};chainScene.MeshLodChain(overlappingLevels,MeshLodDistanceMetric3D::Center,{});if(chainScene.MeshCount()!=1){std::cerr<<"Overlapping LOD transition bands were accepted\n";return 32;}
    auto overflowLevels=chainLevels;overflowLevels[0].transitionWidth=(std::numeric_limits<float>::max)();chainScene.MeshLodChain(overflowLevels,MeshLodDistanceMetric3D::Center,{});chainScene.MeshLodChain(chainLevels,static_cast<MeshLodDistanceMetric3D>(255),{});if(chainScene.MeshCount()!=1){std::cerr<<"Non-finite LOD transition or invalid metric was accepted\n";return 32;}for(std::size_t pair=0;pair+1<chainLevels.size();++pair){const float distance=chainLevels[pair].switchDistance+.25f;Scene3D pairScene;pairScene.MeshLodChain(chainLevels,MeshLodDistanceMetric3D::Center,{{0,1,distance}});draw.Clear();stats=pairScene.Submit(draw,camera,{1280,720});if(draw.meshInstances.size()!=2||stats.lodTransitionInstances!=1||stats.selectedLodLevels[pair+1]!=1||std::count_if(draw.meshInstances.begin(),draw.meshInstances.end(),[](const MeshInstance3D& instance){return instance.castsShadow;})!=1){std::cerr<<"Adjacent LOD transition submission failed\n";return 32;}const uint32_t firstCode=PackMeshInstanceLodData3D(draw.meshInstances[0].lodCoverage,draw.meshInstances[0].lodSeed),secondCode=PackMeshInstanceLodData3D(draw.meshInstances[1].lodCoverage,draw.meshInstances[1].lodSeed);if((firstCode&MeshInstanceLodCoverageMask3D)!=(secondCode&MeshInstanceLodCoverageMask3D)){std::cerr<<"Adjacent LOD transition codes diverged\n";return 32;}for(unsigned sample=0;sample<=255;++sample)if(MeshInstanceLodSampleSurvives3D(firstCode,float(sample)/255.f)==MeshInstanceLodSampleSurvives3D(secondCode,float(sample)/255.f)){std::cerr<<"Adjacent LOD transition was not complementary\n";return 32;}auto waterLevels=chainLevels;waterLevels[pair].material.materialClass=MaterialClass3D::Water;Scene3D waterScene;waterScene.MeshLodChain(waterLevels,MeshLodDistanceMetric3D::Center,{{0,1,distance}});draw.Clear();stats=waterScene.Submit(draw,camera,{1280,720});if(draw.meshInstances.size()!=1||stats.lodTransitionInstances||stats.lodTransitionBudgetFallbacks!=1){std::cerr<<"Water LOD transition did not use hard cut\n";return 32;}Scene3DSubmissionOptions zeroBudget{};zeroBudget.maximumLodTransitionInstances=0;draw.Clear();stats=pairScene.Submit(draw,camera,{1280,720},zeroBudget);if(draw.meshInstances.size()!=1||stats.lodTransitionInstances||stats.lodTransitionBudgetFallbacks!=1||draw.meshInstances.front().mesh.value!=chainLevels[pair+1].mesh.handle.value){std::cerr<<"Adjacent LOD budget fallback failed\n";return 32;}}
    Scene3D culledChainScene;culledChainScene.MeshLodChain(chainLevels,MeshLodDistanceMetric3D::Center,{{1000,1,20}});draw.Clear();stats=culledChainScene.Submit(draw,camera,{1280,720});if(stats.nearLodInstances||stats.farLodInstances||std::ranges::any_of(stats.selectedLodLevels,[](std::size_t count){return count!=0;})){std::cerr<<"Culled LOD polluted selection statistics\n";return 32;}
    Scene3D memoryScene;constexpr std::size_t StressCount=4096;for(std::size_t index=0;index<StressCount;++index){const float x=float(index%64),z=float(index/64);memoryScene.Triangle({x,0,z},{x,1,z},{x+1,0,z},{1,1,1,1});memoryScene.Line({x,0,z},{x,1,z},1,{1,1,1,1});memoryScene.Box({x,0,z},{1,1,1},{1,1,1,1});memoryScene.Mesh(lodNear,{{x,0,z}});memoryScene.MeshLod(lodNear,lodFar,10,{{x,0,z}});}const ReusableMemoryStats peakSceneMemory=memoryScene.MemoryStats();memoryScene.Clear();memoryScene.Reserve({8,8,8,8,8});const ReusableMemoryStats reclaimableSceneMemory=memoryScene.MemoryStats();const std::size_t releasedSceneBytes=memoryScene.TrimUnusedMemory();const ReusableMemoryStats trimmedSceneMemory=memoryScene.MemoryStats();if(!reclaimableSceneMemory.reclaimableCapacityBytes||!releasedSceneBytes||trimmedSceneMemory.retainedCapacityBytes>=peakSceneMemory.retainedCapacityBytes||trimmedSceneMemory.reclaimableCapacityBytes||trimmedSceneMemory.lastReleasedBytes!=releasedSceneBytes||trimmedSceneMemory.trimCount!=1){std::cerr<<"Scene workspace memory recovery failed\n";return 22;}memoryScene.Triangle({0,0,5},{0,1,5},{1,0,5},{1,1,1,1});memoryScene.Line({0,0,5},{0,1,5},1,{1,1,1,1});memoryScene.Box({0,0,5},{1,1,1},{1,1,1,1});memoryScene.Mesh(lodNear,{{0,0,5}});memoryScene.MeshLod(lodNear,lodFar,10,{{0,0,5}});memoryScene.Reserve({1,1,1,1,1});const std::size_t activeReleasedSceneBytes=memoryScene.TrimUnusedMemory();const ReusableMemoryStats activeTrimmedSceneMemory=memoryScene.MemoryStats();if(!activeReleasedSceneBytes||memoryScene.TriangleCount()!=37||memoryScene.LineCount()!=1||memoryScene.MeshCount()!=3||activeTrimmedSceneMemory.reclaimableCapacityBytes||activeTrimmedSceneMemory.trimCount!=2){std::cerr<<"Active scene workspace preservation failed\n";return 23;}std::cout<<"scenePeakBytes="<<peakSceneMemory.retainedCapacityBytes<<" sceneReleasedBytes="<<releasedSceneBytes+activeReleasedSceneBytes<<" sceneRetainedBytes="<<activeTrimmedSceneMemory.retainedCapacityBytes<<'\n';
    const MeshLodSelection3D hardNear=EvaluateMeshLod3D(9,2,{10,0,MeshLodDistanceMetric3D::Center}),hardFar=EvaluateMeshLod3D(10,2,{10,0,MeshLodDistanceMetric3D::Center}),surfaceNear=EvaluateMeshLod3D(11,2,{10,0,MeshLodDistanceMetric3D::BoundsSurface}),blend=EvaluateMeshLod3D(10,2,{10,4,MeshLodDistanceMetric3D::Center}),invalidSelection=EvaluateMeshLod3D(std::numeric_limits<float>::quiet_NaN(),2,{10,4});if(!hardNear.submitNear||hardNear.submitFar||hardFar.submitNear||!hardFar.submitFar||!surfaceNear.submitNear||surfaceNear.submitFar||!blend.submitNear||!blend.submitFar||!Near(blend.nearCoverage,.5f)||!Near(blend.farCoverage,.5f)||!invalidSelection.submitNear||invalidSelection.submitFar){std::cerr<<"LOD policy evaluation failed\n";return 24;}
    Scene3D transitionScene;transitionScene.MeshLod(lodNear,lodFar,{10,4,MeshLodDistanceMetric3D::Center},{{0,1,10},{},{2,2,2}});draw.Clear();stats=transitionScene.Submit(draw,camera,{1280,720});if(stats.lodTransitionInstances!=1||stats.lodDualSubmittedInstances!=1||stats.lodTransitionTriangles!=18||stats.visibleTriangles!=18||stats.farLodInstances!=1||draw.meshInstances.size()!=2||!Near(draw.meshInstances[0].lodCoverage,.5f)||!Near(draw.meshInstances[1].lodCoverage,.5f)||(draw.meshInstances[0].lodSeed&7u)!=(draw.meshInstances[1].lodSeed&7u)||(draw.meshInstances[0].lodSeed&8u)==0||(draw.meshInstances[1].lodSeed&8u)!=0||draw.meshInstances[0].castsShadow==draw.meshInstances[1].castsShadow){std::cerr<<"LOD dither transition submission failed\n";return 25;}
    Scene3DSubmissionOptions noTransitionBudget{};noTransitionBudget.maximumLodTransitionInstances=0;draw.Clear();stats=transitionScene.Submit(draw,camera,{1280,720},noTransitionBudget);if(stats.lodTransitionInstances||stats.lodTransitionBudgetFallbacks!=1||draw.meshInstances.size()!=1||draw.meshInstances.front().mesh.value!=22||draw.meshInstances.front().lodCoverage!=1.f){std::cerr<<"LOD transition budget fallback failed\n";return 26;}
    Scene3D visibilityBudgetScene;visibilityBudgetScene.MeshLod(lodNear,lodFar,{10,4,MeshLodDistanceMetric3D::Center},{{1000,1,10},{},{2,2,2}});visibilityBudgetScene.MeshLod(lodNear,lodFar,{10,4,MeshLodDistanceMetric3D::Center},{{0,1,10},{},{2,2,2}});Scene3DSubmissionOptions oneTransition{};oneTransition.maximumLodTransitionInstances=1;draw.Clear();stats=visibilityBudgetScene.Submit(draw,camera,{1280,720},oneTransition);if(stats.culledMeshInstances!=1||stats.lodTransitionInstances!=1||stats.lodTransitionBudgetFallbacks||draw.meshInstances.size()!=2){std::cerr<<"Culled LOD consumed transition budget\n";return 27;}
    draw.Clear();draw.meshInstances.resize(MaximumMeshInstancesPerFrame3D-3);stats=transitionScene.Submit(draw,camera,{1280,720});if(stats.lodTransitionInstances||stats.lodTransitionBudgetFallbacks!=1||draw.meshInstances.size()!=MaximumMeshInstancesPerFrame3D-2||draw.meshInstances.back().mesh.value!=22||draw.meshInstances.back().lodCoverage!=1.f){std::cerr<<"LOD pair capacity fallback failed\n";return 28;}
    Scene3D asymmetricTransitionScene;asymmetricTransitionScene.MeshLod(lodNear,lodFar,{10,4,MeshLodDistanceMetric3D::Center},{{0,1,9},{},{2,2,2}});draw.Clear();stats=asymmetricTransitionScene.Submit(draw,camera,{1280,720});if(draw.meshInstances.size()!=2){std::cerr<<"Asymmetric LOD transition was not submitted\n";return 29;}const uint32_t nearTransition=PackMeshInstanceLodData3D(draw.meshInstances[0].lodCoverage,draw.meshInstances[0].lodSeed),farTransition=PackMeshInstanceLodData3D(draw.meshInstances[1].lodCoverage,draw.meshInstances[1].lodSeed);if((nearTransition&MeshInstanceLodCoverageMask3D)!=(farTransition&MeshInstanceLodCoverageMask3D)){std::cerr<<"Scene LOD transition codes diverged\n";return 30;}for(unsigned sample=0;sample<=255;++sample){const float threshold=float(sample)/255.f;if(MeshInstanceLodSampleSurvives3D(nearTransition,threshold)==MeshInstanceLodSampleSurvives3D(farTransition,threshold)){std::cerr<<"Scene LOD transition coverage was not complementary\n";return 31;}}
    Scene3D terrainVisibilityScene;
    const MeshBounds3D terrainBounds{{0,0,0},362.1f,{256,8,256}};
    MeshResource3D terrainResource{{91},terrainBounds,4,6,boxCpu};
    Material3D terrainMaterial{};
    terrainMaterial.materialClass=MaterialClass3D::Terrain;
    const RenderObjectIdentity3D terrainIdentity{{0xaec047454e560001ull,1},1};
    terrainVisibilityScene.Mesh(terrainResource,{{0,-1,0}},terrainMaterial,{},terrainIdentity);
    Camera3D terrainCamera=MakeFirstPersonCamera({0,4,5},0,0);
    DrawList terrainDraw;
    terrainDraw.SetDirectionalShadowMapEnabled(true);
    Scene3DSubmissionOptions terrainOptions{true,true,{true,.025f,.25f,600.f,8},{1000.f,.2f}};
    const Scene3DStats terrainStats=terrainVisibilityScene.Submit(terrainDraw,terrainCamera,{1920,1080},terrainOptions);
    if(terrainStats.visibleMeshInstances!=1||terrainStats.screenSizeCulledMeshInstances||terrainDraw.meshInstances.size()!=1||terrainDraw.meshInstances.front().layer!=MeshRenderLayer3D::Opaque||terrainDraw.meshInstances.front().identity.id!=terrainIdentity.id){std::cerr<<"Camera-plane crossing terrain was incorrectly routed to shadow-only\n";return 40;}
    const auto terrainLayerForCamera=[&](const Camera3D& testCamera){DrawList testDraw;testDraw.SetDirectionalShadowMapEnabled(true);terrainVisibilityScene.Submit(testDraw,testCamera,{1920,1080},terrainOptions);const auto found=std::ranges::find_if(testDraw.meshInstances,[&](const MeshInstance3D& instance){return instance.identity.id==terrainIdentity.id;});return found==testDraw.meshInstances.end()?MeshRenderLayer3D::Overlay:found->layer;};
    const Camera3D thirdTerrainCamera=MakeThirdPersonCamera({0,2,10},0,.25f,12.f);
    const Camera3D godTerrainCamera=MakeOverheadCamera({0,0,0},42.f,true);
    if(terrainLayerForCamera(terrainCamera)!=MeshRenderLayer3D::Opaque||terrainLayerForCamera(thirdTerrainCamera)!=MeshRenderLayer3D::Opaque||terrainLayerForCamera(godTerrainCamera)!=MeshRenderLayer3D::Opaque){std::cerr<<"Terrain identity was not opaque in every supported camera mode\n";return 41;}
    const auto classifiedLayer=[&](MeshBounds3D bounds,Transform3D transform){Scene3D testScene;MeshResource3D resource{{92},bounds,4,6,boxCpu};testScene.Mesh(resource,transform,terrainMaterial);DrawList testDraw;testDraw.SetDirectionalShadowMapEnabled(true);testScene.Submit(testDraw,terrainCamera,{1920,1080},terrainOptions);return testDraw.meshInstances.empty()?MeshRenderLayer3D::Overlay:testDraw.meshInstances.front().layer;};
    if(classifiedLayer({{0,0,0},1.f,{1,1,1}},{{0,4,5.05f}})!=MeshRenderLayer3D::Opaque){std::cerr<<"Near-plane crossing sphere was culled\n";return 42;}
    if(classifiedLayer({{0,0,0},.001f,{.001f,.001f,.001f}},{{0,4,105.f}})!=MeshRenderLayer3D::ShadowOnly){std::cerr<<"Fully-ahead tiny sphere bypassed projected-size culling\n";return 43;}
    if(classifiedLayer({{0,0,0},1.f,{1,1,1}},{{0,4,-5.f}})!=MeshRenderLayer3D::ShadowOnly){std::cerr<<"Fully-behind sphere bypassed visibility culling\n";return 44;}
    Scene3D provenanceScene;const RenderObjectIdentity3D identityA{{77,1},3},identityB{{77,2},4};provenanceScene.Mesh(lodNear,{{-1,1,6}}, {},{},identityA);provenanceScene.Mesh(lodNear,{{1,1,6}}, {},{},identityB);DrawList provenanceDraw;const Scene3DStats provenanceFirst=provenanceScene.Submit(provenanceDraw,camera,{1280,720});provenanceDraw.Clear();const Scene3DStats provenanceRepeat=provenanceScene.Submit(provenanceDraw,camera,{1280,720});if(!provenanceFirst.submissionSerial||provenanceRepeat.submissionSerial!=provenanceFirst.submissionSerial+1||!provenanceFirst.drawListRevision||provenanceFirst.drawListRevision!=provenanceRepeat.drawListRevision||!provenanceFirst.acceptedIdentityOrderFingerprint||provenanceFirst.acceptedIdentityOrderFingerprint!=provenanceRepeat.acceptedIdentityOrderFingerprint||provenanceFirst.acceptedIdentityCount!=2){std::cerr<<"Scene submission provenance was not stable and monotonic\n";return 45;}provenanceScene.Clear();provenanceScene.Mesh(lodNear,{{1,1,6}}, {},{},identityB);provenanceScene.Mesh(lodNear,{{-1,1,6}}, {},{},identityA);provenanceDraw.Clear();const Scene3DStats provenanceReordered=provenanceScene.Submit(provenanceDraw,camera,{1280,720});if(provenanceReordered.submissionSerial!=provenanceRepeat.submissionSerial+1||provenanceReordered.acceptedIdentityOrderFingerprint==provenanceFirst.acceptedIdentityOrderFingerprint||provenanceReordered.drawListRevision==provenanceFirst.drawListRevision){std::cerr<<"Scene submission provenance did not detect accepted-order change\n";return 46;}
    return 0;
}
