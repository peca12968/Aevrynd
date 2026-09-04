#include "GameplayCameraProductionBuilder3D.h"

#include <Aevrynd/Camera.h>
#include <Aevrynd/Renderer.h>

#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>

using namespace Aevrynd;
using namespace GameplayLab;

namespace {
constexpr unsigned Width = 320;
constexpr unsigned Height = 180;
constexpr StableObjectId3D ClusterId{0x484c4f445f504958ull,
                                     0x454c5f4f5241434cull};
constexpr RenderObjectIdentity3D SourceIdentity{
    {0x47414d45504c4159ull, 1}, 1, ClusterId, true, false};
constexpr RenderObjectIdentity3D ProxyIdentity{
    {0x47414d45504c4159ull, 2}, 1, ClusterId, true, true};

enum class RasterMode { Source, Transition, Proxy };

struct Capture {
  LinearHdrReadback3D hdr{};
  ObjectIdCapture3D objectId{};
  std::array<std::byte, 32> objectIdPayloadSha256{};
  std::size_t sourcePixels{};
  std::size_t proxyPixels{};
};

std::string Hex(const std::array<std::byte, 32> &value) {
  constexpr char Digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(value.size() * 2);
  for (const auto byte : value) {
    const auto number = std::to_integer<unsigned>(byte);
    result.push_back(Digits[number >> 4]);
    result.push_back(Digits[number & 15]);
  }
  return result;
}

std::optional<std::array<std::byte, 32>>
ObjectIdPayloadSha256(const ObjectIdCapture3D &capture) {
  BCRYPT_ALG_HANDLE algorithm{};
  BCRYPT_HASH_HANDLE hash{};
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                  0) < 0)
    return std::nullopt;
  const auto close = [&] {
    if (hash)
      BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
  };
  if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
    close();
    return std::nullopt;
  }
  const auto update = [&](const void *bytes, std::size_t count) {
    return count <= ULONG_MAX &&
           BCryptHashData(hash,
                          const_cast<PUCHAR>(
                              static_cast<const unsigned char *>(bytes)),
                          static_cast<ULONG>(count), 0) >= 0;
  };
  const auto updateInteger = [&](std::uint64_t value, unsigned byteCount) {
    std::array<unsigned char, 8> littleEndian{};
    for (unsigned index = 0; index < byteCount; ++index)
      littleEndian[index] = static_cast<unsigned char>(value >> (index * 8));
    return update(littleEndian.data(), byteCount);
  };

  constexpr char Schema[] = "aevrynd-oid-payload-v1";
  bool valid = update(Schema, sizeof(Schema) - 1) &&
               updateInteger(capture.width, 4) &&
               updateInteger(capture.height, 4) &&
               updateInteger(capture.tokenMap.size(), 8);
  for (const auto &identity : capture.tokenMap) {
    const unsigned flags = (identity.sharedLodDither ? 1u : 0u) |
                           (identity.hlodProxy ? 2u : 0u);
    valid = valid && updateInteger(identity.id.high, 8) &&
            updateInteger(identity.id.low, 8) &&
            updateInteger(identity.generation, 8) &&
            updateInteger(identity.hlodClusterId.high, 8) &&
            updateInteger(identity.hlodClusterId.low, 8) &&
            updateInteger(flags, 1);
  }
  valid = valid && updateInteger(capture.pixels.size(), 8);
  for (const auto token : capture.pixels)
    valid = valid && updateInteger(token, 4);

  std::array<std::byte, 32> result{};
  valid = valid &&
          BCryptFinishHash(hash, reinterpret_cast<PUCHAR>(result.data()),
                           static_cast<ULONG>(result.size()), 0) >= 0;
  close();
  if (!valid)
    return std::nullopt;
  return result;
}

Mesh3D CharacterProxy() {
  auto mesh = Mesh3D::CreateUnitBox();
  for (auto &vertex : mesh.vertices) {
    vertex.x *= .7f;
    vertex.y = (vertex.y + .5f) * 1.8f;
    vertex.z *= .45f;
  }
  return mesh;
}

std::optional<Camera3D> BuildThirdPersonCamera(const Mesh3D &visual) {
  GameplayCameraProductionQueryAdapters3D queries{};
  queries.environmentSphereSweep =
      [](Vec3, float, Vec3) { return CameraOcclusionSweep3D{}; };
  queries.collisionWorldMaxArm =
      [](const Camera3D &, Vec3) { return 4.5f; };
  queries.heightFieldMaxArm =
      [](const Camera3D &, Vec3) { return 4.5f; };
  queries.roofOrTopBlocked = [](const Camera3D &) { return false; };

  GameplayCameraProductionBuildInput3D input{};
  input.bindVertices = visual.vertices;
  input.viewport = {float(Width), float(Height)};
  input.desiredArm = 4.5f;
  input.candidateIdHigh = 0x47414d4543414d45ull;
  const auto built = BuildGameplayCameraProductionCandidates3D(input, queries);
  if (!built.receipt.valid || !built.receipt.admittedCandidates)
    return std::nullopt;

  GameplayCameraOcclusionPolicy3D policy;
  const ProductionCameraOcclusionInput3D policyInput{
      built.subject, built.lens, built.candidates, input.desiredArm, 1.f / 60.f};
  CameraOcclusionPolicyOutput3D selected{};
  for (unsigned frame = 0; frame < 60; ++frame)
    selected = policy.StepProduction(policyInput);
  if (!selected.valid || selected.candidateIndex >= built.candidates.size())
    return std::nullopt;

  const auto &candidate = built.candidates[selected.candidateIndex];
  return MakeThirdPersonCamera(candidate.cameraFocus,
                               selected.yawOffsetRadians,
                               selected.pitchRadians, selected.arm,
                               input.projection);
}

bool AddInstance(DrawList &draw, MeshHandle3D mesh,
                 RenderObjectIdentity3D identity, float coverage,
                 bool invert) {
  return draw.MeshInstance(
      mesh, Matrix4::Identity(), {.82f, .48f, .16f, 1.f}, {},
      MeshRenderLayer3D::Opaque, {}, .25f, false, MaterialClass3D::Surface,
      std::numeric_limits<float>::quiet_NaN(), false, coverage, 5, invert,
      DirectionalShadowAllCascades3D, ShadowCascadeSelection3D::Automatic,
      Matrix4::Identity(), false, 1.f, MaterialAlphaMode3D::Opaque, .5f,
      false, identity);
}

std::optional<Capture> CaptureRaster(D3D12Renderer &renderer,
                                     MeshHandle3D mesh,
                                     const Camera3D &camera,
                                     RasterMode mode,
                                     std::uint64_t &revision) {
  DrawList draw;
  draw.SetViewProjection(
      CreateViewProjectionMatrix(camera, float(Width) / float(Height)));
  draw.SetBackfaceCulling(false);
  draw.SetDirectionalLight({{.25f, -.8f, -.5f}, .18f, {1, 1, 1, 1}, 1.4f});

  bool added{};
  if (mode == RasterMode::Source)
    added = AddInstance(draw, mesh, SourceIdentity, 1.f, false);
  else if (mode == RasterMode::Proxy)
    added = AddInstance(draw, mesh, ProxyIdentity, 1.f, false);
  else {
    // 來源與代理使用正式 shader 的同一門檻與相反裁切方向；同像素必須恰有一側存活。
    constexpr float Coverage = 64.f / 255.f;
    added = AddInstance(draw, mesh, SourceIdentity, Coverage, false) &&
            AddInstance(draw, mesh, ProxyIdentity, Coverage, true);
  }
  const auto submission = draw.FreezeMeshInstances3D();
  if (!added || !submission)
    return std::nullopt;
  draw.SetStaticMeshSubmission3D(submission);

  const LinearHdrCaptureRequestMetadata3D metadata{
      ++revision, revision + 0x1000, revision + 0x2000};
  if (!renderer.RequestObjectIdReadback3D(metadata) ||
      !renderer.RequestLinearHdrReadback3D(metadata))
    return std::nullopt;
  renderer.Render(draw, {.015f, .02f, .03f, 1.f});
  auto objectId = renderer.TakeObjectIdReadback3D();
  auto hdr = renderer.TakeLinearHdrReadback3D();
  if (!objectId || !hdr || !objectId->Complete() || !hdr->Complete())
    return std::nullopt;

  const auto objectIdPayloadSha256 = ObjectIdPayloadSha256(*objectId);
  if (!objectIdPayloadSha256)
    return std::nullopt;

  Capture result{std::move(*hdr), std::move(*objectId),
                 *objectIdPayloadSha256};
  for (const auto token : result.objectId.pixels) {
    const auto *identity = result.objectId.Resolve(token);
    if (identity && identity->id == SourceIdentity.id)
      ++result.sourcePixels;
    else if (identity && identity->id == ProxyIdentity.id)
      ++result.proxyPixels;
  }
  return result;
}

bool ExactOracle(const Capture &source, const Capture &transition,
                 const Capture &proxy) {
  const auto sourceForeground = source.sourcePixels + source.proxyPixels;
  const auto transitionForeground =
      transition.sourcePixels + transition.proxyPixels;
  const auto proxyForeground = proxy.sourcePixels + proxy.proxyPixels;
  return sourceForeground > 512 && source.proxyPixels == 0 &&
         proxy.sourcePixels == 0 && transition.sourcePixels > 0 &&
         transition.proxyPixels > 0 && sourceForeground == proxyForeground &&
         transitionForeground == sourceForeground &&
         source.hdr.rawHalfBytes == transition.hdr.rawHalfBytes &&
         source.hdr.rawHalfBytes == proxy.hdr.rawHalfBytes;
}
}

int main() {
  const HWND window = CreateWindowExW(
      0, L"STATIC", L"Gameplay HLOD camera transition pixel oracle",
      WS_OVERLAPPED, 0, 0, Width, Height, nullptr, nullptr,
      GetModuleHandleW(nullptr), nullptr);
  if (!window)
    return 1;

  int result{};
  try {
    const auto visual = CharacterProxy();
    const auto thirdPerson = BuildThirdPersonCamera(visual);
    if (!thirdPerson) {
      result = 2;
      throw std::runtime_error("Gameplay production camera fixture failed");
    }
    const auto firstPerson = MakeFirstPersonCamera({0, .9f, -4.5f}, 0, 0);

    D3D12Renderer renderer;
    renderer.Initialize(window, Width, Height, Width, Height, false);
    renderer.SetQualitySettings({false, false, false, false});
    const auto mesh = renderer.CreateMesh(visual.vertices, visual.indices);
    if (!mesh) {
      result = 3;
      throw std::runtime_error("Gameplay transition mesh upload failed");
    }

    std::uint64_t revision = 0x504958454c000000ull;
    std::array<std::array<std::optional<Capture>, 3>, 2> captures{};
    const std::array cameras{firstPerson, *thirdPerson};
    for (std::size_t camera = 0; camera < cameras.size(); ++camera) {
      captures[camera][0] =
          CaptureRaster(renderer, mesh, cameras[camera], RasterMode::Source,
                        revision);
      captures[camera][1] = CaptureRaster(renderer, mesh, cameras[camera],
                                          RasterMode::Transition, revision);
      captures[camera][2] =
          CaptureRaster(renderer, mesh, cameras[camera], RasterMode::Proxy,
                        revision);
      if (!captures[camera][0] || !captures[camera][1] ||
          !captures[camera][2] ||
          !ExactOracle(*captures[camera][0], *captures[camera][1],
                       *captures[camera][2])) {
        result = 4;
        break;
      }
    }

    if (!result) {
      const auto &first = *captures[0][0];
      const auto &third = *captures[1][0];
      const auto &firstTransition = *captures[0][1];
      const auto &thirdTransition = *captures[1][1];
      const bool cameraChanged =
          first.hdr.rawHalfBytes != third.hdr.rawHalfBytes &&
          first.objectId.pixels != third.objectId.pixels;
      const bool cameraChangedOidHash =
          first.objectIdPayloadSha256 != third.objectIdPayloadSha256 &&
          firstTransition.objectIdPayloadSha256 !=
              thirdTransition.objectIdPayloadSha256;
      if (!cameraChanged || !cameraChangedOidHash)
        result = 5;
      else {
        const auto receiptPath =
            std::filesystem::path(AEVRYND_TRANSITION_PIXEL_RECEIPT);
        std::filesystem::create_directories(receiptPath.parent_path());
        std::ofstream receipt(receiptPath, std::ios::trunc);
        receipt << "{\"source\":\"GameplayHlodCameraTransitionFramebuffer\","
                   "\"oracle\":\"RealD3D12ComplementaryTransitionRaster\","
                   "\"scope\":\"DrawListHlodRasterWithGameplayCameraEndpoints\","
                   "\"completeProductionHlodSelectionSceneEcsChain\":0,"
                   "\"cameraEndpoints\":2,\"rasterModesPerCamera\":3,"
                   "\"oidPayloadSchema\":\"aevrynd-oid-payload-v1\","
                   "\"complementaryCoverage8\":64,\"firstPixels\":"
                << first.sourcePixels << ",\"thirdPixels\":"
                << third.sourcePixels << ",\"firstTransitionSourcePixels\":"
                << firstTransition.sourcePixels
                << ",\"firstTransitionProxyPixels\":"
                << firstTransition.proxyPixels
                << ",\"thirdTransitionSourcePixels\":"
                << thirdTransition.sourcePixels
                << ",\"thirdTransitionProxyPixels\":"
                << thirdTransition.proxyPixels << ",\"firstHdrSha256\":\""
                << Hex(first.hdr.receipt.rawHalfSha256)
                << "\",\"thirdHdrSha256\":\""
                << Hex(third.hdr.receipt.rawHalfSha256)
                << "\",\"firstSourceHardOidPayloadSha256\":\""
                << Hex(first.objectIdPayloadSha256)
                << "\",\"firstTransitionOidPayloadSha256\":\""
                << Hex(firstTransition.objectIdPayloadSha256)
                << "\",\"firstProxyHardOidPayloadSha256\":\""
                << Hex(captures[0][2]->objectIdPayloadSha256)
                << "\",\"thirdSourceHardOidPayloadSha256\":\""
                << Hex(third.objectIdPayloadSha256)
                << "\",\"thirdTransitionOidPayloadSha256\":\""
                << Hex(thirdTransition.objectIdPayloadSha256)
                << "\",\"thirdProxyHardOidPayloadSha256\":\""
                << Hex(captures[1][2]->objectIdPayloadSha256)
                << "\",\"transitionExact\":1,\"cameraChanged\":1,"
                   "\"cameraChangedOidHash\":1}";
        receipt.close();
        if (!receipt || !std::filesystem::exists(receiptPath) ||
            std::filesystem::file_size(receiptPath) < 128)
          result = 6;
        else
          std::cout << "realD3D12ComplementaryTransitionRaster=passed "
                       "scope=draw-list-raster-with-gameplay-camera-endpoints "
                       "completeProductionHlodSelectionSceneEcsChain=0 "
                       "cameraEndpoints=2 rasterModes=3 receipt="
                    << receiptPath.string() << '\n';
      }
    }

    renderer.DestroyMesh(mesh);
    renderer.WaitIdle();
  } catch (const std::exception &error) {
    if (!result)
      result = 20;
    std::cerr << error.what() << '\n';
  }
  DestroyWindow(window);
  return result;
}
