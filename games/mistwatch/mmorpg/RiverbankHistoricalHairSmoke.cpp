#include "RiverbankAvatarAsset.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 4) return 2;
    try {
        using namespace Mistwatch;
        const auto asset = LoadRiverbankAvatarAsset(argv[1]);
        const auto fullBody = CompactRiverbankAvatarPart(asset, asset.model, RiverbankAvatarPart::Body);
        const auto body = CompactRiverbankAvatarPart(asset, asset.model, RiverbankAvatarPart::BodyWithoutHair);
        const auto guardian = CompactRiverbankAvatarPart(asset, asset.model, RiverbankAvatarPart::Hair);
        const auto& surface = guardian.submeshes.front();
        const auto ranger = LoadRiverbankAvatarHair(argv[2], asset, surface);
        const auto priest = LoadRiverbankAvatarHair(argv[3], asset, surface);
        const auto correct = [&](const RiverbankAvatarMeshPart& part, std::size_t triangles) {
            return part.submeshes.size() == 1 && part.model.bindMesh.indices.size() == triangles * 3 &&
                part.submeshes.front().materialSlot == surface.materialSlot &&
                part.submeshes.front().flags == surface.flags;
        };
        if (!correct(guardian, 3882) || !correct(ranger, 3882) || !correct(priest, 2328) ||
            body.model.bindMesh.indices.size() + guardian.model.bindMesh.indices.size() != fullBody.model.bindMesh.indices.size())
            throw std::runtime_error("Class hair changed the source mesh partition or material");
        try {
            (void)LoadRiverbankAvatarHair(argv[1], asset, surface);
            throw std::logic_error("A complete avatar was accepted as a hair companion");
        } catch (const std::runtime_error& error) {
            if (std::string_view(error.what()) != "Riverbank hair does not match the shared avatar") throw;
        }
        std::cout << "RIVERBANK_CLASS_HAIR_PASS roles=3 rejected_full_asset=1\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
