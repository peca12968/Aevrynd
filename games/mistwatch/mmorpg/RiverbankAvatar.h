#pragma once

#include "RiverbankAvatarMotion.h"
#include "RiverbankAvatarAsset.h"
#include "RiverbankBattleWire.h"
#include "RiverbankPeerPresentation.h"
#include <Aevrynd/Engine.h>
#include <Aevrynd/HeightField3D.h>
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
        if (motion.HasDownedClip()) {
            const std::array<std::string_view, 2> jointNames{"head", "spine"};
            for (std::size_t n = 0; n < jointNames.size(); ++n) {
                const auto joint = std::ranges::find(asset.jointNames, jointNames[n]);
                if (joint == asset.jointNames.end()) throw std::runtime_error("Riverbank downed view joint is missing");
                downedViewJoints[n] = static_cast<std::size_t>(joint - asset.jointNames.begin());
            }
        }
        mesh = engine.CreateSkinnedMeshResource3D(asset.model);
        if (!mesh) throw std::runtime_error("Riverbank avatar GPU upload failed");
    }

    void Update(const Aevrynd::ReplicationSnapshot3D* snapshot, std::uint32_t localId, float dt) {
        if (!snapshot) {
            players.clear();
            return;
        }
        std::erase_if(players, [&](const auto& entry) {
            return std::ranges::find(snapshot->entities, entry.first, &Aevrynd::ReplicatedEntityState3D::id) == snapshot->entities.end();
        });
        for (const auto& entity : snapshot->entities) {
            auto& player = players[entity.id];
            const bool downed = (entity.flags & RiverbankBattleWire::DownedEntityFlag) != 0;
            if (entity.id.low == localId) {
                player.motion.Advance(entity.velocity, dt, downed);
            } else {
                Aevrynd::Vec3 feet{};
                if (!Aevrynd::ToLocalPosition3D(entity.position, Aevrynd::WorldOrigin3D{}, feet)) {
                    players.erase(entity.id);
                    continue;
                }
                if (player.peer.Observe(snapshot->serverTick, feet, downed))
                    player.motion.distance = player.motion.walking = 0.f;
                player.motion.AdvanceByTravel(player.peer.Advance(dt), dt, downed);
            }
            const auto playerClass = ReadRiverbankClassFlags(entity.flags);
            player.playerClass = IsRiverbankClass(playerClass) ? playerClass : RiverbankClass::None;
        }
    }

    std::optional<Aevrynd::Vec3> PeerFeet(Aevrynd::NetworkEntityId3D id, const Aevrynd::HeightField3D& terrain) const {
        const auto player = players.find(id);
        if (player == players.end()) return {};
        auto feet = player->second.peer.Position();
        if (!feet || !terrain.Contains(feet->x, feet->z)) return {};
        feet->y = terrain.SampleHeight(feet->x, feet->z);
        return feet;
    }

    bool Submit(Aevrynd::DrawList& draw, Aevrynd::NetworkEntityId3D id, Aevrynd::Vec3 feet,
        const Aevrynd::HeightField3D& terrain) {
        const auto found = players.find(id);
        const Player empty{};
        const auto& state = found == players.end() ? empty : found->second;
        const auto normal = state.motion.downedSeconds > 0 ? terrain.SampleNormal(feet.x, feet.z) : Aevrynd::Vec3{0, 1, 0};
        const auto placement = motion.Evaluate(asset.model.skeleton, state.motion, feet, sampled, pose, normal);
        const auto palette = Aevrynd::AddSkinPaletteFromPose(draw, asset.model, pose, skinning);
        return Aevrynd::SubmitSkinnedRenderable3D(draw, mesh.handle, palette, placement,
            static_cast<std::uint32_t>(mesh.indexCount), asset.submeshes, materials.at(static_cast<std::size_t>(state.playerClass)));
    }

    std::optional<std::array<Aevrynd::Vec3, 2>> DownedView(Aevrynd::NetworkEntityId3D id,
        Aevrynd::Vec3 feet, const Aevrynd::HeightField3D& terrain) {
        const auto player = players.find(id);
        if (!motion.HasDownedClip() || player == players.end() || player->second.motion.downedSeconds < .40f) return {};
        const auto placement = motion.Evaluate(asset.model.skeleton, player->second.motion, feet, sampled, pose,
            terrain.SampleNormal(feet.x, feet.z));
        return std::array{
            Aevrynd::TransformPoint(placement * pose.globalMatrices[downedViewJoints[0]], {}),
            Aevrynd::TransformPoint(placement * pose.globalMatrices[downedViewJoints[1]], {})};
    }

    bool DownedCaptureReady(Aevrynd::NetworkEntityId3D id) const {
        const auto player = players.find(id);
        return !motion.HasDownedClip() || (player != players.end() && player->second.motion.downedSeconds >= 1.f);
    }

    void ResetAfterWarp(Aevrynd::NetworkEntityId3D id) {
        if (const auto player = players.find(id); player != players.end()) {
            auto& state = player->second.motion;
            state.distance = state.walking = state.downedSeconds = 0.f;
        }
    }

    void Release(Aevrynd::Engine& engine) {
        engine.DestroySkinnedMesh3D(mesh.handle);
        mesh = {};
    }

private:
    struct Player {
        RiverbankAvatarMotion::Player motion;
        RiverbankPeerPresentation peer;
        RiverbankClass playerClass = RiverbankClass::None;
    };
    Aevrynd::SkinnedModelAssetData asset;
    RiverbankAvatarMotion motion;
    std::array<std::size_t, 2> downedViewJoints{};
    Aevrynd::SkinnedMeshResource3D mesh;
    std::array<std::vector<Aevrynd::Material3D>, 4> materials;
    std::map<Aevrynd::NetworkEntityId3D, Player> players;
    Aevrynd::SkinningWorkspace3D skinning;
    Aevrynd::AnimationPose3D sampled, pose;
};
}
