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
    RiverbankAvatar(Aevrynd::Engine& engine, const std::filesystem::path& path, bool useWardrobe = false)
        : asset(LoadRiverbankAvatarAsset(path)), motion(asset),
          paletteBinding(PrepareSkinPaletteBinding3D(asset.model)),
          materials{RiverbankAvatarMaterials(asset), RiverbankAvatarMaterials(asset, RiverbankClass::Guardian),
                    RiverbankAvatarMaterials(asset, RiverbankClass::Ranger), RiverbankAvatarMaterials(asset, RiverbankClass::Priest)} {
        if (!paletteBinding) throw std::runtime_error("Riverbank avatar palette binding is invalid");
        if (motion.HasDownedClip()) {
            const std::array<std::string_view, 2> jointNames{"head", "spine"};
            for (std::size_t n = 0; n < jointNames.size(); ++n) {
                const auto joint = std::ranges::find(asset.jointNames, jointNames[n]);
                if (joint == asset.jointNames.end()) throw std::runtime_error("Riverbank downed view joint is missing");
                downedViewJoints[n] = static_cast<std::size_t>(joint - asset.jointNames.begin());
            }
        }
        if (!useWardrobe) {
            mesh = engine.CreateSkinnedMeshResource3D(asset.model);
            if (!mesh) throw std::runtime_error("Riverbank avatar GPU upload failed");
            return;
        }

        try {
            auto body = CompactRiverbankAvatarPart(asset, asset.model, false);
            bodySurfaces = std::move(body.submeshes);
            mesh = engine.CreateSkinnedMeshResource3D(body.model);
            if (!mesh) throw std::runtime_error("Riverbank shared body GPU upload failed");

            auto vest = CompactRiverbankAvatarPart(asset, asset.model, true);
            vestSurface = vest.submeshes.front();
            wardrobe.front() = engine.CreateSkinnedMeshResource3D(vest.model);
            if (!wardrobe.front()) throw std::runtime_error("Riverbank base vest GPU upload failed");

            const std::array names{"RiverbankExplorer-Guardian.aeskin", "RiverbankExplorer-Ranger.aeskin",
                                   "RiverbankExplorer-Priest.aeskin"};
            for (std::size_t slot = 0; slot < names.size(); ++slot) {
                const auto outfit = LoadRiverbankAvatarAsset(path.parent_path() / names[slot]);
                if (outfit.model.bindMesh.vertices.size() != asset.model.bindMesh.vertices.size() ||
                    outfit.model.bindMesh.indices != asset.model.bindMesh.indices ||
                    outfit.materialSlotNames != asset.materialSlotNames)
                    throw std::runtime_error("Riverbank wardrobe does not match the shared avatar");
                auto roleVest = CompactRiverbankAvatarPart(asset, outfit.model, true);
                wardrobe[slot + 1] = engine.CreateSkinnedMeshResource3D(roleVest.model);
                if (!wardrobe[slot + 1])
                    throw std::runtime_error(std::string("Riverbank class vest GPU upload failed: ") + names[slot]);
            }
        } catch (...) {
            Release(engine);
            throw;
        }
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
        const auto palette = Aevrynd::AddBoundSkinPaletteFromPose(draw, paletteBinding, pose, skinning);
        const auto role = static_cast<std::size_t>(state.playerClass);
        const auto& bindings = materials.at(role);
        if (!wardrobe.front())
            return Aevrynd::SubmitSkinnedRenderable3D(draw, mesh.handle, palette, placement,
                static_cast<std::uint32_t>(mesh.indexCount), asset.submeshes, bindings);
        const auto& vest = wardrobe.at(role);
        const std::array parts{
            Aevrynd::SkinnedRenderablePart3D{.mesh=mesh.handle, .transform=placement,
                .totalIndexCount=static_cast<std::uint32_t>(mesh.indexCount), .submeshes=bodySurfaces,
                .materialBindings=bindings, .key={1}},
            Aevrynd::SkinnedRenderablePart3D{.mesh=vest.handle, .transform=placement,
                .totalIndexCount=static_cast<std::uint32_t>(vest.indexCount),
                .submeshes=std::span{&vestSurface, std::size_t{1}}, .materialBindings=bindings, .key={2}},
        };
        return Aevrynd::SubmitSkinnedRenderableGroup3D(draw, palette, parts);
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
        for (auto& outfit : wardrobe) {
            if (outfit) engine.DestroySkinnedMesh3D(outfit.handle);
            outfit = {};
        }
        if (mesh) engine.DestroySkinnedMesh3D(mesh.handle);
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
    Aevrynd::SkinPaletteBinding3D paletteBinding;
    std::array<std::size_t, 2> downedViewJoints{};
    Aevrynd::SkinnedMeshResource3D mesh;
    std::vector<Aevrynd::SkinnedSubmesh3D> bodySurfaces;
    Aevrynd::SkinnedSubmesh3D vestSurface{};
    std::array<Aevrynd::SkinnedMeshResource3D, 4> wardrobe{};
    std::array<std::vector<Aevrynd::Material3D>, 4> materials;
    std::map<Aevrynd::NetworkEntityId3D, Player> players;
    Aevrynd::SkinningWorkspace3D skinning;
    Aevrynd::AnimationPose3D sampled, pose;
};
}
