#include <Aevrynd/Renderer.h>
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/Scene2D.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
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
    draw.SetAtmosphere({{.2f,.3f,.4f,.8f},{1.f,2.f,3.f},25.f,20.f,true});
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
    if (draw.Translation().x != 0.f || draw.Translation().y != 0.f || draw.Atmosphere().enabled) {
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
    draw.WorldTriangle({-.5f,-.5f,.5f},{0,.5f,.5f},{.5f,-.5f,.5f},{.2f,.8f,.7f,1.f});
    if(draw.worldIndices.size()!=3||draw.worldIndices[2]!=2){std::cerr<<"World triangle indices failed\n";return 12;}

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
            stats.lastSubmittedMeshInstances != 4 || stats.lastSubmittedOverlayInstances != 1 || stats.peakSubmittedMeshInstances < 4 || stats.totalDroppedMeshInstances != 0 || stats.residentMeshes != 2 || stats.residentMeshBytes != 696 || !stats.lastBackfaceCulling ||
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
