#include <Aevrynd/Camera.h>
#include <Aevrynd/CharacterPresentation3D.h>
#include <Aevrynd/SkinnedMaterialSidecar3D.h>
#include <Aevrynd/SkinnedRenderable3D.h>
#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

using namespace Aevrynd;
using Microsoft::WRL::ComPtr;

namespace {
constexpr unsigned StudioSize = 512;
constexpr std::size_t MinimumFrontActorPixels = 80000;

struct Image {
    unsigned width{}, height{};
    std::vector<std::uint8_t> rgba;
};

struct Lab {
    double l{}, a{}, b{};
};

struct Capture {
    Image color;
    ObjectIdCapture3D oid;
    RendererStats stats;
};

struct CharacterFixture {
    std::string name;
    SkinnedModelAssetData asset;
    HumanoidSkeletonMap3D rig;
    FallbackHumanoidAnimationSet3D animations;
    SkinnedMeshHandle3D mesh{};
    std::vector<Material3D> materials;
    std::vector<MaterialTextureSetHandle3D> textures;
    std::vector<MaterialTextureSetDefinition3D> textureDefinitions;
};

struct WalkMetrics {
    float pelvisRange{}, leftKneeRange{}, rightKneeRange{}, leftAnkleRange{}, rightAnkleRange{};
    float leftToeRange{}, rightToeRange{};
    float leftFingerAngularRange{}, rightFingerAngularRange{};
    std::size_t distinctGpuMasks{}, leftGroundContacts{}, rightGroundContacts{}, contactPhaseSeparation{};
};

struct DeathArticulationMetrics {
    float torso{};
    std::array<float,4> limbs{};
    std::array<float,4> distal{};
    std::size_t activeLimbs{},activeDistal{};
    bool valid{};
};

bool DecodePng(const std::filesystem::path& path, Image& output) {
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    UINT width{}, height{};
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory))) ||
        FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                  WICDecodeMetadataCacheOnLoad, &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&width, &height)) ||
        !width || !height || FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                     WICBitmapDitherTypeNone, nullptr, 0,
                                     WICBitmapPaletteTypeCustom))) return false;
    output = {width, height, std::vector<std::uint8_t>(std::size_t(width) * height * 4)};
    return SUCCEEDED(converter->CopyPixels(nullptr, width * 4, UINT(output.rgba.size()),
                                           output.rgba.data()));
}

double SrgbToLinear(double value) {
    value /= 255.0;
    return value <= .04045 ? value / 12.92 : std::pow((value + .055) / 1.055, 2.4);
}

Lab ToLab(const std::uint8_t* rgba) {
    const double r = SrgbToLinear(rgba[0]), g = SrgbToLinear(rgba[1]), b = SrgbToLinear(rgba[2]);
    double x = (r * .4124564 + g * .3575761 + b * .1804375) / .95047;
    double y = (r * .2126729 + g * .7151522 + b * .0721750);
    double z = (r * .0193339 + g * .1191920 + b * .9503041) / 1.08883;
    const auto f = [](double v) { return v > .008856 ? std::cbrt(v) : 7.787 * v + 16.0 / 116.0; };
    x = f(x); y = f(y); z = f(z);
    return {116.0 * y - 16.0, 500.0 * (x - y), 200.0 * (y - z)};
}

double DeltaE(Lab left, Lab right) {
    return std::sqrt((left.l-right.l)*(left.l-right.l) + (left.a-right.a)*(left.a-right.a) +
                     (left.b-right.b)*(left.b-right.b));
}

RenderObjectIdentity3D SlotIdentity(std::size_t character, std::size_t slot) {
    return {{0xc700000000000000ull + character, 0xa110000000000000ull + slot}, 17};
}

std::size_t PixelsFor(const ObjectIdCapture3D& oid, RenderObjectIdentity3D identity) {
    std::size_t count{};
    for (std::uint32_t token : oid.pixels) {
        const auto* found = oid.Resolve(token);
        count += found && *found == identity;
    }
    return count;
}

Lab MeanLab(const Capture& capture, RenderObjectIdentity3D identity) {
    double l{}, a{}, b{};
    std::size_t count{};
    for (std::size_t pixel = 0; pixel < capture.oid.pixels.size(); ++pixel) {
        const auto* found = capture.oid.Resolve(capture.oid.pixels[pixel]);
        if (!found || *found != identity) continue;
        const Lab value = ToLab(&capture.color.rgba[pixel * 4]);
        l += value.l; a += value.a; b += value.b; ++count;
    }
    return count ? Lab{l / count, a / count, b / count} : Lab{};
}

std::uint64_t HashOid(const ObjectIdCapture3D& capture) {
    std::uint64_t hash = 1469598103934665603ull;
    for (std::uint32_t value : capture.pixels) {
        hash ^= value;
        hash *= 1099511628211ull;
    }
    return hash;
}

Surface3D SourceSurface(std::string_view name) {
    if (name.find("Gold") != std::string_view::npos) return {.22f,.82f,0,.96f};
    if (name.find("Eye") != std::string_view::npos || name.find("Iris") != std::string_view::npos) return {.16f,0,0,1};
    if (name.find("Hair") != std::string_view::npos) return {.31f,0,0,.94f};
    if (name.find("Skin") != std::string_view::npos) return {.43f,0,0,1};
    if (name.find("Lip") != std::string_view::npos) return {.36f,0,0,1};
    if (name.find("Leather") != std::string_view::npos || name.find("Boot") != std::string_view::npos) return {.48f,0,0,.96f};
    return {.68f,0,0,1};
}

bool LoadFixture(D3D12Renderer& renderer, const std::filesystem::path& root,
                 std::string name, const char* assetName, const char* sidecarName,
                 CharacterFixture& output) {
    const auto modelRoot = root / "games" / "character_lab" / "assets" / "model-packs" / assetName;
    const auto materialRoot = root / "games" / "character_lab" / "assets" / "material-packs" / "original";
    auto loaded = LoadSkinnedModelAssetFileDetailed(modelRoot / (std::string(assetName) + ".aeskin"));
    if (!loaded || !ResolveCharacterRig3D(loaded.asset, output.rig) ||
        !CreateFallbackHumanoidAnimationSet3D(loaded.asset, output.rig, output.animations)) return false;
    const auto sidecar = LoadSkinnedMaterialSidecarFile3D(materialRoot / sidecarName,
                                                          loaded.asset.materialSlotNames, materialRoot);
    if (!sidecar || sidecar.materials.size() != loaded.asset.materialSlotNames.size()) return false;
    std::vector<SkinnedVertex3D> vertices;
    if (!BuildGpuSkinnedVertices(loaded.asset.model, vertices)) return false;
    output.mesh = renderer.CreateSkinnedMesh(vertices, loaded.asset.model.bindMesh.indices,
        std::uint32_t(loaded.asset.model.skeleton.joints.size()), loaded.asset.model.skeletonCompatibilityId);
    if (!output.mesh) return false;
    output.name = std::move(name);
    output.asset = std::move(loaded.asset);
    output.materials.reserve(sidecar.materials.size());
    output.textures.reserve(sidecar.materials.size());
    output.textureDefinitions.reserve(sidecar.materials.size());
    for (std::size_t slot = 0; slot < sidecar.materials.size(); ++slot) {
        Material3D material = sidecar.materials[slot].material;
        if (!output.asset.materials.empty()) {
            const auto& source = output.asset.materials[slot];
            material.tint = source.baseColorFactor;
            material.surface = {source.roughnessFactor, source.metallicFactor, 0.f, source.occlusionStrength};
            material.normalScale = source.normalScale;
        } else material.surface = SourceSurface(output.asset.materialSlotNames[slot]);
        material.textures = renderer.CreateMaterialTextureSet(sidecar.materials[slot].textures);
        if (!material.textures) return false;
        output.textures.push_back(material.textures);
        output.textureDefinitions.push_back(sidecar.materials[slot].textures);
        output.materials.push_back(material);
    }
    return true;
}

Camera3D StudioCamera(float yaw, bool fullBody, float targetHeight, float fullBodyHeight) {
    CameraProjection projection;
    projection.mode = ProjectionMode::Orthographic;
    projection.orthographicHeight = fullBody ? fullBodyHeight : 1.18f;
    projection.nearPlane = .1f;
    projection.farPlane = 20.f;
    const Vec3 target{0,targetHeight,0};
    return MakeThirdPersonCamera(target, yaw, 0.f, 5.f, projection);
}

Camera3D CharacterStudioCamera(const CharacterFixture& character,float yaw,bool fullBody=false){
    float minimum=999.f,maximum=-999.f;for(const auto& vertex:character.asset.model.bindMesh.vertices){minimum=(std::min)(minimum,vertex.y);maximum=(std::max)(maximum,vertex.y);}
    const float height=maximum-minimum;return StudioCamera(yaw,fullBody,fullBody?(minimum+maximum)*.5f:maximum-.59f,
        (std::max)(2.05f,height*1.10f));
}

bool SubmitCharacter(DrawList& draw, const CharacterFixture& character,
                     const AnimationPose3D& pose, std::size_t characterIndex,
                     std::span<const Material3D> materials, Matrix4 transform = Matrix4::Identity()) {
    SkinningWorkspace3D workspace;
    const auto palette = AddSkinPaletteFromPose(draw, character.asset.model, pose, workspace);
    if (!palette) return false;
    for (const auto& range : character.asset.submeshes) {
        if (range.materialSlot >= materials.size()) return false;
        const Material3D& material = materials[range.materialSlot];
        if (!draw.SkinnedMeshInstance(character.mesh, palette, transform, material.tint,
            material.surface, material.textures, material.textureScale, material.useUv,
            material.materialClass, material.castsShadow, DirectionalShadowAllCascades3D,
            range.firstIndex, range.indexCount, material.shadingModel, material.alphaMode,
            material.alphaCutoff, material.doubleSided, {}, Matrix4::Identity(), false,
            material.normalScale, material.baseTextureScale, material.detailTextureScale,
            0, SlotIdentity(characterIndex, range.materialSlot))) return false;
    }
    return true;
}

std::optional<Capture> CaptureGpu(D3D12Renderer& renderer, DrawList& draw,
                                  const std::filesystem::path& path) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    if (!renderer.RequestObjectIdReadback3D() || !renderer.CaptureNextFrame(path)) return std::nullopt;
    renderer.Render(draw, {.025f,.028f,.035f,1});
    renderer.WaitIdle();
    auto oid = renderer.TakeObjectIdReadback3D();
    Image image;
    if (!oid || !oid->Complete() || oid->width != StudioSize || oid->height != StudioSize ||
        !DecodePng(path, image) || image.width != StudioSize || image.height != StudioSize) return std::nullopt;
    return Capture{std::move(image), std::move(*oid), renderer.GetStats()};
}

bool JointInsideActor(const ObjectIdCapture3D& oid, ProjectedPoint point, unsigned radius = 10) {
    if (!point.visible) return false;
    const int centerX = int(std::lround(point.position.x)), centerY = int(std::lround(point.position.y));
    for (int y = centerY-int(radius); y <= centerY+int(radius); ++y) for (int x = centerX-int(radius); x <= centerX+int(radius); ++x) {
        if (x >= 0 && y >= 0 && x < int(oid.width) && y < int(oid.height) &&
            oid.pixels[std::size_t(y)*oid.width+std::size_t(x)] != 0) return true;
    }
    return false;
}

std::pair<Vec2,float> NearestActorPixel(const ObjectIdCapture3D& oid,ProjectedPoint point){
    Vec2 nearest{};float best=std::numeric_limits<float>::max();for(unsigned y=0;y<oid.height;++y)for(unsigned x=0;x<oid.width;++x){
        if(!oid.pixels[std::size_t(y)*oid.width+x])continue;const float dx=float(x)-point.position.x,dy=float(y)-point.position.y,distance=dx*dx+dy*dy;
        if(distance<best){best=distance;nearest={float(x),float(y)};}}
    return{nearest,std::sqrt(best)};
}

std::size_t JointByName(const CharacterFixture& character, std::string_view name) {
    const auto found = std::ranges::find(character.asset.jointNames, name);
    return found == character.asset.jointNames.end() ? SIZE_MAX : std::size_t(found-character.asset.jointNames.begin());
}

std::size_t MaterialSlot(const CharacterFixture& character,std::initializer_list<std::string_view> tokens){
    for(std::size_t slot=0;slot<character.asset.materialSlotNames.size();++slot)
        for(const auto token:tokens)if(character.asset.materialSlotNames[slot].find(token)!=std::string::npos)return slot;
    return SIZE_MAX;
}

float QuaternionDistance(Quaternion3D a, Quaternion3D b) {
    return 1.f-std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w);
}

Quaternion3D LocalRotation(const JointTransform3D& transform) {
    return NormalizeQuaternion3D(transform.usesQuaternion ? transform.quaternion
                                                          : QuaternionFromEuler3D(transform.rotation));
}

float AngularDistanceRadians(const JointTransform3D& left, const JointTransform3D& right) {
    const Quaternion3D a=LocalRotation(left),b=LocalRotation(right);
    const float dot=std::clamp(std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w),0.f,1.f);
    return 2.f*std::acos(dot);
}

DeathArticulationMetrics MeasureDeathArticulation(
    const CharacterFixture& character,const std::array<AnimationPose3D,4>& poses) {
    DeathArticulationMetrics result{};
    for(const auto& pose:poses)
        if(pose.localTransforms.size()!=character.asset.model.skeleton.joints.size())return result;
    const auto maximumDelta=[&](HumanoidJoint3D semantic){
        const std::size_t joint=character.rig.Joint(semantic);float maximum{};
        if(joint>=poses[0].localTransforms.size())return std::numeric_limits<float>::quiet_NaN();
        for(std::size_t phase=1;phase<poses.size();++phase)
            maximum=(std::max)(maximum,AngularDistanceRadians(
                poses[0].localTransforms[joint],poses[phase].localTransforms[joint]));
        return maximum;
    };
    result.torso=(std::max)(maximumDelta(HumanoidJoint3D::Spine),maximumDelta(HumanoidJoint3D::Head));
    constexpr std::array<std::array<HumanoidJoint3D,3>,4> LimbGroups{{
        {{HumanoidJoint3D::LeftUpperArm,HumanoidJoint3D::LeftLowerArm,HumanoidJoint3D::LeftHand}},
        {{HumanoidJoint3D::RightUpperArm,HumanoidJoint3D::RightLowerArm,HumanoidJoint3D::RightHand}},
        {{HumanoidJoint3D::LeftUpperLeg,HumanoidJoint3D::LeftLowerLeg,HumanoidJoint3D::LeftFoot}},
        {{HumanoidJoint3D::RightUpperLeg,HumanoidJoint3D::RightLowerLeg,HumanoidJoint3D::RightFoot}}
    }};
    constexpr std::array<HumanoidJoint3D,4> DistalJoints{{
        HumanoidJoint3D::LeftLowerArm,HumanoidJoint3D::RightLowerArm,
        HumanoidJoint3D::LeftLowerLeg,HumanoidJoint3D::RightLowerLeg
    }};
    for(std::size_t region=0;region<LimbGroups.size();++region){
        for(const auto joint:LimbGroups[region])result.limbs[region]=(std::max)(result.limbs[region],maximumDelta(joint));
        result.activeLimbs+=result.limbs[region]>=.10f;
        result.distal[region]=maximumDelta(DistalJoints[region]);
        result.activeDistal+=result.distal[region]>=.08f;
    }
    result.valid=std::isfinite(result.torso)&&
        std::ranges::all_of(result.limbs,[](float value){return std::isfinite(value);})&&
        std::ranges::all_of(result.distal,[](float value){return std::isfinite(value);});
    return result;
}

bool ValidateStudio(const Capture& capture, std::size_t characterIndex,
                    const CharacterFixture& character, bool requireFrontCoverage) {
    std::size_t actor{};
    for (std::uint32_t token : capture.oid.pixels) actor += token != 0;
    if (requireFrontCoverage && actor < MinimumFrontActorPixels) {
        std::cerr<<character.name<<" front actor pixels="<<actor<<" minimum="<<MinimumFrontActorPixels<<'\n';return false;
    }
    if (capture.stats.lastSubmittedTexturedSkinnedInstances != character.asset.submeshes.size() ||
        capture.stats.lastSubmittedSkinShadedInstances < 2 ||
        capture.stats.lastSubmittedEyeShadedInstances < 2 ||
        !capture.stats.lastSubmittedHairShadedInstances || !capture.stats.lastSubmittedClothShadedInstances) {
        std::cerr<<character.name<<" shading telemetry skin/eye/hair/cloth="
            <<capture.stats.lastSubmittedSkinShadedInstances<<'/'<<capture.stats.lastSubmittedEyeShadedInstances<<'/'
            <<capture.stats.lastSubmittedHairShadedInstances<<'/'<<capture.stats.lastSubmittedClothShadedInstances
            <<" textured="<<capture.stats.lastSubmittedTexturedSkinnedInstances<<'\n';return false;
    }
    if (!requireFrontCoverage) return actor >= 18000;
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const std::string& name = character.asset.materialSlotNames[slot];
        const std::size_t count = PixelsFor(capture.oid, SlotIdentity(characterIndex, slot));
        const bool eye = name.find("Eye") != std::string::npos || name.find("Iris") != std::string::npos;
        if (count < (eye ? 6u : 24u)) {std::cerr<<character.name<<" slot="<<name<<" pixels="<<count;
            for(const auto& range:character.asset.submeshes)if(range.materialSlot==slot){Vec3 minimum{999,999,999},maximum{-999,-999,-999};
                for(std::uint32_t offset=0;offset<range.indexCount;++offset){const auto index=character.asset.model.bindMesh.indices[range.firstIndex+offset];const auto& v=character.asset.model.bindMesh.vertices[index];
                    minimum.x=(std::min)(minimum.x,v.x);minimum.y=(std::min)(minimum.y,v.y);minimum.z=(std::min)(minimum.z,v.z);
                    maximum.x=(std::max)(maximum.x,v.x);maximum.y=(std::max)(maximum.y,v.y);maximum.z=(std::max)(maximum.z,v.z);}
                std::cerr<<" range="<<range.firstIndex<<'+'<<range.indexCount<<" bounds="<<minimum.x<<','<<minimum.y<<','<<minimum.z<<".."<<maximum.x<<','<<maximum.y<<','<<maximum.z;}
            std::cerr<<'\n';return false;}
    }
    const auto skinSlot=MaterialSlot(character,{"Skin"}),lipSlot=MaterialSlot(character,{"Lip","FaceHighlight"}),
        scleraSlot=MaterialSlot(character,{"Sclera"}),irisSlot=MaterialSlot(character,{"Iris"}),
        hairSlot=MaterialSlot(character,{"Hair"}),clothSlot=MaterialSlot(character,{"Dress","Shirt","Cloth"});
    if(skinSlot==SIZE_MAX||lipSlot==SIZE_MAX||scleraSlot==SIZE_MAX||irisSlot==SIZE_MAX||hairSlot==SIZE_MAX||clothSlot==SIZE_MAX)return false;
    const Lab skin = MeanLab(capture, SlotIdentity(characterIndex, skinSlot));
    const Lab lips = MeanLab(capture, SlotIdentity(characterIndex, lipSlot));
    const Lab sclera = MeanLab(capture, SlotIdentity(characterIndex, scleraSlot));
    const Lab iris = MeanLab(capture, SlotIdentity(characterIndex, irisSlot));
    const Lab hair = MeanLab(capture, SlotIdentity(characterIndex, hairSlot));
    const Lab cloth = MeanLab(capture, SlotIdentity(characterIndex, clothSlot));
    const double skinLip=DeltaE(skin,lips),eyeDelta=DeltaE(sclera,iris),hairCloth=DeltaE(hair,cloth);
    if(skinLip<2.0||eyeDelta<3.0||hairCloth<5.0){std::cerr<<character.name<<" deltaE skin/lip="<<skinLip
        <<" sclera/iris="<<eyeDelta<<" hair/cloth="<<hairCloth<<'\n';return false;}return true;
}

bool ValidateFrontJointOracle(const Capture& capture, const CharacterFixture& character,
                              const AnimationPose3D& pose, const Camera3D& camera) {
    for (std::string_view name : {"Head","Hips","LeftUpperLeg","RightUpperLeg","LeftFoot",
                                  "RightFoot","LeftFingers","RightFingers","LeftToe","RightToe"}) {
        const std::size_t joint = JointByName(character, name);
        if (joint == SIZE_MAX || joint >= pose.globalMatrices.size()) return false;
        const auto projected=ProjectToViewport(TransformPoint(pose.globalMatrices[joint], {}),camera,{StudioSize,StudioSize});
        if(name.find("Fingers")!=std::string_view::npos){const auto [nearest,distance]=NearestActorPixel(capture.oid,projected);
            if(distance>8.f)std::cerr<<character.name<<' '<<name<<" projected="<<projected.position.x<<','<<projected.position.y
                <<" nearest="<<nearest.x<<','<<nearest.y<<" delta="<<(nearest.x-projected.position.x)<<','<<(nearest.y-projected.position.y)
                <<" distance="<<distance<<'\n';}
        if (!JointInsideActor(capture.oid,projected)) {std::cerr<<character.name<<" joint oracle failed "<<name
            <<" visible="<<projected.visible<<" xy="<<projected.position.x<<','<<projected.position.y<<'\n';return false;}
    }
    return true;
}

WalkMetrics MeasureWalk(const CharacterFixture& character, std::span<const Capture> captures,
                        std::span<const AnimationPose3D> poses, const Camera3D& camera) {
    WalkMetrics result;
    const auto range = [&](std::string_view name, int axis) {
        const std::size_t joint = JointByName(character,name);
        float minimum = std::numeric_limits<float>::max(), maximum = -minimum;
        for (const auto& pose : poses) {
            const ProjectedPoint point = ProjectToViewport(TransformPoint(pose.globalMatrices[joint],{}), camera,
                                                           {StudioSize,StudioSize});
            const float value = axis ? point.position.y : point.position.x;
            minimum = (std::min)(minimum,value); maximum = (std::max)(maximum,value);
        }
        return maximum-minimum;
    };
    result.pelvisRange=range("Hips",1); result.leftKneeRange=range("LeftLowerLeg",0);
    result.rightKneeRange=range("RightLowerLeg",0); result.leftAnkleRange=range("LeftFoot",1);
    result.rightAnkleRange=range("RightFoot",1);
    result.leftToeRange=range("LeftToe",1);result.rightToeRange=range("RightToe",1);
    const auto grounding=[&](std::string_view name,std::size_t& contacts){
        const std::size_t joint=JointByName(character,name);std::array<float,8> y{};float ground{};
        for(std::size_t phase=0;phase<poses.size();++phase){y[phase]=ProjectToViewport(
            TransformPoint(poses[phase].globalMatrices[joint],{}),camera,{StudioSize,StudioSize}).position.y;
            ground=(std::max)(ground,y[phase]);}
        std::size_t groundPhase{};for(std::size_t phase=0;phase<y.size();++phase){contacts+=ground-y[phase]<=2.f;if(y[phase]==ground)groundPhase=phase;}
        return groundPhase;
    };
    const std::size_t leftGroundPhase=grounding("LeftFoot",result.leftGroundContacts);
    const std::size_t rightGroundPhase=grounding("RightFoot",result.rightGroundContacts);
    const std::size_t direct=leftGroundPhase>rightGroundPhase?leftGroundPhase-rightGroundPhase:rightGroundPhase-leftGroundPhase;
    result.contactPhaseSeparation=(std::min)(direct,8-direct);
    const auto angularRange = [&](std::string_view name) {
        const std::size_t joint=JointByName(character,name); float maximum{};
        for (std::size_t index=1;index<poses.size();++index)
            maximum=(std::max)(maximum,QuaternionDistance(poses[0].localTransforms[joint].quaternion,
                                                          poses[index].localTransforms[joint].quaternion));
        return maximum;
    };
    result.leftFingerAngularRange=angularRange("LeftFingers");
    result.rightFingerAngularRange=angularRange("RightFingers");
    std::vector<std::uint64_t> hashes;
    for (const auto& capture : captures) hashes.push_back(HashOid(capture.oid));
    std::ranges::sort(hashes); result.distinctGpuMasks=std::size_t(std::ranges::unique(hashes).begin()-hashes.begin());
    return result;
}

bool ValidWalk(const WalkMetrics& value) {
    return value.pelvisRange >= 2.f && value.leftKneeRange >= 3.f && value.rightKneeRange >= 3.f &&
           value.leftAnkleRange >= 3.f && value.rightAnkleRange >= 3.f &&
           value.leftToeRange >= 2.f && value.rightToeRange >= 2.f &&
           value.leftGroundContacts && value.rightGroundContacts && value.contactPhaseSeparation >= 2 &&
           value.leftFingerAngularRange >= .0001f && value.rightFingerAngularRange >= .0001f &&
           value.distinctGpuMasks >= 6;
}

bool ValidateMaterialReference(const Capture& candidate, const Capture& reference,
                               std::size_t characterIndex) {
    for (std::size_t slot=0;slot<6;++slot)
        if (DeltaE(MeanLab(candidate,SlotIdentity(characterIndex,slot)),
                   MeanLab(reference,SlotIdentity(characterIndex,slot))) > 4.0) return false;
    return true;
}
}

int main() {
    const HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HWND window=CreateWindowExW(0,L"STATIC",L"Aevrynd Character Visual Quality",
        WS_OVERLAPPED,0,0,StudioSize,StudioSize,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if (!window) return 1;
    int result{};
    try {
        const std::filesystem::path root{AEVRYND_SOURCE_DIR};
        const std::filesystem::path output{AEVRYND_TEST_ARTIFACT_DIR};
        std::filesystem::create_directories(output);
        D3D12Renderer renderer;
        renderer.Initialize(window,StudioSize,StudioSize,StudioSize,StudioSize,false);
        renderer.SetQualitySettings({true,true,false,false});
        std::array<CharacterFixture,2> characters;
        if (!LoadFixture(renderer,root,"F2","realistic-female","female.materials.json",characters[0]) ||
            !LoadFixture(renderer,root,"M2","realistic-male","male.materials.json",characters[1])) result=2;

        // 啟動時先輸出兩名角色的 face slot 真 GPU 覆蓋，讓資產軸向錯誤可一次定位。
        for(std::size_t diagnosticIndex=0;diagnosticIndex<characters.size()&&!result;++diagnosticIndex){
            auto& character=characters[diagnosticIndex];AnimationPose3D pose;SampleAnimationClipInto(
                character.asset.model.skeleton,character.animations.clips[std::size_t(FallbackHumanoidClip3D::Idle)],.37f,pose);
            const Camera3D camera=CharacterStudioCamera(character,0.f);DrawList draw;draw.SetViewProjection(CreateViewProjectionMatrix(camera,1.f));draw.SetBackfaceCulling(true);
            if(!SubmitCharacter(draw,character,pose,diagnosticIndex,character.materials)){result=21;break;}
            auto capture=CaptureGpu(renderer,draw,output/(character.name+"-face-diagnostic.png"));if(!capture){result=21;break;}
            std::cout<<character.name<<" faceDiagnostic actor="<<capture->oid.semanticPixelCount;
            for(std::size_t slot=0;slot<5;++slot){Vec3 minimum{999,999,999},maximum{-999,-999,-999};
                for(const auto& range:character.asset.submeshes)if(range.materialSlot==slot)for(std::uint32_t offset=0;offset<range.indexCount;++offset){const auto vertexIndex=character.asset.model.bindMesh.indices[range.firstIndex+offset];const auto& vertex=character.asset.model.bindMesh.vertices[vertexIndex];
                    minimum.x=(std::min)(minimum.x,vertex.x);minimum.y=(std::min)(minimum.y,vertex.y);minimum.z=(std::min)(minimum.z,vertex.z);maximum.x=(std::max)(maximum.x,vertex.x);maximum.y=(std::max)(maximum.y,vertex.y);maximum.z=(std::max)(maximum.z,vertex.z);}
                std::cout<<' '<<character.asset.materialSlotNames[slot]<<'='<<PixelsFor(capture->oid,SlotIdentity(diagnosticIndex,slot))
                    <<"["<<minimum.x<<','<<minimum.y<<','<<minimum.z<<".."<<maximum.x<<','<<maximum.y<<','<<maximum.z<<']';}
            std::cout<<'\n';
        }

        for (std::size_t characterIndex=0;characterIndex<characters.size()&&!result;++characterIndex) {
            auto& character=characters[characterIndex];
            AnimationPose3D idle;
            SampleAnimationClipInto(character.asset.model.skeleton,
                character.animations.clips[std::size_t(FallbackHumanoidClip3D::Idle)],.37f,idle);
            std::optional<Capture> front;
            const std::array<std::pair<const char*,float>,3> views{{{"front",0.f},{"three-quarter",.78539816f},{"profile",1.57079633f}}};
            for (std::size_t view=0;view<views.size();++view) {
                DrawList draw; const Camera3D camera=CharacterStudioCamera(character,views[view].second);
                draw.SetViewProjection(CreateViewProjectionMatrix(camera,1.f));draw.SetBackfaceCulling(true);
                draw.SetAtmosphere({{.16f,.18f,.21f,1},camera.position,100.f,200.f,false});
                draw.SetDirectionalLight({{-.55f,-.72f,-.42f},.2f,{1.f,.9f,.82f,1},3.2f});
                if (!SubmitCharacter(draw,character,idle,characterIndex,character.materials)) {result=3;break;}
                auto capture=CaptureGpu(renderer,draw,output/(character.name+"-studio-"+views[view].first+".png"));
                if (!capture || !ValidateStudio(*capture,characterIndex,character,view==0)) {result=4;break;}
                if (view==0) {
                    front=std::move(capture);
                }
            }
            if (result||!front) break;

            std::array<AnimationPose3D,8> walkPoses;
            std::array<Capture,8> walkCaptures;
            const Camera3D frontCamera=CharacterStudioCamera(character,0.f,true);
            const Camera3D studioFrontCamera=CharacterStudioCamera(character,0.f);
            for (std::size_t phase=0;phase<8;++phase) {
                const AnimationClip3D& walk=character.animations.clips[std::size_t(FallbackHumanoidClip3D::Walk)];
                SampleAnimationClipInto(character.asset.model.skeleton,walk,walk.duration*float(phase)/8.f,walkPoses[phase]);
                DrawList draw;draw.SetViewProjection(CreateViewProjectionMatrix(frontCamera,1.f));draw.SetBackfaceCulling(true);
                draw.SetDirectionalLight({{-.55f,-.72f,-.42f},.2f,{1,.9f,.82f,1},3.2f});
                if (!SubmitCharacter(draw,character,walkPoses[phase],characterIndex,character.materials)) {result=6;break;}
                auto capture=CaptureGpu(renderer,draw,output/(character.name+"-walk-"+std::to_string(phase)+".png"));
                if (!capture) {result=7;break;} walkCaptures[phase]=std::move(*capture);
            }
            if (result) break;
            for(std::size_t phase=0;phase<walkCaptures.size();++phase)
                if(!ValidateFrontJointOracle(walkCaptures[phase],character,walkPoses[phase],frontCamera)){result=5;break;}
            if(result)break;
            const WalkMetrics walk=MeasureWalk(character,walkCaptures,walkPoses,frontCamera);
            if (!ValidWalk(walk)) {std::cerr<<character.name<<" walk semantic gate failed pelvis="<<walk.pelvisRange
                <<" knees="<<walk.leftKneeRange<<'/'<<walk.rightKneeRange<<" ankles="<<walk.leftAnkleRange<<'/'<<walk.rightAnkleRange
                <<" toes="<<walk.leftToeRange<<'/'<<walk.rightToeRange<<" fingers="<<walk.leftFingerAngularRange<<'/'<<walk.rightFingerAngularRange
                <<" contacts="<<walk.leftGroundContacts<<'/'<<walk.rightGroundContacts<<" separation="<<walk.contactPhaseSeparation
                <<" gpuMasks="<<walk.distinctGpuMasks<<'\n';result=8;break;}

            // 正式 Gameplay 的 melee-combat 停在 Attack3 約 0.218 phase。以相同真實
            // F2/M2 蒙皮另存四個戰鬥階段，避免只有 walk/idle 畫面為綠、攻擊時手臂
            // 穿入軀幹或消失卻只能等完整 11-shot 才被肉眼發現。
            const AnimationClip3D& attack3=character.animations.clips[std::size_t(FallbackHumanoidClip3D::Attack3)];
            constexpr std::array<float,4> attackPhases{{0.f,.21818182f,.55f,.82f}};
            for(std::size_t attackIndex=0;attackIndex<attackPhases.size();++attackIndex){
                AnimationPose3D attackPose;SampleAnimationClipInto(character.asset.model.skeleton,attack3,attack3.duration*attackPhases[attackIndex],attackPose);
                DrawList draw;draw.SetViewProjection(CreateViewProjectionMatrix(frontCamera,1.f));draw.SetBackfaceCulling(true);
                draw.SetDirectionalLight({{-.55f,-.72f,-.42f},.2f,{1,.9f,.82f,1},3.2f});
                if(!SubmitCharacter(draw,character,attackPose,characterIndex,character.materials)){result=22;break;}
                auto capture=CaptureGpu(renderer,draw,output/(character.name+"-attack3-"+std::to_string(attackIndex)+".png"));
                if(!capture||!ValidateFrontJointOracle(*capture,character,attackPose,frontCamera)){result=23;break;}
            }
            if(result)break;

            // 死亡姿勢是 Gameplay 戰鬥鏡頭的正式可見內容；保存起始、失衡、
            // 接地與靜止四階段，讓單腳直立、肢體交叉或突然消失可被直接比較。
            const AnimationClip3D& death=character.animations.clips[std::size_t(FallbackHumanoidClip3D::Death)];
            constexpr std::array<float,4> deathPhases{{0.f,.45f,.75f,1.f}};
            std::array<AnimationPose3D,4> deathPoses;
            for(std::size_t deathIndex=0;deathIndex<deathPhases.size();++deathIndex){
                auto& deathPose=deathPoses[deathIndex];SampleAnimationClipInto(character.asset.model.skeleton,death,death.duration*deathPhases[deathIndex],deathPose);
                DrawList draw;draw.SetViewProjection(CreateViewProjectionMatrix(frontCamera,1.f));draw.SetBackfaceCulling(true);
                draw.SetDirectionalLight({{-.55f,-.72f,-.42f},.2f,{1,.9f,.82f,1},3.2f});
                if(!SubmitCharacter(draw,character,deathPose,characterIndex,character.materials)){result=24;break;}
                auto capture=CaptureGpu(renderer,draw,output/(character.name+"-death-"+std::to_string(deathIndex)+".png"));
                if(!capture||!ValidateStudio(*capture,characterIndex,character,false)){result=25;break;}
            }
            if(result)break;
            const auto deathArticulation=MeasureDeathArticulation(character,deathPoses);
            if(!deathArticulation.valid||deathArticulation.torso<.10f||
               deathArticulation.activeLimbs<3||deathArticulation.activeDistal<2){
                std::cerr<<character.name<<" death articulation gate failed torso="<<deathArticulation.torso
                    <<" limbs="<<deathArticulation.limbs[0]<<'/'<<deathArticulation.limbs[1]<<'/'
                    <<deathArticulation.limbs[2]<<'/'<<deathArticulation.limbs[3]
                    <<" activeLimbs="<<deathArticulation.activeLimbs
                    <<" distal="<<deathArticulation.distal[0]<<'/'<<deathArticulation.distal[1]<<'/'
                    <<deathArticulation.distal[2]<<'/'<<deathArticulation.distal[3]
                    <<" activeDistal="<<deathArticulation.activeDistal<<'\n';
                result=26;break;
            }

            // 真 GPU 負向：凍結全部關節與手指後，動作檢查必須關閉放行。
            std::array<AnimationPose3D,8> frozenPoses;std::array<Capture,8> frozenCaptures;
            for (std::size_t phase=0;phase<8;++phase) {
                frozenPoses[phase]=walkPoses[0];DrawList draw;draw.SetViewProjection(CreateViewProjectionMatrix(frontCamera,1.f));
                draw.SetBackfaceCulling(true);if(!SubmitCharacter(draw,character,frozenPoses[phase],characterIndex,character.materials)){result=9;break;}
                auto capture=CaptureGpu(renderer,draw,output/(character.name+"-negative-frozen-"+std::to_string(phase)+".png"));
                if(!capture){result=9;break;}frozenCaptures[phase]=std::move(*capture);
            }
            if(result)break;
            if(ValidWalk(MeasureWalk(character,frozenCaptures,frozenPoses,frontCamera))){result=10;break;}

            // 真 GPU 負向：材質坍縮與交換都必須偏離正式 studio 基準。
            auto collapsed=character.materials;
            for(auto& material:collapsed)material.tint={.55f,.55f,.55f,1};
            DrawList collapseDraw;collapseDraw.SetViewProjection(CreateViewProjectionMatrix(studioFrontCamera,1.f));collapseDraw.SetBackfaceCulling(true);
            if(!SubmitCharacter(collapseDraw,character,idle,characterIndex,collapsed)){result=11;break;}
            auto collapseCapture=CaptureGpu(renderer,collapseDraw,output/(character.name+"-negative-material-collapse.png"));
            if(!collapseCapture||ValidateMaterialReference(*collapseCapture,*front,characterIndex)){result=12;break;}
            auto swapped=character.materials;std::swap(swapped[0],swapped[5]);
            DrawList swapDraw;swapDraw.SetViewProjection(CreateViewProjectionMatrix(studioFrontCamera,1.f));swapDraw.SetBackfaceCulling(true);
            if(!SubmitCharacter(swapDraw,character,idle,characterIndex,swapped)){result=13;break;}
            auto swapCapture=CaptureGpu(renderer,swapDraw,output/(character.name+"-negative-material-swap.png"));
            if(!swapCapture||ValidateMaterialReference(*swapCapture,*front,characterIndex)){result=14;break;}

            // 真 GPU 負向：以同一張正式髮絲貼圖的診斷 UV 重複放大 MASK 邊界訊號，再改為 OPAQUE；
            // 正式 studio 畫面仍使用資產原值，這裡只驗證 GPU alpha 分支不可被忽略。
            const std::size_t hairSlot=MaterialSlot(character,{"Hair"});if(hairSlot==SIZE_MAX){result=15;break;}
            auto maskedProbe=character.materials;maskedProbe[hairSlot].baseTextureScale=2.f;
            DrawList maskedProbeDraw;maskedProbeDraw.SetViewProjection(CreateViewProjectionMatrix(studioFrontCamera,1.f));maskedProbeDraw.SetBackfaceCulling(true);
            if(!SubmitCharacter(maskedProbeDraw,character,idle,characterIndex,maskedProbe)){result=15;break;}
            auto maskedProbeCapture=CaptureGpu(renderer,maskedProbeDraw,output/(character.name+"-negative-masked-hair.png"));
            if(!maskedProbeCapture){result=15;break;}
            auto opaque=character.materials;auto opaqueDefinition=character.textureDefinitions[hairSlot];
            opaqueDefinition.alphaMode=MaterialAlphaMode3D::Opaque;opaqueDefinition.alphaCutoff=.5f;
            const auto opaqueTexture=renderer.CreateMaterialTextureSet(opaqueDefinition);
            if(!opaqueTexture){result=15;break;}opaque[hairSlot].textures=opaqueTexture;opaque[hairSlot].alphaMode=MaterialAlphaMode3D::Opaque;opaque[hairSlot].baseTextureScale=2.f;
            DrawList opaqueDraw;opaqueDraw.SetViewProjection(CreateViewProjectionMatrix(studioFrontCamera,1.f));opaqueDraw.SetBackfaceCulling(true);
            if(!SubmitCharacter(opaqueDraw,character,idle,characterIndex,opaque)){result=15;break;}
            auto opaqueCapture=CaptureGpu(renderer,opaqueDraw,output/(character.name+"-negative-opaque-hair.png"));
            const auto hairIdentity=SlotIdentity(characterIndex,hairSlot);
            const std::size_t maskedHair=PixelsFor(maskedProbeCapture->oid,hairIdentity),opaqueHair=opaqueCapture?PixelsFor(opaqueCapture->oid,hairIdentity):0;
            renderer.DestroyMaterialTextureSet(opaqueTexture);renderer.WaitIdle();
            if(!opaqueCapture||!maskedHair||opaqueHair<=maskedHair||opaqueHair-maskedHair<20){std::cerr<<character.name
                <<" opaque hair negative failed masked="<<maskedHair<<" opaque="<<opaqueHair
                <<" captured="<<bool(opaqueCapture)<<'\n';result=16;break;}

            // 真 GPU 負向：手指保持筆直時，手指角度範圍必須失敗；畫面仍由正式蒙皮管線產生。
            auto straightPoses=walkPoses;
            for(auto& pose:straightPoses)for(std::string_view name:{"LeftFingers","RightFingers"}){
                const auto joint=JointByName(character,name);pose.localTransforms[joint]=character.asset.model.skeleton.joints[joint].bindTransform;
                BuildAnimationPoseGlobalMatrices3D(character.asset.model.skeleton,pose);
            }
            std::array<Capture,8> straightCaptures;
            for(std::size_t phase=0;phase<8;++phase){DrawList draw;draw.SetViewProjection(CreateViewProjectionMatrix(frontCamera,1.f));draw.SetBackfaceCulling(true);
                if(!SubmitCharacter(draw,character,straightPoses[phase],characterIndex,character.materials)){result=17;break;}
                auto capture=CaptureGpu(renderer,draw,output/(character.name+"-negative-straight-fingers-"+std::to_string(phase)+".png"));
                if(!capture){result=17;break;}straightCaptures[phase]=std::move(*capture);}
            if(result)break;
            if(ValidWalk(MeasureWalk(character,straightCaptures,straightPoses,frontCamera))){result=18;break;}
            std::cout<<character.name<<" actorPixels="<<front->oid.semanticPixelCount
                <<" walkMasks="<<walk.distinctGpuMasks<<" pelvisPx="<<walk.pelvisRange
                <<" kneesPx="<<walk.leftKneeRange<<'/'<<walk.rightKneeRange
                <<" anklesPx="<<walk.leftAnkleRange<<'/'<<walk.rightAnkleRange
                <<" toesPx="<<walk.leftToeRange<<'/'<<walk.rightToeRange
                <<" contactSeparation="<<walk.contactPhaseSeparation
                <<" hairEdgePixels="<<(opaqueHair-maskedHair)<<'\n';
        }
        renderer.WaitIdle();
        for(auto& character:characters){for(auto texture:character.textures)renderer.DestroyMaterialTextureSet(texture);if(character.mesh)renderer.DestroySkinnedMesh(character.mesh);}
        renderer.WaitIdle();renderer.TrimTransientMemory();const auto stats=renderer.GetStats();
        if(!result&&stats.gpuValidationRequested&&(!stats.gpuValidationAvailable||!stats.gpuValidationExecuted||
            !stats.gpuValidationPassed||stats.debugErrorCount||stats.debugWarningCount))result=19;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';result=20;}
    DestroyWindow(window);if(apartment==S_OK||apartment==S_FALSE)CoUninitialize();return result;
}
