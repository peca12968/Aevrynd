#pragma once
#include <Aevrynd/StaticAabbIndex3D.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/Camera.h>
#include <algorithm>
#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace MatureContact {
inline void Require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
struct FileSeal {const char* name;std::size_t bytes;const char* sha;};
}
#include "MatureContactAssets.h"
namespace MatureContact {
using namespace Aevrynd;
// 完整來源與每次查詢工作區是不同契約；不把47k來源冒充10k活動清單。
class World final {
    std::array<std::vector<BoxCollider3D>,2> original_,complete_;
    std::array<std::unique_ptr<StaticAabbIndex3D>,2> index_;
    std::array<std::array<float,40>,12> poses_{};
    std::array<BoxCollider3D,12> oldBodies_{};
    double minimumY_{},maximumY_{};
    bool ready_{};
    std::uint64_t drawCalls_{};
    static std::vector<std::byte> Read(const std::filesystem::path& root,const FileSeal& seal){
        const auto path=root/seal.name;
        Require(std::filesystem::is_regular_file(path)&&!std::filesystem::is_symlink(path)&&std::filesystem::file_size(path)==seal.bytes,"MATURE_SOURCE_SIZE_OR_TYPE");
        std::vector<std::byte> raw(seal.bytes);std::ifstream file(path,std::ios::binary);
        file.read(reinterpret_cast<char*>(raw.data()),std::streamsize(raw.size()));Require(bool(file)&&file.peek()==std::char_traits<char>::eof(),"MATURE_COMPLETE_SOURCE_BYTES");
        std::array<std::byte,32> digest{};Require(WorldDeltaSha2563D(raw,digest),"MATURE_SOURCE_SHA256");
        std::string actual;for(const auto value:digest){const auto byte=std::to_integer<unsigned>(value);actual+="0123456789ABCDEF"[byte>>4];actual+="0123456789ABCDEF"[byte&15];}
        Require(actual==seal.sha,"MATURE_SOURCE_HASH_MISMATCH");return raw;
    }
    static ClosedAabb3D Bounds(const BoxCollider3D& box){
        ClosedAabb3D b;const float center[]{box.center.x,box.center.y,box.center.z},extent[]{box.extents.x,box.extents.y,box.extents.z};
        for(unsigned a=0;a<3;++a){Require(std::isfinite(center[a])&&std::isfinite(extent[a])&&extent[a]>0,"MATURE_FINITE_POSITIVE_SOURCE_BOX");b.minimum[a]=double(center[a])-extent[a];b.maximum[a]=double(center[a])+extent[a];}return b;
    }
    static bool Same(const BoxCollider3D& a,const BoxCollider3D& b){static_assert(sizeof(BoxCollider3D)==24);return std::memcmp(&a,&b,24)==0;}
public:
    struct Workspace {
        std::vector<std::uint32_t> scratch,ids;
        std::vector<BoxCollider3D> boxes;
        std::size_t maximum{};
        Workspace():scratch(10000),ids(10000){boxes.reserve(10000);}
    } body,camera;
    bool Ready()const noexcept{return ready_;}
    bool Penetrates(Vec3 feet,Vec3 extents,bool woodOnly=false)const {
        Require(ready_,"MATURE_OVERLAP_BEFORE_SOURCE");const auto center=feet+Vec3{0,extents.y,0};
        const auto& boxes=complete_[0];for(std::size_t i=woodOnly?9356:0;i<boxes.size();++i){const auto& b=boxes[i];
            if(std::abs(double(center.x)-b.center.x)<double(extents.x)+b.extents.x&&
               std::abs(double(center.y)-b.center.y)<double(extents.y)+b.extents.y&&
               std::abs(double(center.z)-b.center.z)<double(extents.z)+b.extents.z)return true;}
        return false;
    }
    std::span<const BoxCollider3D> All(bool repaired)const {Require(ready_,"MATURE_WORLD_NOT_READY");return complete_[unsigned(repaired)];}
    std::span<const BoxCollider3D> Original(bool repaired)const {Require(ready_,"MATURE_ORIGINAL_NOT_READY");return original_[unsigned(repaired)];}
    void Load(const std::filesystem::path& directory,std::span<const BoxCollider3D> before,std::span<const BoxCollider3D> after){
        Require(!ready_&&before.size()==9368&&after.size()==9369,"MATURE_EXACT_ORIGINAL_BOTH_BRIDGE_STATES");
        const auto source=Read(directory,Files[0]);std::uint32_t header[4]{};std::memcpy(header,source.data(),16);
        Require(header[0]==0x3154434du&&header[1]==1&&header[2]==12&&header[3]==9368&&source.size()==226816,"MATURE_NATIVE_SOURCE_SCHEMA");
        Require(std::memcmp(source.data()+16+12*164,before.data(),before.size_bytes())==0,"MATURE_CURRENT_WORLD_DIFFERS_FROM_CAPTURED_COMPLETE_SOURCE");
        for(unsigned tree=0;tree<12;++tree){const auto* entry=source.data()+16+tree*164;std::memcpy(poses_[tree].data(),entry,40*sizeof(float));std::memcpy(&oldBodies_[tree],entry+18*sizeof(float),24);}
        std::vector<BoxCollider3D> wood;
        for(unsigned tree=0;tree<12;++tree){const auto raw=Read(directory,Files[tree+1]);Require(raw.size()%24==0,"MATURE_WOOD_RECORD_SIZE");const auto old=wood.size();wood.resize(old+raw.size()/24);std::memcpy(wood.data()+old,raw.data(),raw.size());}
        Require(wood.size()==37658&&StaticAabbIndex3D::MaximumBounds==65536,"MATURE_COMPLETE_WOOD_AND_UNCHANGED_REGISTRATION_CAP");
        for(unsigned state=0;state<2;++state){const auto input=state?after:before;original_[state].assign(input.begin(),input.end());std::set<std::size_t> owners;
            for(const auto& old:oldBodies_){unsigned matches{};for(std::size_t i=0;i<input.size();++i)if(Same(old,input[i])){++matches;owners.insert(i);}Require(matches==1,"MATURE_OLD_BODY_HAS_ONE_OWNER_IN_EACH_BRIDGE_STATE");}
            Require(owners.size()==12,"MATURE_TWELVE_DISTINCT_OLD_OWNERS");auto& result=complete_[state];result.reserve(input.size()-12+wood.size());
            for(std::size_t i=0;i<input.size();++i)if(!owners.contains(i))result.push_back(input[i]);
            Require(result.size()==9356+state,"MATURE_ALL_NONREPLACED_PROPS_RETAINED");result.insert(result.end(),wood.begin(),wood.end());Require(result.size()==47014+state,"MATURE_COMPLETE_REGISTERED_WORLD");
            std::vector<ClosedAabb3D> bounds;bounds.reserve(result.size());for(const auto& box:result)bounds.push_back(Bounds(box));
            index_[state]=std::make_unique<StaticAabbIndex3D>(bounds);Require(index_[state]->Valid()&&index_[state]->Size()==result.size(),"MATURE_ONE_EXISTING_GENERIC_INDEX");
            for(const auto& b:bounds){minimumY_=(std::min)(minimumY_,b.minimum[1]);maximumY_=(std::max)(maximumY_,b.maximum[1]);}
        }
        ready_=true;
    }
    std::span<const BoxCollider3D> Gather(bool repaired,double lowX,double highX,double lowZ,double highZ,Workspace& workspace){
        Require(ready_,"MATURE_QUERY_BEFORE_REGISTRATION");ClosedAabb3D query{{lowX,minimumY_,lowZ},{highX,maximumY_,highZ}};std::size_t count{};
        Require(index_[unsigned(repaired)]->QueryOrdinals(query,workspace.scratch,workspace.ids,count),"MATURE_ACTIVE10000_QUERY_FAILED_NO_TRUNCATION");
        workspace.boxes.clear();for(std::size_t i=0;i<count;++i)workspace.boxes.push_back(complete_[unsigned(repaired)].at(workspace.ids[i]));
        workspace.maximum=(std::max)(workspace.maximum,count);return workspace.boxes;
    }
    std::span<const BoxCollider3D> Body(Vec3 feet,bool repaired){return Gather(repaired,double(feet.x)-4,double(feet.x)+4,double(feet.z)-4,double(feet.z)+4,body);}
    std::span<const BoxCollider3D> Camera(Vec3 feet,float distance,const Camera3D* previous,bool repaired){
        const double radius=double(distance)+2;double lx=double(feet.x)-radius,hx=double(feet.x)+radius,lz=double(feet.z)-radius,hz=double(feet.z)+radius;
        if(previous){lx=(std::min)(lx,double(previous->position.x)-1);hx=(std::max)(hx,double(previous->position.x)+1);lz=(std::min)(lz,double(previous->position.z)-1);hz=(std::max)(hz,double(previous->position.z)+1);}
        return Gather(repaired,lx,hx,lz,hz,camera);
    }
    void ObserveDraw(const Transform3D& old,const Transform3D& actual){
        Require(ready_,"MATURE_DRAW_WITHOUT_REGISTERED_WORLD");const auto ordinal=std::size_t(drawCalls_%12);
        const std::array<float,18> pose{old.position.x,old.position.y,old.position.z,old.rotation.x,old.rotation.y,old.rotation.z,old.scale.x,old.scale.y,old.scale.z,
            actual.position.x,actual.position.y,actual.position.z,actual.rotation.x,actual.rotation.y,actual.rotation.z,actual.scale.x,actual.scale.y,actual.scale.z};
        const auto matrix=actual.Matrix();Require(std::memcmp(pose.data(),poses_[ordinal].data(),sizeof(pose))==0&&std::memcmp(&matrix,poses_[ordinal].data()+24,64)==0,"MATURE_ACTUAL_DRAW_POSE_DIFFERS_FROM_AUTHORED_WOOD");++drawCalls_;
    }
    void Write(const std::filesystem::path& output)const {
        Require(ready_&&drawCalls_>=12&&drawCalls_%12==0,"MATURE_FULL_TWELVE_DRAW_SUBMISSIONS");
        std::ofstream out(output/"mature-world-source.txt");out<<"registered_before="<<complete_[0].size()<<"\nregistered_after="<<complete_[1].size()<<"\nnew_wood=37658\nreplaced_old_owners=12\nactive_capacity=10000\nregistration_capacity=65536\nmaximum_body_active="<<body.maximum<<"\nmaximum_camera_active="<<camera.maximum<<"\nactual_draw_calls="<<drawCalls_<<"\nbody_camera_scope=bounded_diagnostic_only\nplant_ai_save_accepted=0\n";out.close();Require(bool(out),"MATURE_SOURCE_RECEIPT_WRITE");
        for(unsigned state=0;state<2;++state){std::ofstream raw(output/(state?"mature-world-after.f32":"mature-world-before.f32"),std::ios::binary);
            raw.write(reinterpret_cast<const char*>(complete_[state].data()),std::streamsize(complete_[state].size()*24));raw.close();Require(bool(raw),"MATURE_FULL_REGISTERED_SOURCE_WRITE");}
    }
};
}
