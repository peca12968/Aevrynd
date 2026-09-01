#include "GameplayOccluderMask3D.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace GameplayLab {
namespace {
using Aevrynd::Vec3;
using Aevrynd::Vec4;

constexpr std::array<std::array<unsigned, 3>, 12> BoxTriangles{{
    {0, 2, 1}, {1, 2, 3}, {4, 5, 6}, {5, 7, 6},
    {0, 1, 4}, {1, 5, 4}, {2, 6, 3}, {3, 6, 7},
    {0, 4, 2}, {2, 4, 6}, {1, 3, 5}, {3, 7, 5}}};

float PlaneDistance(const Vec4 &point, unsigned plane) noexcept {
  switch (plane) {
  case 0: return point.w - .00001f;
  case 1: return point.x + point.w;
  case 2: return point.w - point.x;
  case 3: return point.y + point.w;
  case 4: return point.w - point.y;
  case 5: return point.z;
  default: return point.w - point.z;
  }
}

Vec4 Interpolate(const Vec4 &left, const Vec4 &right, float amount) noexcept {
  return {left.x + (right.x - left.x) * amount,
          left.y + (right.y - left.y) * amount,
          left.z + (right.z - left.z) * amount,
          left.w + (right.w - left.w) * amount};
}

bool ClipPolygon(std::vector<Vec4> &polygon, bool &nearClipped) {
  std::vector<Vec4> scratch;
  scratch.reserve(10);
  for (unsigned plane = 0; plane < 7 && !polygon.empty(); ++plane) {
    scratch.clear();
    Vec4 previous = polygon.back();
    float previousDistance = PlaneDistance(previous, plane);
    bool previousInside = previousDistance >= 0.0f;
    for (const Vec4 current : polygon) {
      const float currentDistance = PlaneDistance(current, plane);
      const bool currentInside = currentDistance >= 0.0f;
      if (currentInside != previousInside) {
        const float denominator = previousDistance - currentDistance;
        if (std::isfinite(denominator) && std::abs(denominator) > 0.000001f) {
          const float amount = previousDistance / denominator;
          scratch.push_back(Interpolate(previous, current, amount));
          // w<=epsilon 與 z<0 都屬鏡頭/近裁切邊界；兩者都必須標示。
          nearClipped |= plane == 0 || plane == 5;
        }
      }
      if (currentInside)
        scratch.push_back(current);
      previous = current;
      previousDistance = currentDistance;
      previousInside = currentInside;
    }
    polygon.swap(scratch);
  }
  return polygon.size() >= 3;
}

struct Point2 { float x{}, y{}; };

float Edge(Point2 a, Point2 b, Point2 point) noexcept {
  return (point.x - a.x) * (b.y - a.y) -
         (point.y - a.y) * (b.x - a.x);
}

void RasterTriangle(Point2 a, Point2 b, Point2 c, std::uint32_t width,
                    std::uint32_t height, std::vector<std::uint8_t> &mask) {
  const float area = Edge(a, b, c);
  if (!std::isfinite(area) || std::abs(area) < 0.0001f)
    return;
  const int minimumX = (std::max)(0, int(std::floor((std::min)({a.x,b.x,c.x}))));
  const int maximumX = (std::min)(int(width)-1, int(std::ceil((std::max)({a.x,b.x,c.x}))));
  const int minimumY = (std::max)(0, int(std::floor((std::min)({a.y,b.y,c.y}))));
  const int maximumY = (std::min)(int(height)-1, int(std::ceil((std::max)({a.y,b.y,c.y}))));
  for (int y = minimumY; y <= maximumY; ++y)
    for (int x = minimumX; x <= maximumX; ++x) {
      const Point2 sample{float(x) + .5f, float(y) + .5f};
      const float e0 = Edge(a, b, sample), e1 = Edge(b, c, sample),
                  e2 = Edge(c, a, sample);
      if ((area > 0.f && e0 >= 0.f && e1 >= 0.f && e2 >= 0.f) ||
          (area < 0.f && e0 <= 0.f && e1 <= 0.f && e2 <= 0.f))
        mask[std::size_t(y) * width + std::size_t(x)] = 255;
    }
}

bool Finite(Vec3 value) noexcept {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}
} // namespace

bool AnalyzeGameplayOccluderMask3D(
    const Aevrynd::Camera3D &camera,
    std::span<const GameplayOccluderBox3D> occluders,
    std::uint32_t width, std::uint32_t height,
    GameplayOccluderMask3D &output) noexcept {
  output = {};
  if (!width || !height || width > 1920 || height > 1080 ||
      occluders.empty() || !Finite(camera.position) || !Finite(camera.target))
    return false;
  try {
    const auto viewProjection = Aevrynd::CreateViewProjectionMatrix(
        camera, float(width) / float(height));
    std::vector<std::uint8_t> mask(std::size_t(width) * height);
    std::vector<std::uint8_t> best(mask.size());
    for (const auto &box : occluders) {
      if (!box.stableLow || !Finite(box.center) || !Finite(box.size) ||
          !std::isfinite(box.yaw) || box.size.x <= 0.f || box.size.y <= 0.f ||
          box.size.z <= 0.f)
        return false;
      std::fill(mask.begin(), mask.end(), std::uint8_t{});
      const float sine = std::sin(box.yaw), cosine = std::cos(box.yaw);
      std::array<Vec4, 8> clip{};
      unsigned vertex{};
      for (float z : {-.5f, .5f})
        for (float y : {-.5f, .5f})
          for (float x : {-.5f, .5f}) {
            const Vec3 local{x * box.size.x, y * box.size.y, z * box.size.z};
            const Vec3 world = box.center +
                Vec3{cosine*local.x+sine*local.z,local.y,
                     -sine*local.x+cosine*local.z};
            clip[vertex++] = viewProjection * Vec4{world.x,world.y,world.z,1.f};
          }
      bool nearClipped{};
      for (const auto triangle : BoxTriangles) {
        std::vector<Vec4> polygon{clip[triangle[0]], clip[triangle[1]],
                                  clip[triangle[2]]};
        if (!ClipPolygon(polygon, nearClipped))
          continue;
        const auto project = [&](const Vec4 &point) {
          const float inverseW = 1.f / point.w;
          return Point2{(point.x * inverseW * .5f + .5f) * width,
                        (.5f - point.y * inverseW * .5f) * height};
        };
        const Point2 origin = project(polygon.front());
        for (std::size_t index = 1; index + 1 < polygon.size(); ++index)
          RasterTriangle(origin, project(polygon[index]),
                         project(polygon[index + 1]), width, height, mask);
      }
      const std::uint64_t covered = std::count(mask.begin(), mask.end(), 255);
      if (covered <= output.coveredPixels)
        continue;
      output.stableLow = box.stableLow;
      output.width = width;
      output.height = height;
      output.coveredPixels = covered;
      output.coverage = float(covered) / float(mask.size());
      output.nearPlaneClipped = nearClipped;
      best = mask;
    }
    if (!output.stableLow)
      return false;
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto value : best) { hash ^= value; hash *= 1099511628211ull; }
    output.maskHash = hash;
    output.pixels = std::move(best);
    return true;
  } catch (...) {
    output = {};
    return false;
  }
}

} // namespace GameplayLab
