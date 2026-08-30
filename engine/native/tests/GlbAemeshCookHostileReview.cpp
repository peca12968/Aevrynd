#include <Aevrynd/StaticMeshAssetLibrary3D.h>

#include <array>
#include <exception>
#include <iostream>
#include <memory>

using namespace Aevrynd;
namespace {
int Result{};

struct Chain {
  const char *name;
  const char *lod0;
  const char *lod1;
  const char *lod2;
};

constexpr std::array Chains{
    Chain{"boulder", "boulder-01/boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh", "boulder-01/boulder-01-lod1-node0-mesh0-primitive0-material0-boulder-01.aemesh", "boulder-01/boulder-01-lod2-node0-mesh0-primitive0-material0-boulder-01.aemesh"},
    Chain{"cliff", "coastal-cliff-01/coastal-cliff-01-lod0-node0-mesh0-primitive0-material0-coastal-cliff-01.aemesh", "coastal-cliff-01/coastal-cliff-01-lod1-node0-mesh0-primitive0-material0-coastal-cliff-01.aemesh", "coastal-cliff-01/coastal-cliff-01-lod2-node0-mesh0-primitive0-material0-coastal-cliff-01.aemesh"},
    Chain{"dead-tree", "dead-tree-trunk-02/dead-tree-trunk-02-lod0-node0-mesh0-primitive0-material0-dead-tree-trunk-02.aemesh", "dead-tree-trunk-02/dead-tree-trunk-02-lod1-node0-mesh0-primitive0-material0-dead-tree-trunk-02.aemesh", "dead-tree-trunk-02/dead-tree-trunk-02-lod2-node0-mesh0-primitive0-material0-dead-tree-trunk-02.aemesh"},
    Chain{"island-trunk", "island-tree-01/island-tree-01-lod0-node0-mesh0-primitive0-material0-island-tree-01.aemesh", "island-tree-01/island-tree-01-lod1-node0-mesh0-primitive0-material0-island-tree-01.aemesh", "island-tree-01/island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh"},
    Chain{"island-leaves", "island-tree-01/island-tree-01-lod0-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh", "island-tree-01/island-tree-01-lod1-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh", "island-tree-01/island-tree-01-lod2-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh"},
    Chain{"island-branches", "island-tree-01/island-tree-01-lod0-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh", "island-tree-01/island-tree-01-lod1-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh", "island-tree-01/island-tree-01-lod2-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh"}};

class ReviewGame final : public IGame {
public:
  void OnStart(Engine &engine) override {
    const std::filesystem::path root = std::filesystem::current_path();
    const auto meshRoot = root / "engine/assets/realistic-cc0-first-batch/cooked/meshes";
    if (!library_.Initialize(engine, registry_, root)) return Fail(engine, "initialize");
    std::vector<StaticMeshAssetDefinition3D> definitions;
    for (const auto &chain : Chains) {
      StaticMeshAssetDefinition3D value{};
      value.name = chain.name;
      value.meshPath = std::filesystem::relative(meshRoot / chain.lod0, root);
      value.generateLods = false;
      value.importedLodCount = 2;
      value.importedLods[0].meshPath = std::filesystem::relative(meshRoot / chain.lod1, root);
      value.importedLods[1].meshPath = std::filesystem::relative(meshRoot / chain.lod2, root);
      value.cpuRetention = MeshLodCpuRetention3D::AllLevels;
      value.material.name = std::string(chain.name) + "-review";
      value.material.textures = {"engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg", "engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg", 512, "engine/assets/material-packs/ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg"};
      definitions.push_back(std::move(value));
    }
    if (!library_.LoadAll(definitions) || library_.Stats().meshLevels != 18) return Fail(engine, "load18");
    for (std::size_t index = 0; index < Chains.size(); ++index) {
      if (library_.LevelCount(Chains[index].name) != 3 || !library_.Instantiate(Chains[index].name, {{float(index) * 40.f, 0, 0}, {}, {1, 1, 1}})) return Fail(engine, "lod-or-instance");
    }
    Scene3D scene; DrawList draw;
    if (registry_.Collect(scene, draw).submittedLodRenderables != Chains.size()) return Fail(engine, "collect");
    const auto loaded = LoadAevryndMeshFile(meshRoot / Chains[0].lod0);
    if (!loaded || loaded.mesh.indices.size() < 3) return Fail(engine, "cpu-load");
    const auto point = [&](std::size_t item) { const auto &v = loaded.mesh.vertices[loaded.mesh.indices[item]]; return Vec3{v.x, v.y, v.z}; };
    const Vec3 a = point(0), b = point(1), c = point(2), normal = Normalize(Cross(b - a, c - a)), center = (a + b + c) * (1.f / 3.f);
    SceneQueryStats3D query{};
    if (Length(normal) < .5f || !scene.Raycast(MakeRay3D(center + normal * 2.f, normal * -1.f), 4.f, &query) || !query.trianglesTested) return Fail(engine, "ray");
    draw.Clear(); scene.Clear();
    if (library_.ClearInstances() != Chains.size()) return Fail(engine, "clear");
    for (const auto &chain : Chains) if (!library_.Unload(chain.name)) return Fail(engine, "unload");
    if (library_.Stats().assets || engine.GetRendererStats().residentMeshes) return Fail(engine, "resident0");
    passed_ = true; engine.Quit();
  }
  void OnUpdate(Engine &engine, float) override { if (Result) engine.Quit(); }
  void OnRender(Engine &, float) override {}
  void OnStop(Engine &) override { library_.Shutdown(); if (passed_) std::cout << "GLB_AEMESH_ENGINE_REVIEW=PASS files=18 chains=6 ray=true resident=0\n"; }
private:
  static void Fail(Engine &engine, const char *message) { std::cerr << message << '\n'; Result = 1; engine.Quit(); }
  StaticMeshAssetLibrary3D library_{}; SceneECSRegistry3D registry_{}; bool passed_{};
};
}

int main() {
  try { Engine engine({L"GLB AEMESH Hostile Review", 320, 180, 1.f / 60.f, false, false, false}); engine.Run(std::make_unique<ReviewGame>(), GetModuleHandleW(nullptr), SW_HIDE); }
  catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 2; }
  return Result;
}
