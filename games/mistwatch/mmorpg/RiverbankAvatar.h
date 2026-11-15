#pragma once

#include "RiverbankAvatarMotion.h"
#include "RiverbankAvatarAsset.h"
#include <Aevrynd/Engine.h>
#include <Aevrynd/ReplicationProtocol3D.h>
#include <Aevrynd/SkinnedRenderable3D.h>
#include <map>

namespace Mistwatch {
class RiverbankAvatar {
public:
    RiverbankAvatar(Aevrynd::Engine& engine, const std::filesystem::path& path)
        : asset(LoadRiverbankAvatarAsset(path)), motion(asset), materials(RiverbankAvatarMaterials(asset)) {
        mesh = engine.CreateSkinnedMeshResource3D(asset.model);
        if (!mesh) throw std::runtime_error("Riverbank avatar GPU upload failed");
    }

    void Update(const Aevrynd::ReplicationSnapshot3D* snapshot, float dt) {
        if (!snapshot) {
            players.clear();
            return;
        }
        std::erase_if(players, [&](const auto& entry) {
            return std::ranges::find(snapshot->entities, entry.first, &Aevrynd::ReplicatedEntityState3D::id) == snapshot->entities.end();
        });
        for (const auto& entity : snapshot->entities) players[entity.id].Advance(entity.velocity, dt);
    }

    bool Submit(Aevrynd::DrawList& draw, Aevrynd::NetworkEntityId3D id, Aevrynd::Vec3 feet) {
        const auto found = players.find(id);
        const RiverbankAvatarMotion::Player state = found == players.end() ? RiverbankAvatarMotion::Player{} : found->second;
        const auto placement = motion.Evaluate(asset.model.skeleton, state, feet, sampled, pose);
        const auto palette = Aevrynd::AddSkinPaletteFromPose(draw, asset.model, pose, skinning);
        return Aevrynd::SubmitSkinnedRenderable3D(draw, mesh.handle, palette, placement,
            static_cast<std::uint32_t>(mesh.indexCount), asset.submeshes, materials);
    }

    void Release(Aevrynd::Engine& engine) {
        engine.DestroySkinnedMesh3D(mesh.handle);
        mesh = {};
    }

private:
    Aevrynd::SkinnedModelAssetData asset;
    RiverbankAvatarMotion motion;
    Aevrynd::SkinnedMeshResource3D mesh;
    std::vector<Aevrynd::Material3D> materials;
    std::map<Aevrynd::NetworkEntityId3D, RiverbankAvatarMotion::Player> players;
    Aevrynd::SkinningWorkspace3D skinning;
    Aevrynd::AnimationPose3D sampled, pose;
};
}
