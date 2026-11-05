#include "MistwatchFieldJournal.h"
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
}

int main() {
    using namespace Mistwatch;
    try {
        Session session;
        const auto terrain = CreateTerrain();
        const std::array<Aevrynd::BoxCollider3D, 0> solids{};
        Aevrynd::Camera3D camera;
        camera.position = {0, 3, -6};
        camera.target = {0, 1, 0};
        const Encounter enemy;
        const Evasion evasion;
        auto instruction = [&](bool crouched, bool paused = false) {
            const auto before = session.Serialize();
            const auto guide = BuildObjectiveGuide(session, enemy, evasion, camera, crouched, true, solids, terrain);
            const auto hud = FieldPresentation::Build(session, guide, enemy, true, paused, false);
            Require(hud.valid && guide.valid, "invalid gameplay guide");
            Require(session.Serialize() == before, "the hint modified gameplay state");
            return hud.instruction;
        };
        const auto& roof = CameraCeilingSolids().back();
        const auto tool = Session::repairSites[0];
        const float eastEdge = roof.center.x + roof.extents.x;
        session.feet = {eastEdge + .7f, tool.y, tool.z};
        Require(!session.Near(tool), "the apron must exercise the old missed approach");
        Require(instruction(false).starts_with("按住 Ctrl"), "standing at the east approach lacks the entry hint");
        Require(!instruction(false, true).starts_with("按住 Ctrl"), "paused screen advertises a live approach");
        session.feet.x = eastEdge + 2.f;
        Require(!instruction(false).starts_with("按住 Ctrl"), "east apron hint appears too far away");
        session.feet.x = roof.center.x - roof.extents.x - .7f;
        Require(!instruction(false).starts_with("按住 Ctrl"), "the back wall advertises an entrance");
        session.feet = {eastEdge - .25f, tool.y, tool.z};
        Require(instruction(true).starts_with("保持蹲姿"), "inside approach instruction is missing");
        session.feet = {tool.x + .76f, tool.y, tool.z};
        Require(instruction(true) == "E：拾取低棚工具。", "ready pickup must take priority");
        Require(session.CollectRepairMaterial(0, true), "original tool pickup was rejected");
        Require(!instruction(true).starts_with("保持蹲姿"), "tool hint remains after pickup");
        session.feet = Session::supplies[0];
        Require(instruction(false) == "E：拾取補給", "ready supplies lost their instruction");
        std::cout << "SHELTER_APPROACH_GUIDE_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
