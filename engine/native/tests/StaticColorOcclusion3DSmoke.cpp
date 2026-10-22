#include <Aevrynd/StaticColorOcclusion3D.h>
#include "../shaders/OcclusionWindow.hlsli"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Aevrynd;
namespace {unsigned checks{};void Need(bool v,const char* s){++checks;if(!v)throw std::runtime_error(s);}}
int main(){try{
    const auto window=ProjectScreenOcclusionWindow3D(Matrix4::Identity(),{0,0,.5f},{.2f,.4f,.1f},1.f);
    Need(window.enabled&&std::abs(window.center.x-.5f)<1e-6f&&std::abs(window.center.y-.5f)<1e-6f,"project actual eight subject corners");
    const auto packed=PackScreenOcclusionWindow3D(window);
    const Vec4 ellipse{packed[0],packed[1],packed[2],packed[3]},parameters{packed[4],packed[5],packed[6],packed[7]};
    using OcclusionWindowShader::ScreenWindowOcclusionOpacity;
    Need(ScreenWindowOcclusionOpacity({.5f,.5f},.2f,.12f,ellipse,parameters)==.12f,"subject center fades");
    Need(ScreenWindowOcclusionOpacity({.9f,.5f},.2f,.12f,ellipse,parameters)==1.f,"unrelated crown outside ellipse preserved");
    Need(ScreenWindowOcclusionOpacity({.5f,.5f},.9f,.12f,ellipse,parameters)==1.f,"behind subject stays opaque");
    Need(ScreenWindowOcclusionOpacity({.9f,.5f},.2f,.12f,ellipse,{})==.12f,"disabled keeps old global opacity");
    Need(ScreenWindowOcclusionOpacity({.5f,.5f},.2f,1.f,ellipse,parameters)==1.f,"full shadow copy remains full");
    Need(ScreenWindowOcclusionOpacity({.5f,.5f},.2f,.12f,ellipse,{1,.6f,.65f,1})==1.f,"probe bypass has no player hole");
    const float edge=ScreenWindowOcclusionOpacity({.585f,.5f},.2f,.12f,ellipse,parameters);
    Need(edge>.12f&&edge<1.f,"feathered border");
    Need(!ProjectScreenOcclusionWindow3D(Matrix4::Identity(),{0,0,0},{1,1,1}).enabled,"reject near plane crossing");
    auto invalid=window;invalid.radius.x=std::numeric_limits<float>::quiet_NaN();
    Need(!NormalizeScreenOcclusionWindow3D(invalid).enabled&&PackScreenOcclusionWindow3D({})==std::array<float,8>{},"invalid and disabled pack zero");
    const RenderObjectIdentity3D id{{41,7},2};MeshInstance3D source;
    source.identity=id;source.mesh={3,17,19,100,300};source.occlusionOpacity=.8f;source.lodCoverage=.37f;source.lodSeed=3;
    source.alphaMode=MaterialAlphaMode3D::Mask;source.alphaCutoff=.42f;source.doubleSided=true;
    source.shadowCascadeMask=3;source.transform[0][3]=4;
    auto other=source;other.identity={{41,8},2};other.layer=MeshRenderLayer3D::Transparent;
    auto originalShadow=source;originalShadow.layer=MeshRenderLayer3D::ShadowOnly;originalShadow.castsShadow=true;
    std::vector<MeshInstance3D> items{source,other,originalShadow};
    const auto applied=ApplyStaticColorOcclusion3D(items,id,.12f);
    Need(applied.status==StaticColorOcclusionStatus3D::Applied&&applied.matchedColor==1&&applied.shadowCopies==1&&items.size()==4,"split color and shadow");
    Need(items[0].occlusionOpacity==.12f&&!items[0].castsShadow,"only color fades");
    Need(items[0].shadowCascadeMask==0&&items[3].shadowCascadeMask==source.shadowCascadeMask,"canonical color mask and retained shadow mask");
    Need(std::memcmp(&items[1],&other,sizeof(other))==0&&std::memcmp(&items[2],&originalShadow,sizeof(originalShadow))==0,"unrelated and existing shadow unchanged");
    auto expected=source;expected.layer=MeshRenderLayer3D::ShadowOnly;
    Need(std::memcmp(&items[3],&expected,sizeof(expected))==0,"all original shadow fields preserved");
    Need(ApplyStaticColorOcclusion3D(items,id,.7f).shadowCopies==0&&items.size()==4,"repeat does not duplicate shadows");
    Need(ApplyStaticColorOcclusion3D(items,id,1).status==StaticColorOcclusionStatus3D::Applied&&items[0].occlusionOpacity==1,"restore full color");
    const auto stable=items;
    for(float bad:{-.1f,1.1f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        Need(ApplyStaticColorOcclusion3D(items,id,bad).status==StaticColorOcclusionStatus3D::Invalid,"reject invalid opacity");
        Need(items.size()==stable.size()&&std::memcmp(items.data(),stable.data(),items.size()*sizeof(MeshInstance3D))==0,"invalid leaves all fields");
    }
    Need(ApplyStaticColorOcclusion3D(items,{},.2f).status==StaticColorOcclusionStatus3D::Invalid,"reject missing identity");
    Need(ApplyStaticColorOcclusion3D(items,{{90,8},1},.2f).status==StaticColorOcclusionStatus3D::NotVisible,"culled color is not an error");
    auto conflict=source;conflict.identity.generation=3;items.push_back(conflict);const auto before=items;
    Need(ApplyStaticColorOcclusion3D(items,id,.2f).status==StaticColorOcclusionStatus3D::GenerationConflict,"generation conflict rejects complete update");
    Need(items.size()==before.size()&&std::memcmp(items.data(),before.data(),items.size()*sizeof(MeshInstance3D))==0,"conflict does not partially fade");
    items.assign(65,source);Need(ApplyStaticColorOcclusion3D(items,id,.2f).status==StaticColorOcclusionStatus3D::CapacityRejected&&items.size()==65&&items[0].castsShadow,"bounded multipart request");
    source.castsShadow=false;items={source};Need(ApplyStaticColorOcclusion3D(items,id,0).shadowCopies==0&&items.size()==1&&items[0].occlusionOpacity==0,"no shadow synthesized for noncaster");
    std::cout<<"STATIC_COLOR_OCCLUSION_PASS checks="<<checks<<" preserved_shadow_fields=1 screen_window=1\n";return 0;
}catch(const std::exception& e){std::cerr<<"STATIC_COLOR_OCCLUSION_FIRST_DIVERGENCE "<<e.what()<<'\n';return 1;}}
