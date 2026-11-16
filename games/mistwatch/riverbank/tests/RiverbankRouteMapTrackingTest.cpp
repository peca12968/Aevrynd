#include "RiverbankRouteMap.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace Mistwatch::RiverbankRouteMap;

void Require(bool passed, const char* message) {
    if (!passed) throw std::runtime_error(message);
}

void VerifyMapInput() {
    Menu menu;
    menu.Track(4);
    Require(!menu.Waypoint(), "Closed map consumed a tracking key");
    Require(!menu.Update(true, false, true, false, false), "Map opened before login");
    Require(!menu.Update(true, true, true, false, true), "Map replaced another panel");
    Require(!menu.Update(false, true, true, false, false), "Map opened without focus");
    Require(menu.Update(true, true, true, false, false) && menu.Open(), "Tab did not open map");

    menu.Track(4);
    Require(menu.Waypoint() == 4, "Map did not select the fifth landmark");
    menu.Track(landmarks.size());
    Require(menu.Waypoint() == 4, "Invalid landmark replaced the target");
    Require(menu.Update(false, true, false, true, false) && menu.Open(), "Unfocused input closed map");
    Require(menu.Update(true, true, false, true, false) && !menu.Open() && menu.Waypoint() == 4,
            "Escape released input too early or lost the target");
    Require(!menu.Update(true, true, false, false, false), "Map consumed input on a following closed frame");

    menu.ClearWaypoint();
    Require(menu.Waypoint() == 4, "Closed map consumed Backspace");
    Require(menu.Update(true, true, true, false, false) && menu.Open(), "Tab did not reopen map");
    menu.ClearWaypoint();
    Require(!menu.Waypoint(), "Backspace did not clear the target");
    Require(!menu.Update(true, false, false, false, false) && !menu.Open(), "Disconnect left map open");
}

void VerifyLandmarkGeometry() {
    Aevrynd::HeightField3D terrain({{-32.f, 0, 0}, 65, 81, 1.f});
    for (unsigned z = 0; z < 81; ++z)
        for (unsigned x = 0; x < 65; ++x)
            Require(terrain.SetHeight(x, z, float(x) * .125f + float(z) * .5f), "Terrain setup failed");
    const auto grid = Build(terrain);
    Require(grid.has_value(), "Landmarks did not fit the terrain");
    Require(grid->height.front() > grid->height[(TerrainGrid::rows - 1) * TerrainGrid::columns],
            "Terrain grid faces south");
    Require(grid->points[0].y > grid->points[2].y && grid->points[2].y > grid->points[3].y &&
            grid->points[3].y > grid->points[4].y, "Landmark north/south order changed");
    Require(grid->points[1].x > grid->points[2].x && grid->points[4].x > grid->points[3].x,
            "Landmark west/east order changed");
    for (std::size_t i = 0; i < landmarks.size(); ++i) {
        Aevrynd::Vec2 projected{};
        Require(Mistwatch::SurveyMap::Project(grid->bounds, landmarks[i].world, projected) &&
                projected.x == grid->points[i].x && projected.y == grid->points[i].y,
                "Landmark drifted from world coordinates");
    }
    Require(!Build(Aevrynd::HeightField3D{}), "Empty terrain accepted map projection");
}

void VerifyGuidance() {
    const Landmark origin{"test", {}, {1, 1, 1, 1}};
    const std::array<Aevrynd::Vec3, 8> positions{{
        {0, 0, -10}, {-10, 0, -10}, {-10, 0, 0}, {-10, 0, 10},
        {0, 0, 10}, {10, 0, 10}, {10, 0, 0}, {10, 0, -10},
    }};
    const std::array directions{"北", "東北", "東", "東南", "南", "西南", "西", "西北"};
    for (std::size_t i = 0; i < directions.size(); ++i) {
        const auto guidance = GuideTo(origin, positions[i]);
        Require(guidance && !guidance->arrived && std::string_view(guidance->direction) == directions[i] &&
                guidance->meters == (i % 2 ? 14 : 10), "Direction or straight-line distance changed");
    }
    const auto atThreshold = GuideTo(origin, {0, 25, 2.f});
    Require(atThreshold && atThreshold->arrived && atThreshold->meters == 2,
            "Arrival threshold is not inclusive or depends on elevation");
    const auto outside = GuideTo(origin, {0, 0, 2.001f});
    Require(outside && !outside->arrived, "Arrival leaked past the threshold");
    Require(!GuideTo(origin, {(std::numeric_limits<float>::infinity)(), 0, 0}), "Invalid player location was accepted");
}

void VerifyExpeditionRoute() {
    using Stage = Mistwatch::ExpeditionStage;
    using Quest = Mistwatch::RiverbankExpedition;
    Menu menu;
    menu.TrackExpedition(Stage::Available);
    Require(!menu.TracksExpedition(), "Closed map consumed the expedition key");
    Require(menu.Update(true, true, true, false, false), "Expedition route map did not open");
    menu.Track(2);
    const auto bridge = menu.Target(Stage::CheckTrail);
    Require(bridge && !Quest::CanInteract(Stage::CheckTrail, {bridge->world.x, bridge->world.z}),
            "Bridge fixture must be outside the trail interaction radius");
    menu.TrackExpedition(Stage::Complete);
    Require(menu.Waypoint() == 2, "Completed expedition replaced a manual target");

    menu.TrackExpedition(Stage::Available);
    Require(menu.TracksExpedition() && !menu.Waypoint(), "Expedition mode did not replace the manual target");
    Require(menu.Update(true, true, false, true, false) && !menu.Open(), "Expedition map did not close");
    for (const auto stage : {Stage::Available, Stage::CheckTrail, Stage::Hunt, Stage::Report}) {
        const auto target = menu.Target(stage);
        const auto destination = Quest::Destination(stage);
        Require(target && target->world.x == destination.x && target->world.z == destination.z,
                "Tracking did not follow the authoritative expedition stage");
        if (stage != Stage::Hunt)
            Require(Quest::CanInteract(stage, {target->world.x, target->world.z}),
                    "Waypoint is outside the expedition interaction range");
    }
    Require(!menu.Target(std::nullopt) && !menu.Target(Stage::Complete) && !menu.Target(static_cast<Stage>(255)),
            "Tracking left a marker without a valid quest stage");
    Require(menu.Update(true, true, true, false, false), "Expedition map did not reopen");
    menu.Track(0);
    Require(!menu.TracksExpedition() && menu.Waypoint() == 0 && menu.Target(Stage::Complete),
            "Manual destination did not replace the expedition tracking mode");
    menu.TrackExpedition(Stage::Report);
    menu.ClearWaypoint();
    Require(!menu.TracksExpedition() && !menu.Target(Stage::Report), "Backspace did not clear the expedition");
}
void VerifyBridgeRouting() {
    using Aevrynd::Vec3;
    using Mistwatch::ExpeditionStage;
    using Mistwatch::RiverbankBridgeDeck;
    const auto report = *ExpeditionLandmark(ExpeditionStage::Report);
    const auto hunt = *ExpeditionLandmark(ExpeditionStage::Hunt);
    const auto board = *ExpeditionLandmark(ExpeditionStage::Available);
    const float south = RiverbankBridgeDeck.center.z - RiverbankBridgeDeck.extents.z;
    const float north = RiverbankBridgeDeck.center.z + RiverbankBridgeDeck.extents.z;

    const auto staysOnLandOrDeck = [&](Vec3 from, const ExpeditionRoute& route) {
        for (unsigned leg = 0; leg < route.count; ++leg) {
            const auto end = route.stops[leg];
            for (unsigned i = 0; i <= 128; ++i) {
                const auto p = from + (end - from) * (float(i)/128.f);
                if (Mistwatch::RiverDistance(p.x,p.z) < Mistwatch::RiverOuterWidth(p.x) - .001f &&
                    (std::abs(p.x) > RiverbankBridgeDeck.extents.x || p.z < south || p.z > north))
                    return false;
            }
            from = end;
        }
        return true;
    };

    const auto northBank = Vec3{15, 0, 51};
    const auto southBank = Vec3{-22, 0, 20};
    Require(!staysOnLandOrDeck(northBank, ExpeditionRoute{{report.world}}) &&
            !staysOnLandOrDeck(southBank, ExpeditionRoute{{hunt.world}}),
            "Fixtures must reproduce a straight line through the river");
    for (auto player : {northBank, Vec3{-18, 0, 52}, Vec3{6, 0, 44}}) {
        const auto route = PlanExpeditionRoute(report, player);
        Require(route.leg == BridgeLeg::Approach && route.count == 3 &&
            route.stops[0].x == 0 && route.stops[0].z > north &&
            route.stops[1].x == 0 && route.stops[1].z < south &&
            staysOnLandOrDeck(player, route), "North-bank return bypassed the bridge");
    }
    for (auto player : {southBank, Vec3{17, 0, 21}, Vec3{-6, 0, 28}}) {
        const auto route = PlanExpeditionRoute(hunt, player);
        Require(route.leg == BridgeLeg::Approach && route.count == 3 &&
            route.stops[0].x == 0 && route.stops[0].z < south &&
            route.stops[1].x == 0 && route.stops[1].z > north &&
            staysOnLandOrDeck(player, route), "South-bank hunt bypassed the bridge");
    }
    for (auto player : {Vec3{.5f, 0, north+1}, Vec3{1.f, 0, 38}, Vec3{-.5f, 0, 34}}) {
        const auto route = PlanExpeditionRoute(report, player);
        Require(route.leg == BridgeLeg::Cross && route.count == 2 && route.stops[0].z < south &&
                staysOnLandOrDeck(player, route), "Returning player did not continue south along the bridge");
    }
    unsigned routeSamples = 0, riverBends = 0;
    for (int x = -48; x <= 48; x += 2) {
        for (int z = 10; z <= 64; ++z) {
            const Vec3 player{float(x), 0, float(z)};
            if (Mistwatch::RiverDistance(player.x,player.z) < Mistwatch::RiverOuterWidth(player.x)) continue;
            const bool playerOnNorth = player.z >= Mistwatch::RiverCenterZ(player.x);
            for (const auto& goal : {hunt, report}) {
                const auto route = PlanExpeditionRoute(goal, player);
                if (!staysOnLandOrDeck(player, route)) {
                    std::cerr << "River crossed outside bridge at [" << x << ',' << z << "] goal="
                              << goal.world.x << ',' << goal.world.z << '\n';
                    Require(false, "Guidance routed a dry-bank start through the river");
                }
                ++routeSamples;
                riverBends += route.leg == BridgeLeg::Bank;
                const auto last = route.stops[route.count - 1];
                Require(last.x == goal.world.x && last.z == goal.world.z,
                        "River bends changed the quest destination");
                if ((goal.world.z > RiverbankBridgeDeck.center.z) != playerOnNorth)
                    Require(route.leg != BridgeLeg::None, "Cross-bank guidance lost bridge routing");
            }
        }
    }
    Require(routeSamples > 3900 && riverBends > 100, "Bank survey did not exercise outer river bends");
    std::cout << "RIVERBANK_BRIDGE_ROUTES " << routeSamples << " bank_detours=" << riverBends << '\n';

    const auto crossing = PlanExpeditionRoute(hunt, {.5f, 0, south-1});
    Require(crossing.leg == BridgeLeg::Cross && crossing.count == 2 && crossing.stops[0].z > north,
            "Southern bridge entry did not advance to the north exit");
    Require(PlanExpeditionRoute(board, {0, 0, south-1}).leg == BridgeLeg::None &&
            PlanExpeditionRoute(hunt, {0, 0, north+1}).leg == BridgeLeg::None,
            "Completed crossing continued to direct the player back onto the bridge");
}
}

int main() {
    try {
        VerifyMapInput();
        VerifyLandmarkGeometry();
        VerifyGuidance();
        VerifyExpeditionRoute();
        VerifyBridgeRouting();
        std::cout << "RIVERBANK_ROUTE_MAP_TRACKING_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
