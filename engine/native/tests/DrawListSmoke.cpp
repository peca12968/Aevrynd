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
constexpr Vec2 VerifiedTranslation = Vec2{12.f, 8.f} + Vec2{-5.f, 3.f};
static_assert(VerifiedDepthEntries[0].index == 3);
static_assert(VerifiedDepthEntries[1].index == 1);
static_assert(VerifiedDepthEntries[2].index == 0);
static_assert(VerifiedDepthEntries[3].index == 2);
static_assert(VerifiedTranslation.x == 7.f && VerifiedTranslation.y == 11.f);
static_assert(D3D12Renderer::BufferedFrames >= 2);
static_assert(D3D12Renderer::MaxVertices % 3 == 0);
static_assert(DrawList::InitialVertexCapacity <= D3D12Renderer::MaxVertices);
static_assert(D3D12Renderer::DrawableVertexCount(8) == 6);
static_assert(D3D12Renderer::DrawableVertexCount(D3D12Renderer::MaxVertices + 100) == D3D12Renderer::MaxVertices);

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
    if (draw.vertices.capacity() < DrawList::InitialVertexCapacity) {
        std::cerr << "DrawList must reserve its initial vertex batch\n";
        return 2;
    }
    draw.SetTranslation({7.f, -3.f});
    draw.Triangle({1.f, 2.f}, {3.f, 4.f}, {5.f, 6.f}, {1.f, 1.f, 1.f, 1.f});
    if (draw.vertices[0].x != 8.f || draw.vertices[0].y != -1.f || draw.vertices[2].x != 12.f || draw.vertices[2].y != 3.f) {
        std::cerr << "DrawList translation must affect every triangle vertex\n";
        return 3;
    }
    draw.Clear();
    if (draw.Translation().x != 0.f || draw.Translation().y != 0.f) {
        std::cerr << "DrawList clear must reset translation\n";
        return 4;
    }
    draw.IsoPrism({100.f, 80.f}, 30.f, 15.f, 12.f,
                  {1.f, 1.f, 1.f, 1.f}, {.5f, .5f, .5f, 1.f}, {.7f, .7f, .7f, 1.f});
    if (draw.vertices.size() != 18) {
        std::cerr << "IsoPrism must emit six triangles\n";
        return 5;
    }
    const auto [minY, maxY] = std::minmax_element(draw.vertices.begin(), draw.vertices.end(),
        [](const Vertex& a, const Vertex& b) { return a.y < b.y; });
    if (std::abs(minY->y - 53.f) > .001f || std::abs(maxY->y - 95.f) > .001f) {
        std::cerr << "IsoPrism bounds are incorrect\n";
        return 6;
    }

    draw.Clear();
    draw.Ellipse({50.f, 40.f}, 20.f, 8.f, {1.f, 1.f, 1.f, 1.f}, 24);
    if (draw.vertices.size() != 72) {
        std::cerr << "Ellipse must emit one triangle per segment\n";
        return 7;
    }
    if (std::any_of(draw.vertices.begin(), draw.vertices.end(), [](const Vertex& vertex) {
        return !std::isfinite(vertex.x) || !std::isfinite(vertex.y);
    })) {
        std::cerr << "Ellipse emitted a non-finite vertex\n";
        return 8;
    }
    return 0;
}
