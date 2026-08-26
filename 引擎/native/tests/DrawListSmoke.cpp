#include <Aevrynd/Renderer.h>
#include <algorithm>
#include <cmath>
#include <iostream>

using namespace Aevrynd;

int main() {
    DrawList draw;
    draw.IsoPrism({100.f, 80.f}, 30.f, 15.f, 12.f,
                  {1.f, 1.f, 1.f, 1.f}, {.5f, .5f, .5f, 1.f}, {.7f, .7f, .7f, 1.f});
    if (draw.vertices.size() != 18) {
        std::cerr << "IsoPrism must emit six triangles\n";
        return 1;
    }
    const auto [minY, maxY] = std::minmax_element(draw.vertices.begin(), draw.vertices.end(),
        [](const Vertex& a, const Vertex& b) { return a.y < b.y; });
    if (std::abs(minY->y - 53.f) > .001f || std::abs(maxY->y - 95.f) > .001f) {
        std::cerr << "IsoPrism bounds are incorrect\n";
        return 2;
    }

    draw.Clear();
    draw.Ellipse({50.f, 40.f}, 20.f, 8.f, {1.f, 1.f, 1.f, 1.f}, 24);
    if (draw.vertices.size() != 72) {
        std::cerr << "Ellipse must emit one triangle per segment\n";
        return 3;
    }
    if (std::any_of(draw.vertices.begin(), draw.vertices.end(), [](const Vertex& vertex) {
        return !std::isfinite(vertex.x) || !std::isfinite(vertex.y);
    })) {
        std::cerr << "Ellipse emitted a non-finite vertex\n";
        return 4;
    }
    return 0;
}
