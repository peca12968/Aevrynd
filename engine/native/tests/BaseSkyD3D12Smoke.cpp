#include "Aevrynd/Camera.h"
#include "Aevrynd/Mesh3D.h"
#include "Aevrynd/Renderer.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>

using namespace Aevrynd;

namespace {
constexpr unsigned Width=480,Height=270;
float Luma(Vec3 value){return .2126f*value.x+.7152f*value.y+.0722f*value.z;}
Vec3 Rgb(const Vec4& value){return{value.x,value.y,value.z};}
std::uint64_t Median(std::vector<std::uint64_t> values){std::ranges::sort(values);return values[values.size()/2];}
double Rms(const D3D12RendererTestAccess3D::LinearHdrCapture3D& a,const D3D12RendererTestAccess3D::LinearHdrCapture3D& b){if(a.pixels.size()!=b.pixels.size()||a.pixels.empty())return std::numeric_limits<double>::infinity();double squares{};for(std::size_t index=0;index<a.pixels.size();++index){const Vec3 first=Rgb(a.pixels[index]),second=Rgb(b.pixels[index]);for(float value:{first.x-second.x,first.y-second.y,first.z-second.z})squares+=double(value)*value;}return std::sqrt(squares/double(a.pixels.size()*3));}

struct Capture {
    D3D12RendererTestAccess3D::LinearHdrCapture3D hdr{};
    std::optional<ObjectIdCapture3D> oid{};
    std::optional<DepthCapture3D> depth{};
    RendererStats stats{};
};
struct SkyMetrics {Vec3 top{},horizon{},bottom{};float topLuma{},horizonLuma{},bottomLuma{},maximumLuma{},maximumAdjacentRowEv{};std::size_t topPixels{},horizonPixels{},bottomPixels{};};

SkyMetrics MeasureSky(const Capture& capture){
    SkyMetrics metrics{};
    std::vector<double> rowLuma(capture.hdr.height);std::vector<std::size_t> rowPixels(capture.hdr.height);
    for(unsigned y=0;y<capture.hdr.height;++y)for(unsigned x=0;x<capture.hdr.width;++x){
        const auto index=std::size_t(y)*capture.hdr.width+x;
        // 中央保留區由遮蔽物佔用；MSAA 路徑沒有 OID MRT，因此兩條路徑共用固定背景 ROI。
        if(x>=capture.hdr.width/3&&x<capture.hdr.width*2/3&&y>=capture.hdr.height/3)continue;
        if(capture.oid&&capture.oid->Resolve(capture.oid->pixels[index]))continue;
        const Vec3 rgb=Rgb(capture.hdr.pixels[index]);
        rowLuma[y]+=Luma(rgb);++rowPixels[y];
        metrics.maximumLuma=(std::max)(metrics.maximumLuma,Luma(rgb));
        if(y<capture.hdr.height/4){metrics.top=metrics.top+rgb;++metrics.topPixels;}
        if(y>=capture.hdr.height*9/20&&y<capture.hdr.height*11/20){metrics.horizon=metrics.horizon+rgb;++metrics.horizonPixels;}
        if(y>=capture.hdr.height*3/4){metrics.bottom=metrics.bottom+rgb;++metrics.bottomPixels;}
    }
    if(metrics.topPixels)metrics.top=metrics.top*(1.f/float(metrics.topPixels));
    if(metrics.horizonPixels)metrics.horizon=metrics.horizon*(1.f/float(metrics.horizonPixels));
    if(metrics.bottomPixels)metrics.bottom=metrics.bottom*(1.f/float(metrics.bottomPixels));
    metrics.topLuma=Luma(metrics.top);metrics.horizonLuma=Luma(metrics.horizon);metrics.bottomLuma=Luma(metrics.bottom);
    for(unsigned y=1;y<capture.hdr.height;++y)if(rowPixels[y-1]&&rowPixels[y]){const double previous=(std::max)(rowLuma[y-1]/double(rowPixels[y-1]),1e-6),current=(std::max)(rowLuma[y]/double(rowPixels[y]),1e-6);metrics.maximumAdjacentRowEv=(std::max)(metrics.maximumAdjacentRowEv,float(std::abs(std::log2(current/previous))));}
    return metrics;
}

PhaseRadianceProfile3D Profile(unsigned phase){
    PhaseRadianceProfile3D result{};
    if(phase==0){result.skyZenith={.12f,.31f,.72f};result.skyHorizon={.88f,.48f,.18f};result.groundBounce={.08f,.065f,.05f};result.skyIntensity=1.15f;result.sunDisk={1.f,.82f,.45f};result.sunDiskIntensity=7.f;result.phaseFingerprint=0x444159u;}
    else if(phase==1){result.skyZenith={.045f,.075f,.18f};result.skyHorizon={.42f,.16f,.07f};result.groundBounce={.035f,.025f,.03f};result.skyIntensity=.72f;result.sunDisk={1.f,.28f,.08f};result.sunDiskIntensity=2.5f;result.phaseFingerprint=0x4455534bu;}
    else{result.skyZenith={.008f,.014f,.035f};result.skyHorizon={.025f,.035f,.075f};result.groundBounce={.006f,.009f,.018f};result.skyIntensity=.42f;result.sunDisk={.12f,.18f,.35f};result.sunDiskIntensity=.08f;result.phaseFingerprint=0x4e49474854ull;}
    result.horizonExponent=4.f;result.sunAngularRadius=.035f;return result;
}

bool SkyGate(const SkyMetrics& metrics,unsigned phase,bool orthographic){
    if(metrics.topPixels<1000||metrics.horizonPixels<1000||metrics.bottomPixels<1000)return false;
    if(phase==2&&metrics.topLuma<=.002f)return false;
    const float gradient=std::abs(metrics.topLuma-metrics.horizonLuma)+std::abs(metrics.bottomLuma-metrics.horizonLuma);
    const float minimumGradient=phase==0?.025f:(phase==1?.004f:.0004f);
    if(orthographic?gradient>.05f:gradient<minimumGradient)return false;
    if(metrics.maximumAdjacentRowEv>.75f)return false;
    if(!orthographic&&phase==0&&metrics.maximumLuma<metrics.topLuma+.2f)return false;
    return true;
}

Mesh3D Card(Color color){
    Mesh3D mesh;const auto normal=PackNormalSnorm8({0,0,-1}),packed=PackColorRgba8(color);
    mesh.vertices={{-.5f,-.5f,0,normal,packed,0,1},{-.5f,.5f,0,normal,packed,0,0},{.5f,.5f,0,normal,packed,1,0},{.5f,-.5f,0,normal,packed,1,1}};
    mesh.indices={0,1,2,0,2,3};return mesh;
}
Mesh3D SkyProxy(){Mesh3D mesh=Card({1,1,1,1});for(auto& vertex:mesh.vertices)vertex.normal=PackNormalSnorm8({0,0,-1},-1.f);return mesh;}
}

int main(){
    HWND window=CreateWindowExW(0,L"STATIC",L"Aevrynd Base Sky D3D12",WS_OVERLAPPED,0,0,Width,Height,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!window)return 1;
    int result{};
    try{
        D3D12Renderer renderer;renderer.Initialize(window,Width,Height,Width,Height,false);
        const Mesh3D boxSource=Mesh3D::CreateUnitBox();const auto box=renderer.CreateMesh(boxSource.vertices,boxSource.indices);
        const std::array<Color,3> cardColors{{{.5f,.5f,.5f,1},{.35f,.16f,.07f,1},{.12f,.42f,.09f,1}}};
        std::array<MeshHandle3D,3> cards{};for(std::size_t index=0;index<cards.size();++index){const auto source=Card(cardColors[index]);cards[index]=renderer.CreateMesh(source.vertices,source.indices);}
        const auto skyProxySource=SkyProxy();const auto skyProxy=renderer.CreateMesh(skyProxySource.vertices,skyProxySource.indices);
        if(!box||!skyProxy||std::ranges::any_of(cards,[](MeshHandle3D handle){return!handle;}))result=2;

        const RenderObjectIdentity3D boxIdentity{{0x534b594f43434c55ull,0x53494f4e424f5831ull},1};
        auto capture=[&](unsigned phase,bool orthographic,bool msaa,D3D12RendererTestAccess3D::BaseSkyMutation mutation,bool semantics)->std::optional<Capture>{
            renderer.SetQualitySettings({false,false,msaa,false});if(!D3D12RendererTestAccess3D::SetBaseSkyMutation(renderer,mutation))return{};
            Camera3D camera{{0,0,-5},{0,0,0},{0,1,0},{}};if(orthographic){camera.projection.mode=ProjectionMode::Orthographic;camera.projection.orthographicHeight=5.f;}
            DrawList draw;draw.SetViewProjection(CreateViewProjectionMatrix(camera,float(Width)/Height));draw.SetBackfaceCulling(false);Atmosphere3D atmosphere{};atmosphere.cameraPosition=camera.position;draw.SetAtmosphere(atmosphere);draw.SetDirectionalLight({{0,-.08f,-1.f},0.f,{1,1,1,1},0.f});if(!draw.SetPhaseRadianceProfile(Profile(phase)))return{};
            const Vec3 fixtureScale=semantics?Vec3{1.25f,1.25f,.6f}:Vec3{.05f,.05f,.05f};
            draw.MeshInstance(box,CreateTransformMatrix({0,-.45f,1.5f},{},fixtureScale),{.18f,.21f,.24f,1},{1,0,1,1},MeshRenderLayer3D::Opaque,{},1.f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false,1,0,false,DirectionalShadowAllCascades3D,ShadowCascadeSelection3D::Automatic,Matrix4::Identity(),false,1.f,MaterialAlphaMode3D::Opaque,.5f,false,boxIdentity);
            const ObjectIdCaptureRequestMetadata3D metadata{0x7000u+phase,0x7100u+unsigned(orthographic),0x7200u+unsigned(msaa)};if(semantics&&(!renderer.RequestObjectIdReadback3D(metadata)||!renderer.RequestDepthReadback3D(metadata)))return{};
            renderer.Render(draw,{.91f,.02f,.73f,1});Capture output{};if(semantics){output.oid=renderer.TakeObjectIdReadback3D();output.depth=renderer.TakeDepthReadback3D();}auto hdr=D3D12RendererTestAccess3D::CaptureLinearHdr(renderer,0.f);renderer.WaitIdle();if(!hdr||!hdr->Complete()||(semantics&&(!output.oid||!output.depth||!output.oid->Complete()||!output.depth->Complete())))return{};output.hdr=std::move(*hdr);output.stats=renderer.GetStats();return output;
        };

        std::array<std::array<std::array<SkyMetrics,2>,2>,3> production{};std::optional<Capture> semanticProduction{};
        auto verifySemantics=[&](const Capture& value){std::size_t background{},objectPixels{};for(std::size_t index=0;index<value.oid->pixels.size();++index){const auto* identity=value.oid->Resolve(value.oid->pixels[index]);if(identity){if(*identity!=boxIdentity||!(value.depth->pixels[index]<1.f))return false;++objectPixels;}else{if(value.depth->pixels[index]!=1.f)return false;++background;}}return background>=10000&&objectPixels>=1000;};
        for(unsigned phase=0;!result&&phase<3;++phase)for(unsigned projection=0;projection<2;++projection)for(unsigned msaa=0;msaa<2;++msaa){const auto current=capture(phase,projection!=0,msaa!=0,D3D12RendererTestAccess3D::BaseSkyMutation::Production,false),semantic=capture(phase,projection!=0,msaa!=0,D3D12RendererTestAccess3D::BaseSkyMutation::Production,true);if(!current||!semantic){result=3;break;}production[phase][projection][msaa]=MeasureSky(*current);std::cout<<"sky phase="<<phase<<" projection="<<projection<<" msaa="<<msaa<<" top="<<production[phase][projection][msaa].topLuma<<" horizon="<<production[phase][projection][msaa].horizonLuma<<" bottom="<<production[phase][projection][msaa].bottomLuma<<" max="<<production[phase][projection][msaa].maximumLuma<<" maxAdjacentRowEv="<<production[phase][projection][msaa].maximumAdjacentRowEv<<'\n';const bool sampleMode=msaa?current->stats.multisampleEnabled&&current->stats.multisampleCount==4:!current->stats.multisampleEnabled;if(!SkyGate(production[phase][projection][msaa],phase,projection!=0)||!verifySemantics(*semantic)||!sampleMode){result=4;break;}if(phase==0&&projection==0&&msaa==0)semanticProduction=semantic;}
        if(!result)for(unsigned projection=0;projection<2;++projection)for(unsigned msaa=0;msaa<2;++msaa)if(!(production[0][projection][msaa].topLuma>production[1][projection][msaa].topLuma&&production[1][projection][msaa].topLuma>production[2][projection][msaa].topLuma))result=5;

        if(!result&&semanticProduction&&!verifySemantics(*semanticProduction))result=6;

        const auto legacy=capture(0,false,false,D3D12RendererTestAccess3D::BaseSkyMutation::LegacyBlue,true);D3D12RendererTestAccess3D::SetBaseSkyMutation(renderer,D3D12RendererTestAccess3D::BaseSkyMutation::Production);
        if(!result&&(!legacy||!semanticProduction))result=7;
        if(!result){const auto legacyMetrics=MeasureSky(*legacy);const bool legacyRejected=!SkyGate(legacyMetrics,0,false)||std::abs(legacyMetrics.topLuma-production[0][0][0].topLuma)>.08f;if(!legacyRejected||legacy->hdr.pixelSha256==semanticProduction->hdr.pixelSha256)result=8;
            double objectDelta{};std::size_t objectCount{};for(std::size_t index=0;index<semanticProduction->oid->pixels.size();++index)if(const auto* identity=semanticProduction->oid->Resolve(semanticProduction->oid->pixels[index]);identity&&*identity==boxIdentity){const Vec3 a=Rgb(semanticProduction->hdr.pixels[index]),b=Rgb(legacy->hdr.pixels[index]);objectDelta+=std::abs(a.x-b.x)+std::abs(a.y-b.y)+std::abs(a.z-b.z);++objectCount;}if(objectCount<1000||objectDelta/double(objectCount)>.001)result=9;std::cout<<"mutations legacyRejected="<<legacyRejected<<" opaqueDelta="<<objectDelta/double((std::max)(std::size_t{1},objectCount))<<'\n';}
        if(!result&&semanticProduction){for(const auto mutation:{D3D12RendererTestAccess3D::BaseSkyMutation::LegacyClipMatrix,D3D12RendererTestAccess3D::BaseSkyMutation::HemisphereFlip}){const auto mutated=capture(0,false,false,mutation,true);const double delta=mutated?Rms(mutated->hdr,semanticProduction->hdr):0;std::cout<<"ray mutation="<<unsigned(mutation)<<" rms="<<delta<<'\n';if(!mutated||delta<.01){result=17;break;}}const auto signedNegative=capture(0,false,false,D3D12RendererTestAccess3D::BaseSkyMutation::SignedNegativeW,true),absNegative=capture(0,false,false,D3D12RendererTestAccess3D::BaseSkyMutation::LegacyAbsW,true);const double absDelta=signedNegative&&absNegative?Rms(signedNegative->hdr,absNegative->hdr):0;std::cout<<"ray mutation=abs-w rms="<<absDelta<<'\n';if(!signedNegative||!absNegative||absDelta<.01)result=17;D3D12RendererTestAccess3D::SetBaseSkyMutation(renderer,D3D12RendererTestAccess3D::BaseSkyMutation::Production);}

        struct CloudCapture{D3D12RendererTestAccess3D::LinearHdrCapture3D hdr{};RendererStats stats{};};
        auto captureCloud=[&](D3D12RendererTestAccess3D::CloudContributorRouting routing,bool enabled,VolumetricAtmospherePhase3D phase=VolumetricAtmospherePhase3D::Day,D3D12RendererTestAccess3D::VolumetricAtmosphereMutation mutation=D3D12RendererTestAccess3D::VolumetricAtmosphereMutation::Production)->std::optional<CloudCapture>{
            if(!D3D12RendererTestAccess3D::SetCloudContributorRouting(renderer,routing)||!D3D12RendererTestAccess3D::SetCloudContributorSkyProxy(renderer,true))return{};renderer.SetQualitySettings({false,false,false,false});
            if(!D3D12RendererTestAccess3D::SetVolumetricAtmosphereMutation(renderer,mutation))return{};
            Camera3D camera{{0,95,-260},{0,280,1200},{0,1,0},{}};camera.projection.nearPlane=.25f;camera.projection.farPlane=2600.f;DrawList draw;draw.SetBackfaceCulling(false);draw.SetViewProjection(CreateViewProjectionMatrix(camera,float(Width)/Height));draw.SetAtmosphere({{.025f,.04f,.075f,1},camera.position,210.f,360.f,true,.008f,0,.02f,.85f});draw.SetDirectionalLight({{-.35f,-.82f,.44f},.12f,{1,.9f,.72f,1},2.2f});VolumetricCloudSettings3D clouds{enabled,210,360,.05f,4.f,0.f,16,false,0.f};clouds.maximumDistance=420.f;draw.SetVolumetricClouds(clouds);draw.SetDirectionalLightShafts({false,0,.72f,520,4});draw.SetVolumetricAtmospherePhase(VolumetricAtmospherePhase3D::Day);draw.SetEnvironmentTime(12.5f);draw.MeshInstance(skyProxy,CreateTransformMatrix({0,300,1200},{},{1800,1100,1}),{1,1,1,1},{1,0,1,1},MeshRenderLayer3D::Opaque,{},1.f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false);
            draw.SetVolumetricAtmospherePhase(phase);draw.SetEnvironmentTime(phase==VolumetricAtmospherePhase3D::Night?0.5f:12.5f);
            for(unsigned frame=0;frame<10;++frame){draw.SetCameraCut3D(frame==0);renderer.Render(draw,{0,0,0,1});}draw.SetCameraCut3D(false);renderer.Render(draw,{0,0,0,1});auto hdr=D3D12RendererTestAccess3D::CaptureLinearHdr(renderer,0.f);renderer.WaitIdle();if(!hdr||!hdr->Complete())return{};return CloudCapture{std::move(*hdr),renderer.GetStats()};
        };
        const auto cloudOff=captureCloud(D3D12RendererTestAccess3D::CloudContributorRouting::Production,false),inlineOnly=captureCloud(D3D12RendererTestAccess3D::CloudContributorRouting::InlineOnly,true),physicalOnly=captureCloud(D3D12RendererTestAccess3D::CloudContributorRouting::PhysicalOnly,true),cloudProduction=captureCloud(D3D12RendererTestAccess3D::CloudContributorRouting::Production,true),fullDouble=captureCloud(D3D12RendererTestAccess3D::CloudContributorRouting::FullDouble,true);D3D12RendererTestAccess3D::SetCloudContributorSkyProxy(renderer,false);D3D12RendererTestAccess3D::SetCloudContributorRouting(renderer,D3D12RendererTestAccess3D::CloudContributorRouting::Production);
        if(!result&&(!cloudOff||!inlineOnly||!physicalOnly||!cloudProduction||!fullDouble))result=14;
        if(!result){const double inlineContribution=Rms(cloudOff->hdr,inlineOnly->hdr),physicalContribution=Rms(cloudOff->hdr,physicalOnly->hdr),productionPhysical=Rms(cloudProduction->hdr,physicalOnly->hdr),doubleProduction=Rms(fullDouble->hdr,cloudProduction->hdr);const bool contributorsExecuted=inlineContribution>.001&&physicalContribution>.001&&physicalOnly->stats.volumetricAtmosphereDispatched&&cloudProduction->stats.volumetricAtmosphereDispatched&&fullDouble->stats.volumetricAtmosphereDispatched&&!inlineOnly->stats.volumetricAtmosphereDispatched;const bool productionSingle=productionPhysical<(std::max)(.001,physicalContribution*.08);const bool doubleRejected=doubleProduction>(std::max)(.003,productionPhysical*4.0);std::cout<<"cloud routing inlineDelta="<<inlineContribution<<" physicalDelta="<<physicalContribution<<" productionVsPhysical="<<productionPhysical<<" doubleVsProduction="<<doubleProduction<<" inlineDispatch="<<inlineOnly->stats.volumetricAtmosphereDispatched<<" physicalDispatch="<<physicalOnly->stats.volumetricAtmosphereDispatched<<" productionSingle="<<productionSingle<<" doubleRejected="<<doubleRejected<<'\n';if(!contributorsExecuted||!productionSingle||!doubleRejected)result=15;}
        const auto nightCloudOff=captureCloud(D3D12RendererTestAccess3D::CloudContributorRouting::PhysicalOnly,false,VolumetricAtmospherePhase3D::Night),nightCloudProduction=captureCloud(D3D12RendererTestAccess3D::CloudContributorRouting::PhysicalOnly,true,VolumetricAtmospherePhase3D::Night),nightIncidentZero=captureCloud(D3D12RendererTestAccess3D::CloudContributorRouting::PhysicalOnly,true,VolumetricAtmospherePhase3D::Night,D3D12RendererTestAccess3D::VolumetricAtmosphereMutation::NightIncidentZero);D3D12RendererTestAccess3D::SetVolumetricAtmosphereMutation(renderer,D3D12RendererTestAccess3D::VolumetricAtmosphereMutation::Production);
        if(!result&&(!nightCloudOff||!nightCloudProduction||!nightIncidentZero))result=18;
        if(!result){const double incidentDelta=Rms(nightCloudProduction->hdr,nightIncidentZero->hdr),opacityDelta=Rms(nightCloudOff->hdr,nightIncidentZero->hdr);std::cout<<"night cloud mutation incidentDelta="<<incidentDelta<<" opacityDelta="<<opacityDelta<<'\n';if(incidentDelta<=.001||opacityDelta<=.001)result=18;}

        if(!result){DrawList draw;Camera3D camera{{0,0,-4},{0,0,0},{0,1,0},{}};draw.SetViewProjection(CreateViewProjectionMatrix(camera,float(Width)/Height));Atmosphere3D atmosphere{};atmosphere.cameraPosition=camera.position;draw.SetAtmosphere(atmosphere);draw.SetDirectionalLight({{0,0,-1},0.f,{1,1,1,1},0.f});for(std::size_t index=0;index<cards.size();++index){const RenderObjectIdentity3D identity{{0x43415244434f4c52ull,0x4e47420000000000ull+index},1};draw.MeshInstance(cards[index],CreateTranslationMatrix({float(int(index)-1)*1.25f,0,0}),{1,1,1,1},{1,0,1,1},MeshRenderLayer3D::Opaque,{},1.f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false,1,0,false,DirectionalShadowAllCascades3D,ShadowCascadeSelection3D::Automatic,Matrix4::Identity(),false,1.f,MaterialAlphaMode3D::Opaque,.5f,false,identity);}renderer.SetQualitySettings({false,false,false,false});renderer.RequestObjectIdReadback3D({0x8001,0x8002,0x8003});renderer.Render(draw,{0,0,0,1});const auto oid=renderer.TakeObjectIdReadback3D();const auto hdr=D3D12RendererTestAccess3D::CaptureLinearHdr(renderer,0.f);renderer.WaitIdle();if(!oid||!hdr||!oid->Complete()||!hdr->Complete())result=10;else for(std::size_t card=0;card<cards.size();++card){const RenderObjectIdentity3D identity{{0x43415244434f4c52ull,0x4e47420000000000ull+card},1};Vec3 mean{};std::size_t count{};for(std::size_t pixel=0;pixel<oid->pixels.size();++pixel)if(const auto* resolved=oid->Resolve(oid->pixels[pixel]);resolved&&*resolved==identity){mean=mean+Rgb(hdr->pixels[pixel]);++count;}if(count)mean=mean*(1.f/float(count));const Vec3 expected=Vec3{cardColors[card].r,cardColors[card].g,cardColors[card].b}*2.f;const float error=std::abs(mean.x-expected.x)+std::abs(mean.y-expected.y)+std::abs(mean.z-expected.z);if(count<500||error>.02f){std::cerr<<"absolute card failed index="<<card<<" rgb="<<mean.x<<','<<mean.y<<','<<mean.z<<" error="<<error<<'\n';result=11;break;}}}

        auto verifyCardCell=[&](unsigned phase,bool orthographic,bool msaa){Camera3D camera{{0,0,-4},{0,0,0},{0,1,0},{}};if(orthographic){camera.projection.mode=ProjectionMode::Orthographic;camera.projection.orthographicHeight=5.f;}DrawList draw;draw.SetViewProjection(CreateViewProjectionMatrix(camera,float(Width)/Height));Atmosphere3D atmosphere{};atmosphere.cameraPosition=camera.position;draw.SetAtmosphere(atmosphere);draw.SetDirectionalLight({{0,0,-1},0.f,{1,1,1,1},0.f});if(!draw.SetPhaseRadianceProfile(Profile(phase)))return false;for(std::size_t index=0;index<cards.size();++index){const RenderObjectIdentity3D identity{{0x43415244434f4c52ull,0x4e47420000000000ull+index},1};draw.MeshInstance(cards[index],CreateTranslationMatrix({float(int(index)-1)*1.25f,0,0}),{1,1,1,1},{1,0,1,1},MeshRenderLayer3D::Opaque,{},1.f,false,MaterialClass3D::Surface,std::numeric_limits<float>::quiet_NaN(),false,1,0,false,DirectionalShadowAllCascades3D,ShadowCascadeSelection3D::Automatic,Matrix4::Identity(),false,1.f,MaterialAlphaMode3D::Opaque,.5f,false,identity);}renderer.SetQualitySettings({false,false,msaa,false});renderer.Render(draw,{0,0,0,1});renderer.WaitIdle();const auto colorStats=renderer.GetStats();if(msaa?(!colorStats.multisampleEnabled||colorStats.multisampleCount!=4):colorStats.multisampleEnabled)return false;if(!renderer.RequestObjectIdReadback3D({0x8100u+phase,0x8200u+unsigned(orthographic),0x8300u+unsigned(msaa)}))return false;renderer.Render(draw,{0,0,0,1});const auto oid=renderer.TakeObjectIdReadback3D();const auto hdr=D3D12RendererTestAccess3D::CaptureLinearHdr(renderer,0.f);renderer.WaitIdle();if(!oid||!hdr||!oid->Complete()||!hdr->Complete())return false;for(std::size_t card=0;card<cards.size();++card){const RenderObjectIdentity3D identity{{0x43415244434f4c52ull,0x4e47420000000000ull+card},1};Vec3 mean{};std::size_t count{};for(std::size_t pixel=0;pixel<oid->pixels.size();++pixel)if(const auto* resolved=oid->Resolve(oid->pixels[pixel]);resolved&&*resolved==identity){mean=mean+Rgb(hdr->pixels[pixel]);++count;}if(count)mean=mean*(1.f/float(count));const Vec3 expected=Vec3{cardColors[card].r,cardColors[card].g,cardColors[card].b}*2.f;const float error=std::abs(mean.x-expected.x)+std::abs(mean.y-expected.y)+std::abs(mean.z-expected.z);if(count<500||error>.02f)return false;}return true;};
        for(unsigned phase=0;!result&&phase<3;++phase)for(unsigned projection=0;projection<2;++projection)for(unsigned msaa=0;msaa<2;++msaa)if(!verifyCardCell(phase,projection!=0,msaa!=0)){std::cerr<<"card matrix failed phase="<<phase<<" projection="<<projection<<" msaa="<<msaa<<'\n';result=16;break;}

        const auto stats=renderer.GetStats();const bool invalidGbv=stats.gpuValidationRequested&&(!stats.gpuValidationAvailable||!stats.gpuValidationExecuted||!stats.gpuValidationPassed);if(!result&&(stats.debugErrorCount||stats.debugWarningCount||invalidGbv))result=12;
        if(!result){DrawList clearOnly,skyDraw;Camera3D camera{{0,0,-5},{0,0,0},{0,1,0},{}};skyDraw.SetViewProjection(CreateViewProjectionMatrix(camera,float(Width)/Height));Atmosphere3D atmosphere{};atmosphere.cameraPosition=camera.position;skyDraw.SetAtmosphere(atmosphere);skyDraw.SetDirectionalLight({{0,-.08f,-1.f},0.f,{1,1,1,1},0.f});skyDraw.SetPhaseRadianceProfile(Profile(0));skyDraw.MeshInstance(box,CreateScaleMatrix({.02f,.02f,.02f}),{.18f,.21f,.24f,1},{1,0,1,1},MeshRenderLayer3D::Opaque);renderer.SetQualitySettings({false,false,false,false});for(unsigned warm=0;warm<8;++warm){renderer.Render(clearOnly,{0,0,0,1});renderer.WaitIdle();renderer.Render(skyDraw,{0,0,0,1});renderer.WaitIdle();}std::vector<std::uint64_t> baseline,withSky;for(unsigned sample=0;sample<21;++sample){renderer.Render(clearOnly,{0,0,0,1});renderer.WaitIdle();baseline.push_back(renderer.GetStats().lastGpuFrameNanoseconds);renderer.Render(skyDraw,{0,0,0,1});renderer.WaitIdle();withSky.push_back(renderer.GetStats().lastGpuFrameNanoseconds);}const auto baselineMedian=Median(baseline),skyMedian=Median(withSky);const auto delta=std::int64_t(skyMedian)-std::int64_t(baselineMedian);std::cout<<"baseSkyGpuMedianNs="<<baselineMedian<<"->"<<skyMedian<<" deltaNs="<<delta<<'\n';if(delta>750000)result=13;}
        for(auto handle:cards)renderer.DestroyMesh(handle);renderer.DestroyMesh(skyProxy);renderer.DestroyMesh(box);renderer.WaitIdle();renderer.TrimTransientMemory();
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';result=20;}
    DestroyWindow(window);if(result)std::cerr<<"Base sky D3D12 smoke failed at "<<result<<'\n';return result;
}
