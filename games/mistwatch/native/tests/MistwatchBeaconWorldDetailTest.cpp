#include "MistwatchBeaconDetail.h"
#include "MistwatchWorld.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
unsigned checks{};
void Require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
bool Same(Aevrynd::Vec3 a,Aevrynd::Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool Same(Aevrynd::Color a,Aevrynd::Color b){return a.r==b.r&&a.g==b.g&&a.b==b.b&&a.a==b.a;}
Mistwatch::Session Ready() {
    Mistwatch::Session s;
    s.collected=7;s.repairMaterials=7;s.bridgeRepaired=true;s.guardianDefeated=true;
    s.fuseClueRead=true;s.fuseDials=Mistwatch::Session::solvedFuseDials;s.fuseCollected=true;
    s.circuit=7;s.powerRestored=true;
    for(unsigned index:{0u,2u}) {
        s.feet=Mistwatch::Session::relaySites[index];
        auto unconfirmed=s;
        for(unsigned turn=0;turn<Mistwatch::Session::RelayTargetSector(index);++turn)
            Require(unconfirmed.TuneRelay(index,1),"ready fixture probe tuning failed");
        const auto beforeLocal=unconfirmed.Serialize();
        Require(!beforeLocal.empty()&&!unconfirmed.ConnectRelay(index)&&unconfirmed.Serialize()==beforeLocal,
            "ready fixture unconfirmed relay connected or changed state");
        if(index==0)Require(s.CycleRelayPatch(0)&&s.ConfirmRelayCircuit(),"ready fixture west confirmation failed");
        else Require(s.CycleExtractionRoute()&&s.ConfirmExtractionRoute(),"ready fixture northwest confirmation failed");
        Require(s.RelayFieldTaskReady(index)&&!s.relayLinked&&!s.relaySectors,
            "ready local confirmation changed original link or angles");
    }
    s.relayLinked=7;s.feet=Mistwatch::Session::beacon;
    for(unsigned i=0;i<3;++i)s.relaySectors|=Mistwatch::Session::RelayTargetSector(i)<<(i*4);
    return s;
}
}
int main() {
    try {
        using namespace Aevrynd;using namespace Mistwatch;
        const auto unit=Mesh3D::CreateUnitBox();Require(unit.Valid(),"Unit box invalid");
        Require(unit.vertices.size()==24&&unit.indices.size()==36,"Original box index contract changed");
        for(auto i:unit.indices)Require(i<unit.vertices.size(),"Box index out of bounds");
        auto off=Ready();Require(off.Valid()&&off.CurrentStage()==Stage::Beacon,"Invalid ready Session");
        const auto savedOff=off.Serialize();const auto a=CreateBeaconDetail(off);Require(a.has_value(),"Off geometry rejected");
        Require(off.Serialize()==savedOff,"Geometry mutated Session");
        auto on=off;Require(on.LightBeacon(),"Real Session activation failed");
        const auto savedOn=on.Serialize();const auto b=CreateBeaconDetail(on);Require(b.has_value()&&b->lit,"On geometry rejected");
        Require(on.Serialize()==savedOn&&on.CurrentStage()==Stage::Complete,"Geometry mutated activated Session");
        Vec3 low{10000,10000,10000},high{-10000,-10000,-10000};
        for(std::size_t i=0;i<a->parts.size();++i) {
            const auto& p=a->parts[i];const auto& q=b->parts[i];
            Require(Same(p.center,q.center)&&Same(p.size,q.size),"Activation moved geometry");
            Require(Session::Finite(p.center)&&Session::Finite(p.size)&&p.size.x>0&&p.size.y>0&&p.size.z>0,"Invalid dimensions");
            for(const auto* m:{&p.material,&q.material}) {
                Require(std::isfinite(m->surface.roughness)&&std::isfinite(m->surface.metallic)&&std::isfinite(m->surface.emissive)&&
                    m->surface.roughness>=0&&m->surface.roughness<=1&&m->surface.metallic>=0&&m->surface.metallic<=1&&
                    m->surface.emissive>=0&&m->surface.emissive<=1,"Invalid surface");
                Require(m->alphaMode==MaterialAlphaMode3D::Opaque&&m->tint.a==1&&m->castsShadow,"Transparent or nonshadow detail");
            }
            Require(p.material.surface.emissive==0,"Off device emits light");
            if(i!=BeaconLensPart&&i!=BeaconStatusPart)
                Require(Same(p.material.tint,q.material.tint)&&q.material.surface.emissive==0,"Activation recolored structural parts");
            const auto matrix=Transform3D{p.center,{},p.size}.Matrix();
            for(const auto& v:unit.vertices) {
                const auto pos=TransformPoint(matrix,{v.x,v.y,v.z})-Session::beacon;
                Require(Session::Finite(pos)&&std::abs(pos.x)<=.35f&&std::abs(pos.z)<=.35f&&pos.y>=-.000001f&&pos.y<=2.301f,
                    "Actual vertex escaped old footprint/new height");
                low.x=(std::min)(low.x,pos.x);low.y=(std::min)(low.y,pos.y);low.z=(std::min)(low.z,pos.z);
                high.x=(std::max)(high.x,pos.x);high.y=(std::max)(high.y,pos.y);high.z=(std::max)(high.z,pos.z);
            }
        }
        Require(b->parts[BeaconLensPart].material.surface.emissive==.9f&&b->parts[BeaconStatusPart].material.surface.emissive==.5f,"Lit signal not emissive");
        Require(high.y-low.y<2.301f&&high.x-low.x<.35f&&high.z-low.z<.35f,"New assembly not slender");
        // 真 Scene3D / DrawList 純CPU提交；假handle僅測descriptor，不建立D3D裝置。
        const auto cpu=std::make_shared<Mesh3D>(unit);
        const MeshResource3D mock{{1},unit.Bounds(),unit.vertices.size(),unit.indices.size(),cpu};
        Scene3D scene;scene.SetBoxMesh(mock);DrawList draw;
        const auto submit=[&](const Session& s){
            scene.Clear();draw.Clear();scene.SetBoxMesh(mock);
            Require(SubmitBeaconDetail(scene,s),"Scene detail insertion rejected");
            Require(scene.MeshCount()==23&&scene.TriangleCount()==276,"Scene part/triangle count wrong");
            const Camera3D camera{{0,2.97312f,64.268f},{0,1.2f,70}};
            Scene3DSubmissionOptions options;options.shadows.enabled=false;options.frustumCulling=false;
            const auto stats=scene.Submit(draw,camera,{1280,720},options);
            Require(stats.inputTriangles==276&&draw.meshInstances.size()==23,"Actual scene submission lost parts");
            Require(draw.PointLights().empty(),"Device added unexpected light slot");
            for(std::size_t i=0;i<23;++i){
                Require(draw.meshInstances[i].surface==PackSurface3D(CreateBeaconDetail(s)->parts[i].material.surface),"Submitted material mismatch");
                Require(draw.meshInstances[i].alphaMode==MaterialAlphaMode3D::Opaque&&draw.meshInstances[i].occlusionOpacity==1,"Submitted alpha/occlusion changed");
            }
        };
        submit(off);submit(on);submit(off);
        auto bad=off;bad.feet.x=std::numeric_limits<float>::quiet_NaN();
        scene.Clear();Require(!SubmitBeaconDetail(scene,bad)&&scene.MeshCount()==0,"Invalid Session partially submitted");
        for(float x:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),1001.f})
            Require(!CreateBeaconDetail(off,{x,0,70}),"Invalid anchor accepted");
        Session restored;Require(restored.Restore(savedOn)&&restored.beaconLit,"Activated save restore failed");
        Require(CreateBeaconDetail(restored)->lit,"Restored Session lost visible state");
        // 原世界函数完全不接收新视觉作者；真再作者列表与序列不应变化。
        for(auto selector:{FirLayoutSelector::Legacy,FirLayoutSelector::ForestWood}) {
            const auto before=WorldSolids(true,selector);Require(SubmitBeaconDetail(scene,on),"Repeat scene insertion failed");
            const auto after=WorldSolids(true,selector);Require(before.size()==after.size(),"World count changed");
            for(std::size_t i=0;i<before.size();++i)Require(Same(before[i].center,after[i].center)&&Same(before[i].extents,after[i].extents),"World geometry changed");
        }
        std::cout<<"BEACON_DETAIL_CPU_PASS parts=23 triangles=276 states=2 submissions=3 height="<<high.y
            <<" checks="<<checks<<" native_accepted=0 point_lights=0\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"FIRST_DIVERGENCE: "<<error.what()<<'\n';return 1;}
}
