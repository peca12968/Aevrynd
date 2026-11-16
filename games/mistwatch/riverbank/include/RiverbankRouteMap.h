#pragma once

#include "MistwatchSurveyMap.h"
#include "MistwatchBridgeDeck.h"
#include "MistwatchRiverProfile.h"
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

inline std::optional<Landmark> ExpeditionLandmark(std::optional<ExpeditionStage> stage) {
    constexpr std::array labels{"到營地領取巡林委託", "勘查橋前路標", "挑戰巡林野豬", "返回營地回報"};
    if (!stage || static_cast<std::size_t>(*stage) >= labels.size()) return {};
    const auto destination = RiverbankExpedition::Destination(*stage);
    return Landmark{labels[static_cast<std::size_t>(*stage)], {destination.x, 0, destination.z}, {.99f,.81f,.43f,1}};
}

struct Guidance {
    long meters{};
    const char* direction{};
    bool arrived{};
};

inline std::optional<Guidance> GuideTo(const Landmark& landmark, Aevrynd::Vec3 position,
                                      double arrivalRadius = 2.) {
    constexpr std::array directionNames{"北", "東北", "東", "東南", "南", "西南", "西", "西北"};
    const double east = double(landmark.world.x) - position.x, north = double(landmark.world.z) - position.z;
    const double distance = std::hypot(east, north);
    if (!std::isfinite(distance) || distance >= double((std::numeric_limits<long>::max)())) return {};
    if (distance <= arrivalRadius) return Guidance{std::lround(distance), "", true};
    const auto octant = std::lround(std::atan2(east, north) * 4. / std::numbers::pi);
    const auto index = static_cast<std::size_t>((octant + 8) % 8);
    return Guidance{std::lround(distance), directionNames[index], false};
}

enum class BridgeLeg { None, Bank, Approach, Cross };
struct ExpeditionRoute {
    std::array<Aevrynd::Vec3, 5> stops{};
    unsigned count{1};
    BridgeLeg leg{};
};

inline bool IsDryBankSegment(Aevrynd::Vec3 from, Aevrynd::Vec3 to) {
    // 村落外的溪谷較寬，橋口附近才收窄。以半公尺取樣檢查是否需要先沿岸繞行。
    const auto length = std::hypot(to.x - from.x, to.z - from.z);
    if (!std::isfinite(length)) return false;
    const auto samples = unsigned(std::clamp(std::ceil(2.f * length), 1.f, 512.f));
    for (unsigned i = 0; i <= samples; ++i) {
        const auto point = from + (to - from) * (float(i) / float(samples));
        if (RiverDistance(point.x, point.z) < RiverOuterWidth(point.x) + .05f) return false;
    }
    return true;
}

inline ExpeditionRoute PlanExpeditionRoute(const Landmark& target, Aevrynd::Vec3 player) {
    const auto& deck = RiverbankBridgeDeck;
    const float southEdge = deck.center.z - deck.extents.z;
    const float northEdge = deck.center.z + deck.extents.z;
    const bool targetNorth = target.world.z >= RiverCenterZ(target.world.x);
    const bool playerNorth = player.z >= RiverCenterZ(player.x);
    const auto lateral = std::abs(player.x - deck.center.x);
    const bool onDeck = lateral <= deck.extents.x && player.z >= southEdge && player.z <= northEdge;
    const bool cross = onDeck || targetNorth != playerNorth;

    // 橋口落在乾地，讓斜向接近的玩家先對準橋面中心。
    constexpr float bankClearance = 1.f;
    const Aevrynd::Vec3 south{deck.center.x, RiverbankBridgeDeckTop, southEdge - bankClearance};
    const Aevrynd::Vec3 north{deck.center.x, RiverbankBridgeDeckTop, northEdge + bankClearance};
    const auto entry = targetNorth ? south : north;
    const auto exit = targetNorth ? north : south;
    constexpr float entryRadius = .75f;
    const bool entering = lateral <= entryRadius &&
        (targetNorth ? player.z >= entry.z - entryRadius && player.z <= southEdge
                     : player.z <= entry.z + entryRadius && player.z >= northEdge);
    if (cross && (onDeck || entering)) return {{exit, target.world}, 2, BridgeLeg::Cross};

    const auto first = cross ? entry : target.world;
    if (IsDryBankSegment(player, first)) {
        return cross ? ExpeditionRoute{{entry, exit, target.world}, 3, BridgeLeg::Approach}
                     : ExpeditionRoute{{target.world}};
    }

    // 河岸高度帶位於最寬河段與彎道外，先到同一岸的高度帶再橫向前進。
    constexpr float bankOffset = 15.f;
    const float bankZ = deck.center.z + (playerNorth ? bankOffset : -bankOffset);
    ExpeditionRoute route;
    route.count = 0;
    route.leg = BridgeLeg::Bank;
    if (playerNorth ? player.z < bankZ : player.z > bankZ)
        route.stops[route.count++] = {player.x, 0, bankZ};
    if (std::abs(player.x - first.x) > .01f)
        route.stops[route.count++] = {first.x, 0, bankZ};
    route.stops[route.count++] = first;
    if (cross) {
        route.stops[route.count++] = exit;
        route.stops[route.count++] = target.world;
    }
    return route;
}

class Menu {
public:
    bool Open() const noexcept { return open_; }
    std::optional<std::size_t> Waypoint() const noexcept { return waypoint_; }
    bool TracksExpedition() const noexcept { return expedition_; }

    std::optional<Landmark> Target(std::optional<ExpeditionStage> stage) const {
        if (expedition_) return ExpeditionLandmark(stage);
        if (waypoint_) return landmarks[*waypoint_];
        return {};
    }

    void Track(std::size_t landmark) noexcept {
        if (!open_ || landmark >= landmarks.size()) return;
        expedition_ = false;
        waypoint_ = landmark;
    }

    void TrackExpedition(std::optional<ExpeditionStage> stage) {
        if (!open_ || !ExpeditionLandmark(stage)) return;
        waypoint_.reset();
        expedition_ = true;
    }

    void ClearWaypoint() noexcept {
        if (!open_) return;
        waypoint_.reset();
        expedition_ = false;
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
    bool expedition_{};
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
                 const Menu& menu, std::optional<ExpeditionStage> stage) {
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
    const auto waypoint = menu.Waypoint();
    if (const auto target = menu.Target(stage)) {
        const auto route = menu.TracksExpedition() ? PlanExpeditionRoute(*target, player)
                                                   : ExpeditionRoute{{target->world}};
        auto previous = marker;
        for (unsigned i = 0; i < route.count; ++i) {
            Vec2 projected{};
            if (!SurveyMap::Project(grid.bounds, route.stops[i], projected)) break;
            const auto stop = screen(projected);
            draw.Line(previous, stop, 2.f, target->color);
            if (i + 1 < route.count) draw.Circle(stop, 4.f, target->color, 14);
            previous = stop;
        }
    }
    const auto expedition = ExpeditionLandmark(stage);
    if (expedition) {
        Vec2 projected{};
        if (SurveyMap::Project(grid.bounds, expedition->world, projected))
            draw.Circle(screen(projected), 11.f, expedition->color, 16);
    }
    draw.SystemText({732,171}, "1–5 追蹤地標 · E 追蹤委託", 18.f, text);
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
    if (expedition) {
        if (menu.TracksExpedition()) draw.Quad({918,474},{380,33},{.15f,.22f,.20f,1});
        draw.SystemText({737,465}, std::string(menu.TracksExpedition() ? "追蹤中 · " : "E  ")+expedition->label,
                        17.f, expedition->color);
    }
    draw.Circle(marker, 8.f, {.046f,.065f,.060f,1}, 16);
    draw.Circle(marker, 4.f, {1,1,1,1}, 16);
    draw.SystemText({190,566}, "白點是你的位置 · 北方朝上 · 底色為地形高度", 16.f, text);
    draw.SystemText({732,546}, "Backspace 取消追蹤", 16.f, text);
    draw.SystemText({732,575}, "開啟地圖時，世界照常運作。", 16.f, text);
}

inline void DrawTrackedLandmark(Aevrynd::DrawList& draw, const Menu& menu,
                                std::optional<ExpeditionStage> stage, Aevrynd::Vec3 player) {
    const auto target = menu.Target(stage);
    if (!target) return;
    const auto route = menu.TracksExpedition() ? PlanExpeditionRoute(*target, player)
                                               : ExpeditionRoute{{target->world}};
    auto next = *target;
    next.world = route.stops.front();
    const bool bridge = route.leg != BridgeLeg::None;
    const auto guidance = GuideTo(next, player, bridge ? .25 : 2.);
    if (!guidance) return;
    const std::string distance = guidance->arrived ? "已接近地標" :
        std::string(guidance->direction) + " · " + (bridge ? "" : "直線 ") +
        std::to_string(guidance->meters) + " 公尺";
    if (bridge) {
        const auto south = next.world.z < RiverbankBridgeDeck.center.z;
        const auto label = route.leg == BridgeLeg::Bank ? (south ? "沿南岸繞行" : "沿北岸繞行") :
            route.leg == BridgeLeg::Approach ? (south ? "先到南岸橋口" : "先到北岸橋口") :
            (south ? "沿木橋前往南岸" : "沿木橋前往北岸");
        draw.Quad({640,630},{354,77},{.036f,.060f,.059f,.94f});
        draw.SystemText({478,600}, target->label, 17.f, target->color);
        draw.SystemText({478,623}, label, 16.f, {.89f,.95f,.91f,1});
        draw.SystemText({478,645}, distance, 15.f, {.89f,.95f,.91f,1});
    } else {
        draw.Quad({640,642},{354,53},{.036f,.060f,.059f,.94f});
        draw.SystemText({478,621}, target->label, 17.f, target->color);
        draw.SystemText({478,645}, distance, 15.f, {.89f,.95f,.91f,1});
    }
}
}
