#include "MistwatchBridgeTimberVisual.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <set>

using namespace Aevrynd;
namespace { unsigned checks{};void Need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);} }
int main(int argc,char** argv) {
    try {
        Need(argc==2,"CURRENT_HEIGHTFIELD_REQUIRED");
        HeightField3D field({{-50,0,-20},201,241,.5f});
        std::ifstream file(argv[1]);std::string header;std::getline(file,header);
        Need(header=="x\tz\tbefore\tafter","REAL_TERRAIN_SCHEMA");
        float x{},z{},before{},after{};unsigned rows{};
        while(file>>x>>z>>before>>after) {
            Need(rows<48441&&x==-50+float(rows%201)*.5f&&z==-20+float(rows/201)*.5f,"CURRENT_ORDERED_TERRAIN");
            Need(field.SetHeight(rows%201,rows/201,after),"SET_HEIGHT");++rows;
        }
        Need(file.eof()&&rows==48441,"COMPLETE_TERRAIN");field.RecalculateRange();
        const auto original=Mistwatch::BearingBridge::Build(field);const auto saved=original;
        const auto visual=Mistwatch::BridgeTimberVisual::Create(original);
        Need(visual.ranges.size()==53&&visual.wood.vertices.size()==5088&&visual.wood.indices.size()==6996,"COMPLETE53MEMBERS");
        for(std::size_t i=0;i<original.parts.size();++i) {
            const auto& a=original.parts[i];const auto& b=saved.parts[i];
            Need(a.center.x==b.center.x&&a.center.y==b.center.y&&a.center.z==b.center.z&&a.size.x==b.size.x&&a.size.y==b.size.y&&a.size.z==b.size.z&&a.role==b.role&&a.tone==b.tone,"ORIGINAL_PART_NOT_MUTATED");
        }
        Need(original.additions.size()==saved.additions.size()&&std::memcmp(original.additions.data(),saved.additions.data(),original.additions.size()*sizeof(BoxCollider3D))==0,"COLLISION_SOURCE_UNCHANGED");
        unsigned plankTops{},reversePlanks{};std::array<bool,57> members{};
        std::set<int> plankPhases;
        for(const auto& range:visual.ranges) {
            Need(range.part<57&&!members[range.part],"UNIQUE_PART_OWNERSHIP");members[range.part]=true;
            const auto& part=original.parts[range.part];Need(part.role!=Mistwatch::BearingBridge::Role::Footing,"STONE_NOT_RETEXTURED");
            Vec3 low{1.e6f,1.e6f,1.e6f},high{-1.e6f,-1.e6f,-1.e6f};
            unsigned topVertices{};
            float leftGrain{},rightGrain{};
            for(std::size_t i=range.firstVertex;i<range.firstVertex+96u;++i) {
                const auto& v=visual.wood.vertices[i];const Vec3 p{v.x,v.y,v.z};
                const auto n=UnpackNormalSnorm8(v.normal);
                Need(std::isfinite(v.u)&&std::isfinite(v.v)&&n.x*n.x+n.y*n.y+n.z*n.z>.97f,"FINITE_AUTHORED_VERTEX");
                low.x=(std::min)(low.x,p.x);low.y=(std::min)(low.y,p.y);low.z=(std::min)(low.z,p.z);
                high.x=(std::max)(high.x,p.x);high.y=(std::max)(high.y,p.y);high.z=(std::max)(high.z,p.z);
                if(part.role==Mistwatch::BearingBridge::Role::Plank&&n.y>.99f) {
                    Need(std::abs(v.y)<1.e-7f,"VISIBLE_DECK_TOP_STAYS_ZERO");++topVertices;
                    const float grain=v.v*Mistwatch::WorkshopVisual::StaticDecodedScale;
                    if(v.x<part.center.x)leftGrain=grain;else rightGrain=grain;
                }
            }
            for(unsigned axis=0;axis<3;++axis) {
                const std::array<float,3> lo{low.x,low.y,low.z},hi{high.x,high.y,high.z},c{part.center.x,part.center.y,part.center.z},s{part.size.x,part.size.y,part.size.z};
                Need(std::abs(lo[axis]-(c[axis]-s[axis]*.5f))<4.e-6f&&std::abs(hi[axis]-(c[axis]+s[axis]*.5f))<4.e-6f,"MEMBER_OUTER_BOUNDS_UNCHANGED");
            }
            if(part.role==Mistwatch::BearingBridge::Role::Plank){
                Need(topVertices==4,"ONE_COMPLETE_TOP_FACE_PER_PLANK");++plankTops;
                const float span=std::abs(rightGrain-leftGrain);
                const float meters=(part.size.x-2.f*.006f)/span;
                Need(meters>=1.319f&&meters<=1.481f,"GRAIN_ALONG_FOUR_METER_PLANK_NOT_STRETCHED_ATLAS");
                if(rightGrain<leftGrain)++reversePlanks;
                const float phase=(std::min)(leftGrain,rightGrain)-.006f/meters;
                Need(phase>=-1.e-5f&&phase<1.f,"PHASE_WITHIN_TEXTURE_PERIOD");
                Need(plankPhases.insert(int(std::lround(1024.f*phase))).second,"REPEATED_PLANK_GRAIN_PHASE");
            }
        }
        Need(plankTops==32,"ALL32DECK_PLANKS");
        Need(reversePlanks>0&&reversePlanks<plankTops,"BOTH_GRAIN_DIRECTIONS");
        for(std::size_t i=0;i<57;++i)Need(members[i]==(original.parts[i].role!=Mistwatch::BearingBridge::Role::Footing),"NO_MEMBER_HIDDEN_OR_ADDED");
        for(std::size_t i=0;i<visual.wood.indices.size();i+=3) {
            const auto& a=visual.wood.vertices[visual.wood.indices[i]];
            const auto& b=visual.wood.vertices[visual.wood.indices[i+1]];
            const auto& c=visual.wood.vertices[visual.wood.indices[i+2]];
            const auto cross=Cross(Vec3{b.x-a.x,b.y-a.y,b.z-a.z},Vec3{c.x-a.x,c.y-a.y,c.z-a.z});
            const auto n=UnpackNormalSnorm8(a.normal);
            Need(Dot(cross,{n.x,n.y,n.z})>0,"OUTWARD_NONDEGENERATE_TRIANGLES");
            const double area=(double(b.u)-a.u)*(double(c.v)-a.v)-(double(c.u)-a.u)*(double(b.v)-a.v);
            Need(std::isfinite(area)&&area!=0,"ALL_TRIANGLES_HAVE_UV_AREA");
        }
        std::cout<<"BRIDGE_TIMBER_PASS members=53 planks=32 collision_boxes=25 triangles=2332 checks="<<checks<<"\n";return 0;
    } catch(const std::exception& e){std::cerr<<"BRIDGE_TIMBER_FIRST_DIVERGENCE="<<e.what()<<"\n";return 1;}
}
