#pragma once
#include <Aevrynd/Mesh3D.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace Aevrynd {
inline constexpr unsigned MinimumCappedCylinderSides3D=3;
inline constexpr unsigned MaximumCappedCylinderSides3D=128;

// 獨立的新API；不改舊CreateUnitCylinder的clamp、records或indices語義。
// canonical長軸+Y，半徑／半高0.5；UV=+0、白色、照明權重=1。
// records依序為side 2n、bottom rim n+center、top rim n+center。
// 無效side數在配置前拒絕，不clamp、不猜fallback；配置失敗照Mesh3D慣例拋出。
[[nodiscard]] inline Mesh3D CreateUnitCappedCylinder3D(unsigned sides=12) {
    if(sides<MinimumCappedCylinderSides3D||sides>MaximumCappedCylinderSides3D)
        throw std::invalid_argument("Capped cylinder sides must be in [3,128]");
    Mesh3D mesh;
    mesh.vertices.reserve(std::size_t(sides)*4+2);
    mesh.indices.reserve(std::size_t(sides)*12);
    constexpr float Pi=3.14159265358979323846f;
    for(unsigned side=0;side<sides;++side){
        // 保留原primitive的float相位、乘法和除法順序，供有界raw遷移使用。
        const float angle=2.f*Pi*float(side)/float(sides);
        const Vec3 radial{std::sin(angle),0,std::cos(angle)};
        if(!std::isfinite(radial.x)||!std::isfinite(radial.z))
            throw std::runtime_error("Nonfinite capped-cylinder radial direction");
        const auto normal=PackNormalSnorm8(radial);
        mesh.vertices.push_back({radial.x*.5f,-.5f,radial.z*.5f,normal,0xffffffffu,0,0});
        mesh.vertices.push_back({radial.x*.5f,.5f,radial.z*.5f,normal,0xffffffffu,0,0});
    }
    const std::uint32_t bottomRim=2*sides,bottomCenter=3*sides;
    const std::uint32_t topRim=3*sides+1,topCenter=4*sides+1;
    for(unsigned side=0;side<sides;++side){
        auto vertex=mesh.vertices[2*side];vertex.normal=PackNormalSnorm8({0,-1,0});
        mesh.vertices.push_back(vertex);
    }
    mesh.vertices.push_back({0,-.5f,0,PackNormalSnorm8({0,-1,0}),0xffffffffu,0,0});
    for(unsigned side=0;side<sides;++side){
        auto vertex=mesh.vertices[2*side+1];vertex.normal=PackNormalSnorm8({0,1,0});
        mesh.vertices.push_back(vertex);
    }
    mesh.vertices.push_back({0,.5f,0,PackNormalSnorm8({0,1,0}),0xffffffffu,0,0});
    for(unsigned side=0;side<sides;++side){
        const unsigned next=(side+1)%sides;
        const std::uint32_t a=side*2,b=a+1,c=next*2,d=c+1;
        // 由圓周+相位推導的外向順序；cap只用自己的rim，不共享radial normals。
        mesh.indices.insert(mesh.indices.end(),{a,c,b,c,d,b,
            topRim+side,topRim+next,topCenter,
            bottomRim+side,bottomCenter,bottomRim+next});
    }
    return mesh;
}
}
