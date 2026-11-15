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
        : asset(LoadRiverbankAvatarAsset(path)), motion(asset),
          materials{RiverbankAvatarMaterials(asset), RiverbankAvatarMaterials(asset, RiverbankClass::Guardian),
                    RiverbankAvatarMaterials(asset, RiverbankClass::Ranger), RiverbankAvatarMaterials(asset, RiverbankClass::Priest)} {
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
        for (const auto& entity : snapshot->entities) {
            auto& player = players[entity.id];
            player.motion.Advance(entity.velocity, dt);
            const auto playerClass = ReadRiverbankClassFlags(entity.flags);
            player.playerClass = IsRiverbankClass(playerClass) ? playerClass : RiverbankClass::None;
        }
    }

    bool Submit(Aevrynd::DrawList& draw, Aevrynd::NetworkEntityId3D id, Aevrynd::Vec3 feet) {
        const auto found = players.find(id);
        const Player state = found == players.end() ? Player{} : found->second;
        const auto placement = motion.Evaluate(asset.model.skeleton, state.motion, feet, sampled, pose);
        const auto palette = Aevrynd::AddSkinPaletteFromPose(draw, asset.model, pose, skinning);
        return Aevrynd::SubmitSkinnedRenderable3D(draw, mesh.handle, palette, placement,
            static_cast<std::uint32_t>(mesh.indexCount), asset.submeshes, materials.at(static_cast<std::size_t>(state.playerClass)));
    }

    void Release(Aevrynd::Engine& engine) {
        engine.DestroySkinnedMesh3D(mesh.handle);
        mesh = {};
    }

private:
    struct Player {
        RiverbankAvatarMotion::Player motion;
        RiverbankClass playerClass = RiverbankClass::None;
    };
    Aevrynd::SkinnedModelAssetData asset;
    RiverbankAvatarMotion motion;
    Aevrynd::SkinnedMeshResource3D mesh;
    std::array<std::vector<Aevrynd::Material3D>, 4> materials;
    std::map<Aevrynd::NetworkEntityId3D, Player> players;
    Aevrynd::SkinningWorkspace3D skinning;
    Aevrynd::AnimationPose3D sampled, pose;
};
}
