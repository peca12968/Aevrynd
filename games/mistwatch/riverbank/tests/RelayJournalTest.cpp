#include "MistwatchFieldJournal.h"
#include "MistwatchSurveyMap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void Require(bool condition, const char* detail) {
    if (!condition) throw std::runtime_error(detail);
}

bool HasRow(const Mistwatch::FieldPresentation::Hud& hud, std::string_view text) {
    return std::ranges::any_of(hud.journal, [text](const auto& row) { return row == text; });
}
}

int main() {
    using namespace Mistwatch;
    try {
        Session session;
        const Encounter enemy;
        const Evasion evasion;
        const auto terrain = CreateTerrain();
        const std::array<Aevrynd::BoxCollider3D, 0> solids{};
        Aevrynd::Camera3D camera;
        camera.position = {0, 3, -6};
        camera.target = {0, 1, 0};
        const auto journal = [&](bool paused = false, bool open = true, unsigned tracked = NoTrackedRelay) {
            const auto before = session.Serialize();
            const auto guide = BuildObjectiveGuide(session, enemy, evasion, camera, false, true, solids, terrain, tracked);
            const auto hud = FieldPresentation::Build(session, guide, enemy, true, paused, open);
            Require(hud.valid && session.Serialize() == before, "journal changed the session");
            return hud;
        };
        const auto exploration = journal(false, false);
        Require(exploration.controls.find("Space 跳躍") != std::string::npos &&
                exploration.controls.find("Ctrl 蹲下") != std::string::npos,
                "ordinary exploration lost its traversal controls");
        const auto showsCamera = [](const FieldPresentation::Hud& hud) {
            return hud.controls.find("左鍵拖曳轉鏡頭") != std::string::npos &&
                   hud.controls.find("滾輪縮放") != std::string::npos;
        };
        Require(showsCamera(exploration) && exploration.controls.find("E 互動") != std::string::npos &&
                exploration.controls.find("Q 後撤") == std::string::npos,
                "opening controls must teach camera movement and interaction before combat");
        Session repair;
        repair.collected = 7;
        const auto repairGuide = BuildObjectiveGuide(repair, enemy, evasion, camera, false, true, solids, terrain);
        const auto repairHud = FieldPresentation::Build(repair, repairGuide, enemy, true, false, false);
        const auto pausedRepair = FieldPresentation::Build(repair, repairGuide, enemy, true, true, false);
        Require(repairHud.valid && repair.CurrentStage() == Stage::RepairMaterials &&
                showsCamera(repairHud) && !showsCamera(pausedRepair) && pausedRepair.controls.starts_with("Esc 繼續"),
                "repair route lost the camera hint or replaced the paused controls");
        Session combat;
        combat.collected = combat.repairMaterials = 7;
        combat.bridgeRepaired = true;
        combat.feet = Session::bridge;
        combat.feet.y = GroundElevation(combat.feet.x, combat.feet.z);
        const auto savedCombat = combat.Serialize();
        const auto combatGuide = BuildObjectiveGuide(combat, enemy, evasion, camera, false, true, solids, terrain);
        const auto combatHud = FieldPresentation::Build(combat, combatGuide, enemy, true, false, false);
        Require(combatHud.valid && combat.CurrentStage() == Stage::Encounter &&
                combatHud.controls.find("Q 後撤") != std::string::npos &&
                combatHud.controls.find("WASD＋Q 定向閃避") != std::string::npos &&
                combatHud.controls.find("F 反擊") != std::string::npos &&
                combatHud.controls.find("J 手冊") != std::string::npos,
                "combat HUD must distinguish the dodge inputs and retain counter/help");
        const auto pausedCombat = FieldPresentation::Build(combat, combatGuide, enemy, true, true, false);
        Require(pausedCombat.controls.starts_with("Esc 繼續") &&
                pausedCombat.controls.find("F5 儲存") != std::string::npos &&
                pausedCombat.controls.find("定向閃避") == std::string::npos &&
                combat.Serialize() == savedCombat,
                "encounter HUD changed pause controls or gameplay state");
        auto fullControls = combatHud;
        fullControls.controls = exploration.controls;
        const Aevrynd::DrawList hudDraw;
        const auto action = FieldPresentation::LastAction("Boar approaching - watch for the marked circle");
        const ActionFeedbackToast toast;
        const auto fullLayout = FieldPresentation::MeasureLayout(hudDraw, fullControls, combat, action, toast, true);
        const auto combatLayout = FieldPresentation::MeasureLayout(hudDraw, combatHud, combat, action, toast, true);
        Require(fullLayout.bottom.width - combatLayout.bottom.width >= 120.f,
                "encounter controls did not give back meaningful scene width");
        std::cout << "encounter_hud_width_before=" << fullLayout.bottom.width
                  << " encounter_hud_width_after=" << combatLayout.bottom.width << '\n';
        unsigned mapChecks = 0;
        float widestPanel = 0;
        const auto checkMapPanel = [&](bool relayControls, std::optional<float> waterSurface) {
            const auto guide = BuildObjectiveGuide(session, enemy, evasion, camera, false, true, solids, terrain);
            const auto data = SurveyMap::Build(session, guide, camera, terrain,
                                              relayControls ? 0u : NoTrackedRelay,
                                              relayControls ? 0u : NoTrackedRelay,
                                              relayControls, waterSurface);
            Aevrynd::DrawList draw;
            const auto layout = SurveyMap::MeasureLayout(draw, data);
            const auto receipt = SurveyMap::Draw(draw, data);
            Require(receipt.drawn && receipt.tiles == SurveyMap::TileBudget &&
                    receipt.markers == data.markerCount && receipt.textLines == layout.count &&
                    draw.vertices.size() >= 6 && draw.Stats2D().textGlyphs > 0,
                    "survey panel lost its map or text");
            float textRight = 0;
            for (unsigned index = 0; index < layout.count; ++index) {
                const auto& line = layout.lines[index];
                Require(line.pixels >= SurveyMap::MinimumTextPixels, "survey legend text was reduced");
                textRight = (std::max)(textRight, line.position.x + line.size.x);
            }
            float left = draw.vertices.front().x;
            float right = left;
            for (unsigned index = 0; index < 6; ++index) {
                left = (std::min)(left, draw.vertices[index].x);
                right = (std::max)(right, draw.vertices[index].x);
            }
            Require(left == 8.f && right >= textRight + 12.f, "survey panel clips or moves the legend");
            Require(right <= std::ceil(textRight) + 12.f, "survey panel hides scene beyond measured legend");
            widestPanel = (std::max)(widestPanel, right - left);
            ++mapChecks;
        };
        const auto goToRelay = [&](unsigned index) {
            session.feet = Session::relaySites[index];
            session.feet.y = GroundElevation(session.feet.x, session.feet.z);
            Require(session.NearRelay(index), "relay fixture is not on the actual station ground");
        };
        const auto align = [&](unsigned index) {
            for (unsigned turn = 0; turn < 16 && session.RelaySignal(index) < 100; ++turn)
                Require(session.TuneRelay(index, 1), "could not adjust the station antenna");
            Require(session.RelaySignal(index) == 100, "did not reach the true antenna lock");
        };

        checkMapPanel(false, std::nullopt);
        Require(std::ranges::none_of(journal().journal, [](const auto& row) { return row.starts_with("中繼進度 "); }),
                "relay progress is visible before power");
        Require(session.Restore("MISTWATCH 3 7 80 1 1 0 0 0 69 1 1 7"), "powered legacy checkpoint rejected");
        Require(session.CurrentStage() == Stage::Relays, "powered checkpoint lacks relay stage");
        checkMapPanel(true, 1.f);
        goToRelay(1);
        align(1);
        Require(session.TuneRelay(1, 1) && session.RelaySignal(1) < 100, "east relay fixture failed to leave lock");
        Require(HasRow(journal(), "中繼進度 西站 待查線 · 東站 待調向 · 西北站 待確認入口"),
                "powered journal does not identify which stations still need work");
        Require(journal(false, false).journal.empty(), "closed journal rendered a relay row");
        align(1);
        const auto awaitingLink = "中繼進度 西站 待查線 · 東站 待連線 · 西北站 待確認入口";
        Require(HasRow(journal(), awaitingLink) && HasRow(journal(true, true, 0), awaitingLink),
                "antenna lock or tracked station misreported radio link progress");
        Require(FieldInput::Interact(session) == FieldInput::Result::RelayConnected, "east station link failed");
        Require(HasRow(journal(), "中繼進度 西站 待查線 · 東站 已連線 · 西北站 待確認入口"), "east station link was not reported");

        goToRelay(FieldRelayTask::WestRelay);
        align(FieldRelayTask::WestRelay);
        Require(FieldInput::Lever(session, 0) == FieldInput::Result::PatchChanged && session.relayLinked == 2,
                "changing west patch also linked a station");
        Require(FieldInput::Interact(session) == FieldInput::Result::CircuitConfirmed && session.relayLinked == 2,
                "confirming west patch also linked a station");
        Require(HasRow(journal(), "中繼進度 西站 待連線 · 東站 已連線 · 西北站 待確認入口"), "west station field check missing");
        Require(FieldInput::Interact(session) == FieldInput::Result::RelayConnected, "west station link failed");

        goToRelay(FieldRelayTask::NorthWestRelay);
        align(FieldRelayTask::NorthWestRelay);
        Require(FieldInput::Lever(session, 2) == FieldInput::Result::RouteChanged && session.relayLinked == 3,
                "changing route also linked a station");
        Require(FieldInput::Interact(session) == FieldInput::Result::RouteConfirmed && session.relayLinked == 3,
                "confirming route also linked a station");
        Require(HasRow(journal(), "中繼進度 西站 已連線 · 東站 已連線 · 西北站 待連線"), "north-west field check missing");
        Require(FieldInput::Interact(session) == FieldInput::Result::RelayConnected && session.CurrentStage() == Stage::Beacon,
                "linking all stations did not reach the beacon stage");
        Require(HasRow(journal(), "中繼進度 西站 已連線 · 東站 已連線 · 西北站 已連線"), "beacon stage lost the linked checklist");
        session.feet = Session::beacon;
        Require(session.LightBeacon(), "fixture could not submit the beacon");
        Require(journal().journal.front() == "救援任務完成", "completion stopped showing the existing debrief");
        checkMapPanel(false, 1.f);
        std::cout << "COMPACT_SURVEY_PANEL_PASS cases=" << mapChecks << " widest=" << widestPanel << '\n';
        std::cout << "RELAY_JOURNAL_PROGRESS_PASS stations=3 stages=4 field_checks=2 state_unchanged=1\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
