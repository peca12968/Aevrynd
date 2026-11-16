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
}

int main() {
    try {
        VerifyMapInput();
        VerifyLandmarkGeometry();
        VerifyGuidance();
        std::cout << "RIVERBANK_ROUTE_MAP_TRACKING_PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
