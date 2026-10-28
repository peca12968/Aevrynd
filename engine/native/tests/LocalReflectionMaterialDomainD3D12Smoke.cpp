#include <Aevrynd/Renderer.h>
#include <Windows.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace Aevrynd;
namespace {
void Need(bool value,const char* why){if(!value)throw std::runtime_error(why);}
size_t Difference(const LinearHdrReadback3D& a,const LinearHdrReadback3D& b,unsigned x0,unsigned x1){
    Need(a.receipt.width==128&&a.receipt.height==96&&a.rawHalfBytes.size()==b.rawHalfBytes.size(),"HDR_SHAPE");size_t count{};
    for(unsigned y=0;y<96;++y)for(unsigned x=x0;x<x1;++x)for(unsigned byte=0;byte<8;++byte){const size_t i=(size_t(y)*128+x)*8+byte;count+=a.rawHalfBytes[i]!=b.rawHalfBytes[i];}
    return count;
}
}
int main(int argc,char**argv){try{
    Need(argc==2,"FRESH_OUTPUT_REQUIRED");const std::filesystem::path out=argv[1];Need(out.is_absolute()&&!std::filesystem::exists(out),"OUTPUT_EXISTS");std::filesystem::create_directories(out);
    HWND window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"Aevrynd reflection material domain",WS_OVERLAPPED,-30000,-30000,128,96,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);Need(window!=nullptr,"WINDOW");
    {
        D3D12Renderer renderer;renderer.Initialize(window,128,96,128,96,false);renderer.SetQualitySettings({false,false,false,false});
        Need(!renderer.SetLocalReflectionProbeMaterialMask3D({},LocalReflectionMaterialMask3D::Water)&&!renderer.SetLocalReflectionProbeMaterialMask3D({999999},LocalReflectionMaterialMask3D::Water),"UNKNOWN_ENVIRONMENT_ACCEPTED");
        const std::array<uint32_t,6> indices{0,1,2,0,2,3};
        std::array<Vertex3D,4> bottom{{{-1,-1,.8f,PackNormalSnorm8({0,0,-1}),0xff2080e0u},{-1,1,.8f,PackNormalSnorm8({0,0,-1}),0xff20e080u},{1,1,.8f,PackNormalSnorm8({0,0,-1}),0xffe08020u},{1,-1,.8f,PackNormalSnorm8({0,0,-1}),0xff8020e0u}}};
        std::array<Vertex3D,4> water{{{0,-1,.3f,PackNormalSnorm8({0,1,0},1),0xffffffffu},{0,1,.3f,PackNormalSnorm8({0,1,0},1),0xffffffffu},{1,1,.3f,PackNormalSnorm8({0,1,0},1),0xffffffffu},{1,-1,.3f,PackNormalSnorm8({0,1,0},1),0xffffffffu}}};
        const auto bed=renderer.CreateMesh(bottom,indices),surface=renderer.CreateMesh(water,indices);Need(bed&&surface,"MESH");
        std::array<SkinnedVertex3D,3> vertices{};vertices[0].x=-.9f;vertices[0].y=-.75f;vertices[1].x=-.85f;vertices[1].y=.75f;vertices[2].x=-.15f;vertices[2].y=-.5f;
        for(auto& v:vertices){v.z=.2f;v.normal=PackNormalSnorm8({0,0,-1});v.color=0xff30c060u;v.weights[0]=65535;}
        const std::array<uint32_t,3> skinIndices{0,1,2};const SkeletonCompatibilityId3D compatibility{0x4184};const auto skin=renderer.CreateSkinnedMesh(vertices,skinIndices,1,compatibility);Need(bool(skin),"SKIN");
        DrawList draw;draw.SetViewProjection(Matrix4::Identity());draw.SetBackfaceCulling(false);draw.SetEnvironmentTime(0);draw.SetDirectionalLight({{.3f,-1,.2f},.2f,{1,.96f,.9f,1},1.8f});draw.SetScreenSpaceReflections({false});draw.SetWaterOptics({true,0,1,.08f});draw.SetAtmosphere({{.01f,.02f,.03f,1},{0,2,-4},50,100,false});
        WaterSurfaceSettings3D settings;settings.absorption={0,0,0};settings.deepColor={0,0,0};settings.broadWaveStrength=settings.detailWaveStrength=0;settings.foamStrength=0;settings.shallowAlpha=settings.deepAlpha=.7f;draw.SetWaterSurface(settings);
        Need(draw.MeshInstance(bed,Matrix4::Identity(),{1,1,1,1},{.1f,.8f,0,1}),"BED_SUBMIT");
        SkinPaletteJoint3D joint{};joint.skin=Matrix4::Identity();joint.normalRows={Vec4{1,0,0,0},Vec4{0,1,0,0},Vec4{0,0,1,0}};
        const auto palette=draw.AddSkinPalette(std::span<const SkinPaletteJoint3D>{&joint,1},compatibility);Need(bool(palette)&&draw.SkinnedMeshInstance(skin,palette,Matrix4::Identity(),{1,1,1,1},{.1f,.8f,0,1},{},.25f,false,MaterialClass3D::Surface,false,0,0,3),"SKIN_SUBMIT");
        Need(draw.MeshInstance(surface,Matrix4::Identity(),{.01f,.02f,.03f,1},{.08f,0,0,1},MeshRenderLayer3D::Transparent,{},.25f,false,MaterialClass3D::Water,1,false),"WATER_SUBMIT");
        const auto probe=renderer.CreateReflectionProbeCapture3D({{0,0,0},32,.1f,20});Need(bool(probe),"CAPTURE");
        for(unsigned i=0;i<4;++i){renderer.Render(draw,{.008f,.018f,.03f,1});renderer.WaitIdle();}
        const auto status=renderer.ReflectionProbeCaptureStatus(probe);Need(status&&status->stage==ReflectionProbeCaptureStage3D::ReadyFiltered&&status->environment,"FILTERED_CAPTURE");const auto environment=status->environment;
        const auto capture=[&](const DrawList& value,const char* name){Need(renderer.CaptureNextFrame(out/(std::string(name)+".png")),"PNG");Need(renderer.RequestLinearHdrReadback3D({0x4184,0x1234,0x5678}),"HDR_REQUEST");renderer.Render(value,{.008f,.018f,.03f,1});renderer.WaitIdle();auto hdr=renderer.TakeLinearHdrReadback3D();Need(hdr&&hdr->Complete(),"COMPLETE_HDR");std::ofstream raw(out/(std::string(name)+".rgba16f"),std::ios::binary);raw.write(reinterpret_cast<const char*>(hdr->rawHalfBytes.data()),std::streamsize(hdr->rawHalfBytes.size()));raw.close();Need(bool(raw),"HDR_WRITE");return std::move(*hdr);};
        const auto before=capture(draw,"without-probe"),repeat=capture(draw,"without-probe-repeat");Need(Difference(before,repeat,0,128)==0,"SELF_REPEAT");Need(renderer.GetStats().lastSubmittedSkinnedInstances==1,"ACTUAL_SKINNED_SUBMISSION_MISSING");
        DrawList local=draw;Need(local.AddLocalReflectionProbe({environment,{-2,-2,-2},{2,2,2},{0,0,0},1}),"LOCAL_PROBE");
        const auto all=capture(local,"all-materials");const auto allOpaque=Difference(before,all,0,64);Need(allOpaque>0,"ALL_MATERIAL_CONTROL_INEFFECTIVE");
        Need(renderer.SetLocalReflectionProbeMaterialMask3D(environment,LocalReflectionMaterialMask3D::Water),"SET_WATER_MASK");
        Need(!renderer.SetLocalReflectionProbeMaterialMask3D(environment,LocalReflectionMaterialMask3D(16)),"INVALID_MASK_ACCEPTED");
        const auto only=capture(local,"water-only");const auto opaque=Difference(before,only,0,64),changedWater=Difference(before,only,64,128);Need(opaque==0&&changedWater>0,"ACTUAL_MATERIAL_DOMAIN_NOT_PRESERVED");
        Need(renderer.SetLocalReflectionProbeMaterialMask3D(environment,LocalReflectionMaterialMask3D::All),"RESTORE_ALL");const auto restored=capture(local,"restored-all");Need(Difference(all,restored,0,128)==0,"RESTORE_CHANGED_HDR");
        renderer.DestroyReflectionProbeCapture3D(probe);Need(!renderer.SetLocalReflectionProbeMaterialMask3D(environment,LocalReflectionMaterialMask3D::Water),"DESTROYED_OWNER_ACCEPTED");renderer.DestroySkinnedMesh(skin);renderer.DestroyMesh(surface);renderer.DestroyMesh(bed);renderer.WaitIdle();const auto clean=renderer.GetStats();Need(!clean.reflectionProbeCaptureCount&&!clean.reflectionProbeCaptureResidentBytes&&!clean.residentMeshBytes&&!clean.residentSkinnedMeshBytes,"CLEANUP");
        const auto checked=renderer.GetStats();Need(checked.gpuValidationRequested&&checked.gpuValidationExecuted&&checked.debugLayerEnabled&&!checked.debugErrorCount&&!checked.debugWarningCount,"ACTUAL_GBV_OR_ZERO_MESSAGES_MISSING");
        std::ofstream receipt(out/"result.txt");receipt<<"debug_layer="<<checked.debugLayerEnabled<<"\ngbv_requested="<<checked.gpuValidationRequested<<"\ngbv_executed="<<checked.gpuValidationExecuted<<"\ndebug_errors="<<checked.debugErrorCount<<"\ndebug_warnings="<<checked.debugWarningCount<<"\nschema=local-reflection-material-domain-v1\nall_opaque_changed_bytes="<<allOpaque<<"\nwater_only_opaque_changed_bytes="<<opaque<<"\nwater_only_water_changed_bytes="<<changedWater<<"\nself_repeat_changed_bytes=0\nrestored_changed_bytes=0\nunknown_invalid_destroyed_rejected=1\nstatic_and_skinned=1\ncleanup=success\n";receipt.close();Need(bool(receipt),"RECEIPT");std::cout<<"MATERIAL_DOMAIN_GPU_PASS opaque="<<opaque<<" water="<<changedWater<<" allOpaque="<<allOpaque<<'\n';
    }
    DestroyWindow(window);return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
