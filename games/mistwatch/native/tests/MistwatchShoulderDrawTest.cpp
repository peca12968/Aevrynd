#include "MistwatchShoulderDraw.h"
#include <iostream>
#include <limits>

namespace {
using namespace Aevrynd;
using namespace Mistwatch::ShoulderDraw;
unsigned checks{},negatives{};
void Need(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class F>void Reject(F run){bool bad=false;try{run();}catch(const std::invalid_argument&){bad=true;}Need(bad,"negative accepted");++negatives;}
std::array<Expected,SlotCount> Fixture(){
    std::array<Expected,SlotCount> e{};
    for(std::size_t i=0;i<e.size();++i){auto& a=e[i];
        a.mesh.handle={std::uint32_t(i+1),100+i,200+i,6,12};a.mesh.vertexCount=6;a.mesh.indexCount=12;
        a.pose={{float(i)*2.f,3,4},{.1f,.2f,.3f},{1.05f,1.2f,.85f}};
        a.material.textures={std::uint32_t(i%3+1),300+i%3};a.material.useUv=true;
        a.material.tint={.3f,.5f,.7f,1};a.material.surface={.6f,.1f,.2f,.9f};
        a.material.normalScale=.75f;a.material.textureScale=1.3f;a.material.alphaCutoff=.4f;
        if(i%3==1){a.material.alphaMode=MaterialAlphaMode3D::Mask;a.material.materialClass=MaterialClass3D::Foliage;a.material.doubleSided=true;}
    }return e;
}
MeshInstance3D Item(const Expected& e,std::size_t slot,MeshRenderLayer3D layer=MeshRenderLayer3D::Opaque,bool shadow=true){
    const auto& m=e.material;DrawList d;
    Need(d.MeshInstance(e.mesh.handle,e.pose.Matrix(),m.tint,m.surface,layer,m.textures,m.textureScale,m.useUv,
        m.materialClass,0.f,shadow,1.f,0,false,DirectionalShadowAllCascades3D,ShadowCascadeSelection3D::Automatic,
        Matrix4::Identity(),false,m.normalScale,m.alphaMode,m.alphaCutoff,m.doubleSided,{{InstanceNamespace,slot+1},1}),"draw insert");
    return d.meshInstances.at(0);
}
}
int main(){try{
    const auto e=Fixture();
    Need(Observer(e).Finish().draws==0,"culled allowed");
    Observer full(e);for(std::size_t i=0;i<SlotCount;++i)full.Observe(Item(e[i],i,i%2?MeshRenderLayer3D::ShadowOnly:MeshRenderLayer3D::Opaque));
    auto r=full.Finish();Need(r.draws==48&&r.opaque==24&&r.shadowOnly==24&&r.triangles==192,"counts");
    for(std::size_t i=0;i<48;++i)Need(r.visibility[i]==(i%2?2:1),"visibility");
    Observer partial(e);partial.Observe(Item(e[47],47));Need(partial.Finish().draws==1&&partial.Finish().visibility[0]==0,"partial valid");
    for(std::size_t i=0;i<48;++i){Observer o(e);auto wrong=Item(e[i],i);wrong.identity.id.low=(i+1)%48+1;Reject([&]{o.Observe(wrong);});}
    auto clamp=e;clamp[0].material.textureScale=.0001f;clamp[0].material.normalScale=40.f;clamp[0].material.alphaCutoff=-.2f;
    Observer clamped(clamp);clamped.Observe(Item(clamp[0],0));Need(clamped.Finish().draws==1,"actual draw packing clamps");
    auto mutableExpected=e;Observer copied(mutableExpected);mutableExpected[0].pose.position.x=999;
    copied.Observe(Item(e[0],0));Need(copied.Finish().draws==1,"expected snapshot owns values");
    Observer disabled(std::span<const Expected>{});auto foreign=Item(e[0],0);foreign.identity.id.high=123;
    disabled.Observe(foreign);Need(disabled.Finish().draws==0,"unrelated ignored");Reject([&]{disabled.Observe(Item(e[0],0));});Reject([&]{(void)disabled.Finish();});
    auto noShadow=e;noShadow[0].material.castsShadow=false;Observer noCast(noShadow);noCast.Observe(Item(noShadow[0],0,MeshRenderLayer3D::Opaque,false));
    Need(noCast.Finish().draws==1,"noncaster valid");
    Observer distance(e);distance.Observe(Item(e[0],0,MeshRenderLayer3D::Opaque,false));Need(distance.Finish().opaque==1,"distance disabled shadow");
    auto mutate=[&](auto fn){Observer o(e);auto a=Item(e[1],1);fn(a);Reject([&]{o.Observe(a);});Reject([&]{(void)o.Finish();});Reject([&]{o.Observe(foreign);});};
    mutate([](auto& a){a.identity.id.low=0;});mutate([](auto& a){a.identity.id.low=49;});
    mutate([](auto& a){a.identity.generation=0;});mutate([](auto& a){a.identity.generation=2;});
    mutate([](auto& a){a.identity.hlodClusterId={1,2};});mutate([](auto& a){a.identity.sharedLodDither=true;});mutate([](auto& a){a.identity.hlodProxy=true;});
    mutate([](auto& a){++a.mesh.value;});mutate([](auto& a){++a.mesh.contentPrimary;});mutate([](auto& a){++a.mesh.contentSecondary;});
    mutate([](auto& a){++a.mesh.vertexCount;});mutate([](auto& a){a.mesh.indexCount+=3;});
    mutate([](auto& a){++a.textures.value;});mutate([](auto& a){++a.textures.semanticIdentity;});
    for(std::size_t row=0;row<4;++row)for(std::size_t col=0;col<4;++col){
        mutate([&](auto& a){a.transform[row][col]=std::bit_cast<float>(std::bit_cast<std::uint32_t>(a.transform[row][col])^1u);});
        mutate([&](auto& a){a.normalTransform[row][col]=std::bit_cast<float>(std::bit_cast<std::uint32_t>(a.normalTransform[row][col])^1u);});
    }
    mutate([](auto& a){a.previousTransform[0][3]+=1;});mutate([](auto& a){a.motionValid=true;});
    mutate([](auto& a){a.transform[3][0]=-0.f;});
    mutate([](auto& a){a.transform[0][0]=std::numeric_limits<float>::quiet_NaN();});
    for(unsigned b=0;b<32;++b){mutate([&](auto& a){a.tint^=1u<<b;});mutate([&](auto& a){a.surface^=1u<<b;});}
    mutate([](auto& a){a.textureScale+=.01f;});mutate([](auto& a){a.normalScale+=.01f;});mutate([](auto& a){a.useUv=false;});
    mutate([](auto& a){a.materialClass=MaterialClass3D::Surface;});mutate([](auto& a){a.alphaMode=MaterialAlphaMode3D::Opaque;});
    mutate([](auto& a){a.alphaCutoff+=.01f;});mutate([](auto& a){a.doubleSided=false;});
    mutate([](auto& a){a.occlusionOpacity=.99f;});mutate([](auto& a){a.lodCoverage=.99f;});mutate([](auto& a){a.lodSeed=1;});
    mutate([](auto& a){a.layer=MeshRenderLayer3D::Overlay;});mutate([](auto& a){a.layer=MeshRenderLayer3D::Transparent;});
    mutate([](auto& a){a.layer=static_cast<MeshRenderLayer3D>(255);});
    mutate([](auto& a){a.layer=MeshRenderLayer3D::ShadowOnly;a.castsShadow=false;a.shadowCascadeMask=0;});
    mutate([](auto& a){a.shadowCascadeMask=0;});mutate([](auto& a){a.shadowCascadeMask=255;});
    mutate([](auto& a){a.castsShadow=false;});
    mutate([](auto& a){a.shadowCascadeSelection=static_cast<ShadowCascadeSelection3D>(255);});
    Reject([&]{Observer o(noShadow);o.Observe(Item(noShadow[0],0));});
    Reject([&]{Observer o(e);o.Observe(Item(e[0],0));o.Observe(Item(e[0],0,MeshRenderLayer3D::ShadowOnly));});
    Reject([&]{Observer o(std::span<const Expected>(e.data(),47));});
    auto expectedBad=[&](auto fn){auto bad=e;fn(bad[0]);Reject([&]{Observer o(bad);});};
    expectedBad([](auto& a){a.mesh.handle={};});expectedBad([](auto& a){++a.mesh.indexCount;});expectedBad([](auto& a){++a.mesh.vertexCount;});
    expectedBad([](auto& a){a.mesh.handle.contentPrimary=0;});expectedBad([](auto& a){a.mesh.handle.contentSecondary=0;});
    expectedBad([](auto& a){a.pose.scale.x=0;});expectedBad([](auto& a){a.pose.position.x=std::numeric_limits<float>::infinity();});
    expectedBad([](auto& a){a.material.surface.occlusion=std::numeric_limits<float>::quiet_NaN();});
    expectedBad([](auto& a){a.material.textures.semanticIdentity=0;});expectedBad([](auto& a){a.material.alphaMode=MaterialAlphaMode3D::Blend;});
    expectedBad([](auto& a){a.material.textures.value=0;});expectedBad([](auto& a){a.material.useUv=false;});
    expectedBad([](auto& a){a.material.textureScale=0;});expectedBad([](auto& a){a.material.materialClass=MaterialClass3D::Water;});
    expectedBad([](auto& a){a.material.shadingModel=static_cast<CharacterShadingModel3D>(255);});
    expectedBad([](auto& a){a.material.baseTextureScale=2;});expectedBad([](auto& a){a.material.detailTextureScale=2;});
    std::cout<<"SHOULDER_DRAW_PASS checks="<<checks<<" negatives="<<negatives<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
