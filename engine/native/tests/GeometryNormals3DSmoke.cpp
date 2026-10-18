#include <Aevrynd/GeometryNormals3D.h>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace {
unsigned checks{};
bool Check(bool condition, const char* message) {
    ++checks;
    if (!condition) std::fprintf(stderr, "FAIL: %s\n", message);
    return condition;
}
bool SameBits(Aevrynd::Vec3 a, Aevrynd::Vec3 b) {
    return std::bit_cast<uint32_t>(a.x) == std::bit_cast<uint32_t>(b.x) &&
           std::bit_cast<uint32_t>(a.y) == std::bit_cast<uint32_t>(b.y) &&
           std::bit_cast<uint32_t>(a.z) == std::bit_cast<uint32_t>(b.z);
}
bool Direction(Aevrynd::Vec3 a, Aevrynd::Vec3 b, Aevrynd::Vec3 c, Aevrynd::Vec3 expected) {
    Aevrynd::Vec3 normal{7.f, 8.f, 9.f};
    if (!Check(Aevrynd::TryComputeTriangleNormal(a, b, c, normal), "valid triangle accepted")) return false;
    if (!Check(std::abs(Aevrynd::Length(normal) - 1.f) < 0.000001f, "unit normal")) return false;
    if (!Check(Aevrynd::Dot(normal, expected) > 0.999999f, "winding direction")) return false;
    const auto packed = Aevrynd::UnpackNormalSnorm8(Aevrynd::PackNormalSnorm8(normal));
    if (!Check(std::abs(Aevrynd::Length({packed.x, packed.y, packed.z}) - 1.f) < .02f,
               "packed normal retains unit length")) return false;
    return Check(packed.w == 1.f, "normal lighting weight preserved");
}
bool Reject(Aevrynd::Vec3 a, Aevrynd::Vec3 b, Aevrynd::Vec3 c) {
    const Aevrynd::Vec3 original{-0.f, 8.f, -9.f};
    Aevrynd::Vec3 normal = original;
    const bool accepted = Aevrynd::TryComputeTriangleNormal(a, b, c, normal);
    return Check(!accepted, "invalid triangle rejected") &&
           Check(SameBits(original, normal), "failure leaves output bits unchanged");
}
}

int main() {
    using Aevrynd::Vec3;
    constexpr std::array<float, 9> scales{
        std::numeric_limits<float>::denorm_min(), 1e-35f, 1e-20f, .0001f,
        .0006f, .006f, 1.f, 1e20f, std::numeric_limits<float>::max() / 4.f
    };
    for (const float scale : scales) {
        if (!Direction({}, {scale, 0, 0}, {0, scale, 0}, {0, 0, 1})) return 1;
        if (!Direction({}, {0, scale, 0}, {scale, 0, 0}, {0, 0, -1})) return 2;
        if (!Direction({}, {scale, 0, 0}, {0, 0, scale}, {0, -1, 0})) return 3;
        if (!Direction({}, {0, scale, 0}, {0, 0, scale}, {1, 0, 0})) return 4;
    }

    // 保留實際六毫米倒角失敗的重現，不能藉改舊 Normalize 或放寬驗收來通過。
    const Vec3 a{0, 0, .006f}, b{.006f, 0, 0}, c{0, .006f, 0};
    const Vec3 legacy = Aevrynd::Normalize(Aevrynd::Cross(b - a, c - a));
    if (!Check(SameBits(legacy, {}), "legacy six-millimetre corner reproduces zero normal")) return 5;
    const float diagonal = float(1.0 / std::sqrt(3.0));
    if (!Direction(a, b, c, {diagonal, diagonal, diagonal})) return 6;

    const float maximum = std::numeric_limits<float>::max();
    if (!Direction({maximum, maximum, 0}, {-maximum, maximum, 0},
                   {maximum, -maximum, 0}, {0, 0, 1})) return 7;
    if (!Direction({2, 3, 4}, {2.006f, 3, 4}, {2, 3.006f, 4}, {0, 0, 1})) return 8;

    if (!Reject({}, {}, {1, 0, 0}) || !Reject({}, {1, 1, 1}, {2, 2, 2})) return 9;
    // 單精度本身已抹去的位移不能被推估成有效幾何。
    if (!Reject({maximum, maximum, maximum}, {maximum, maximum, maximum}, {})) return 10;
    constexpr std::array<float, 3> invalidValues{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()
    };
    for (const float invalid : invalidValues) {
        for (unsigned position = 0; position < 3; ++position) {
            for (unsigned component = 0; component < 3; ++component) {
                std::array<Vec3, 3> vertices{{{}, {1, 0, 0}, {0, 1, 0}}};
                Vec3& vertex = vertices[position];
                if (component == 0) vertex.x = invalid;
                if (component == 1) vertex.y = invalid;
                if (component == 2) vertex.z = invalid;
                if (!Reject(vertices[0], vertices[1], vertices[2])) return 11;
            }
        }
    }
    std::printf("GEOMETRY_NORMALS_PASS checks=%u legacy_corner_reproduced=1\n", checks);
    return 0;
}
