#include "Aevrynd/Renderer.h"

#include <filesystem>
#include <iostream>
#include <windows.h>

using namespace Aevrynd;

int main() {
    HWND window = CreateWindowExW(0, L"STATIC", L"FrameBoundDeterminismReadback", WS_OVERLAPPED,
                                  0, 0, 96, 96, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return 1;
    int result{};
    const char* stage = "setup";
    try {
        D3D12Renderer renderer;
        renderer.Initialize(window, 96, 96, 96, 96, false);
        renderer.SetQualitySettings({false, false, false, false});
        const std::array<Vertex3D, 3> vertices{{
            {-.7f, -.7f, .4f, PackNormalSnorm8({0, 0, -1}), ~0u, 0, 1},
            {0, .7f, .4f, PackNormalSnorm8({0, 0, -1}), ~0u, .5f, 0},
            {.7f, -.7f, .4f, PackNormalSnorm8({0, 0, -1}), ~0u, 1, 1}}};
        const std::array<std::uint32_t, 3> indices{{0, 1, 2}};
        const auto mesh = renderer.CreateMesh(vertices, indices);
        if (!mesh) throw std::runtime_error("mesh setup failed");
        std::array<SkinnedVertex3D, 3> skinnedVertices{};
        for (std::size_t index = 0; index < skinnedVertices.size(); ++index) {
            skinnedVertices[index].x = vertices[index].x;
            skinnedVertices[index].y = vertices[index].y;
            skinnedVertices[index].z = vertices[index].z;
            skinnedVertices[index].normal = vertices[index].normal;
            skinnedVertices[index].color = vertices[index].color;
            skinnedVertices[index].weights[0] = 65535;
        }
        const SkeletonCompatibilityId3D skinCompatibility{0x4652414d45424f55ull};
        const auto skinnedMesh = renderer.CreateSkinnedMesh(skinnedVertices, indices, 1, skinCompatibility);
        if (!skinnedMesh) throw std::runtime_error("skinned mesh setup failed");
        std::cerr << "frame-bound stage=setup mesh=" << bool(mesh) << '\n';

        DrawList sealed;
        sealed.SetViewProjection(Matrix4::Identity());
        sealed.SetBackfaceCulling(false);
        if (!sealed.SetDeterminismIntegrityMode3D(DeterminismIntegrityMode3D::SealedCapture) ||
            !sealed.MeshInstance(mesh, Matrix4::Identity(), {1, 1, 1, 1}, {.4f, 0, 0, 1},
                                 MeshRenderLayer3D::Opaque, {}, 1, false, MaterialClass3D::Surface,
                                 std::numeric_limits<float>::quiet_NaN(), false, 1, 0, false,
                                 DirectionalShadowAllCascades3D, ShadowCascadeSelection3D::Automatic,
                                 Matrix4::Identity(), false, 1, MaterialAlphaMode3D::Opaque, .5f, false,
                                 {{0xfbde000000000001ull, 1}, 1}))
            throw std::runtime_error("sealed draw setup failed");

        const auto token = renderer.BeginReadbackCaptureSequence3D({1, ReadbackCaptureSequenceJitterPolicy3D::FixedSample, 0});
        if (!token) throw std::runtime_error("sequence begin failed");
        const ObjectIdCaptureRequestMetadata3D metadata{41, 43, 47, ReadbackEmptyReason3D::None, {*token, 0}, 53};
        const std::filesystem::path capture = "test-artifacts/frame-bound-determinism/sealed.png";
        std::filesystem::create_directories(capture.parent_path());
        if (!renderer.CaptureNextFrame(capture) || !renderer.RequestObjectIdReadback3D(metadata) ||
            !renderer.RequestDepthReadback3D(metadata))
            throw std::runtime_error("paired request failed");
        stage = "sealed-render";
        std::cerr << "frame-bound stage=request token=" << token->value << " phase=" << metadata.sequence.index
                  << " draw=" << metadata.drawListRevision << '\n';
        renderer.Render(sealed, {0, 0, 0, 1});
        const auto sealedStats = renderer.GetStats();
        std::cerr << "frame-bound stage=sealed-render requested=" << unsigned(sealedStats.lastDeterminismIntegrity.requestedMode)
                  << " actual=" << unsigned(sealedStats.lastDeterminismIntegrity.actualMode)
                  << " reason=" << unsigned(sealedStats.lastDeterminismIntegrity.rejectionReason)
                  << " counts=" << sealedStats.lastDeterminismIntegrity.sampledTexturedMaterials << '/'
                  << sealedStats.lastDeterminismIntegrity.verifiedTexturedMaterials << '/'
                  << sealedStats.lastDeterminismIntegrity.rejectedTexturedMaterials
                  << " accepted=" << sealedStats.lastDeterminismIntegrity.accepted
                  << " canonical=" << sealedStats.lastCanonicalDrawFingerprint.Complete() << '\n';

        DrawList runtime = sealed;
        if (!runtime.SetDeterminismIntegrityMode3D(DeterminismIntegrityMode3D::Runtime))
            throw std::runtime_error("runtime mode failed");
        stage = "runtime-frames";
        for (unsigned frame = 0; frame < 3; ++frame) {
            renderer.Render(runtime, {0, 0, 0, 1});
            const auto stats = renderer.GetStats();
            std::cerr << "frame-bound stage=runtime frame=" << frame << " requested="
                      << unsigned(stats.lastDeterminismIntegrity.requestedMode) << " frameSerial="
                      << stats.completedGpuFrameSerial << '\n';
        }

        stage = "take";
        auto object = renderer.TakeObjectIdReadback3D();
        auto depth = renderer.TakeDepthReadback3D();
        const auto sequence = renderer.EndReadbackCaptureSequence3D(*token);
        std::cerr << "frame-bound stage=take object=" << bool(object) << " objectComplete="
                  << (object && object->Complete()) << " objectFrame=" << (object ? object->rasterProvenance.frameSerial : 0)
                  << " evidenceFrame=" << (object ? object->determinism.frameSerial : 0) << " evidenceComplete="
                  << (object && object->determinism.Complete()) << " depth=" << bool(depth) << " depthComplete="
                  << (depth && depth->Complete()) << " depthFrame=" << (depth ? depth->rasterProvenance.frameSerial : 0)
                  << " pairExact=" << (object && depth && object->determinism == depth->determinism)
                  << " sequenceStatus=" << unsigned(sequence.status) << " phases=" << sequence.receipt.phases.size()
                  << " sequenceComplete=" << bool(sequence) << '\n';
        if (!object || !depth || !object->Complete() || !depth->Complete() || !sequence ||
            object->determinism != depth->determinism ||
            object->determinism.integrity.requestedMode != DeterminismIntegrityMode3D::SealedCapture ||
            object->determinism.integrity.actualMode != DeterminismIntegrityMode3D::SealedCapture ||
            !object->determinism.integrity.accepted || object->determinism.integrity.sampledTexturedMaterials ||
            object->determinism.frameSerial != object->rasterProvenance.frameSerial ||
            !object->determinism.render.Complete() || object->determinism.render.frameSerial != object->rasterProvenance.frameSerial ||
            object->determinism.render.sceneDrawRevision != metadata.drawListRevision ||
            sequence.receipt.phases.size() != 1 ||
            sequence.receipt.phases[0].determinism != object->determinism)
            result = 2, stage = "take-contract";
        if (!result) {
            stage = "mutations";
            auto wrongFrame = *object;
            ++wrongFrame.determinism.frameSerial;
            auto wrongCanonical = *object;
            wrongCanonical.determinism.canonical.material = 0;
            auto wrongRenderFrame = *object;
            ++wrongRenderFrame.determinism.render.frameSerial;
            auto wrongSceneRevision = *object;
            wrongSceneRevision.determinism.render.sceneDrawRevision = 0;
            if (wrongFrame.Complete() || wrongCanonical.Complete() || wrongRenderFrame.Complete() || wrongSceneRevision.Complete() ||
                renderer.EndReadbackCaptureSequence3D({token->value + 1}).status != ReadbackCaptureSequenceStatus3D::TokenMismatch)
                result = 3, stage = "mutation-contract";
        }
        if (!result) {
            stage = "second-request";
            const ObjectIdCaptureRequestMetadata3D secondMetadata{59, 61, 67};
            if (!renderer.RequestObjectIdReadback3D(secondMetadata) || !renderer.RequestDepthReadback3D(secondMetadata)) {
                result = 6;
            } else {
                renderer.Render(runtime, {0, 0, 0, 1});
                const auto secondObject = renderer.TakeObjectIdReadback3D();
                const auto secondDepth = renderer.TakeDepthReadback3D();
                if (!secondObject || !secondDepth || !secondObject->Complete() || !secondDepth->Complete() ||
                    secondObject->determinism.integrity.requestedMode != DeterminismIntegrityMode3D::Runtime ||
                    secondObject->determinism.frameSerial <= object->determinism.frameSerial ||
                    secondObject->determinism != secondDepth->determinism)
                    result = 6;
            }
        }
        if (!result) {
            stage = "depth-only";
            const DepthCaptureRequestMetadata3D depthOnlyMetadata{81, 83, 89};
            if (!renderer.RequestDepthReadback3D(depthOnlyMetadata)) result = 9;
            else {
                renderer.Render(runtime, {0, 0, 0, 1});
                const auto depthOnly = renderer.TakeDepthReadback3D();
                if (!depthOnly || !depthOnly->Complete() || !depthOnly->determinism.Complete() ||
                    depthOnly->determinism.frameSerial != depthOnly->rasterProvenance.frameSerial ||
                    depthOnly->determinism.render.sceneDrawRevision != depthOnlyMetadata.drawListRevision ||
                    depthOnly->determinism.canonical != object->determinism.canonical)
                    result = 9;
                else {
                    auto missingEvidence = *depthOnly;
                    missingEvidence.determinism = {};
                    auto wrongEvidenceFrame = *depthOnly;
                    ++wrongEvidenceFrame.determinism.frameSerial;
                    if (missingEvidence.Complete() || wrongEvidenceFrame.Complete()) result = 9;
                }
            }
        }
        if (!result) {
            stage = "renderer-canonical-floats";
            auto captureDepth = [&](DrawList& draw, std::uint64_t revision) {
                const DepthCaptureRequestMetadata3D request{revision, 97, 101};
                if (!renderer.RequestDepthReadback3D(request)) return std::optional<DepthCapture3D>{};
                renderer.Render(draw, {0, 0, 0, 1});
                return renderer.TakeDepthReadback3D();
            };
            DrawList positiveZero = runtime, negativeZero = runtime;
            positiveZero.meshInstances[0].transform[3][0] = 0.f;
            negativeZero.meshInstances[0].transform[3][0] = -0.f;
            const auto positive = captureDepth(positiveZero, 103), negative = captureDepth(negativeZero, 107);
            if (!positive || !negative || !positive->Complete() || !negative->Complete() ||
                positive->determinism.canonical != negative->determinism.canonical)
                result = 10;
            else {
                DrawList nonfinite = runtime;
                nonfinite.meshInstances[0].transform[3][0] = std::numeric_limits<float>::quiet_NaN();
                const auto rejected = captureDepth(nonfinite, 109);
                if (!rejected || rejected->Complete() || rejected->determinism.canonical.Complete()) result = 10;
            }
        }
        if (!result) {
            stage = "palette-allocation-churn";
            auto capturePaletteDepth = [&](DrawList& draw, std::uint64_t revision) {
                const DepthCaptureRequestMetadata3D request{revision, 131, 137};
                if (!renderer.RequestDepthReadback3D(request)) return std::optional<DepthCapture3D>{};
                renderer.Render(draw, {0, 0, 0, 1});
                return renderer.TakeDepthReadback3D();
            };
            SkinPaletteJoint3D joint{};
            joint.skin = Matrix4::Identity();
            joint.normalRows = {Vec4{1, 0, 0, 0}, Vec4{0, 1, 0, 0}, Vec4{0, 0, 1, 0}};
            DrawList paletteA, paletteB;
            paletteA.SetBackfaceCulling(false);
            paletteB.SetBackfaceCulling(false);
            const auto rangeA = paletteA.AddSkinPalette(std::span<const SkinPaletteJoint3D>{&joint, 1}, skinCompatibility);
            SkinPaletteJoint3D dummy = joint;
            dummy.skin[3][0] = 7.f;
            (void)paletteB.AddSkinPalette(std::span<const SkinPaletteJoint3D>{&dummy, 1}, skinCompatibility);
            const auto rangeB = paletteB.AddSkinPalette(std::span<const SkinPaletteJoint3D>{&joint, 1}, skinCompatibility);
            const RenderObjectIdentity3D skinIdentity{{0x50414c4554544543ull, 1}, 1};
            if (!paletteA.SkinnedMeshInstance(skinnedMesh, rangeA, Matrix4::Identity(), {1, 1, 1, 1}, {}, {}, .25f,
                                              false, MaterialClass3D::Surface, false, 0, 0, 0,
                                              CharacterShadingModel3D::Default, MaterialAlphaMode3D::Opaque, .5f,
                                              false, {}, Matrix4::Identity(), false, 1.f, 0, 0, 0, skinIdentity) ||
                !paletteB.SkinnedMeshInstance(skinnedMesh, rangeB, Matrix4::Identity(), {1, 1, 1, 1}, {}, {}, .25f,
                                              false, MaterialClass3D::Surface, false, 0, 0, 0,
                                              CharacterShadingModel3D::Default, MaterialAlphaMode3D::Opaque, .5f,
                                              false, {}, Matrix4::Identity(), false, 1.f, 0, 0, 0, skinIdentity))
                result = 11;
            else {
                const auto first = capturePaletteDepth(paletteA, 113), second = capturePaletteDepth(paletteB, 127);
                if (!first || !second || !first->Complete() || !second->Complete() ||
                    first->determinism.canonical != second->determinism.canonical)
                    result = 11;
            }
        }
        if (!result) {
            stage = "abort";
            const auto aborted = renderer.BeginReadbackCaptureSequence3D({1});
            if (!aborted || !renderer.CancelReadbackCaptureSequence3D(*aborted) ||
                renderer.EndReadbackCaptureSequence3D(*aborted).status != ReadbackCaptureSequenceStatus3D::Cancelled)
                result = 7;
        }
        stage = "release";
        if (!result) {
            const ObjectIdCaptureRequestMetadata3D resizeMetadata{71, 73, 79};
            if (!renderer.RequestObjectIdReadback3D(resizeMetadata) || !renderer.RequestDepthReadback3D(resizeMetadata))
                result = 8;
        }
        renderer.Resize(80, 80);
        if (!result && (renderer.TakeObjectIdReadback3D() || renderer.TakeDepthReadback3D())) result = 8;
        renderer.TrimTransientMemory();
        renderer.DestroyMesh(mesh);
        renderer.DestroySkinnedMesh(skinnedMesh);
        renderer.WaitIdle();
        const auto stats = renderer.GetStats();
        std::cerr << "frame-bound stage=release pending=" << stats.pendingGpuReleaseBytes << " info="
                  << stats.debugErrorCount << '/' << stats.debugWarningCount << " gbv="
                  << stats.gpuValidationRequested << '/' << stats.gpuValidationExecuted << '/'
                  << stats.gpuValidationPassed << '\n';
        if (!result && (stats.pendingGpuReleaseBytes || stats.debugErrorCount || stats.debugWarningCount ||
                        (stats.gpuValidationRequested && (!stats.gpuValidationExecuted || !stats.gpuValidationPassed))))
            result = 4;
    } catch (const std::exception& error) {
        std::cerr << "frame-bound exception stage=" << stage << " message=" << error.what() << '\n';
        if (!result) result = 5;
    }
    if (result) std::cerr << "frame-bound exit=" << result << " stage=" << stage << '\n';
    DestroyWindow(window);
    return result;
}
