#pragma once

#include "MistwatchSurveyMap.h"
#include "RiverbankExpedition.h"
#include "RiverbankTerritory.h"
#include <numbers>

namespace Mistwatch::RiverbankRouteMap {
struct Landmark {
    const char* label;
    Aevrynd::Vec3 world;
    Aevrynd::Color color;
};

inline constexpr std::array landmarks{
    Landmark{"南岸營地與委託牌", {RiverbankExpedition::Board.x, 0, RiverbankExpedition::Board.z}, {.52f,.93f,.75f,1}},
    Landmark{"東側草坡 · 野豬", {RiverbankEncounter::EastPastureCenter.x, 0, RiverbankEncounter::EastPastureCenter.z}, {.98f,.86f,.42f,1}},
    Landmark{"河岸橋梁", Session::bridge, {.58f,.86f,1,1}},
    Landmark{"北岸林間空地 · 首領", {RiverbankEncounter::NorthBankCenter.x, 0, RiverbankEncounter::NorthBankCenter.z}, {1,.59f,.52f,1}},
    Landmark{"北岸瞭望地 · 公會領地", {RiverbankTerritory::CenterX, 0, RiverbankTerritory::CenterZ}, {.87f,.70f,1,1}},
};

struct Guidance {
    long meters{};
    const char* direction{};
    bool arrived{};
};

inline std::optional<Guidance> GuideTo(const Landmark& landmark, Aevrynd::Vec3 position) {
    constexpr std::array directionNames{"北", "東北", "東", "東南", "南", "西南", "西", "西北"};
    const double east = double(landmark.world.x) - position.x, north = double(landmark.world.z) - position.z;
    const double distance = std::hypot(east, north);
    if (!std::isfinite(distance) || distance >= double((std::numeric_limits<long>::max)())) return {};
    if (distance <= 2.) return Guidance{std::lround(distance), "", true};
    const auto octant = std::lround(std::atan2(east, north) * 4. / std::numbers::pi);
    const auto index = static_cast<std::size_t>((octant + 8) % 8);
    return Guidance{std::lround(distance), directionNames[index], false};
}

class Menu {
public:
    bool Open() const noexcept { return open_; }
    std::optional<std::size_t> Waypoint() const noexcept { return waypoint_; }

    void Track(std::size_t landmark) noexcept {
        if (open_ && landmark < landmarks.size()) waypoint_ = landmark;
    }

    void ClearWaypoint() noexcept {
        if (open_) waypoint_.reset();
    }

    bool Update(bool focused, bool ready, bool toggle, bool escape, bool otherPanel) noexcept {
        if (!ready) {
            open_ = false;
            return false;
        }
        if (open_) {
            if (focused && (toggle || escape)) open_ = false;
            return true;
        }
        open_ = focused && toggle && !otherPanel;
        return open_;
    }

private:
    bool open_{};
    std::optional<std::size_t> waypoint_;
};

struct TerrainGrid {
    static constexpr unsigned columns = 24, rows = 26;
    SurveyMap::Bounds bounds;
    SurveyMap::Rect plot;
    std::array<float, columns * rows> height{};
    float minimum{}, maximum{};
    std::array<Aevrynd::Vec2, landmarks.size()> points{};
};

inline std::optional<TerrainGrid> Build(const Aevrynd::HeightField3D& terrain) {
    const auto& s = terrain.Settings();
    if (s.width < 2 || s.depth < 2 || !Session::Finite(s.origin) || !std::isfinite(s.cellSize) || s.cellSize <= 0 ||
        std::uint64_t(s.width) * s.depth != terrain.SampleCount()) return {};

    TerrainGrid grid;
    grid.bounds = {s.origin.x, s.origin.z, s.origin.x + float(s.width - 1) * s.cellSize,
                   s.origin.z + float(s.depth - 1) * s.cellSize};
    for (std::size_t i = 0; i < landmarks.size(); ++i) {
        if (!SurveyMap::Project(grid.bounds, landmarks[i].world, grid.points[i])) return {};
    }
    const auto spanX = grid.bounds.maxX - grid.bounds.minX, spanZ = grid.bounds.maxZ - grid.bounds.minZ;
    const auto scale = (std::min)(500. / spanX, 374. / spanZ);
    const auto width = float(spanX * scale), height = float(spanZ * scale);
    grid.plot = {190.f + (500.f - width) * .5f, 172.f + (374.f - height) * .5f, width, height};
    grid.minimum = (std::numeric_limits<float>::infinity)();
    grid.maximum = -grid.minimum;
    for (unsigned row = 0; row < TerrainGrid::rows; ++row) {
        for (unsigned column = 0; column < TerrainGrid::columns; ++column) {
            const auto x = unsigned((double(column) + .5) / TerrainGrid::columns * double(s.width - 1));
            const auto z = unsigned((double(TerrainGrid::rows - row) - .5) / TerrainGrid::rows * double(s.depth - 1));
            const auto heightSample = terrain.Height(x, z);
            if (!std::isfinite(heightSample)) return {};
            grid.height[row * TerrainGrid::columns + column] = heightSample;
            grid.minimum = (std::min)(grid.minimum, heightSample);
            grid.maximum = (std::max)(grid.maximum, heightSample);
        }
    }
    return grid;
}

inline void Draw(Aevrynd::DrawList& draw, const std::optional<TerrainGrid>& data, Aevrynd::Vec3 player,
                 std::optional<std::size_t> waypoint = {}) {
    using namespace Aevrynd;
    constexpr Color text{.89f,.95f,.91f,1};
    draw.Quad({640,364}, {964,544}, {.036f,.060f,.059f,.98f});
    draw.SystemText({190,114}, "河岸地圖", 26.f, text);
    draw.SystemText({732,121}, "Tab／Esc 關閉", 17.f, text);
    Vec2 playerUnit{};
    if (!data || !SurveyMap::Project(data->bounds, player, playerUnit)) {
        draw.SystemText({190,190}, "目前無法顯示玩家的河岸座標", 18.f, text);
        return;
    }
    if (waypoint && *waypoint >= landmarks.size()) waypoint.reset();

    const auto& grid = *data;
    const auto& plot = grid.plot;
    const auto screen = [&](Vec2 point) { return Vec2{plot.x + point.x * plot.width, plot.y + point.y * plot.height}; };
    const auto cellWidth = plot.width / TerrainGrid::columns, cellHeight = plot.height / TerrainGrid::rows;
    const auto range = double(grid.maximum) - grid.minimum;
    for (unsigned row = 0; row < TerrainGrid::rows; ++row) {
        for (unsigned column = 0; column < TerrainGrid::columns; ++column) {
            const auto elevation = grid.height[row * TerrainGrid::columns + column];
            const float t = range > 0 ? float((double(elevation) - grid.minimum) / range) : .5f;
            draw.Quad({plot.x + (float(column) + .5f) * cellWidth, plot.y + (float(row) + .5f) * cellHeight},
                      {cellWidth, cellHeight}, {.12f + .20f*t,.25f + .28f*t,.23f + .20f*t,1});
        }
    }
    const auto marker = screen(playerUnit);
    if (waypoint) draw.Line(marker, screen(grid.points[*waypoint]), 2.f, landmarks[*waypoint].color);
    draw.SystemText({732,171}, "1–5 追蹤地標 · 直線距離", 18.f, text);
    for (std::size_t i = 0; i < landmarks.size(); ++i) {
        const auto& landmark = landmarks[i];
        const float y = 216.f + 48.f * float(i);
        const auto guidance = GuideTo(landmark, player);
        const bool tracking = waypoint == i;
        const auto point = screen(grid.points[i]);
        if (tracking) {
            draw.Quad({918,y + 10.f},{380,33},{.15f,.22f,.20f,1});
            draw.Circle(point, 10.f, text, 18);
        }
        draw.Circle(point, 6.f, landmark.color, 16);
        draw.Circle({738,y + 10.f}, 5.f, landmark.color, 16);
        const auto distance = guidance ? std::to_string(guidance->meters) + " 公尺" : std::string{};
        draw.SystemText({755,y}, std::to_string(i + 1) + "  " + landmark.label + "  " + distance, 17.f, text);
    }
    draw.Circle(marker, 8.f, {.046f,.065f,.060f,1}, 16);
    draw.Circle(marker, 4.f, {1,1,1,1}, 16);
    draw.SystemText({190,566}, "白點是你的位置 · 北方朝上 · 底色為地形高度", 16.f, text);
    draw.SystemText({732,546}, "Backspace 取消追蹤", 16.f, text);
    draw.SystemText({732,575}, "開啟地圖時，世界照常運作。", 16.f, text);
}

inline void DrawTrackedLandmark(Aevrynd::DrawList& draw, std::optional<std::size_t> waypoint, Aevrynd::Vec3 player) {
    if (!waypoint || *waypoint >= landmarks.size()) return;
    const auto& landmark = landmarks[*waypoint];
    const auto guidance = GuideTo(landmark, player);
    if (!guidance) return;
    const auto detail = guidance->arrived ? std::string("已接近地標") :
        std::string(guidance->direction) + " · 直線 " + std::to_string(guidance->meters) + " 公尺";
    draw.Quad({640,642},{354,53},{.036f,.060f,.059f,.94f});
    draw.SystemText({478,621}, landmark.label, 17.f, landmark.color);
    draw.SystemText({478,645}, detail, 15.f, {.89f,.95f,.91f,1});
}
}
