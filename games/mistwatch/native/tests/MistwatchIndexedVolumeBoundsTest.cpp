#include "MistwatchIndexedVolumeBounds.h"
#include "MistwatchImmutableVolumeBounds.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
using namespace Aevrynd;using namespace Mistwatch::CanopyVolume;
namespace {unsigned checks{};void Need(bool value){++checks;if(!value)throw std::runtime_error("INDEXED_VOLUME_ASSERT");}
void Compare(const PlantVolume& plant,std::span<const BoxCollider3D> boxes,float margin){
    const bool expected=ClearsCanopyWood(plant,boxes,margin);IndexedVolumeBounds indexed(boxes);
    Need(indexed.IndexedParts()==0&&indexed.Clears(plant,margin)==expected&&indexed.Clears(plant,margin)==expected);
}
float& Axis(Vec3& v,unsigned a){return a==0?v.x:a==1?v.y:v.z;}
}
int main(int argc,char** argv){try{
    const PlantVolume plant{{0,0,0},{.5f,0,1},1,{}};const BoxCollider3D overlap{{0,.5f,0},{.5f,.5f,.5f}},far{{100,100,100},{1,1,1}};
    Compare(plant,{},0);Compare(plant,std::span(&overlap,1),0);Compare(plant,std::span(&far,1),0);
    for(unsigned axis=0;axis<3;++axis)for(float sign:{-1.f,1.f})for(float margin:{0.f,.03125f}){
        auto box=overlap;const float touch=axis==1?(sign>0?1.5f:-.5f):sign;Axis(box.center,axis)=touch+sign*margin;
        Compare(plant,std::span(&box,1),margin);Axis(box.center,axis)=std::nextafter(touch+sign*margin,sign>0?std::numeric_limits<float>::infinity():-std::numeric_limits<float>::infinity());Compare(plant,std::span(&box,1),margin);
        Axis(box.center,axis)=std::nextafter(touch+sign*margin,sign>0?-std::numeric_limits<float>::infinity():std::numeric_limits<float>::infinity());Compare(plant,std::span(&box,1),margin);}
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    for(float value:{nan,inf,-inf})for(unsigned axis=0;axis<3;++axis){auto p=plant;Axis(p.root,axis)=value;Compare(p,std::span(&far,1),0);
        p=plant;Axis(p.rotation,axis)=value;Compare(p,{},0);auto box=far;Axis(box.extents,axis)=value;Compare(plant,std::span(&box,1),0);box=far;Axis(box.center,axis)=value;Compare(plant,std::span(&box,1),0);}
    for(float bad:{0.f,-1.f,nan,inf}){auto p=plant;p.local.radius=bad;Compare(p,{},0);p=plant;p.uniformScale=bad;Compare(p,{},0);Compare(plant,std::span(&far,1),bad);}
    auto extremePlant=plant;extremePlant.uniformScale=std::numeric_limits<float>::denorm_min();Compare(extremePlant,{},0);
    extremePlant=plant;extremePlant.root.x=std::numeric_limits<float>::max();Compare(extremePlant,{},0);
    extremePlant=plant;extremePlant.local.radius=2;extremePlant.uniformScale=std::numeric_limits<float>::max();Compare(extremePlant,{},0);
    extremePlant=plant;extremePlant.local.maximumY=extremePlant.local.minimumY;Compare(extremePlant,{},0);
    for(float bad:{nan,inf,-inf}){extremePlant=plant;extremePlant.local.minimumY=bad;Compare(extremePlant,{},0);extremePlant=plant;extremePlant.local.maximumY=bad;Compare(extremePlant,{},0);}
    auto invalid=far;invalid.extents.x=0;const std::array<BoxCollider3D,2> tail{overlap,invalid};Compare(plant,tail,0);
    std::vector<BoxCollider3D> excessive(10001,far);Compare(plant,excessive,0);
    auto badPlant=plant;badPlant.local.radius=0;IndexedVolumeBounds lazy(std::span(&far,1));Need(!lazy.Clears(badPlant)&&lazy.IndexedParts()==0&&lazy.Queries()==0);
    // 舊順序／惰性計數API不改，仍使用原helper的owner。
    ImmutableVolumeBoundsCache original(tail);Need(!original.Clears(plant,0)&&original.ComputedParts()==1);
    std::vector<BoxCollider3D> world;
    if(argc==2){std::ifstream file(argv[1]);Need(bool(file));std::string line;Need(bool(std::getline(file,line))&&line=="index\tx\ty\tz\tex\tey\tez");
        while(std::getline(file,line)){std::istringstream row(line);std::size_t id{};BoxCollider3D box;Need(bool(row>>id>>box.center.x>>box.center.y>>box.center.z>>box.extents.x>>box.extents.y>>box.extents.z)&&id==world.size());world.push_back(box);}
        Need(world.size()==9369);
    }else{Need(argc==1);for(unsigned z=0;z<64;++z)for(unsigned x=0;x<128;++x)world.push_back({{float(x)*2-128,.5f,float(z)*2-64},{.4f,.7f,.4f}});}
    IndexedVolumeBounds indexed(world);ImmutableVolumeBoundsCache linear(world);std::uint32_t seed=0x504c414eu;const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    for(unsigned i=0;i<2048;++i){const PlantVolume p{{-50+100*random(),-2+15*random(),-20+120*random()},{.1f+.6f*random(),-.05f,.8f},.5f+random(),{0,random()*6.2f,0}};
        Need(indexed.Clears(p)==linear.Clears(p)&&indexed.Clears(p)==ClearsCanopyWood(p,world));}
    Need(indexed.IndexedParts()==world.size());
    std::cout<<"INDEXED_VOLUME_BOUNDS_PASS checks="<<checks<<" queries="<<indexed.Queries()<<" source_parts="<<world.size()<<" box_tests="<<indexed.BoxesTested()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 20;}}
