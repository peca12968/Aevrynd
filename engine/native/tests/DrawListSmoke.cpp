#include <Aevrynd/Renderer.h>
#include <Aevrynd/Animation3D.h>
#include <Aevrynd/Effects2D.h>
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/Navigation3D.h>
#include <Aevrynd/Scene2D.h>
#include <Aevrynd/Spatial2D.h>
#include <Aevrynd/WorldStreamer3D.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>

using namespace Aevrynd;
constexpr Color Cyan{.1f,.9f,.8f,1.f};

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
static_assert(sizeof(Vertex) == 12);
static_assert(offsetof(Vertex, color) == 8);
static_assert(sizeof(Vertex3D) == 20);
static_assert(offsetof(Vertex3D, normal) == 12);
static_assert(offsetof(Vertex3D, color) == 16);
static_assert(D3D12Renderer::MaxVertices * sizeof(Vertex) * D3D12Renderer::BufferedFrames == 8640000);
static_assert(D3D12Renderer::MaxWorldVertices * sizeof(Vertex3D) * D3D12Renderer::BufferedFrames == 14400000);
static_assert(D3D12Renderer::MaxWorldIndices * sizeof(uint32_t) * D3D12Renderer::BufferedFrames == 5760000);
static_assert(D3D12Renderer::MeshInstanceUploadBytes == 120);
static_assert(D3D12Renderer::MaxMeshInstances * D3D12Renderer::MeshInstanceUploadBytes * D3D12Renderer::BufferedFrames == 23592960);
static_assert(PackSurface3D({.2f,.8f,.4f,.6f}) == PackColorRgba8({.2f,.8f,.4f,.6f}));
static_assert(PackColorRgba8({1.f, .5f, 0.f, 1.f}) == 0xff0080ffu);
static_assert(PackColorRgba8({-1.f, 2.f, .25f, .5f}) == 0x8040ff00u);

int ValidateSceneRuntime2D() {
    MotionState2D motion;
    motion.Reset({10, 20});
    motion.Advance({30, 60});
    if (Distance(motion.Previous(), {10, 20}) > .001f || Distance(motion.Current(), {30, 60}) > .001f || Distance(motion.Sample(.25f), {15, 30}) > .001f || Distance(motion.Sample(-1), {10, 20}) > .001f || Distance(motion.Sample(2), {30, 60}) > .001f) {
        std::cerr << "Fixed-step 2D motion interpolation failed\n";
        return 35;
    }
    motion.Synchronize();
    if (Distance(motion.Sample(.25f), {30, 60}) > .001f) {
        std::cerr << "Stationary 2D motion synchronization failed\n";
        return 39;
    }
    DepthBatch2D batch(64);
    batch.Add(3, 0);
    batch.Add(1, 1);
    batch.Add(3, 2);
    batch.Add(std::numeric_limits<float>::quiet_NaN(), 3);
    batch.Sort();
    const auto& small = batch.Entries();
    if (small[0].index != 3 || small[1].index != 1 || small[2].index != 0 || small[3].index != 2) {
        std::cerr << "Stable small depth batch failed\n";
        return 36;
    }
    batch.Clear();
    constexpr std::size_t EntryCount = 10000;
    for (std::size_t index = 0; index < EntryCount; ++index) batch.Add(float((EntryCount - index) % 257), index, index % 3);
    const auto start = std::chrono::steady_clock::now();
    batch.Sort();
    const uint64_t elapsedMicroseconds = uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
    const auto& large = batch.Entries();
    for (std::size_t index = 1; index < large.size(); ++index) {
        if (large[index].depth < large[index - 1].depth || (large[index].depth == large[index - 1].depth && large[index].index < large[index - 1].index)) {
            std::cerr << "Adaptive large depth batch failed\n";
            return 37;
        }
    }
    const DepthBatch2DStats sortedStats = batch.Stats();
    const std::size_t retainedCapacity = sortedStats.capacity;
    batch.Clear();
    const DepthBatch2DStats clearedStats = batch.Stats();
    if (elapsedMicroseconds > 20000 || sortedStats.peakEntries != EntryCount || sortedStats.smallSorts != 1 || sortedStats.largeSorts != 1 || clearedStats.entries != 0 || clearedStats.capacity != retainedCapacity) {
        std::cerr << "Reusable depth batch budget or telemetry failed\n";
        return 38;
    }
    std::cout << "depthEntries=" << sortedStats.peakEntries << " depthCapacity=" << retainedCapacity << " depthSortUs=" << elapsedMicroseconds << " depthSmallSorts=" << sortedStats.smallSorts << " depthLargeSorts=" << sortedStats.largeSorts << '\n';
    return 0;
}

int ValidateRuntimeSelection3D() {
    const std::array<PointLight3D,4> orderedLights={PointLight3D{{},1,{.1f,0,0,1},1},PointLight3D{{},1,{.2f,0,0,1},5},PointLight3D{{},1,{.3f,0,0,1},3},PointLight3D{{},1,{.4f,0,0,1},5}};
    const auto ordered=SelectInfluentialPointLights<3>(orderedLights,{});
    if(ordered.count!=3||ordered.candidates!=4||ordered.lights[0].color.r!=.2f||ordered.lights[1].color.r!=.4f||ordered.lights[2].color.r!=.3f){std::cerr<<"Stable point-light influence selection failed\n";return 40;}
    constexpr std::size_t LightCount=10000;std::vector<PointLight3D> lights;lights.reserve(LightCount);for(std::size_t index=0;index<LightCount;++index)lights.push_back({{},1,{float(index),0,0,1},float(index%997+1)});
    constexpr int Iterations=100;float checksum=0;const auto lightStart=std::chrono::steady_clock::now();for(int iteration=0;iteration<Iterations;++iteration){const auto selected=SelectInfluentialPointLights<3>(lights,{});checksum+=selected.lights[0].color.r+selected.lights[1].color.r+selected.lights[2].color.r;}const uint64_t lightAverageNanoseconds=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-lightStart).count())/Iterations;
    if(lightAverageNanoseconds>2000000||checksum<=0){std::cerr<<"Allocation-free point-light selection budget failed\n";return 41;}
    Skeleton3D skeleton;for(int joint=0;joint<6;++joint)skeleton.joints.push_back({joint-1,{{0,1,0},{},{1,1,1}}});AnimationClip3D clip{1,true,{{0,{{0,{{0,1,0},{},{1,1,1}}},{1,{{1,1,0},{0,.5f,0},{1,1,1}}}}}}};AnimationPose3D pose;SampleAnimationClipInto(skeleton,clip,.25f,pose);const auto* localData=pose.localTransforms.data();const auto* globalData=pose.globalMatrices.data();constexpr int PoseIterations=10000;const auto poseStart=std::chrono::steady_clock::now();for(int iteration=0;iteration<PoseIterations;++iteration)SampleAnimationClipInto(skeleton,clip,float(iteration)*.001f,pose);const uint64_t poseAverageNanoseconds=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-poseStart).count())/PoseIterations;
    if(pose.localTransforms.data()!=localData||pose.globalMatrices.data()!=globalData||pose.localTransforms.size()!=6||poseAverageNanoseconds>100000){std::cerr<<"Reusable animation pose budget failed\n";return 42;}
    std::cout<<"lightCandidates="<<LightCount<<" lightSelectionAverageNs="<<lightAverageNanoseconds<<" animationJoints="<<pose.localTransforms.size()<<" animationAverageNs="<<poseAverageNanoseconds<<'\n';return 0;
}

int ValidateWorldRuntime3D() {
    const Mesh3D cylinder = Mesh3D::CreateUnitCylinder(12), cone = Mesh3D::CreateUnitCone(12);
    if (cylinder.vertices.size() != 26 || cylinder.indices.size() != 144 || cone.vertices.size() != 14 || cone.indices.size() != 72) {
        std::cerr << "Procedural vegetation mesh topology failed\n";
        return 48;
    }
    HeightField3D naturalA({{-32, 0, -32}, 33, 33, 2}), naturalB({{-32, 0, -32}, 33, 33, 2});
    naturalA.GenerateNatural(0x4a17u, {18, .014f, .6f, 20, 6});naturalB.GenerateNatural(0x4a17u, {18, .014f, .6f, 20, 6});
    const Mesh3D naturalMesh = naturalA.CreateMesh({{.08f,.2f,.07f,1},{.38f,.48f,.24f,1},{.3f,.28f,.25f,1},.76f,{.24f,.16f,.08f,1},0,4});
    if (naturalA.MaximumHeight() - naturalA.MinimumHeight() < 8.f || std::abs(naturalA.Height(17, 19) - naturalB.Height(17, 19)) > .0001f || naturalMesh.vertices.size() != 1089 || naturalMesh.indices.size() != 6144) {
        std::cerr << "Natural height-field generation failed\n";
        return 49;
    }
    WorldStreamer3D streamer({32, 2, 3, 4, 8, 0x4a17u});
    WorldStreamingStats3D streamingStats{};
    for (int update = 0; update < 7; ++update) streamingStats = streamer.Update({0, 0, 0});
    if (streamingStats.residentChunks != 25 || streamingStats.queuedChunks != 0 || streamingStats.loadScratchCapacity < 25 || streamingStats.unloadScratchCapacity < 49) {
        std::cerr << "World streaming scratch reservation failed\n";
        return 43;
    }
    const std::size_t loadCapacity = streamingStats.loadScratchCapacity, unloadCapacity = streamingStats.unloadScratchCapacity;
    constexpr int StreamingIterations = 10000;
    const auto streamingStart = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < StreamingIterations; ++iteration) streamingStats = streamer.Update({float(iteration % 31), 0, float((iteration * 7) % 31)});
    const uint64_t streamingAverageNanoseconds = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - streamingStart).count()) / StreamingIterations;
    if (streamingStats.loadScratchCapacity != loadCapacity || streamingStats.unloadScratchCapacity != unloadCapacity || streamingAverageNanoseconds > 250000) {
        std::cerr << "Reusable world streaming budget failed\n";
        return 44;
    }

    NavigationGrid3D grid({{-16, 0, -16}, 32, 32, 1, 2048, true});
    for (int z = 0; z < 32; ++z) if (z != 15) grid.SetBlocked({16, z});
    NavigationQueryWorkspace3D workspace;
    std::vector<Vec3> path;
    NavigationPathStats3D pathStats = grid.FindPathInto({-14, 0, -14}, {14, 0, 14}, workspace, path);
    if (!pathStats.reachedGoal || path.empty()) {
        std::cerr << "Reusable navigation path failed\n";
        return 45;
    }
    const NavigationWorkspaceStats3D warmStats = workspace.Stats();
    const std::size_t pathCapacity = path.capacity();
    constexpr int NavigationIterations = 1000;
    const auto navigationStart = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < NavigationIterations; ++iteration) {
        const float offset = float(iteration % 12);
        pathStats = grid.FindPathInto({-14, 0, -14 + offset}, {14, 0, 14 - offset}, workspace, path);
        if (!pathStats.reachedGoal) {
            std::cerr << "Repeated navigation query failed\n";
            return 46;
        }
    }
    const uint64_t navigationAverageNanoseconds = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - navigationStart).count()) / NavigationIterations;
    const NavigationWorkspaceStats3D reusedStats = workspace.Stats();
    if (reusedStats.cellCapacity != warmStats.cellCapacity || reusedStats.openCapacity != warmStats.openCapacity || reusedStats.rawCapacity != warmStats.rawCapacity || reusedStats.simplifiedCapacity != warmStats.simplifiedCapacity || path.capacity() != pathCapacity || navigationAverageNanoseconds > 1000000) {
        std::cerr << "Reusable navigation workspace budget failed\n";
        return 47;
    }
    std::cout << "streamResident=" << streamingStats.residentChunks << " streamLoadCapacity=" << loadCapacity << " streamUnloadCapacity=" << unloadCapacity << " streamAverageNs=" << streamingAverageNanoseconds << " navCellCapacity=" << reusedStats.cellCapacity << " navOpenCapacity=" << reusedStats.openCapacity << " navAverageNs=" << navigationAverageNanoseconds << '\n';
    return 0;
}

int ValidateSpatial2D() {
    const Vec2 points[] = {{0, 0}, {30, 0}, {30, 40}};
    PolylinePath2D path;
    path.SetPoints(points);
    const PathSample2D middle = path.Sample(.5f);
    const PathSample2D closest = path.ClosestPoint({24, 17});
    if (std::abs(path.Length() - 70.f) > .001f || path.SegmentCount() != 2 || Distance(middle.position, {30, 5}) > .001f || middle.segment != 1 || Distance(middle.tangent, {0, 1}) > .001f || Distance(closest.position, {30, 17}) > .001f || closest.segment != 1) {
        std::cerr << "Cached arc-length path queries failed\n";
        return 23;
    }
    SpatialIndex2D index(32.f);
    index.Reserve(4096);
    for (std::size_t item = 0; item < 4096; ++item) index.Insert(item + 1, item, {float(item % 64) * 8.f, float(item / 64) * 8.f}, item == 0 ? 3.f : 0.f, item % 2 ? 1u : 2u);
    std::vector<SpatialHit2D> hits;
    index.QueryCircle({0, 0}, 1.f, 2u, hits);
    if (hits.size() != 1 || hits.front().id != 1 || !index.Find(1) || index.Find(1)->userIndex != 0) {
        std::cerr << "Spatial radius and stable lookup failed\n";
        return 24;
    }
    index.QueryCircle({128, 128}, 12.f, 1u, hits);
    const auto nearest = index.Nearest({129, 129}, 20.f);
    if (hits.empty() || !nearest || nearest->distanceSquared > 3.f) {
        std::cerr << "Masked spatial query failed\n";
        return 25;
    }
    constexpr int Iterations = 10000;
    const auto start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < Iterations; ++iteration) index.QueryCircle({float(iteration % 512), float((iteration * 7) % 512)}, 48.f, 0xffffffffu, hits);
    const uint64_t averageNanoseconds = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()) / Iterations;
    const SpatialIndex2DStats stats = index.Stats();
    if (averageNanoseconds > 250000 || stats.candidates >= uint64_t(Iterations) * 4096u) {
        std::cerr << "Spatial query budget failed\n";
        return 26;
    }
    std::cout << "pathLength=" << path.Length() << " spatialEntries=" << stats.entries << " spatialCells=" << stats.occupiedCells << " spatialQueries=" << stats.queries << " spatialCandidates=" << stats.candidates << " spatialAverageNs=" << averageNanoseconds << '\n';
    return 0;
}

int ValidateClipCulling2D() {
    DrawList transformed;
    transformed.SetTransform2D({{10,20},{2,2},{},1.570796327f});
    transformed.Triangle({1,0},{2,0},{1,1},Cyan);
    if (transformed.vertices.size() != 3 || std::abs(transformed.vertices.front().x - 10.f) > .001f || std::abs(transformed.vertices.front().y - 22.f) > .001f) {
        std::cerr << "DrawList affine transform failed\n";
        return 31;
    }
    DrawList clipped;
    clipped.SetClipRect({0, 0}, {100, 100});
    constexpr int PrimitiveCount = 20000;
    const auto start = std::chrono::steady_clock::now();
    for (int primitive = 0; primitive < PrimitiveCount; ++primitive) clipped.Quad(primitive % 2 ? Vec2{50, 50} : Vec2{-50, -50}, {20, 20}, Cyan);
    const uint64_t elapsedMicroseconds = uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
    DrawListStats2D stats = clipped.Stats2D();
    if (stats.inputTriangles != PrimitiveCount * 2u || stats.submittedTriangles != PrimitiveCount || stats.culledTriangles != PrimitiveCount || clipped.vertices.size() != PrimitiveCount * 3u || elapsedMicroseconds > 20000) {
        std::cerr << "Canvas-aware triangle culling failed\n";
        return 27;
    }
    clipped.SetTranslation({-200, 0});
    clipped.Triangle({10, 10}, {20, 10}, {10, 20}, Cyan);
    if (clipped.Stats2D().culledTriangles != PrimitiveCount + 1u) {
        std::cerr << "Translated triangle culling failed\n";
        return 28;
    }
    const uint64_t totalCulled = clipped.Stats2D().totalCulledTriangles;
    clipped.Clear();
    if (clipped.ClipEnabled() || clipped.Stats2D().inputTriangles || clipped.Stats2D().totalCulledTriangles != totalCulled) {
        std::cerr << "DrawList frame telemetry reset failed\n";
        return 29;
    }
    std::cout << "clipInput=" << stats.inputTriangles << " clipSubmitted=" << stats.submittedTriangles << " clipCulled=" << stats.culledTriangles << " clipElapsedUs=" << elapsedMicroseconds << '\n';
    return 0;
}

int ValidateTextGeometry2D() {
    DrawList textDraw;
    textDraw.Text({0,0},"ENGINE TELEMETRY 12345",2.f,Cyan);
    const DrawListStats2D asciiStats=textDraw.Stats2D();
    const Vec2 asciiSize=textDraw.MeasureText("AB",2.f);
    if(asciiStats.textGlyphs!=22||!asciiStats.textSpans||asciiStats.textSpans>=asciiStats.textSourcePixels||textDraw.vertices.size()!=asciiStats.textSpans*6||std::abs(asciiSize.x-24.f)>.001f||std::abs(asciiSize.y-14.f)>.001f){std::cerr<<"ASCII span text geometry failed\n";return 32;}
    textDraw.Clear();textDraw.Text({0,0},"繁體中文",2.f,Cyan);const DrawListStats2D unicodeStats=textDraw.Stats2D();const Vec2 unicodeSize=textDraw.MeasureText("繁體中文",2.f);if(unicodeStats.textGlyphs!=4||unicodeStats.textSpans>unicodeStats.textSourcePixels||textDraw.vertices.size()!=unicodeStats.textSpans*6||unicodeSize.x<=0||unicodeSize.y<=0){std::cerr<<"Unicode span text geometry failed\n";return 33;}
    constexpr int Iterations=10000;float measuredWidth=0;const auto start=std::chrono::steady_clock::now();for(int iteration=0;iteration<Iterations;++iteration)measuredWidth+=textDraw.MeasureText("裂隙守望者 RIFTWARDEN",2.f).x;const uint64_t averageNanoseconds=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count())/Iterations;if(averageNanoseconds>50000||measuredWidth<=0){std::cerr<<"Cached UTF-8 measurement budget failed\n";return 34;}
    const size_t saved=asciiStats.textSourcePixels-asciiStats.textSpans;std::cout<<"textPixels="<<asciiStats.textSourcePixels<<" textSpans="<<asciiStats.textSpans<<" textSaved="<<saved<<" unicodePixels="<<unicodeStats.textSourcePixels<<" unicodeSpans="<<unicodeStats.textSpans<<" measureAverageNs="<<averageNanoseconds<<'\n';return 0;
}

int main() {
    if (const int sceneRuntimeResult = ValidateSceneRuntime2D()) return sceneRuntimeResult;
    if (const int runtimeSelectionResult = ValidateRuntimeSelection3D()) return runtimeSelectionResult;
    if (const int worldRuntimeResult = ValidateWorldRuntime3D()) return worldRuntimeResult;
    if (const int spatialResult = ValidateSpatial2D()) return spatialResult;
    if (const int clipResult = ValidateClipCulling2D()) return clipResult;
    if (const int textResult = ValidateTextGeometry2D()) return textResult;
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
    draw.SetAtmosphere({{.2f,.3f,.4f,.8f},{1.f,2.f,3.f},25.f,20.f,true});
    draw.AddPointLight({{1,2,3},0,{1,.5f,.2f,1},2});
    if (!draw.Atmosphere().enabled || std::abs(draw.Atmosphere().fogEnd - 25.001f) > .0001f) {
        std::cerr << "Atmosphere range must remain valid\n";
        return 16;
    }
    draw.SetTranslation({7.f, -3.f});
    draw.Triangle({1.f, 2.f}, {3.f, 4.f}, {5.f, 6.f}, {1.f, 1.f, 1.f, 1.f});
    if (draw.vertices[0].x != 8.f || draw.vertices[0].y != -1.f || draw.vertices[2].x != 12.f || draw.vertices[2].y != 3.f || draw.vertices[0].color != 0xffffffffu) {
        std::cerr << "DrawList translation must affect every triangle vertex\n";
        return 3;
    }
    draw.Clear();
    if (draw.Translation().x != 0.f || draw.Translation().y != 0.f || draw.Atmosphere().enabled || !draw.PointLights().empty()) {
        std::cerr << "DrawList clear must reset transient scene state\n";
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
    const std::size_t shadowStart=draw.vertices.size();draw.ProjectedEllipseShadow({10,20},8,3,{1,0},5,.35f,18);if(draw.vertices.size()!=shadowStart+54||std::abs(draw.vertices[shadowStart].x-15.f)>.001f||std::abs(draw.vertices[shadowStart].y-20.f)>.001f){std::cerr<<"Projected 2.5D ellipse shadow failed\n";return 17;}
    ParticleBurst2D particleSettings{};particleSettings.minimumSpeed=particleSettings.maximumSpeed=40;particleSettings.minimumLife=particleSettings.maximumLife=.05f;particleSettings.minimumSize=particleSettings.maximumSize=3;particleSettings.gravity=0;particleSettings.drag=0;particleSettings.shape=ParticleShape2D::Circle;ParticleSystem2D particleA(4),particleB(4);particleA.EmitBurst({20,30},Cyan,6,particleSettings,42);particleB.EmitBurst({20,30},Cyan,6,particleSettings,42);DrawList particleDrawA,particleDrawB;particleA.Render(particleDrawA);particleB.Render(particleDrawB);const ParticleSystem2DStats particleStats=particleA.Stats();if(particleA.ActiveCount()!=4||particleStats.emitted!=4||particleStats.dropped!=2||particleStats.peakActive!=4||particleDrawA.vertices.size()!=96||particleDrawA.vertices.size()!=particleDrawB.vertices.size()||particleDrawA.vertices.front().x!=particleDrawB.vertices.front().x||particleDrawA.vertices.front().y!=particleDrawB.vertices.front().y){std::cerr<<"Bounded deterministic particle burst failed\n";return 19;}particleA.Update(.1f);if(particleA.ActiveCount()!=0||particleA.Stats().recycled!=4){std::cerr<<"Particle recycling failed\n";return 20;}ScreenShake2D shake;shake.AddImpulse(8,.4f);shake.Update(.2f);if(std::abs(shake.Amplitude()-4.f)>.001f||!std::isfinite(shake.Offset().x)){std::cerr<<"Screen shake impulse failed\n";return 21;}shake.Update(.2f);if(shake.Active()||shake.Amplitude()!=0){std::cerr<<"Screen shake decay failed\n";return 22;}
    draw.WorldTriangle({-.5f,-.5f,.5f},{0,.5f,.5f},{.5f,-.5f,.5f},{.2f,.8f,.7f,1.f});
    if(draw.worldIndices.size()!=3||draw.worldIndices[2]!=2){std::cerr<<"World triangle indices failed\n";return 12;}
    draw.SetAtmosphere({{.2f,.3f,.4f,.8f},{0,0,0},20,80,true});draw.AddPointLight({{2,3,4},10,{1,.4f,.2f,1},2});draw.AddPointLight({{-2,2,3},8,{.2f,.6f,1,1},1.5f});draw.AddPointLight({{0,4,8},12,{.5f,1,.4f,1},1});draw.AddPointLight({{20,2,20},-4,{1,1,1,1},.5f});draw.AddPointLight({{},5,{1,1,1,1},0});if(draw.PointLights().size()!=4||draw.PointLights().back().radius<.001f){std::cerr<<"Point light validation failed\n";return 18;}
    draw.SetClipRect({0,0},{1280,720});const std::size_t clippedVertexCount=draw.vertices.size();draw.Triangle({-30,-30},{-20,-30},{-30,-20},Cyan);if(draw.vertices.size()!=clippedVertexCount||draw.Stats2D().culledTriangles!=1){std::cerr<<"Renderer clip telemetry setup failed\n";return 30;}

    HWND window = CreateWindowExW(0, L"STATIC", L"AevryndRendererSmoke", WS_OVERLAPPED,
                                  0, 0, 1280, 720, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) {
        std::cerr << "Hidden renderer window creation failed\n";
        return 9;
    }
    int rendererResult = 0;
    try {
        D3D12Renderer renderer;
        renderer.Initialize(window, 1280, 720);
        if (renderer.DLSSOn()) renderer.ToggleDLSS();
        Mesh3D box=Mesh3D::CreateUnitBox();Mesh3D invalid=box;invalid.indices.back()=999;if(renderer.CreateMesh(invalid.vertices,invalid.indices)){std::cerr<<"Invalid resident mesh was accepted\n";rendererResult=13;}
        const MeshHandle3D boxHandle=renderer.CreateMesh(box.vertices,box.indices);const uint32_t forwardNormal=PackNormalSnorm8({0,0,-1});const std::array<Vertex3D,3> triangleVertices={Vertex3D{-.5f,-.5f,0,forwardNormal,0xffffffffu},Vertex3D{0,.5f,0,forwardNormal,0xffffffffu},Vertex3D{.5f,-.5f,0,forwardNormal,0xffffffffu}};const std::array<uint32_t,3> triangleIndices={0,1,2};const MeshHandle3D triangleHandle=renderer.CreateMesh(triangleVertices,triangleIndices);if(!boxHandle||!triangleHandle){std::cerr<<"Resident mesh creation failed\n";rendererResult=14;}else{draw.MeshInstance(boxHandle,CreateTransformMatrix({-1,0,3},{},{1,2,1}),{.4f,.8f,1,1});draw.MeshInstance(boxHandle,CreateTransformMatrix({1,0,3},{},{1,1,1}),{1,.5f,.2f,1});draw.MeshInstance(triangleHandle,CreateTransformMatrix({0,1,2},{},{1,1,1}),{.7f,1,.4f,1});draw.MeshInstance(triangleHandle,CreateDirectionalShadowMatrix({.4f,-1,.2f},.01f)*CreateTransformMatrix({0,1,2},{},{1,1,1}),{0,0,0,.25f},{1,1,0,1},MeshRenderLayer3D::Overlay);}
        uint64_t expectedFrames = 4;
        for (int frame = 0; frame < 4; ++frame) renderer.Render(draw, {.02f, .04f, .05f, 1.f});
        if (renderer.SupportsDLSS()) {
            renderer.ToggleDLSS();
            for (int frame = 0; frame < 4; ++frame) renderer.Render(draw, {.02f, .04f, .05f, 1.f});
            expectedFrames += 4;
        }
        renderer.WaitIdle();
        const RendererStats stats = renderer.GetStats();
        if (stats.submittedFrames != expectedFrames || stats.lastSubmittedVertices != draw.vertices.size() ||
            stats.lastSubmittedWorldVertices != draw.worldVertices.size() ||
            stats.lastSubmittedWorldIndices != draw.worldIndices.size() ||
            stats.lastUploadBytes != draw.vertices.size() * sizeof(Vertex) + draw.worldVertices.size() * sizeof(Vertex3D) + draw.worldIndices.size() * sizeof(uint32_t) + draw.meshInstances.size() * D3D12Renderer::MeshInstanceUploadBytes ||
            stats.peakSubmittedVertices < draw.vertices.size() || stats.peakSubmittedWorldVertices < draw.worldVertices.size() || stats.peakSubmittedWorldIndices < draw.worldIndices.size() ||
            stats.lastSubmittedMeshInstances != 4 || stats.lastSubmittedOverlayInstances != 1 || stats.lastSubmittedPointLights != D3D12Renderer::MaxPointLights || stats.lastPointLightCandidates != 4 || stats.totalDroppedPointLights != expectedFrames || stats.peakSubmittedMeshInstances < 4 || stats.totalDroppedMeshInstances != 0 || stats.residentMeshes != 2 || stats.residentMeshBytes != 696 || !stats.lastBackfaceCulling || stats.lastInputTriangles2D != draw.Stats2D().inputTriangles || stats.lastCulledTriangles2D != draw.Stats2D().culledTriangles || stats.totalCulledTriangles2D != draw.Stats2D().culledTriangles*expectedFrames ||
            stats.totalDroppedVertices != 0 || stats.totalDroppedWorldVertices != 0 || stats.totalDroppedWorldIndices != 0 || stats.lastDrawCalls != 5 ||
            stats.totalResourceWaitNanoseconds < stats.peakResourceWaitNanoseconds || stats.totalValidationWaitNanoseconds < stats.peakValidationWaitNanoseconds || stats.gpuTimingSamples != expectedFrames || !stats.lastGpuFrameNanoseconds || stats.totalGpuFrameNanoseconds < stats.peakGpuFrameNanoseconds ||
            (stats.resourceWaits == 0 && (stats.lastResourceWaitNanoseconds != 0 || stats.totalResourceWaitNanoseconds != 0)) ||
            (stats.validationWaits == 0 && (stats.lastValidationWaitNanoseconds != 0 || stats.totalValidationWaitNanoseconds != 0))) {
            std::cerr << "Renderer telemetry did not match submitted GPU work\n";
            rendererResult = 11;
        }
        const uint64_t averageResourceWaitUs=stats.resourceWaits?stats.totalResourceWaitNanoseconds/stats.resourceWaits/1000:0;
        const uint64_t averageValidationWaitUs=stats.validationWaits?stats.totalValidationWaitNanoseconds/stats.validationWaits/1000:0;
        const uint64_t averageGpuUs=stats.gpuTimingSamples?stats.totalGpuFrameNanoseconds/stats.gpuTimingSamples/1000:0;
        std::cout<<"frames="<<stats.submittedFrames<<" resourceWaits="<<stats.resourceWaits<<" resourceAverageUs="<<averageResourceWaitUs<<" resourcePeakUs="<<stats.peakResourceWaitNanoseconds/1000<<" validationWaits="<<stats.validationWaits<<" validationAverageUs="<<averageValidationWaitUs<<" validationPeakUs="<<stats.peakValidationWaitNanoseconds/1000<<" gpuSamples="<<stats.gpuTimingSamples<<" gpuAverageUs="<<averageGpuUs<<" gpuPeakUs="<<stats.peakGpuFrameNanoseconds/1000<<'\n';
        renderer.DestroyMesh(boxHandle);renderer.DestroyMesh(triangleHandle);renderer.Render(draw,{.02f,.04f,.05f,1.f});renderer.WaitIdle();const RendererStats destroyedStats=renderer.GetStats();if(destroyedStats.residentMeshes!=0||destroyedStats.residentMeshBytes!=0||destroyedStats.lastSubmittedMeshInstances!=0||destroyedStats.lastSubmittedOverlayInstances!=0||destroyedStats.totalDroppedMeshInstances!=draw.meshInstances.size()||destroyedStats.lastDrawCalls!=2){std::cerr<<"Resident mesh destruction failed\n";rendererResult=15;}
    } catch (const std::exception& error) {
        std::cerr << "D3D12 renderer smoke test failed: " << error.what() << '\n';
        rendererResult = 10;
    }
    DestroyWindow(window);
    if (rendererResult) return rendererResult;
    return 0;
}
