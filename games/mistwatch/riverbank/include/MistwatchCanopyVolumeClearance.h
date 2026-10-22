#pragma once
#include <Aevrynd/CollisionWorld3D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>

namespace Mistwatch::CanopyVolume {
struct PlantLocalVolume {float radius{},minimumY{},maximumY{};};
struct PlantVolume {
    Aevrynd::Vec3 root{};
    PlantLocalVolume local{};
    float uniformScale{1.f};
    Aevrynd::Vec3 rotation{};
};
namespace Detail {
inline bool Finite(Aevrynd::Vec3 value) noexcept {
    return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);
}
inline bool ValidLocal(PlantLocalVolume local) noexcept {
    return std::isfinite(local.radius)&&local.radius>0&&std::isfinite(local.minimumY)&&
        std::isfinite(local.maximumY)&&local.minimumY<local.maximumY;
}
struct Interval {double minimum{},maximum{};};
using Bounds=std::array<Interval,3>;
inline bool ValidInterval(Interval interval) noexcept {
    constexpr double limit=std::numeric_limits<float>::max();
    // 雙精度先算邊界，再拒絕超出 float 世界或轉回 float 已塌縮的體積。
    return std::isfinite(interval.minimum)&&std::isfinite(interval.maximum)&&
        interval.minimum>=-limit&&interval.maximum<=limit&&interval.minimum<interval.maximum&&
        static_cast<float>(interval.minimum)<static_cast<float>(interval.maximum);
}
inline bool ValidBounds(const Bounds& bounds) noexcept {
    for(const auto interval:bounds)if(!ValidInterval(interval))return false;
    return true;
}
inline Interval Outward(Interval interval) noexcept {
    constexpr double infinity=std::numeric_limits<double>::infinity();
    return {std::nextafter(interval.minimum,-infinity),std::nextafter(interval.maximum,infinity)};
}
inline bool WoodBounds(const Aevrynd::BoxCollider3D& box,Bounds& output) noexcept {
    if(!Finite(box.center)||!Finite(box.extents)||box.extents.x<=0||box.extents.y<=0||box.extents.z<=0)return false;
    const double center[]{box.center.x,box.center.y,box.center.z},extent[]{box.extents.x,box.extents.y,box.extents.z};
    for(unsigned axis=0;axis<3;++axis)output[axis]=Outward({center[axis]-extent[axis],center[axis]+extent[axis]});
    return ValidBounds(output);
}
inline bool PlantBounds(const PlantVolume& plant,float margin,Bounds& output) noexcept {
    if(!Finite(plant.root)||!Finite(plant.rotation)||plant.rotation.x!=0||plant.rotation.z!=0||
       !ValidLocal(plant.local)||!std::isfinite(plant.uniformScale)||plant.uniformScale<=0||
       !std::isfinite(margin)||margin<0)return false;
    // 水平半徑包住完整 yaw 旋轉；此介面明確拒絕 pitch、roll。
    const double scale=plant.uniformScale,radius=double(plant.local.radius)*scale;
    const double minimumY=double(plant.local.minimumY)*scale,maximumY=double(plant.local.maximumY)*scale;
    constexpr double limit=std::numeric_limits<float>::max();
    if(!std::isfinite(radius)||radius>limit||static_cast<float>(radius)<=0||
       !std::isfinite(minimumY)||!std::isfinite(maximumY)||minimumY<-limit||maximumY>limit||
       static_cast<float>(minimumY)>=static_cast<float>(maximumY))return false;
    const Detail::Bounds raw{{{double(plant.root.x)-radius,double(plant.root.x)+radius},
        {double(plant.root.y)+minimumY,double(plant.root.y)+maximumY},
        {double(plant.root.z)-radius,double(plant.root.z)+radius}}};
    if(!ValidBounds(raw))return false;
    // 先驗未擴張體積，margin 不能把無效／塌縮的植物補成有效。
    for(unsigned axis=0;axis<3;++axis)output[axis]=Outward({raw[axis].minimum-double(margin),raw[axis].maximum+double(margin)});
    return ValidBounds(output);
}
}
// 所有木件在配置器入口完整驗證，不能以零目標或早期不相交繞過壞資料。
[[nodiscard]] inline bool ValidWoodParts(std::span<const Aevrynd::BoxCollider3D> woodParts) noexcept {
    if(woodParts.size()>10000)return false;
    for(const auto& part:woodParts){Detail::Bounds bounds;if(!Detail::WoodBounds(part,bounds))return false;}
    return true;
}
// 保留原作者 float 半徑與指紋；僅3D檢查向外一個float ULP涵蓋舊 hypot 的捨入。
[[nodiscard]] inline PlantLocalVolume ConservativeLocalVolume(float radius,float minimumY,float maximumY) noexcept {
    if(std::isfinite(radius)&&radius>0)radius=std::nextafter(radius,std::numeric_limits<float>::infinity());
    return {radius,minimumY,maximumY};
}
[[nodiscard]] inline bool ClearsCanopyWood(const PlantVolume& plant,
    std::span<const Aevrynd::BoxCollider3D> woodParts={},float touchMargin=.03f) noexcept {
    Detail::Bounds plantBounds;
    // 空木件相容舊世界，helper仍先拒絕無效植物。
    if(!Detail::PlantBounds(plant,touchMargin,plantBounds)||woodParts.size()>10000)return false;
    for(const auto& part:woodParts){
        Detail::Bounds woodBounds;if(!Detail::WoodBounds(part,woodBounds))return false;
        bool separated=false;
        for(unsigned axis=0;axis<3;++axis)separated|=plantBounds[axis].maximum<woodBounds[axis].minimum||
            woodBounds[axis].maximum<plantBounds[axis].minimum;
        // 真三軸重疊、邊界相觸或margin內接近皆拒絕；不合併木件之間的空隙。
        if(!separated)return false;
    }
    return true;
}
template<class Pose>
[[nodiscard]] inline bool ClearsCanopyWood(const Pose& pose,PlantLocalVolume local,
    std::span<const Aevrynd::BoxCollider3D> woodParts={},float touchMargin=.03f) noexcept {
    if(!Detail::Finite(pose.scale)||pose.scale.x<=0||pose.scale.x!=pose.scale.y||pose.scale.x!=pose.scale.z)return false;
    return ClearsCanopyWood(PlantVolume{pose.position,local,pose.scale.x,pose.rotation},woodParts,touchMargin);
}
template<class Vertex>
[[nodiscard]] inline bool MeasureLocalVolume(std::span<const Vertex> vertices,PlantLocalVolume& output) noexcept {
    if(vertices.empty())return false;
    double radius=0;
    float minimumY=std::numeric_limits<float>::infinity(),maximumY=-minimumY;
    for(const auto& vertex:vertices){
        if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z))return false;
        radius=(std::max)(radius,std::hypot(double(vertex.x),double(vertex.z)));
        minimumY=(std::min)(minimumY,vertex.y);maximumY=(std::max)(maximumY,vertex.y);
    }
    if(!std::isfinite(radius)||radius>std::numeric_limits<float>::max())return false;
    float roundedRadius=static_cast<float>(radius);
    if(double(roundedRadius)<radius)roundedRadius=std::nextafter(roundedRadius,std::numeric_limits<float>::infinity());
    const PlantLocalVolume measured{roundedRadius,minimumY,maximumY};
    if(!Detail::ValidLocal(measured))return false;
    output=measured;return true;
}
template<class Vertex>
[[nodiscard]] inline bool MeasureLocalVolume(std::span<const Vertex> stems,std::span<const Vertex> leaves,
    PlantLocalVolume& output) noexcept {
    PlantLocalVolume first,second;
    if(!MeasureLocalVolume(stems,first)||!MeasureLocalVolume(leaves,second))return false;
    output={(std::max)(first.radius,second.radius),(std::min)(first.minimumY,second.minimumY),
        (std::max)(first.maximumY,second.maximumY)};
    return true;
}
}
