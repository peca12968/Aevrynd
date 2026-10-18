#include "MistwatchLandscapeTwigDraw.h"
#include <iostream>
#include <limits>

namespace {
using namespace Aevrynd;
using namespace Mistwatch::LandscapeTwigDraw;
unsigned checks{},negatives{};
void Need(bool value,const char* message) { ++checks;if(!value)throw std::runtime_error(message); }
template<class F> void Reject(F action,const char* message) {
    bool rejected=false;try{action();}catch(const std::invalid_argument&){rejected=true;}
    Need(rejected,message);++negatives;
}
Expected Fixture(float distance) {
    Expected e;e.nearMesh={10,101,102,120,15048};e.farMesh={11,201,202,80,3732};
    e.nearTriangles=5016;e.farTriangles=1244;e.identity={{0x1234,6},1};
    e.material.textures={2,345};e.material.useUv=true;e.material.materialClass=MaterialClass3D::Foliage;
    e.material.alphaMode=MaterialAlphaMode3D::Mask;e.material.alphaCutoff=.25f;e.material.doubleSided=true;
    MeshResource3D nearMesh{},farMesh{};nearMesh.handle=e.nearMesh;farMesh.handle=e.farMesh;
    const std::array<MeshLodLevel3D,2> levels{{{nearMesh,e.material,30.f,6.f},{farMesh,e.material}}};
    e.selection=EvaluateMeshLodChain3D(distance,2.f,levels,MeshLodDistanceMetric3D::BoundsSurface);
    return e;
}
// 經真正 DrawList 插入取得打包欄位；不另寫一份測試專用 pack 規則。
MeshInstance3D Item(const Expected& e,bool isNear,MeshRenderLayer3D layer=MeshRenderLayer3D::Opaque) {
    const auto& m=e.material;DrawList draw;
    Need(draw.MeshInstance(isNear?e.nearMesh:e.farMesh,Matrix4::Identity(),m.tint,m.surface,layer,
        m.textures,m.textureScale,m.useUv,m.materialClass,0.f,true,e.selection.transition?e.selection.farCoverage:1.f,
        e.selection.transition?3u:0u,e.selection.transition&&isNear,DirectionalShadowAllCascades3D,
        ShadowCascadeSelection3D::Automatic,Matrix4::Identity(),false,m.normalScale,m.alphaMode,m.alphaCutoff,
        m.doubleSided,e.identity),"fixture draw insertion");
    return draw.meshInstances.at(0);
}
}
int main() {
    try {
        {
            const auto e=Fixture(50);const auto item=Item(e,false);
            Need(ExactResourceBindings(item,e.farMesh,e.material.textures),"exact static resource bindings");
            const auto badBinding=[&](auto change){auto bad=item;change(bad);
                Need(!ExactResourceBindings(bad,e.farMesh,e.material.textures),"corrupted static resource binding rejected");++negatives;};
            badBinding([](auto& bad){++bad.mesh.value;});
            badBinding([](auto& bad){++bad.mesh.contentPrimary;});
            badBinding([](auto& bad){++bad.mesh.contentSecondary;});
            badBinding([](auto& bad){++bad.mesh.vertexCount;});
            badBinding([](auto& bad){bad.mesh.indexCount+=3;});
            badBinding([](auto& bad){++bad.textures.value;});
            badBinding([](auto& bad){++bad.textures.semanticIdentity;});
        }
        for(const auto metric:{MeshLodDistanceMetric3D::Center,MeshLodDistanceMetric3D::BoundsSurface}) {
            const float radius=metric==MeshLodDistanceMetric3D::BoundsSurface?4.f:0.f;
            for(const auto bits:{0x4287FFFFu,0x4287FF4Bu,0x4287FF4Au}) {
                auto e=Fixture(50);MeshResource3D nearMesh{},farMesh{};
                nearMesh.handle=e.nearMesh;farMesh.handle=e.farMesh;
                const std::array<MeshLodLevel3D,2> levels{{{nearMesh,e.material,60.f,16.f},{farMesh,e.material}}};
                volatile float distance=std::bit_cast<float>(bits)+radius;
                e.selection=EvaluateMeshLodChain3D(distance,radius,levels,metric);
                // 保持 Observer 嚴格檢查；正常引擎不能產生覆蓋為 1 的假過渡。
                Observer observer(e);
                const bool transition=bits==0x4287FF4Au;
                Need(e.selection.transition==transition,"rounded endpoint canonical state");
                if(transition)observer.Observe(Item(e,true));
                observer.Observe(Item(e,false));
                Need(observer.Finish().draws==(transition?2u:1u),"rounded endpoint actual count");
            }
        }
        const auto nearCase=Fixture(20),farCase=Fixture(50),middle=Fixture(32);
        Need(!nearCase.selection.transition&&nearCase.selection.primaryLevel==0&&!farCase.selection.transition&&
             farCase.selection.primaryLevel==1&&middle.selection.transition,"independent distance selection");
        MeshResource3D first{},second{};first.bounds={{1,2,3},0,{2,3,4}};second.bounds={{-2,5,1},0,{1,2,2}};
        const auto unionBounds=UnionBounds(first,second);
        Need(unionBounds.center.x==0&&unionBounds.center.y==3&&unionBounds.center.z==3&&
             unionBounds.extents.x==3&&unionBounds.extents.y==4&&unionBounds.extents.z==4&&
             unionBounds.radius==std::sqrt(41.f),"union bounds exact endpoints");
        for(const auto& e:{nearCase,farCase,middle}) {
            const Observer culled(e);const auto empty=culled.Finish();
            Need(empty.culled&&empty.draws==0&&empty.triangles==0,"culled logical slot");
            for(const auto layer:{MeshRenderLayer3D::Opaque,MeshRenderLayer3D::ShadowOnly}) {
                Observer observer(e);
                if(e.selection.transition) {observer.Observe(Item(e,false,layer));observer.Observe(Item(e,true,layer));}
                else observer.Observe(Item(e,e.selection.primaryLevel==0,layer));
                const auto r=observer.Finish();
                Need(!r.culled&&r.layer==layer&&r.transition==e.selection.transition,"actual layer and state");
                Need(r.nearDraws==(e.selection.transition||e.selection.primaryLevel==0?1u:0u)&&
                     r.farDraws==(e.selection.transition||e.selection.primaryLevel==1?1u:0u),"exact endpoint counts");
                Need(r.triangles==(e.selection.transition?6260u:e.selection.primaryLevel==0?5016u:1244u),"actual submitted triangle sum");
            }
        }
        const auto n=Item(middle,true),f=Item(middle,false);
        // Scene 的 chain index 可改變 seed；所有合法 seed 均須接受，不鎖定 fixture 的值。
        for(unsigned seed=0;seed<8;++seed) {
            Observer observer(middle);auto a=n,b=f;a.lodSeed=seed|8u;b.lodSeed=seed;
            observer.Observe(a);observer.Observe(b);
            Need(observer.Finish().draws==2,"legal pair seed rejected");
        }
        for(float distance:{30.f,34.f}) {
            const auto e=Fixture(distance);Need(e.selection.transition,"off-center transition fixture");
            Observer observer(e);observer.Observe(Item(e,true));observer.Observe(Item(e,false));
            Need(observer.Finish().triangles==6260,"off-center transition rejected");
        }
        {
            Observer observer(middle);auto a=n,b=f;a.castsShadow=false;b.castsShadow=false;
            observer.Observe(a);observer.Observe(b);
            Need(observer.Finish().draws==2,"opaque pair beyond shadow distance rejected");
        }
        const auto badItem=[&](auto mutate,const char* message) {
            Reject([&]{Observer observer(middle);auto bad=n;mutate(bad);observer.Observe(bad);observer.Observe(f);(void)observer.Finish();},message);
        };
        badItem([](auto& i){++i.identity.id.low;},"wrong identity accepted");
        badItem([](auto& i){++i.identity.generation;},"wrong generation accepted");
        badItem([](auto& i){++i.mesh.value;},"wrong endpoint handle accepted");
        badItem([](auto& i){++i.mesh.contentPrimary;},"wrong mesh content accepted");
        badItem([](auto& i){++i.mesh.indexCount;},"wrong mesh shape accepted");
        badItem([](auto& i){++i.textures.value;},"wrong texture handle accepted");
        badItem([](auto& i){++i.textures.semanticIdentity;},"wrong texture identity accepted");
        badItem([](auto& i){i.tint^=1u;},"wrong tint accepted");
        badItem([](auto& i){i.surface^=1u;},"wrong surface accepted");
        badItem([](auto& i){i.alphaCutoff=.5f;},"wrong alpha cutoff accepted");
        badItem([](auto& i){i.normalScale=.5f;},"wrong normal scale accepted");
        badItem([](auto& i){i.useUv=false;},"wrong uv mode accepted");
        badItem([](auto& i){i.doubleSided=false;},"wrong sidedness accepted");
        badItem([](auto& i){i.textureScale=2.f;},"wrong texture scale accepted");
        badItem([](auto& i){i.materialClass=MaterialClass3D::Surface;},"wrong material class accepted");
        badItem([](auto& i){i.alphaMode=MaterialAlphaMode3D::Opaque;},"wrong alpha mode accepted");
        badItem([](auto& i){i.occlusionOpacity=.5f;},"wrong opacity accepted");
        badItem([](auto& i){i.lodCoverage=.25f;},"wrong coverage accepted");
        badItem([](auto& i){i.lodCoverage=std::numeric_limits<float>::quiet_NaN();},"nan coverage accepted");
        badItem([](auto& i){i.lodSeed^=1u;},"mismatched pair seed accepted");
        badItem([](auto& i){i.lodSeed^=8u;},"wrong invert accepted");
        badItem([](auto& i){i.lodSeed|=16u;},"unknown seed bits accepted");
        badItem([](auto& i){i.layer=MeshRenderLayer3D::ShadowOnly;},"mixed layers accepted");
        badItem([](auto& i){i.layer=MeshRenderLayer3D::Overlay;},"overlay accepted");
        badItem([](auto& i){i.castsShadow=false;},"pair shadow mismatch accepted");
        Reject([&]{Observer o(middle);auto item=Item(middle,true,MeshRenderLayer3D::ShadowOnly);
            item.castsShadow=false;o.Observe(item);},"shadow-only noncaster accepted");
        Reject([&]{Observer o(middle);o.Observe(n);(void)o.Finish();},"missing far accepted");
        Reject([&]{Observer o(middle);o.Observe(f);(void)o.Finish();},"missing near accepted");
        Reject([&]{Observer o(middle);o.Observe(n);o.Observe(n);},"duplicate near accepted");
        Reject([&]{Observer o(middle);o.Observe(f);o.Observe(f);},"duplicate far accepted");
        Reject([&]{Observer o(middle);o.Observe(n);o.Observe(f);o.Observe(n);},"third draw accepted");
        for(const auto& e:{nearCase,farCase}) {
            Reject([&]{Observer o(e);o.Observe(Item(e,e.selection.primaryLevel!=0));},"wrong single endpoint accepted");
            for(unsigned change=0;change<3;++change)Reject([&]{Observer o(e);auto i=Item(e,e.selection.primaryLevel==0);
                if(change==0)i.lodCoverage=.5f;else if(change==1)i.lodSeed=8;else i.lodSeed=1;o.Observe(i);},"single coverage or seed accepted");
        }
        Reject([&]{auto e=middle;e.selection.farCoverage=0;(void)Observer(e);},"invalid expected transition accepted");
        Reject([&]{auto e=nearCase;e.farMesh=e.nearMesh;(void)Observer(e);},"aliased endpoints accepted");
        Reject([&]{auto e=nearCase;e.material.normalScale=std::numeric_limits<float>::infinity();(void)Observer(e);},"nonfinite expected material accepted");
        Observer poisoned(middle);try{poisoned.Observe(n);poisoned.Observe(n);}catch(const std::invalid_argument&){}
        Reject([&]{(void)poisoned.Finish();},"ignored invalid observation accepted");
        std::cout<<"LANDSCAPE_TWIG_DRAW_CPU_PASS checks="<<checks<<" negatives="<<negatives<<" native=0\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"LANDSCAPE_TWIG_DRAW_FIRST_DIVERGENCE "<<error.what()<<" checks="<<checks<<'\n';return 1;
    }
}
