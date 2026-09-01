#include <Aevrynd/Camera.h>
#include <Aevrynd/Mesh3D.h>
#include <Aevrynd/Renderer.h>
#include <array>
#include <filesystem>
#include <iostream>

using namespace Aevrynd;

namespace {
struct Fixture {
    MeshHandle3D mesh{}, alternateMesh{}, dummyMesh{};
    SkinnedMeshHandle3D skinnedMesh{}, dummySkinnedMesh{};
    MaterialTextureSetHandle3D material{}, alternateMaterial{}, dummyMaterial{};
    SkeletonCompatibilityId3D skeleton{0x534841444f574649ull};
};

Fixture CreateFixture(D3D12Renderer& renderer, bool perturbedAllocationOrder) {
    Fixture fixture;
    const Mesh3D box = Mesh3D::CreateUnitBox();
    const Mesh3D pyramid = Mesh3D::CreateUnitPyramid();
    const Mesh3D plane = Mesh3D::CreateUnitCylinder(7);
    const auto createDummyMesh = [&] { fixture.dummyMesh = renderer.CreateMesh(plane.vertices, plane.indices); };
    if (perturbedAllocationOrder) createDummyMesh();
    fixture.mesh = renderer.CreateMesh(box.vertices, box.indices);
    fixture.alternateMesh = renderer.CreateMesh(pyramid.vertices, pyramid.indices);
    if (!perturbedAllocationOrder) createDummyMesh();

    std::array<SkinnedVertex3D, 3> skinned{};
    for (auto& vertex : skinned) {
        vertex.normal = PackNormalSnorm8({0, 1, 0});
        vertex.color = 0xffffffffu;
        vertex.weights[0] = 65535;
    }
    skinned[0].x = -.5f;
    skinned[1].x = .5f;
    skinned[2].y = 1.f;
    const std::array<uint32_t, 3> indices{0, 1, 2};
    const auto createDummySkinned = [&] {
        fixture.dummySkinnedMesh = renderer.CreateSkinnedMesh(skinned, indices, 1, {fixture.skeleton.value + 1});
    };
    if (perturbedAllocationOrder) createDummySkinned();
    fixture.skinnedMesh = renderer.CreateSkinnedMesh(skinned, indices, 1, fixture.skeleton);
    if (!perturbedAllocationOrder) createDummySkinned();

    const auto materialRoot = std::filesystem::path{AEVRYND_SOURCE_DIR} / "engine" / "assets" / "material-packs" / "ambientcg";
    const auto definition = [&](const std::filesystem::path& root, const char* prefix, unsigned resolution) {
        return MaterialTextureSetDefinition3D{root / (std::string{prefix} + "_Color.jpg"), root / (std::string{prefix} + "_Roughness.jpg"), resolution, root / (std::string{prefix} + "_NormalDX.jpg")};
    };
    const auto groundRoot = materialRoot / "ground037" / "1k";
    const auto woodRoot = materialRoot / "woodsiding006" / "1k";
    const auto createDummyMaterial = [&] { fixture.dummyMaterial = renderer.CreateMaterialTextureSet(definition(groundRoot, "Ground037_1K-JPG", 32)); };
    if (perturbedAllocationOrder) createDummyMaterial();
    fixture.material = renderer.CreateMaterialTextureSet(definition(groundRoot, "Ground037_1K-JPG", 64));
    fixture.alternateMaterial = renderer.CreateMaterialTextureSet(definition(woodRoot, "WoodSiding006_1K-JPG", 64));
    if (!perturbedAllocationOrder) createDummyMaterial();
    return fixture;
}

DrawList CreateDraw(const Fixture& fixture) {
    const Camera3D camera{{4, 5, -8}, {0, .5f, 4}, {0, 1, 0}, {ProjectionMode::Perspective, .9f, 20.f, .1f, 120.f}};
    DrawList draw;
    draw.SetViewProjection(CreateViewProjectionMatrix(camera, 16.f / 9.f));
    draw.SetDirectionalLight({{.45f, -1.f, .25f}, .1f, {1, .95f, .85f, 1}, 2.f});
    draw.SetDirectionalShadowCascades({4, 512, .1f, 100.f, .65f, .08f});
    draw.MeshInstance(fixture.mesh, CreateTranslationMatrix({-1, 0, 5}), {1, 1, 1, 1}, {.4f, .2f, 0, 1}, MeshRenderLayer3D::Opaque, fixture.material, .5f, true, MaterialClass3D::Surface, 5.f, true, 1, 0, false, DirectionalShadowAllCascades3D, ShadowCascadeSelection3D::Manual, Matrix4::Identity(), false, 1.f, MaterialAlphaMode3D::Opaque, .5f, false, {{0x1100, 0x2200}, 7});
    SkinPaletteJoint3D joint{};
    joint.skin = Matrix4::Identity();
    joint.normalRows = {Vec4{1, 0, 0, 0}, Vec4{0, 1, 0, 0}, Vec4{0, 0, 1, 0}};
    const auto palette = draw.AddSkinPalette(std::span<const SkinPaletteJoint3D>{&joint, 1}, fixture.skeleton);
    draw.SkinnedMeshInstance(fixture.skinnedMesh, palette, CreateTranslationMatrix({1, 0, 5}), {1, 1, 1, 1}, {.3f, .1f, 0, 1}, fixture.material, .5f, true, MaterialClass3D::Surface, true, DirectionalShadowAllCascades3D, 0, 0, CharacterShadingModel3D::Skin, MaterialAlphaMode3D::Opaque, .5f, false, {}, Matrix4::Identity(), false, 1.f, .5f, .25f, 0, {{0x3300, 0x4400}, 11});
    draw.WorldTriangle({-2, -.5f, 3}, {2, -.5f, 3}, {0, -.5f, 8}, {1, 1, 1, 1});
    return draw;
}

std::array<uint64_t, MaximumDirectionalShadowCascades3D> RenderFingerprint(D3D12Renderer& renderer, const DrawList& draw) {
    renderer.Render(draw, {.01f, .02f, .03f, 1});
    return renderer.GetStats().lastDirectionalShadowCascadeFingerprints;
}

bool AllDifferent(const std::array<uint64_t, MaximumDirectionalShadowCascades3D>& left,
                  const std::array<uint64_t, MaximumDirectionalShadowCascades3D>& right) {
    for (size_t cascade = 0; cascade < MaximumDirectionalShadowCascades3D; ++cascade)
        if (left[cascade] == right[cascade]) return false;
    return true;
}

void DestroyFixture(D3D12Renderer& renderer, const Fixture& fixture) {
    renderer.DestroyMaterialTextureSet(fixture.dummyMaterial);
    renderer.DestroyMaterialTextureSet(fixture.alternateMaterial);
    renderer.DestroyMaterialTextureSet(fixture.material);
    renderer.DestroySkinnedMesh(fixture.dummySkinnedMesh);
    renderer.DestroySkinnedMesh(fixture.skinnedMesh);
    renderer.DestroyMesh(fixture.dummyMesh);
    renderer.DestroyMesh(fixture.alternateMesh);
    renderer.DestroyMesh(fixture.mesh);
    renderer.WaitIdle();
}
}

int main() {
    HWND window = CreateWindowExW(0, L"STATIC", L"ShadowSemanticFingerprint", WS_OVERLAPPED, 0, 0, 320, 180, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return 1;
    int result{};
    std::array<uint64_t, MaximumDirectionalShadowCascades3D> reference{};
    uint32_t referenceMeshHandle{}, referenceMaterialHandle{}, referenceSkinnedHandle{};
    uint64_t referenceSkinnedResourceIdentity{};
    try {
        {
            D3D12Renderer renderer;
            renderer.Initialize(window, 320, 180, 320, 180, false);
            renderer.SetQualitySettings({true, false, false, false});
            const Fixture fixture = CreateFixture(renderer, false);
            referenceMeshHandle = fixture.mesh.value;
            referenceMaterialHandle = fixture.material.value;
            referenceSkinnedHandle = fixture.skinnedMesh.value;
            referenceSkinnedResourceIdentity = fixture.skinnedMesh.resourceIdentity;
            DrawList draw = CreateDraw(fixture);
            reference = RenderFingerprint(renderer, draw);
            const auto stable = RenderFingerprint(renderer, draw);
            if (reference != stable || !renderer.GetStats().lastDirectionalShadowCascadeReused[1]) result = 2;

            draw.meshInstances[0].mesh = fixture.alternateMesh;
            const auto meshMutation = RenderFingerprint(renderer, draw);
            draw.meshInstances[0].mesh = fixture.mesh;
            draw.meshInstances[0].textures = fixture.alternateMaterial;
            const auto materialMutation = RenderFingerprint(renderer, draw);
            draw.meshInstances[0].textures = fixture.material;
            draw.meshInstances[0].transform = CreateTranslationMatrix({-1.25f, 0, 5});
            draw.meshInstances[0].normalTransform = CreateNormalMatrix(draw.meshInstances[0].transform);
            const auto transformMutation = RenderFingerprint(renderer, draw);
            draw.meshInstances[0].transform = CreateTranslationMatrix({-1, 0, 5});
            draw.meshInstances[0].normalTransform = CreateNormalMatrix(draw.meshInstances[0].transform);
            draw.meshInstances[0].identity.generation++;
            const auto identityMutation = RenderFingerprint(renderer, draw);
            draw.meshInstances[0].identity.generation--;
            draw.skinPaletteJoints[0].skin = CreateTranslationMatrix({.125f, 0, 0});
            const auto paletteMutation = RenderFingerprint(renderer, draw);
            draw.skinPaletteJoints[0].skin = Matrix4::Identity();
            draw.worldVertices[0].x -= .25f;
            const auto worldMutation = RenderFingerprint(renderer, draw);
            draw.worldVertices[0].x += .25f;
            draw.SetDirectionalShadowCascades({4, 512, .1f, 100.f, .73f, .08f});
            const auto cascadeMutation = RenderFingerprint(renderer, draw);
            if (!AllDifferent(reference, meshMutation) || !AllDifferent(reference, materialMutation) ||
                !AllDifferent(reference, transformMutation) || !AllDifferent(reference, identityMutation) ||
                !AllDifferent(reference, paletteMutation) || !AllDifferent(reference, worldMutation) ||
                !AllDifferent(reference, cascadeMutation)) result = 3;
            DestroyFixture(renderer, fixture);
        }
        {
            D3D12Renderer renderer;
            renderer.Initialize(window, 320, 180, 320, 180, false);
            renderer.SetQualitySettings({true, false, false, false});
            const Fixture fixture = CreateFixture(renderer, true);
            const auto reordered = RenderFingerprint(renderer, CreateDraw(fixture));
            const bool allocationActuallyChanged = fixture.mesh.value != referenceMeshHandle &&
                fixture.material.value != referenceMaterialHandle && fixture.skinnedMesh.value != referenceSkinnedHandle &&
                fixture.skinnedMesh.resourceIdentity != referenceSkinnedResourceIdentity;
            if (!allocationActuallyChanged || reference != reordered) result = 4;
            DestroyFixture(renderer, fixture);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 5;
    }
    DestroyWindow(window);
    if (!result) std::cout << "shadow_semantic_fingerprint fresh_renderers=2 allocation_orders=2 mutations=7 stable=1\n";
    return result;
}
