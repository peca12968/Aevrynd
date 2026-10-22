#pragma once
#include <Aevrynd/BeveledBox3D.h>
#include <Aevrynd/CappedCylinder3D.h>
#include <bit>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace Mistwatch {
inline constexpr Aevrynd::Vec3 RepairToolsOffset{0,.49f,0};
inline constexpr Aevrynd::Vec3 RepairToolsEnvelope{.25f,.03f,.15f};
inline constexpr unsigned RepairToolsCylinderSides=12;
inline constexpr unsigned RepairToolsMetalVertices=530,RepairToolsMetalIndices=804;
inline constexpr unsigned RepairToolsHandleVertices=50,RepairToolsHandleIndices=144;

enum class RepairToolPart : unsigned {WrenchShank,WrenchHeel,WrenchJawRear,WrenchJawFront,
    ScrewdriverShaft,ScrewdriverBlade,ScrewdriverGrip};
enum class RepairToolPrimitive : unsigned {BeveledBox,CylinderAlongX};
struct RepairToolSourcePart {
    RepairToolPart name;
    RepairToolPrimitive primitive;
    unsigned mesh;
    Aevrynd::Vec3 center,size;
    float bevel;
};
// 尺寸皆為全長。四件鋼材形成開口扳手；各接縫真隔2mm，不藏交疊內面。
// 兩個工具並排，所有構件留在原0.5×0.06×0.3m視覺盒內。
inline constexpr std::array<RepairToolSourcePart,7> RepairToolsSource{{
    {RepairToolPart::WrenchShank,RepairToolPrimitive::BeveledBox,0,{-.070f,-.012f,-.082f},{.338f,.022f,.032f},.003f},
    {RepairToolPart::WrenchHeel,RepairToolPrimitive::BeveledBox,0,{.117f,-.012f,-.082f},{.032f,.024f,.066f},.003f},
    {RepairToolPart::WrenchJawRear,RepairToolPrimitive::BeveledBox,0,{.169f,-.012f,-.129f},{.136f,.024f,.024f},.003f},
    {RepairToolPart::WrenchJawFront,RepairToolPrimitive::BeveledBox,0,{.169f,-.012f,-.035f},{.136f,.024f,.024f},.003f},
    {RepairToolPart::ScrewdriverShaft,RepairToolPrimitive::CylinderAlongX,0,{.0455f,-.006f,.087f},{.269f,.012f,.012f},0},
    {RepairToolPart::ScrewdriverBlade,RepairToolPrimitive::BeveledBox,0,{.209f,-.006f,.087f},{.054f,.009f,.023f},.002f},
    {RepairToolPart::ScrewdriverGrip,RepairToolPrimitive::CylinderAlongX,1,{-.162f,-.006f,.087f},{.142f,.046f,.046f},0}
}};
struct RepairToolPartRange {
    RepairToolPart name{};
    unsigned mesh{},firstVertex{},vertexCount{},firstIndex{},indexCount{};
};
struct RepairToolsGeometry {
    Aevrynd::Mesh3D metal,handle;
    std::array<RepairToolPartRange,7> parts{};
};
namespace RepairToolsDetail {
// 使用獨立共用封口圓柱；精確signed-byte swizzle保留既有工具字段。
inline void AppendCylinder(Aevrynd::Mesh3D& out,const RepairToolSourcePart& part,
    Aevrynd::Color color) {
    using namespace Aevrynd;
    const auto primitive=CreateUnitCappedCylinder3D(RepairToolsCylinderSides);
    const auto base=static_cast<unsigned>(out.vertices.size());
    for(auto v:primitive.vertices){
        const Vec3 p{part.center.x+v.y*part.size.x,
            part.center.y+v.x*part.size.y,part.center.z-v.z*part.size.z};
        const auto z=std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(v.normal>>16));
        const auto minusZ=std::bit_cast<std::uint8_t>(static_cast<std::int8_t>(-int(z)));
        v.normal=((v.normal>>8)&255u)|((v.normal&255u)<<8)|
            (std::uint32_t(minusZ)<<16)|(v.normal&0xff000000u);
        v.x=p.x;v.y=p.y;v.z=p.z;
        v.color=PackColorRgba8(color);v.u=0;v.v=0;out.vertices.push_back(v);
    }
    for(const auto index:primitive.indices)out.indices.push_back(base+index);
    out.InvalidateRaycastIndex();
}
}

// 僅幾何，不擁有pickup/state、collider、設備part4、camera或世界來源。
inline RepairToolsGeometry CreateRepairToolsVisual() {
    using namespace Aevrynd;
    RepairToolsGeometry result;
    result.metal.vertices.reserve(RepairToolsMetalVertices);result.metal.indices.reserve(RepairToolsMetalIndices);
    result.handle.vertices.reserve(RepairToolsHandleVertices);result.handle.indices.reserve(RepairToolsHandleIndices);
    constexpr Color steel{.66f,.69f,.71f,1},grip{.86f,.32f,.065f,1};
    for(unsigned i=0;i<RepairToolsSource.size();++i){
        const auto& part=RepairToolsSource[i];auto& mesh=part.mesh?result.handle:result.metal;
        auto& range=result.parts[i];range.name=part.name;range.mesh=part.mesh;
        range.firstVertex=static_cast<unsigned>(mesh.vertices.size());
        range.firstIndex=static_cast<unsigned>(mesh.indices.size());
        const auto color=part.mesh?grip:steel;
        if(part.primitive==RepairToolPrimitive::BeveledBox){
            if(!AppendBeveledBox3D(mesh,part.center,part.size,part.bevel,color))
                throw std::runtime_error("Repair-tools beveled source part failed");
        }else RepairToolsDetail::AppendCylinder(mesh,part,color);
        range.vertexCount=static_cast<unsigned>(mesh.vertices.size())-range.firstVertex;
        range.indexCount=static_cast<unsigned>(mesh.indices.size())-range.firstIndex;
    }
    if(result.metal.vertices.size()!=RepairToolsMetalVertices||result.metal.indices.size()!=RepairToolsMetalIndices||
       result.handle.vertices.size()!=RepairToolsHandleVertices||result.handle.indices.size()!=RepairToolsHandleIndices)
        throw std::runtime_error("Repair-tools source count contract changed");
    return result;
}
}
