#pragma once
#include <Aevrynd/ExactTriangleContact3D.h>
#include <Aevrynd/Mesh3D.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cfenv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#if defined(_M_X64)
#include <xmmintrin.h>
#endif

namespace Mistwatch::FullSourceSupport {
namespace Kernel = Aevrynd::ExactTriangleContactDetail3D;
using Rational = Kernel::Rational;
using Point = std::array<Rational, 3>;
using Id = std::uint32_t;
using Triangle = std::array<Id, 3>;

enum class FailureKind { Input, Dimension, Topology, Certificate, NoWinner, PredicateLimit,
    RationalLimit, FilterLimit, TopologyLimit, FacetLimit, ContactLimit, TimeLimit };
struct Failure : std::runtime_error {
    FailureKind kind;
    Failure(FailureKind value, const char* message) : std::runtime_error(message), kind(value) {}
};
inline void Require(bool value, FailureKind kind, const char* message) {
    if (!value) throw Failure(kind, message);
}
struct Limits {
    std::size_t maximumVertices{10000}, maximumIndices{50000}, maximumLiveFacets{20000};
    std::uint64_t maximumCreatedFacets{2000000}, maximumPredicates{20000000};
    std::uint64_t maximumRationalOperations{50000000}, maximumFilterOperations{200000000};
    std::uint64_t maximumTopologyOperations{50000000}, maximumContactIds{2000000};
    std::uint64_t maximumMilliseconds{240000};
};
struct Statistics {
    std::uint64_t predicates{}, filteredSigns{}, exactSigns{}, rationalOperations{},
        filterOperations{}, topologyOperations{}, createdFacets{}, certificateTests{}, contactIds{};
    std::size_t maximumLiveFacets{};
};
class Budget {
    Limits limits_;
    Statistics statistics_;
    std::chrono::steady_clock::time_point start_{std::chrono::steady_clock::now()};
    std::uint64_t clockTicks_{};
    void Add(std::uint64_t& counter, std::uint64_t maximum, FailureKind kind, const char* text) {
        Require(counter < maximum, kind, text); ++counter;
        if (!(++clockTicks_ & 255u)) CheckTime();
    }
public:
    explicit Budget(Limits limits = {}) : limits_(limits) {
        const Limits ceiling;
        Require(limits.maximumVertices <= ceiling.maximumVertices && limits.maximumIndices <= ceiling.maximumIndices &&
            limits.maximumLiveFacets <= ceiling.maximumLiveFacets && limits.maximumCreatedFacets <= ceiling.maximumCreatedFacets &&
            limits.maximumPredicates <= ceiling.maximumPredicates && limits.maximumRationalOperations <= ceiling.maximumRationalOperations &&
            limits.maximumFilterOperations <= ceiling.maximumFilterOperations && limits.maximumTopologyOperations <= ceiling.maximumTopologyOperations &&
            limits.maximumContactIds <= ceiling.maximumContactIds && limits.maximumMilliseconds <= ceiling.maximumMilliseconds,
            FailureKind::Input, "LIMIT_ABOVE_SEALED_CEILING");
        CheckTime();
    }
    const Limits& Bounds() const { return limits_; }
    const Statistics& Stats() const { return statistics_; }
    std::uint64_t ElapsedMilliseconds() const {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_).count());
    }
    void CheckTime() const { Require(ElapsedMilliseconds() < limits_.maximumMilliseconds,
        FailureKind::TimeLimit, "SOURCE_SUPPORT_TIME_LIMIT_UNRESOLVED"); }
    void Predicate() { Add(statistics_.predicates, limits_.maximumPredicates, FailureKind::PredicateLimit, "PREDICATE_LIMIT_UNRESOLVED"); }
    void Arithmetic() { Add(statistics_.rationalOperations, limits_.maximumRationalOperations, FailureKind::RationalLimit, "RATIONAL_OPERATION_LIMIT_UNRESOLVED"); }
    void Filter() { Add(statistics_.filterOperations, limits_.maximumFilterOperations, FailureKind::FilterLimit, "FILTER_OPERATION_LIMIT_UNRESOLVED"); }
    void Topology() { Add(statistics_.topologyOperations, limits_.maximumTopologyOperations, FailureKind::TopologyLimit, "TOPOLOGY_OPERATION_LIMIT_UNRESOLVED"); }
    void Created(std::size_t live) {
        Add(statistics_.createdFacets, limits_.maximumCreatedFacets, FailureKind::FacetLimit, "CREATED_FACET_LIMIT_UNRESOLVED");
        Require(live <= limits_.maximumLiveFacets, FailureKind::FacetLimit, "LIVE_FACET_LIMIT_UNRESOLVED");
        statistics_.maximumLiveFacets = (std::max)(statistics_.maximumLiveFacets, live);
    }
    void Contact() { Add(statistics_.contactIds, limits_.maximumContactIds, FailureKind::ContactLimit, "CONTACT_ID_LIMIT_UNRESOLVED"); }
    void Filtered() { ++statistics_.filteredSigns; }
    void Exact() { ++statistics_.exactSigns; }
    void CertificateTest() { ++statistics_.certificateTests; }
};
inline Rational AddQ(const Rational& a, const Rational& b, Budget& w) { w.Arithmetic(); return a + b; }
inline Rational SubQ(const Rational& a, const Rational& b, Budget& w) { w.Arithmetic(); return a - b; }
inline Rational MulQ(const Rational& a, const Rational& b, Budget& w) { w.Arithmetic(); return a * b; }
inline Rational DivQ(const Rational& a, const Rational& b, Budget& w) { w.Arithmetic(); return a / b; }
inline int CompareQ(const Rational& a, const Rational& b, Budget& w) { w.Arithmetic(); return Compare(a, b); }
inline Point SubQ(const Point& a, const Point& b, Budget& w) {
    return {{SubQ(a[0], b[0], w), SubQ(a[1], b[1], w), SubQ(a[2], b[2], w)}};
}
inline Point CrossQ(const Point& a, const Point& b, Budget& w) {
    return {{SubQ(MulQ(a[1], b[2], w), MulQ(a[2], b[1], w), w),
        SubQ(MulQ(a[2], b[0], w), MulQ(a[0], b[2], w), w),
        SubQ(MulQ(a[0], b[1], w), MulQ(a[1], b[0], w), w)}};
}
inline Rational DotQ(const Point& a, const Point& b, Budget& w) {
    return AddQ(AddQ(MulQ(a[0], b[0], w), MulQ(a[1], b[1], w), w), MulQ(a[2], b[2], w), w);
}
inline bool Nonzero(const Point& p) { return p[0].Sign() || p[1].Sign() || p[2].Sign(); }
inline Point ToQ(Aevrynd::Vec3 p, Budget& w) {
    w.Arithmetic(); const auto x = Rational::Float(p.x);
    w.Arithmetic(); const auto y = Rational::Float(p.y);
    w.Arithmetic(); const auto z = Rational::Float(p.z); return {{x, y, z}};
}
inline Point Negate(Point p) { for (auto& item : p) item = -item; return p; }
inline bool EqualQ(const Point& a, const Point& b, Budget& w) {
    for (unsigned k = 0; k < 3; ++k) if (CompareQ(a[k], b[k], w)) return false; return true;
}
inline bool SamePosition(Aevrynd::Vec3 a, Aevrynd::Vec3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
inline bool Finite(Aevrynd::Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
inline void CheckFloatingEnvironment() {
    Require(std::fegetround() == FE_TONEAREST, FailureKind::Input, "FILTER_REQUIRES_IEEE_NEAREST_ROUNDING");
#if defined(_M_X64)
    Require((_mm_getcsr() & 0x8040u) == 0, FailureKind::Input, "FILTER_REJECTS_FTZ_OR_DAZ");
#endif
}

// 每步結果向兩側各一個 double representable value，不能以快路徑的零當共面。
struct Interval { double lo{}, hi{}; };
using IntervalPoint = std::array<Interval, 3>;
inline Interval Widen(double lo, double hi) {
    Require(std::isfinite(lo) && std::isfinite(hi), FailureKind::Input, "NONFINITE_FILTER_INTERVAL");
    return {std::nextafter(lo, -(std::numeric_limits<double>::infinity)()),
        std::nextafter(hi, (std::numeric_limits<double>::infinity)())};
}
inline Interval SubI(Interval a, Interval b, Budget& w) { w.Filter(); return Widen(a.lo - b.hi, a.hi - b.lo); }
inline Interval AddI(Interval a, Interval b, Budget& w) { w.Filter(); return Widen(a.lo + b.lo, a.hi + b.hi); }
inline Interval MulI(Interval a, Interval b, Budget& w) {
    w.Filter(); const std::array<double, 4> values{{a.lo * b.lo, a.lo * b.hi, a.hi * b.lo, a.hi * b.hi}};
    return Widen(*(std::min_element)(values.begin(), values.end()), *(std::max_element)(values.begin(), values.end()));
}
inline IntervalPoint ToI(Aevrynd::Vec3 p) { return {{{p.x, p.x}, {p.y, p.y}, {p.z, p.z}}}; }
inline IntervalPoint SubI(const IntervalPoint& a, const IntervalPoint& b, Budget& w) {
    return {{SubI(a[0], b[0], w), SubI(a[1], b[1], w), SubI(a[2], b[2], w)}};
}
inline IntervalPoint CrossI(const IntervalPoint& a, const IntervalPoint& b, Budget& w) {
    return {{SubI(MulI(a[1], b[2], w), MulI(a[2], b[1], w), w),
        SubI(MulI(a[2], b[0], w), MulI(a[0], b[2], w), w),
        SubI(MulI(a[0], b[1], w), MulI(a[1], b[0], w), w)}};
}
inline Interval DotI(const IntervalPoint& a, const IntervalPoint& b, Budget& w) {
    return AddI(AddI(MulI(a[0], b[0], w), MulI(a[1], b[1], w), w), MulI(a[2], b[2], w), w);
}
struct UniquePoint { Aevrynd::Vec3 position{}; std::vector<Id> sourceIds; };
struct SourceData {
    std::vector<UniquePoint> unique;
    std::vector<Id> sourceToUnique;
    std::vector<Point> exact;
};
inline SourceData ReadSource(const Aevrynd::Mesh3D& mesh, Budget& w) {
    Require(!mesh.vertices.empty() && mesh.vertices.size() <= w.Bounds().maximumVertices &&
        !mesh.indices.empty() && mesh.indices.size() <= w.Bounds().maximumIndices && !(mesh.indices.size() % 3),
        FailureKind::Input, "SOURCE_COUNT_OR_INDEX_LIMIT");
    for (auto index : mesh.indices) { w.Topology(); Require(index < mesh.vertices.size(), FailureKind::Input, "SOURCE_INDEX_OUTSIDE"); }
    std::vector<Id> order(mesh.vertices.size()); std::iota(order.begin(), order.end(), Id{0});
    const auto vertex = [&](Id id) { const auto& v = mesh.vertices[id]; return Aevrynd::Vec3{v.x, v.y, v.z}; };
    for (auto id : order) { w.Topology(); Require(Finite(vertex(id)), FailureKind::Input, "NONFINITE_RAW_SOURCE_XYZ"); }
    std::sort(order.begin(), order.end(), [&](Id a, Id b) {
        w.Topology(); const auto p = vertex(a), q = vertex(b);
        if (p.x != q.x) return p.x < q.x; if (p.y != q.y) return p.y < q.y;
        if (p.z != q.z) return p.z < q.z; return a < b;
    });
    SourceData result; result.sourceToUnique.resize(mesh.vertices.size());
    for (auto id : order) {
        w.Topology(); const auto p = vertex(id);
        if (result.unique.empty() || !SamePosition(result.unique.back().position, p)) result.unique.push_back({p, {}});
        result.unique.back().sourceIds.push_back(id);
        result.sourceToUnique[id] = static_cast<Id>(result.unique.size() - 1);
    }
    for (const auto& item : result.unique) result.exact.push_back(ToQ(item.position, w));
    return result;
}
struct Face {
    Triangle vertices{};
    Point normal;
    IntervalPoint filterNormal;
    Aevrynd::Vec3 origin{};
};
inline int Sign(const Face& face, Aevrynd::Vec3 raw, const Point& exact, const Point& origin, Budget& w) {
    w.Predicate(); const auto interval = DotI(face.filterNormal, SubI(ToI(raw), ToI(face.origin), w), w);
    if (interval.lo > 0) { w.Filtered(); return 1; }
    if (interval.hi < 0) { w.Filtered(); return -1; }
    w.Exact(); return DotQ(face.normal, SubQ(exact, origin, w), w).Sign();
}
inline Face MakeFace(Triangle vertices, const SourceData& data, const Point& inside, Budget& w) {
    Face face; face.vertices = vertices;
    face.normal = CrossQ(SubQ(data.exact[vertices[1]], data.exact[vertices[0]], w),
        SubQ(data.exact[vertices[2]], data.exact[vertices[0]], w), w);
    Require(::Mistwatch::FullSourceSupport::Nonzero(face.normal), FailureKind::Topology, "DEGENERATE_HULL_FACET");
    const int side = DotQ(face.normal, SubQ(inside, data.exact[vertices[0]], w), w).Sign();
    Require(side != 0, FailureKind::Topology, "HULL_INTERIOR_ON_FACET");
    if (side > 0) { std::swap(face.vertices[1], face.vertices[2]); face.normal = Negate(std::move(face.normal)); }
    face.origin = data.unique[face.vertices[0]].position;
    face.filterNormal = CrossI(SubI(ToI(data.unique[face.vertices[1]].position), ToI(face.origin), w),
        SubI(ToI(data.unique[face.vertices[2]].position), ToI(face.origin), w), w);
    return face;
}
inline std::array<Id, 4> Seed(const SourceData& data, Budget& w) {
    Require(data.unique.size() >= 4, FailureKind::Dimension, "SOURCE_AFFINE_DIMENSION_LT3");
    const Id a = 0, b = static_cast<Id>(data.unique.size() - 1);
    Id c = a, d = a; Rational best;
    const auto line = SubQ(data.exact[b], data.exact[a], w);
    for (Id id = 0; id < data.unique.size(); ++id) {
        const auto normal = CrossQ(line, SubQ(data.exact[id], data.exact[a], w), w);
        const auto length = DotQ(normal, normal, w);
        if (CompareQ(length, best, w) > 0) { best = length; c = id; }
    }
    Require(best.Sign() > 0, FailureKind::Dimension, "SOURCE_AFFINE_DIMENSION_LT2");
    const auto normal = CrossQ(line, SubQ(data.exact[c], data.exact[a], w), w); best = Rational{};
    for (Id id = 0; id < data.unique.size(); ++id) {
        auto volume = DotQ(normal, SubQ(data.exact[id], data.exact[a], w), w);
        if (volume.Sign() < 0) volume = -volume;
        if (CompareQ(volume, best, w) > 0) { best = volume; d = id; }
    }
    Require(best.Sign() > 0, FailureKind::Dimension, "SOURCE_AFFINE_DIMENSION_LT3"); return {{a, b, c, d}};
}
inline std::vector<Face> Hull(const SourceData& data, Budget& w) {
    const auto seed = Seed(data, w); Point inside;
    for (unsigned k = 0; k < 3; ++k) {
        Rational sum; for (auto id : seed) sum = AddQ(sum, data.exact[id][k], w);
        inside[k] = DivQ(sum, Rational(4), w);
    }
    std::vector<Face> faces;
    for (const Triangle tri : {Triangle{seed[0], seed[1], seed[2]}, Triangle{seed[0], seed[3], seed[1]},
        Triangle{seed[0], seed[2], seed[3]}, Triangle{seed[1], seed[3], seed[2]}}) {
        faces.push_back(MakeFace(tri, data, inside, w)); w.Created(faces.size());
    }
    for (Id id = 0; id < data.unique.size(); ++id) {
        w.CheckTime(); if (std::find(seed.begin(), seed.end(), id) != seed.end()) continue;
        std::vector<bool> visible(faces.size()); std::size_t count{};
        for (std::size_t index = 0; index < faces.size(); ++index) {
            visible[index] = Sign(faces[index], data.unique[id].position, data.exact[id],
                data.exact[faces[index].vertices[0]], w) > 0; count += visible[index] ? 1u : 0u;
        }
        if (!count) continue;
        struct Edge { Id a{}, b{}; unsigned count{}; };
        std::map<std::pair<Id, Id>, Edge> edges;
        for (std::size_t index = 0; index < faces.size(); ++index) if (visible[index]) {
            for (unsigned k = 0; k < 3; ++k) {
                w.Topology(); const Id a = faces[index].vertices[k], b = faces[index].vertices[(k + 1) % 3];
                const auto key = std::minmax(a, b); auto& edge = edges[{key.first, key.second}];
                if (!edge.count) { edge = {a, b, 1}; } else {
                    Require(edge.count == 1 && edge.a == b && edge.b == a, FailureKind::Topology, "VISIBLE_EDGE_ORIENTATION_NONMANIFOLD"); ++edge.count;
                }
            }
        }
        std::vector<Edge> horizon; std::map<Id, Id> next; std::map<Id, unsigned> incoming;
        for (const auto& item : edges) if (item.second.count == 1) {
            w.Topology(); const auto edge = item.second; horizon.push_back(edge);
            Require(next.emplace(edge.a, edge.b).second, FailureKind::Topology, "HORIZON_BRANCH"); ++incoming[edge.b];
        }
        Require(horizon.size() >= 3, FailureKind::Topology, "EMPTY_OR_SMALL_HORIZON");
        for (const auto& item : next) Require(incoming[item.first] == 1, FailureKind::Topology, "HORIZON_NOT_CLOSED");
        Id cursor = next.begin()->first; const Id first = cursor;
        for (std::size_t k = 0; k < horizon.size(); ++k) {
            w.Topology(); const auto found = next.find(cursor); Require(found != next.end(), FailureKind::Topology, "HORIZON_BREAK");
            cursor = found->second; Require(cursor != first || k + 1 == horizon.size(), FailureKind::Topology, "MULTIPLE_HORIZON_LOOPS");
        }
        Require(cursor == first, FailureKind::Topology, "HORIZON_NOT_A_SINGLE_LOOP");
        const std::size_t newCount = faces.size() - count + horizon.size();
        Require(newCount <= w.Bounds().maximumLiveFacets && newCount <= 2 * data.unique.size() - 4,
            FailureKind::FacetLimit, "HULL_FACE_BOUND_UNRESOLVED");
        std::vector<Face> updated; updated.reserve(newCount);
        for (std::size_t index = 0; index < faces.size(); ++index) if (!visible[index]) updated.push_back(std::move(faces[index]));
        for (const auto& edge : horizon) {
            auto face = MakeFace({{edge.a, edge.b, id}}, data, inside, w);
            Require(face.vertices[0] == edge.a && face.vertices[1] == edge.b,
                FailureKind::Topology, "NEW_HORIZON_FACET_REVERSED");
            updated.push_back(std::move(face)); w.Created(newCount);
        }
        faces = std::move(updated);
    }
    w.CheckTime(); return faces;
}
inline Triangle SourceTriangle(const Face& face, const SourceData& data) {
    Triangle result; for (unsigned k = 0; k < 3; ++k) result[k] = data.unique[face.vertices[k]].sourceIds.front();
    const auto smallest = static_cast<unsigned>(std::min_element(result.begin(), result.end()) - result.begin());
    return {{result[smallest], result[(smallest + 1) % 3], result[(smallest + 2) % 3]}};
}
struct Plane {
    Triangle canonicalIds{};
    int crossOrientation{1};
    Point outwardNormal;
    Rational offset, xSpan, downwardNumerator, normSquared;
    std::vector<Id> coplanarIds;
    std::vector<Triangle> facets;
    std::size_t negativePoints{}, zeroPoints{};
    bool eligible{};
};
struct Result {
    std::vector<UniquePoint> unique;
    std::vector<Id> sourceToUnique;
    std::vector<Triangle> facets;
    std::vector<Plane> planes;
    std::size_t winner{(std::numeric_limits<std::size_t>::max)()};
};
inline const Point& SourceQ(const SourceData& data, Id id) { return data.exact.at(data.sourceToUnique.at(id)); }
inline Aevrynd::Vec3 SourceRaw(const Aevrynd::Mesh3D& mesh, Id id) {
    const auto& v = mesh.vertices.at(id); return {v.x, v.y, v.z};
}
inline Triangle CanonicalTriple(const std::vector<Id>& ids, const SourceData& data, Budget& w) {
    Require(ids.size() >= 3, FailureKind::Certificate, "SUPPORT_PLANE_CONTACT_SET_TOO_SMALL");
    const Id a = ids.front(); std::size_t second = 1;
    while (second < ids.size() && EqualQ(SourceQ(data, a), SourceQ(data, ids[second]), w)) ++second;
    Require(second < ids.size(), FailureKind::Certificate, "CONTACT_POINTS_ALL_EQUAL"); const Id b = ids[second];
    const auto direction = SubQ(SourceQ(data, b), SourceQ(data, a), w);
    for (std::size_t k = second + 1; k < ids.size(); ++k) {
        if (::Mistwatch::FullSourceSupport::Nonzero(CrossQ(direction, SubQ(SourceQ(data, ids[k]), SourceQ(data, a), w), w))) return {{a, b, ids[k]}};
    }
    throw Failure(FailureKind::Certificate, "CONTACT_POINTS_COLLINEAR");
}
inline Face PlaneFace(const Plane& plane, const Aevrynd::Mesh3D& mesh, Budget& w) {
    Face face; face.normal = plane.outwardNormal; face.origin = SourceRaw(mesh, plane.canonicalIds[0]);
    auto interval = CrossI(SubI(ToI(SourceRaw(mesh, plane.canonicalIds[1])), ToI(face.origin), w),
        SubI(ToI(SourceRaw(mesh, plane.canonicalIds[2])), ToI(face.origin), w), w);
    if (plane.crossOrientation < 0) for (auto& axis : interval) axis = {-axis.hi, -axis.lo};
    face.filterNormal = interval; return face;
}
inline bool CertifyPoints(const Aevrynd::Mesh3D& mesh, const Plane& plane, Budget& w,
    std::vector<Id>* zeros = nullptr, std::size_t* negatives = nullptr) {
    CheckFloatingEnvironment();
    Require(!mesh.vertices.empty() && mesh.vertices.size() <= w.Bounds().maximumVertices,
        FailureKind::Input, "CERTIFICATE_SOURCE_COUNT_LIMIT");
    Require(plane.crossOrientation == 1 || plane.crossOrientation == -1, FailureKind::Certificate, "INVALID_PLANE_ORIENTATION");
    auto raw = CrossQ(SubQ(ToQ(SourceRaw(mesh, plane.canonicalIds[1]), w), ToQ(SourceRaw(mesh, plane.canonicalIds[0]), w), w),
        SubQ(ToQ(SourceRaw(mesh, plane.canonicalIds[2]), w), ToQ(SourceRaw(mesh, plane.canonicalIds[0]), w), w), w);
    if (plane.crossOrientation < 0) raw = Negate(std::move(raw));
    if (!::Mistwatch::FullSourceSupport::Nonzero(raw) || !EqualQ(raw, plane.outwardNormal, w)) return false;
    const auto origin = ToQ(SourceRaw(mesh, plane.canonicalIds[0]), w); const auto face = PlaneFace(plane, mesh, w);
    if (zeros) zeros->clear(); if (negatives) *negatives = 0;
    for (Id id = 0; id < mesh.vertices.size(); ++id) {
        w.CertificateTest(); const auto p = SourceRaw(mesh, id);
        const int side = Sign(face, p, ToQ(p, w), origin, w);
        if (side > 0) return false;
        if (!side && zeros) { w.Contact(); zeros->push_back(id); }
        if (side < 0 && negatives) ++*negatives;
    }
    return true;
}
inline void Score(Plane& plane, const SourceData& data, Budget& w) {
    Rational lo = SourceQ(data, plane.coplanarIds.front())[0], hi = lo;
    for (auto id : plane.coplanarIds) {
        const auto& x = SourceQ(data, id)[0]; if (CompareQ(x, lo, w) < 0) lo = x; if (CompareQ(x, hi, w) > 0) hi = x;
    }
    plane.xSpan = SubQ(hi, lo, w); plane.normSquared = DotQ(plane.outwardNormal, plane.outwardNormal, w);
    plane.downwardNumerator = MulQ(plane.outwardNormal[1], plane.outwardNormal[1], w);
    const auto projectedX = SubQ(plane.normSquared, MulQ(plane.outwardNormal[0], plane.outwardNormal[0], w), w);
    plane.eligible = plane.outwardNormal[1].Sign() < 0 && projectedX.Sign() > 0;
}
inline bool Better(const Plane& a, const Plane& b, Budget& w) {
    const int span = CompareQ(a.xSpan, b.xSpan, w); if (span) return span > 0;
    const int tilt = CompareQ(MulQ(a.downwardNumerator, b.normSquared, w), MulQ(b.downwardNumerator, a.normSquared, w), w);
    if (tilt) return tilt > 0; return a.coplanarIds < b.coplanarIds;
}
inline std::size_t Select(const std::vector<Plane>& planes, Budget& w) {
    std::size_t winner = (std::numeric_limits<std::size_t>::max)();
    for (std::size_t k = 0; k < planes.size(); ++k) {
        w.Topology(); if (planes[k].eligible && (winner == (std::numeric_limits<std::size_t>::max)() || Better(planes[k], planes[winner], w))) winner = k;
    }
    Require(winner != (std::numeric_limits<std::size_t>::max)(), FailureKind::NoWinner, "NO_FULL_SOURCE_DOWNWARD_SUPPORT_PLANE"); return winner;
}
inline void CheckClosedHull(const std::vector<Triangle>& facets, Budget& w) {
    struct Edge { Id a{}, b{}; std::size_t face{}; unsigned count{}; };
    std::map<std::pair<Id, Id>, Edge> edges; std::set<Id> vertices; std::vector<std::vector<std::size_t>> links(facets.size());
    std::set<Triangle> unorderedFacets;
    for (std::size_t index = 0; index < facets.size(); ++index) {
        Triangle sorted = facets[index]; std::sort(sorted.begin(), sorted.end());
        Require(sorted[0] != sorted[1] && sorted[1] != sorted[2] && unorderedFacets.insert(sorted).second,
            FailureKind::Topology, "DUPLICATE_OR_DEGENERATE_HULL_IDS");
        for (unsigned k = 0; k < 3; ++k) {
            w.Topology(); const Id a = facets[index][k], b = facets[index][(k + 1) % 3]; vertices.insert(a);
            const auto pair = std::minmax(a, b); auto& edge = edges[{pair.first, pair.second}];
            if (!edge.count) edge = {a, b, index, 1}; else {
                Require(edge.count == 1 && edge.a == b && edge.b == a, FailureKind::Topology, "FINAL_HULL_NONMANIFOLD_OR_REVERSED_EDGE");
                ++edge.count; links[index].push_back(edge.face); links[edge.face].push_back(index);
            }
        }
    }
    for (const auto& edge : edges) Require(edge.second.count == 2, FailureKind::Topology, "FINAL_HULL_OPEN_EDGE");
    Require(!facets.empty() && vertices.size() + facets.size() == edges.size() + 2,
        FailureKind::Topology, "FINAL_HULL_EULER_FAILURE");
    std::vector<bool> seen(facets.size()); std::vector<std::size_t> pending{0}; seen[0] = true; std::size_t visited{};
    while (!pending.empty()) { w.Topology(); const auto index = pending.back(); pending.pop_back(); ++visited;
        for (auto neighbor : links[index]) if (!seen[neighbor]) { seen[neighbor] = true; pending.push_back(neighbor); } }
    Require(visited == facets.size(), FailureKind::Topology, "FINAL_HULL_DISCONNECTED");
}
using PlaneKey = std::array<Rational, 4>;
struct KeyLess {
    Budget* budget{};
    bool operator()(const PlaneKey& a, const PlaneKey& b) const {
        for (unsigned k = 0; k < 4; ++k) { const int compare = CompareQ(a[k], b[k], *budget); if (compare) return compare < 0; } return false;
    }
};
inline PlaneKey Key(const Face& face, const Point& origin, Budget& w) {
    unsigned pivot{}; while (pivot < 3 && !face.normal[pivot].Sign()) ++pivot;
    Require(pivot < 3, FailureKind::Certificate, "ZERO_SUPPORT_NORMAL");
    auto divisor = face.normal[pivot]; if (divisor.Sign() < 0) divisor = -divisor;
    return {{DivQ(face.normal[0], divisor, w), DivQ(face.normal[1], divisor, w), DivQ(face.normal[2], divisor, w),
        DivQ(DotQ(face.normal, origin, w), divisor, w)}};
}
inline Result Build(const Aevrynd::Mesh3D& source, Budget& w) {
    static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<float>::is_iec559);
    CheckFloatingEnvironment();
    const auto data = ReadSource(source, w); auto hull = Hull(data, w); Result result;
    result.unique = data.unique; result.sourceToUnique = data.sourceToUnique;
    for (const auto& face : hull) result.facets.push_back(SourceTriangle(face, data));
    std::sort(result.facets.begin(), result.facets.end()); CheckClosedHull(result.facets, w);
    std::map<PlaneKey, std::size_t, KeyLess> groups(KeyLess{&w});
    for (const auto& face : hull) {
        w.Topology(); const auto key = Key(face, data.exact[face.vertices[0]], w); const auto found = groups.find(key);
        std::size_t index{};
        if (found == groups.end()) { index = result.planes.size(); groups.emplace(key, index); result.planes.emplace_back();
            result.planes.back().outwardNormal = face.normal; } else index = found->second;
        result.planes[index].facets.push_back(SourceTriangle(face, data));
    }
    for (auto& plane : result.planes) {
        std::sort(plane.facets.begin(), plane.facets.end());
        const auto first = plane.facets.front(); plane.canonicalIds = first; plane.crossOrientation = 1;
        // 首 facet 的最小 source-ID cyclic 表示保留原 winding，先以它核全原點。
        plane.outwardNormal = CrossQ(SubQ(SourceQ(data, first[1]), SourceQ(data, first[0]), w),
            SubQ(SourceQ(data, first[2]), SourceQ(data, first[0]), w), w);
        Require(CertifyPoints(source, plane, w, &plane.coplanarIds, &plane.negativePoints),
            FailureKind::Certificate, "FULL_SOURCE_SUPPORT_CERTIFICATE_FAILED");
        Require(plane.negativePoints > 0, FailureKind::Dimension, "ALL_SOURCE_COPLANAR");
        plane.zeroPoints = plane.coplanarIds.size(); const auto originalNormal = plane.outwardNormal;
        plane.canonicalIds = CanonicalTriple(plane.coplanarIds, data, w);
        plane.outwardNormal = CrossQ(SubQ(SourceQ(data, plane.canonicalIds[1]), SourceQ(data, plane.canonicalIds[0]), w),
            SubQ(SourceQ(data, plane.canonicalIds[2]), SourceQ(data, plane.canonicalIds[0]), w), w);
        if (DotQ(plane.outwardNormal, originalNormal, w).Sign() < 0) {
            plane.crossOrientation = -1; plane.outwardNormal = Negate(std::move(plane.outwardNormal));
        }
        plane.offset = DotQ(plane.outwardNormal, SourceQ(data, plane.canonicalIds[0]), w); Score(plane, data, w);
    }
    std::sort(result.planes.begin(), result.planes.end(), [](const Plane& a, const Plane& b) { return a.coplanarIds < b.coplanarIds; });
    result.winner = Select(result.planes, w); w.CheckTime(); return result;
}
inline void Validate(const Aevrynd::Mesh3D& source, const Result& result, Budget& w) {
    CheckFloatingEnvironment();
    const auto data = ReadSource(source, w);
    Require(result.facets.size() <= w.Bounds().maximumLiveFacets && !result.planes.empty() &&
        result.planes.size() <= result.facets.size(), FailureKind::Certificate, "RESULT_FACET_OR_PLANE_LIMIT");
    Require(result.sourceToUnique == data.sourceToUnique && result.unique.size() == data.unique.size(),
        FailureKind::Certificate, "SEAM_SOURCE_MAPPING_CHANGED");
    for (std::size_t k = 0; k < data.unique.size(); ++k) {
        w.Topology(); Require(SamePosition(result.unique[k].position, data.unique[k].position) &&
            result.unique[k].sourceIds == data.unique[k].sourceIds, FailureKind::Certificate, "UNIQUE_SOURCE_IDS_CHANGED");
    }
    CheckClosedHull(result.facets, w); std::vector<Triangle> covered;
    std::set<std::vector<Id>> planeSets;
    for (const auto& plane : result.planes) {
        w.CheckTime(); std::vector<Id> zeros; std::size_t negatives{};
        Require(CertifyPoints(source, plane, w, &zeros, &negatives), FailureKind::Certificate, "RECHECK_SOURCE_OUTSIDE_PLANE");
        Require(zeros == plane.coplanarIds && negatives == plane.negativePoints && zeros.size() == plane.zeroPoints && negatives > 0 &&
            planeSets.insert(zeros).second, FailureKind::Certificate, "COPLANAR_FULL_SET_INCOMPLETE_OR_DUPLICATED");
        Require(CanonicalTriple(zeros, data, w) == plane.canonicalIds, FailureKind::Certificate, "NONCANONICAL_SOURCE_TRIPLE");
        Require(CompareQ(plane.offset, DotQ(plane.outwardNormal, SourceQ(data, plane.canonicalIds[0]), w), w) == 0,
            FailureKind::Certificate, "PLANE_OFFSET_CHANGED");
        for (const auto& tri : plane.facets) {
            w.Topology(); for (auto id : tri) Require(std::binary_search(zeros.begin(), zeros.end(), id), FailureKind::Certificate, "FACET_OUTSIDE_COPLANAR_GROUP");
            const auto normal = CrossQ(SubQ(SourceQ(data, tri[1]), SourceQ(data, tri[0]), w),
                SubQ(SourceQ(data, tri[2]), SourceQ(data, tri[0]), w), w);
            Require(::Mistwatch::FullSourceSupport::Nonzero(normal) && !::Mistwatch::FullSourceSupport::Nonzero(CrossQ(normal, plane.outwardNormal, w)) && DotQ(normal, plane.outwardNormal, w).Sign() > 0,
                FailureKind::Certificate, "FACET_NOT_SAME_OUTWARD_SUPPORT_PLANE"); covered.push_back(tri);
        }
        Plane score = plane; Score(score, data, w);
        Require(CompareQ(score.xSpan, plane.xSpan, w) == 0 && CompareQ(score.downwardNumerator, plane.downwardNumerator, w) == 0 &&
            CompareQ(score.normSquared, plane.normSquared, w) == 0 && score.eligible == plane.eligible,
            FailureKind::Certificate, "DERIVED_WINNER_SCORE_CHANGED");
    }
    std::sort(covered.begin(), covered.end()); Require(covered == result.facets, FailureKind::Certificate, "HULL_FACET_COVERAGE_CHANGED");
    Require(Select(result.planes, w) == result.winner, FailureKind::Certificate, "UNIQUE_WINNER_CHANGED"); w.CheckTime();
}
}
