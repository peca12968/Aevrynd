#pragma once
#include "MistwatchRiverbankConstruction.h"
#include <Aevrynd/HeightField3D.h>
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/CollisionWorld3D.h>
#include <Aevrynd/RigidTerrainMetrics3D.h>
#include <span>
#include <vector>
namespace Mistwatch::BankRocks {
inline constexpr unsigned Count=8;
struct Part {
    Aevrynd::Transform3D pose;
    Aevrynd::BoxCollider3D collider;
    float maximumAbove{},maximumSupportGap{};
    unsigned supportVertices{};
};
struct Model {unsigned rejectedHeightParts{};std::vector<Part> parts;std::vector<Aevrynd::BoxCollider3D> additions;std::vector<Aevrynd::RigidTerrainMetrics3D> measurements;};
Model Build(const Aevrynd::HeightField3D&,const Aevrynd::Mesh3D&,
            std::span<const RiverbankConstruction::Guard>,std::span<const Aevrynd::BoxCollider3D>);
// Explicit unaccepted development preview. The original strict Build rejection
// stays intact and is tested. This does not promote the physical/appearance gate.
Model BuildRejectedPreview(const Aevrynd::HeightField3D&,const Aevrynd::Mesh3D&,std::span<const RiverbankConstruction::Guard>,std::span<const Aevrynd::BoxCollider3D>);
}
