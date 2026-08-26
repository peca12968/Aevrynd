#include <Aevrynd/Renderer.h>
#include <Aevrynd/Scene2D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

using namespace Aevrynd;

constexpr auto SortedDepthEntries() {
    std::array<DepthEntry, 4> entries = {{{320.f, 0, 0}, {120.f, 1, 1}, {320.f, 2, 2}, {-40.f, 3, 0}}};
    SortDepthEntries(entries);
    return entries;
}

constexpr auto VerifiedDepthEntries = SortedDepthEntries();
static_assert(VerifiedDepthEntries[0].index == 3);
static_assert(VerifiedDepthEntries[1].index == 1);
static_assert(VerifiedDepthEntries[2].index == 0);
static_assert(VerifiedDepthEntries[3].index == 2);

int main() {
    std::vector<DepthEntry> entries = {
        {320.f, 0, 0},
        {120.f, 1, 1},
        {320.f, 2, 2},
        {-40.f, 3, 0}
    };
    SortDepthEntries(entries);
    if (entries[0].index != 3 || entries[1].index != 1 || entries[2].index != 0 || entries[3].index != 2) {
        std::cerr << "Depth sorting must be back-to-front and stable\n";
        return 1;
    }

    DrawList draw;
    draw.IsoPrism({100.f, 80.f}, 30.f, 15.f, 12.f,
                  {1.f, 1.f, 1.f, 1.f}, {.5f, .5f, .5f, 1.f}, {.7f, .7f, .7f, 1.f});
    if (draw.vertices.size() != 18) {
        std::cerr << "IsoPrism must emit six triangles\n";
        return 2;
    }
    const auto [minY, maxY] = std::minmax_element(draw.vertices.begin(), draw.vertices.end(),
        [](const Vertex& a, const Vertex& b) { return a.y < b.y; });
    if (std::abs(minY->y - 53.f) > .001f || std::abs(maxY->y - 95.f) > .001f) {
        std::cerr << "IsoPrism bounds are incorrect\n";
        return 3;
    }

    draw.Clear();
    draw.Ellipse({50.f, 40.f}, 20.f, 8.f, {1.f, 1.f, 1.f, 1.f}, 24);
    if (draw.vertices.size() != 72) {
        std::cerr << "Ellipse must emit one triangle per segment\n";
        return 4;
    }
    if (std::any_of(draw.vertices.begin(), draw.vertices.end(), [](const Vertex& vertex) {
        return !std::isfinite(vertex.x) || !std::isfinite(vertex.y);
    })) {
        std::cerr << "Ellipse emitted a non-finite vertex\n";
        return 5;
    }
    return 0;
}
