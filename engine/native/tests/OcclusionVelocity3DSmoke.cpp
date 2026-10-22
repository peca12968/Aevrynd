#include <Aevrynd/Renderer.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace Aevrynd;
namespace {
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(int argc,char** argv){
    const bool expectGbv=argc==2&&std::string(argv[1])=="--gbv";
    if(argc>1&&!expectGbv)return 2;
    if(!SetEnvironmentVariableW(L"AEVRYND_GPU_VALIDATION",expectGbv?L"1":L"0"))return 2;
    HWND window=CreateWindowExW(0,L"STATIC",L"OcclusionVelocity",WS_OVERLAPPED,0,0,128,128,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return 1;
    int result{};
    try{
        D3D12Renderer renderer;renderer.Initialize(window,128,128,128,128,false);renderer.SetQualitySettings({false,false,false,false});
        const uint32_t normal=PackNormalSnorm8({0,0,-1});
        const std::array<Vertex3D,4> vertices{{{-.95f,-.95f,0,normal,~0u,0,1},{-.95f,.95f,0,normal,~0u,0,0},{.95f,.95f,0,normal,~0u,1,0},{.95f,-.95f,0,normal,~0u,1,1}}};
        const std::array<uint32_t,6> indices{0,1,2,0,2,3};
        const auto mesh=renderer.CreateMesh(vertices,indices);Need(bool(mesh),"STATIC_MESH");
        std::array<SkinnedVertex3D,4> skinVertices{};
        for(size_t i=0;i<vertices.size();++i){auto& v=skinVertices[i];v.x=vertices[i].x;v.y=vertices[i].y;v.z=0;v.normal=normal;v.color=~0u;v.u=vertices[i].u;v.v=vertices[i].v;v.weights[0]=65535;}
        const SkeletonCompatibilityId3D compatibility{0x4d574f434356454cull};
        const auto skin=renderer.CreateSkinnedMesh(skinVertices,indices,1,compatibility);Need(bool(skin),"SKINNED_MESH");
        const auto root=std::filesystem::path{AEVRYND_SOURCE_DIR}/"games/character_lab/assets/material-packs/original/hair";
        MaterialTextureSetDefinition3D material{root/"base-color.png",root/"roughness.png",512,root/"normal-dx.png"};material.alphaMode=MaterialAlphaMode3D::Mask;material.alphaCutoff=.5f;
        const auto textures=renderer.CreateMaterialTextureSet(material);Need(bool(textures),"MASK_MATERIAL");
        SkinPaletteJoint3D joint{};joint.skin=Matrix4::Identity();joint.normalRows={Vec4{1,0,0,0},Vec4{0,1,0,0},Vec4{0,0,1,0}};
        std::ofstream samples("velocity-depth.tsv");samples.precision(9);samples<<"case\tx\ty\tdepth\tvx\tvy\texpected_vx\n";
        size_t checked{},caseCount{};
        for(bool skinned:{false,true})for(bool masked:{false,true})for(float uvScale:{.25f,1.f})for(unsigned policy=0;policy<3;++policy){
            const std::string label=std::string(skinned?"skinned":"static")+(masked?"-mask":"-opaque")+(uvScale<1?"-quarter":"-full")+"-"+std::to_string(policy);
            // 同一幀的可見深度是速度歸屬依據；不是以提交數或CPU遮罩替代GPU畫面。
            DrawList draw;draw.SetViewProjection(Matrix4::Identity());draw.SetBackfaceCulling(false);
            Atmosphere3D atmosphere{};atmosphere.cameraPosition={0,0,-2};draw.SetAtmosphere(atmosphere);
            draw.SetDirectionalLight({{0,0,1},.4f,{1,1,1,1},1.f});
            draw.MeshInstance(mesh,CreateTranslationMatrix({0,0,.8f}),{.12f,.28f,.48f,1});
            auto& background=draw.meshInstances.back();background.identity={{0x4d574f4343ull,1},1};background.castsShadow=false;background.motionValid=true;background.previousTransform=background.transform;
            const auto current=CreateTranslationMatrix({.1f,0,.3f})*CreateScaleMatrix({.8f,.8f,1});
            const auto previous=CreateTranslationMatrix({0,0,.3f})*CreateScaleMatrix({.8f,.8f,1});
            const float opacity=policy==0?1.f:0.f;
            if(skinned){
                const auto palette=draw.AddSkinPalette(std::span<const SkinPaletteJoint3D>{&joint,1},compatibility);
                Need(draw.SkinnedMeshInstance(skin,palette,current,{.75f,.32f,.08f,1}),"SKINNED_SUBMIT");auto& item=draw.skinnedMeshInstances.back();
                item.previousPalette=palette;item.previousTransform=previous;item.motionValid=true;item.identity={{0x4d574f4343ull,2},1};item.castsShadow=false;item.occlusionOpacity=opacity;
                if(masked){item.textures=textures;item.useUv=true;item.baseTextureScale=uvScale;item.detailTextureScale=uvScale;item.alphaMode=MaterialAlphaMode3D::Mask;item.alphaCutoff=.5f;item.doubleSided=true;}
            }else{
                draw.MeshInstance(mesh,current,{.75f,.32f,.08f,1});auto& item=draw.meshInstances.back();item.previousTransform=previous;item.motionValid=true;item.identity={{0x4d574f4343ull,2},1};item.castsShadow=false;item.occlusionOpacity=opacity;
                if(masked){item.textures=textures;item.useUv=true;item.textureScale=uvScale;item.alphaMode=MaterialAlphaMode3D::Mask;item.alphaCutoff=.5f;item.doubleSided=true;}
            }
            if(policy==2)draw.SetScreenOcclusionWindow({true,{.5f,.5f},{.22f,.30f},.6f,.65f});
            if(caseCount==0)Need(renderer.CaptureNextFrame("static-without-readback.png"),"WARM_COLOR_CONTROL_REQUEST");
            renderer.Render(draw,{0,0,0,1});
            if(caseCount==0){const auto warm=renderer.GetStats();std::cout<<"WARM_COLOR_CONTROL accepted="<<warm.successfulFrameCaptures<<" rejected="<<warm.rejectedFrameCaptures<<'\n';}
            Need(renderer.RequestVelocityReadback3D()&&renderer.RequestDepthReadback3D({++caseCount,0x4d574f4343ull,caseCount}),"READBACK_REQUEST");
            Need(renderer.CaptureNextFrame(std::filesystem::path(label+".png")),"ACTUAL_CAPTURE_REQUEST");
            renderer.Render(draw,{0,0,0,1});auto velocity=renderer.TakeVelocityReadback3D();auto depth=renderer.TakeDepthReadback3D();
            Need(velocity&&depth&&depth->Complete()&&velocity->width==128&&velocity->height==128&&velocity->pixels.size()==16384,"ACTUAL_READBACK_COMPLETE");
            size_t foreground{},back{},mismatch{};
            for(unsigned y=16;y<112;++y)for(unsigned x=16;x<112;++x){
                const auto index=size_t(y)*128+x;const float z=depth->pixels[index];const auto v=velocity->pixels[index];
                Need(std::isfinite(z)&&std::isfinite(v.x)&&std::isfinite(v.y),"NONFINITE_GPU_SAMPLE");
                const bool front=std::abs(z-.3f)<.001f;Need(front||std::abs(z-.8f)<.001f,"UNKNOWN_VISIBLE_SURFACE");
                foreground+=front;back+=!front;const float expected=front?6.4f:0.f;
                const bool bad=std::abs(v.x-expected)>.08f||std::abs(v.y)>.02f;mismatch+=bad;++checked;
                samples<<label<<'\t'<<x<<'\t'<<y<<'\t'<<z<<'\t'<<v.x<<'\t'<<v.y<<'\t'<<expected<<'\n';
            }
            samples.flush();Need(bool(samples),"SAMPLE_RECEIPT_WRITE");
            std::cout<<"VELOCITY_VISIBILITY_CASE "<<label<<" foreground="<<foreground<<" background="<<back<<" mismatch="<<mismatch<<'\n';
            if(mismatch)throw std::runtime_error("OCCLUSION_VELOCITY_PIXEL_MISMATCH "+label+" count="+std::to_string(mismatch));
            if(policy==0)Need(foreground>500,"FULL_OPACITY_CONTROL_VISIBLE");
            if(policy==1)Need(foreground==0&&back==9216,"GLOBAL_ZERO_OPACITY_CONTROL_HIDDEN");
            if(policy==2)Need(foreground>200&&back>400,"LOCAL_WINDOW_KEEPS_SURROUNDING_SURFACE");
        }
        const auto stats=renderer.GetStats();
        std::cout<<"VALIDATION requested="<<stats.gpuValidationRequested<<" executed="<<stats.gpuValidationExecuted<<" passed="<<stats.gpuValidationPassed<<" debug_layer="<<stats.debugLayerEnabled<<" errors="<<stats.debugErrorCount<<" warnings="<<stats.debugWarningCount<<'\n';
        Need(stats.gpuValidationRequested==expectGbv,"VALIDATION_REQUEST_MUST_MATCH_ARGUMENT");
        if(expectGbv)Need(stats.gpuValidationExecuted&&stats.gpuValidationPassed&&stats.debugLayerEnabled&&!stats.debugErrorCount&&!stats.debugWarningCount,"GBV_MUST_EXECUTE_AND_PASS");
        std::cout<<"CAPTURE_COUNTS accepted="<<stats.successfulFrameCaptures<<" rejected="<<stats.rejectedFrameCaptures<<'\n';
        Need(stats.successfulFrameCaptures==25&&!stats.rejectedFrameCaptures,"ALL_ACTUAL_CAPTURES");
        renderer.DestroyMaterialTextureSet(textures);renderer.DestroySkinnedMesh(skin);renderer.DestroyMesh(mesh);renderer.WaitIdle();
        std::cout<<"OCCLUSION_VELOCITY_PASS cases="<<caseCount<<" samples="<<checked<<" gpu_validation="<<stats.gpuValidationExecuted<<'\n';
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';result=20;}
    DestroyWindow(window);return result;
}
