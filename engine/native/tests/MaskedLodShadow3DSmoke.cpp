#include <Aevrynd/Scene3D.h>
#include <iostream>
#include <array>
using namespace Aevrynd;
int main() {
    auto cpu=std::make_shared<Mesh3D>(Mesh3D::CreateUnitBox());
    std::cout<<"FIXTURE vertices="<<cpu->vertices.size()<<" indices="<<cpu->indices.size()<<" radius="<<cpu->Bounds().radius<<'\n';
    MeshResource3D nearMesh{{21},cpu->Bounds(),cpu->vertices.size(),cpu->indices.size(),cpu};
    MeshResource3D farMesh{{22},cpu->Bounds(),cpu->vertices.size(),cpu->indices.size(),cpu};
    unsigned cases{}, failures{}, missingSamples{};
    for(float distance:std::array{8.9f,9.f,9.1f,11.99f,12.f,12.01f,14.9f,15.f,15.1f})
    for(bool culled:{false,true}) for(bool linear:{false,true}) {
        Camera3D camera=MakeFirstPersonCamera({0,1,0},0,0);
        if(culled)camera.target={0,1,-1};
        // 僅檢查 CPU 提交：遮罩材質仍須提供非空紋理識別與 UV，未上傳 GPU。
        Material3D material{};material.alphaMode=MaterialAlphaMode3D::Mask;material.textures={7};material.useUv=true;
        std::array<MeshLodLevel3D,2> levels{{{nearMesh,material,12.f,6.f},{farMesh,material}}};
        Scene3D scene;const auto key=scene.MeshLodChain(levels,MeshLodDistanceMetric3D::BoundsSurface,{{0,1,distance+cpu->Bounds().radius}});
        if(!key||scene.MeshCount()!=1){std::cerr<<"Fixture registration rejected\n";return 2;}
        DrawList draw;draw.SetDirectionalShadowMapEnabled(true);
        Scene3DSubmissionOptions options{};options.shadows.enabled=true;options.shadows.maximumDistance=100.f;options.linearVisibilityReference=linear;
        const auto stats=scene.Submit(draw,camera,{1280,720},options);
        const auto selection=EvaluateMeshLodChain3D(distance+cpu->Bounds().radius,cpu->Bounds().radius,levels,MeshLodDistanceMetric3D::BoundsSurface);
        unsigned casters{}, holes{}, overlap{};
        for(const auto& item:draw.meshInstances)if(item.castsShadow)++casters;
        for(unsigned sample=0;sample<=255;++sample) {
            unsigned survivors{};
            for(const auto& item:draw.meshInstances)if(item.castsShadow && MeshInstanceLodSampleSurvives3D(PackMeshInstanceLodData3D(item.lodCoverage,item.lodSeed),float(sample)/255.f))++survivors;
            holes+=survivors==0;overlap+=survivors>1;
        }
        const auto expected=selection.transition?2u:1u;
        const bool valid=draw.meshInstances.size()==expected&&casters==expected&&!holes&&!overlap&&
            (!culled||(!stats.nearLodInstances&&!stats.farLodInstances&&stats.visibleMeshInstances==0))&&
            !stats.lodTransitionBudgetFallbacks;
        ++cases;failures+=!valid;missingSamples+=holes;
        std::cout<<"CASE surface="<<distance<<" culled="<<culled<<" linear="<<linear<<" transition="<<selection.transition<<" instances="<<draw.meshInstances.size()<<" casters="<<casters<<" holes="<<holes<<" overlap="<<overlap<<" valid="<<valid<<'\n';
    }
    for(float distance:{11.99f,12.01f}) for(bool culled:{false,true})
    for(unsigned castMask=0;castMask<4;++castMask) for(unsigned budget=0;budget<=2;++budget)
    for(unsigned transitionBudget:{0u,1u}) for(unsigned room:{0u,1u,2u,4u}) {
        Camera3D camera=MakeFirstPersonCamera({0,1,0},0,0);if(culled)camera.target={0,1,-1};
        Material3D nearMat{},farMat{};nearMat.castsShadow=(castMask&1)!=0;farMat.castsShadow=(castMask&2)!=0;
        nearMat.alphaMode=farMat.alphaMode=MaterialAlphaMode3D::Mask;nearMat.textures=farMat.textures={7};nearMat.useUv=farMat.useUv=true;
        std::array<MeshLodLevel3D,2> levels{{{nearMesh,nearMat,12.f,6.f},{farMesh,farMat}}};
        Scene3D scene;if(!scene.MeshLodChain(levels,MeshLodDistanceMetric3D::BoundsSurface,{{0,1,distance+cpu->Bounds().radius}}))return 2;
        DrawList draw;draw.SetDirectionalShadowMapEnabled(true);draw.meshInstances.resize(MaximumMeshInstancesPerFrame3D-room);
        const auto initial=draw.meshInstances.size();
        Scene3DSubmissionOptions options{};options.shadows.enabled=true;options.shadows.maximumDistance=100.f;
        options.maximumShadowOnlyInstances=budget;options.maximumLodTransitionInstances=transitionBudget;
        const auto stats=scene.Submit(draw,camera,{1280,720},options);
        const bool primaryFar=distance>=12.f;
        const auto primary=primaryFar?farMat:nearMat;
        const unsigned required=unsigned(nearMat.castsShadow)+unsigned(farMat.castsShadow);
        const bool pair=culled?(required&&transitionBudget&&room>=required&&budget>=required):(transitionBudget&&room>=4);
        const unsigned expected=culled?(pair?required:(primary.castsShadow&&budget&&room?1u:0u)):(pair?2u:unsigned(room!=0));
        const unsigned expectedCasters=culled?expected:(pair?required:unsigned(primary.castsShadow&&room));
        unsigned casters{};for(std::size_t i=initial;i<draw.meshInstances.size();++i)casters+=draw.meshInstances[i].castsShadow;
        bool valid=draw.meshInstances.size()-initial==expected&&casters==expectedCasters&&draw.meshInstances.size()<=MaximumMeshInstancesPerFrame3D;
        if(culled)valid=valid&&stats.shadowOnlyInstances<=budget&&!stats.nearLodInstances&&!stats.farLodInstances;
        if(!pair&&expected)valid=valid&&draw.meshInstances.back().lodCoverage==1.f;
        const unsigned expectedFallback=unsigned(!pair&&(!culled||required));
        valid=valid&&stats.lodTransitionBudgetFallbacks==expectedFallback;
        valid=valid&&stats.lodDualSubmittedInstances==unsigned(pair&&(!culled||required==2));
        ++cases;failures+=!valid;
        if(!valid)std::cout<<"BUDGET_FAIL surface="<<distance<<" culled="<<culled<<" casts="<<castMask<<" budget="<<budget<<" transition="<<transitionBudget<<" room="<<room<<" expected="<<expected<<" actual="<<draw.meshInstances.size()-initial<<" fallback="<<stats.lodTransitionBudgetFallbacks<<'\n';
    }
    for(unsigned side:{0u,1u}) for(unsigned error=0;error<4;++error) for(bool culled:{false,true}) for(bool linear:{false,true}) {
        Camera3D camera=MakeFirstPersonCamera({0,1,0},0,0);if(culled)camera.target={0,1,-1};
        Material3D material{};material.alphaMode=MaterialAlphaMode3D::Mask;material.textures={7};material.useUv=true;
        std::array<MeshLodLevel3D,2> levels{{{nearMesh,material,12.f,6.f},{farMesh,material}}};
        auto& bad=levels[side].material;
        if(error==0)bad.useUv=false;else if(error==1)bad.textures={};else if(error==2)bad.alphaCutoff=std::numeric_limits<float>::quiet_NaN();else bad.alphaMode=MaterialAlphaMode3D::Blend;
        Scene3D scene;if(!scene.MeshLodChain(levels,MeshLodDistanceMetric3D::BoundsSurface,{{0,1,12.f+cpu->Bounds().radius}}))return 2;
        DrawList draw;draw.SetDirectionalShadowMapEnabled(true);Scene3DSubmissionOptions options{};options.shadows.enabled=true;options.shadows.maximumDistance=100;options.linearVisibilityReference=linear;
        const auto stats=scene.Submit(draw,camera,{1280,720},options);
        const bool valid=draw.meshInstances.empty()&&stats.cpuFallbackUnavailable==1&&!stats.lodTransitionInstances&&!stats.lodDualSubmittedInstances;
        ++cases;failures+=!valid;if(!valid)std::cout<<"INVALID_PAIR_FAIL side="<<side<<" error="<<error<<" culled="<<culled<<'\n';
    }
    for(bool culled:{false,true}) for(bool linear:{false,true}) {
        Camera3D camera=MakeFirstPersonCamera({0,1,0},0,0);if(culled)camera.target={0,1,-1};
        Material3D material{};material.alphaMode=MaterialAlphaMode3D::Mask;material.textures={7};material.useUv=true;
        std::array<MeshLodLevel3D,2> levels{{{nearMesh,material,12.f,6.f},{farMesh,material}}};
        Scene3D scene;if(!scene.MeshLodChain(levels,MeshLodDistanceMetric3D::BoundsSurface,{{0,1,12.f+cpu->Bounds().radius}}))return 2;
        DrawList draw;draw.SetDirectionalShadowMapEnabled(true);draw.meshInstances.resize(MaximumMeshInstancesPerFrame3D+1);
        Scene3DSubmissionOptions options{};options.shadows.enabled=true;options.shadows.maximumDistance=100;options.linearVisibilityReference=linear;
        scene.Submit(draw,camera,{1280,720},options);
        const bool valid=draw.meshInstances.size()==MaximumMeshInstancesPerFrame3D+1;++cases;failures+=!valid;
        if(!valid)std::cout<<"OVER_CAP_APPEND_FAIL culled="<<culled<<" linear="<<linear<<'\n';
    }
    for(unsigned modes=0;modes<4;++modes) for(bool culled:{false,true}) for(bool linear:{false,true}) {
        Camera3D camera=MakeFirstPersonCamera({0,1,0},0,0);if(culled)camera.target={0,1,-1};
        Material3D nearMat{},farMat{};nearMat.alphaMode=(modes&1)?MaterialAlphaMode3D::Mask:MaterialAlphaMode3D::Opaque;
        farMat.alphaMode=(modes&2)?MaterialAlphaMode3D::Mask:MaterialAlphaMode3D::Opaque;
        nearMat.textures=farMat.textures={7};nearMat.useUv=farMat.useUv=true;
        std::array<MeshLodLevel3D,2> levels{{{nearMesh,nearMat,12.f,6.f},{farMesh,farMat}}};
        Scene3D scene;if(!scene.MeshLodChain(levels,MeshLodDistanceMetric3D::BoundsSurface,{{0,1,12.f+cpu->Bounds().radius}}))return 2;
        DrawList draw;draw.SetDirectionalShadowMapEnabled(true);Scene3DSubmissionOptions options{};options.shadows.enabled=true;options.shadows.maximumDistance=100;options.linearVisibilityReference=linear;
        scene.Submit(draw,camera,{1280,720},options);
        unsigned casters{};for(const auto& item:draw.meshInstances)casters+=item.castsShadow;
        const unsigned expectedCasters=modes==3?2u:1u,expectedInstances=culled?expectedCasters:2u;
        const bool valid=draw.meshInstances.size()==expectedInstances&&casters==expectedCasters;++cases;failures+=!valid;
        if(!valid)std::cout<<"PIPELINE_ROUTING_FAIL modes="<<modes<<" culled="<<culled<<" linear="<<linear<<'\n';
    }
    std::cout<<"SUMMARY cases="<<cases<<" failures="<<failures<<" missing_samples="<<missingSamples<<'\n';
    return failures?1:0;
}
