#include <Aevrynd/FirstPersonArmsSkinnedMesh3D.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>

using namespace Aevrynd;

namespace {
bool SameMatrices(const std::vector<Matrix4>& left, const std::vector<Matrix4>& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index)
        for (std::size_t row = 0; row < 4; ++row)
            for (std::size_t column = 0; column < 4; ++column)
                if (left[index][row][column] != right[index][row][column]) return false;
    return true;
}

float JointSetWeight(const SkinInfluences3D& influence,
                     const std::vector<std::uint16_t>& joints) {
    std::uint32_t total{};
    for (std::size_t slot = 0; slot < influence.weights.size(); ++slot)
        if (std::find(joints.begin(), joints.end(), influence.joints[slot]) != joints.end())
            total += influence.weights[slot];
    return static_cast<float>(total) / 65535.f;
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

std::vector<std::uint16_t> ForbiddenBodyJoints(const SkinnedModelAssetData& asset) {
    std::vector<std::uint16_t> result;
    for (std::size_t index = 0; index < asset.jointNames.size(); ++index) {
        const std::string name = Lower(asset.jointNames[index]);
        for (const std::string_view token :
             {"root", "hips", "leg", "foot", "spine", "chest", "neck", "head"}) {
            if (name.find(token) != std::string::npos) {
                result.push_back(static_cast<std::uint16_t>(index));
                break;
            }
        }
    }
    return result;
}

using Triangle = std::array<std::uint32_t, 3>;

Triangle Canonical(Triangle triangle) {
    std::sort(triangle.begin(), triangle.end());
    return triangle;
}

bool CheckAsset(const std::filesystem::path& path) {
    const SkinnedModelAssetDetailedResult loaded = LoadSkinnedModelAssetFileDetailed(path);
    if (!loaded) {
        std::cerr << "load: " << loaded.error << '\n';
        return false;
    }
    constexpr float Threshold = .55f;
    const FirstPersonArmsMeshResult3D derived =
        BuildFirstPersonArmsSkinnedMesh3D(loaded.asset, {Threshold});
    if (!derived || !derived.model.Valid() || derived.model.bindMesh.indices.empty() ||
        derived.model.bindMesh.vertices.empty() || derived.stats.leftArmTriangles == 0 ||
        derived.stats.rightArmTriangles == 0 ||
        derived.stats.retainedTriangles * 3 != derived.model.bindMesh.indices.size() ||
        derived.stats.sourceTriangles != loaded.asset.model.bindMesh.indices.size() / 3 ||
        derived.stats.rejectedTriangles + derived.stats.retainedTriangles !=
            derived.stats.sourceTriangles ||
        derived.model.bindMesh.vertices.size() >= loaded.asset.model.bindMesh.vertices.size() ||
        derived.model.skeleton.joints.size() != loaded.asset.model.skeleton.joints.size() ||
        !SameMatrices(derived.model.inverseBindMatrices, loaded.asset.model.inverseBindMatrices) ||
        derived.model.skeletonCompatibilityId != loaded.asset.model.skeletonCompatibilityId ||
        derived.sourceVertexIndices.size() != derived.model.bindMesh.vertices.size() ||
        derived.retainedTriangleProvenance.size()!=derived.stats.retainedTriangles) {
        std::cerr << "derived contract status=" << static_cast<unsigned>(derived.status) << '\n';
        return false;
    }

    // 使用正式骨架的兩個封閉四面體，明確區分萃取切口、原始缺口與 UV 接縫。
    SkinnedModelAssetData fixture=loaded.asset;
    const auto prototype=fixture.model.bindMesh.vertices.front();
    fixture.model.bindMesh.vertices.clear();fixture.model.bindMesh.indices.clear();
    fixture.model.influences.clear();fixture.model.tangents.clear();
    const auto appendFixtureVertex=[&](Vec3 position,std::uint16_t joint){
        auto vertex=prototype;vertex.x=position.x;vertex.y=position.y;vertex.z=position.z;
        fixture.model.bindMesh.vertices.push_back(vertex);SkinInfluences3D skin{};skin.joints[0]=joint;skin.weights[0]=65535;
        fixture.model.influences.push_back(skin);
    };
    for(unsigned side=0;side<2;++side){
        const float x=static_cast<float>(side)*3.f;
        const auto joint=side?derived.rightArmJoints.front():derived.leftArmJoints.front();
        appendFixtureVertex({x,0,0},joint);appendFixtureVertex({x+1,0,0},joint);appendFixtureVertex({x,1,0},joint);appendFixtureVertex({x,0,1},0);
        for(const auto index:std::array<std::uint32_t,12>{0,2,1,0,1,3,1,2,3,2,0,3})fixture.model.bindMesh.indices.push_back(index+side*4);
    }
    fixture.submeshes={{0,24,0,0}};
    const auto closed=BuildFirstPersonArmsSkinnedMesh3D(fixture);
    if(!closed||!closed.cutTopology.Available()||!closed.cutTopology.BoundaryInventoryAvailable()||!closed.cutTopology.surfaceManifold||closed.cutTopology.edges.size()!=6||closed.cutTopology.sourceBoundaryEdges||closed.retainedTriangleProvenance.size()!=2)return false;
    for(const auto& edge:closed.cutTopology.edges){
        if(edge.retainedSourceFirstIndex%12||edge.rejectedSourceFirstIndex%12==0)return false;
        for(std::size_t corner=0;corner<2;++corner)if(closed.sourceVertexIndices[edge.compactVertices[corner]]!=edge.sourceVertices[corner])return false;
    }
    auto corrupted=closed;corrupted.retainedTriangleProvenance.front().sourceFirstIndex=3;
    if(BuildFirstPersonArmsCutTopology3D(fixture,corrupted).status!=FirstPersonArmsCutStatus3D::InvalidProvenance)return false;
    corrupted=closed;corrupted.retainedTriangleProvenance.front().side=FirstPersonArmSide3D::Right;
    if(BuildFirstPersonArmsCutTopology3D(fixture,corrupted).status!=FirstPersonArmsCutStatus3D::InvalidProvenance)return false;
    corrupted=closed;std::swap(corrupted.leftArmJoints,corrupted.rightArmJoints);
    if(BuildFirstPersonArmsCutTopology3D(fixture,corrupted).status!=FirstPersonArmsCutStatus3D::InvalidProvenance)return false;
    // 各變造仍是有效模型，必須因來源身分不一致而拒絕，不靠通用 Valid 偶然攔截。
    const auto rejectsModelMutation=[&](auto mutate){
        auto changed=closed;mutate(changed.model);
        if(!changed.model.Valid())return false;
        const auto topology=BuildFirstPersonArmsCutTopology3D(fixture,changed);
        return topology.status==FirstPersonArmsCutStatus3D::InvalidProvenance&&!topology.BoundaryInventoryAvailable();
    };
    const auto armJoint=closed.leftArmJoints.front();
    if(!rejectsModelMutation([](auto& model){model.skeletonCompatibilityId.value^=1ull;})||
       !rejectsModelMutation([&](auto& model){model.inverseBindMatrices[armJoint][0][3]+=1.f;})||
       !rejectsModelMutation([&](auto& model){model.skeleton.joints[armJoint].parent=-1;})||
       !rejectsModelMutation([&](auto& model){model.skeleton.joints[armJoint].bindTransform.translation.x+=.01f;})||
       !rejectsModelMutation([&](auto& model){model.skeleton.joints[armJoint].bindTransform.rotation.y+=.01f;})||
       !rejectsModelMutation([&](auto& model){model.skeleton.joints[armJoint].bindTransform.scale.z+=.01f;})||
       !rejectsModelMutation([&](auto& model){auto& transform=model.skeleton.joints[armJoint].bindTransform;transform.usesQuaternion=!transform.usesQuaternion;})||
       !rejectsModelMutation([&](auto& model){auto& transform=model.skeleton.joints[armJoint].bindTransform;transform.quaternion.x+=.01f;}))return false;
    corrupted=closed;corrupted.model.bindMesh.indices.erase(corrupted.model.bindMesh.indices.begin(),corrupted.model.bindMesh.indices.begin()+3);
    corrupted.retainedTriangleProvenance.erase(corrupted.retainedTriangleProvenance.begin());
    if(BuildFirstPersonArmsCutTopology3D(fixture,corrupted).BoundaryInventoryAvailable())return false;
    auto seam=fixture;
    for(std::uint32_t index=0;index<2;++index){auto vertex=seam.model.bindMesh.vertices[index];vertex.u+=.25f;seam.model.bindMesh.vertices.push_back(vertex);seam.model.influences.push_back(seam.model.influences[index]);}
    seam.model.bindMesh.indices[3]=8;seam.model.bindMesh.indices[4]=9;
    const auto joined=BuildFirstPersonArmsSkinnedMesh3D(seam);
    if(!joined||!joined.cutTopology.Available()||joined.cutTopology.edges.size()!=6||joined.cutTopology.seamPairs!=3)return false;
    seam.model.influences[8].joints[0]=0;
    const auto mismatched=BuildFirstPersonArmsSkinnedMesh3D(seam);
    if(!mismatched||mismatched.cutTopology.Available()||mismatched.cutTopology.BoundaryInventoryAvailable()||!mismatched.cutTopology.influenceMismatchEdges)return false;
    auto open=fixture;open.model.bindMesh.indices.erase(open.model.bindMesh.indices.begin()+9,open.model.bindMesh.indices.begin()+12);open.submeshes[0].indexCount-=3;
    const auto sourceHole=BuildFirstPersonArmsSkinnedMesh3D(open);
    if(!sourceHole||!sourceHole.cutTopology.sourceBoundaryEdges||sourceHole.cutTopology.Available()||
       !sourceHole.cutTopology.BoundaryInventoryAvailable()||sourceHole.cutTopology.retainedBoundaries.empty()||sourceHole.cutTopology.combinedOpenVertices)return false;
    auto nonmanifold=fixture;nonmanifold.model.bindMesh.indices.insert(nonmanifold.model.bindMesh.indices.end(),{0,2,1});nonmanifold.submeshes[0].indexCount+=3;
    const auto multiple=BuildFirstPersonArmsSkinnedMesh3D(nonmanifold);
    if(!multiple||multiple.cutTopology.Available()||multiple.cutTopology.BoundaryInventoryAvailable()||!multiple.cutTopology.ambiguousEdges)return false;
    auto degenerate=fixture;degenerate.model.bindMesh.indices[3]=degenerate.model.bindMesh.indices[4];
    const auto collapsed=BuildFirstPersonArmsSkinnedMesh3D(degenerate);
    if(!collapsed||collapsed.cutTopology.Available()||!collapsed.cutTopology.degenerateTriangles)return false;

    if(!derived.cutTopology.BoundaryInventoryAvailable()||derived.cutTopology.surfaceManifold||derived.cutTopology.Available()||
       derived.cutTopology.retainedBoundaries.size()!=derived.cutTopology.retainedSourceBoundaryEdges||
       derived.cutTopology.retainedSameDirectionSeams.empty()||derived.cutTopology.combinedOpenVertices||derived.cutTopology.relevantAmbiguousEdges)return false;
    const auto verifyBoundary=[&](const FirstPersonArmsBoundaryEdge3D& edge){
        if(std::size_t(edge.sourceFirstIndex)+3>loaded.asset.model.bindMesh.indices.size())return false;
        for(std::size_t corner=0;corner<2;++corner){
            if(edge.compactVertices[corner]>=derived.sourceVertexIndices.size()||derived.sourceVertexIndices[edge.compactVertices[corner]]!=edge.sourceVertices[corner])return false;
            const auto begin=loaded.asset.model.bindMesh.indices.begin()+edge.sourceFirstIndex;
            if(std::find(begin,begin+3,edge.sourceVertices[corner])==begin+3)return false;
        }
        return true;
    };
    for(const auto& edge:derived.cutTopology.retainedBoundaries)if(!verifyBoundary(edge))return false;
    for(const auto& seamPair:derived.cutTopology.retainedSameDirectionSeams){
        if(!verifyBoundary(seamPair.edges[0])||!verifyBoundary(seamPair.edges[1])||seamPair.edges[0].side!=seamPair.edges[1].side)return false;
        for(std::size_t corner=0;corner<2;++corner){
            const auto& a=loaded.asset.model.bindMesh.vertices[seamPair.edges[0].sourceVertices[corner]];
            const auto& b=loaded.asset.model.bindMesh.vertices[seamPair.edges[1].sourceVertices[corner]];
            if(a.x!=b.x||a.y!=b.y||a.z!=b.z)return false;
        }
    }

    std::uint32_t rangeCursor{};
    for (const SkinnedSubmesh3D& range : derived.submeshes) {
        if (range.firstIndex != rangeCursor || !range.indexCount || range.indexCount % 3 ||
            std::size_t(range.firstIndex) + range.indexCount >
                derived.model.bindMesh.indices.size() ||
            range.materialSlot >= loaded.asset.materialSlotNames.size())
            return false;
        rangeCursor += range.indexCount;
    }
    if (rangeCursor != derived.model.bindMesh.indices.size()) return false;

    const MeshBounds3D bounds = derived.model.bindMesh.Bounds();
    if (!std::isfinite(bounds.center.x) || !std::isfinite(bounds.center.y) ||
        !std::isfinite(bounds.center.z) || !std::isfinite(bounds.radius) ||
        !std::isfinite(bounds.extents.x) || !std::isfinite(bounds.extents.y) ||
        !std::isfinite(bounds.extents.z) || bounds.radius <= 0.f || bounds.extents.x <= 0.f ||
        bounds.extents.y <= 0.f || bounds.extents.z <= 0.f)
        return false;

    const std::vector<std::uint16_t> forbidden = ForbiddenBodyJoints(loaded.asset);
    std::set<Triangle> forbiddenSourceTriangles;
    const auto& sourceIndices = loaded.asset.model.bindMesh.indices;
    for (std::size_t offset = 0; offset < sourceIndices.size(); offset += 3) {
        bool allForbidden = true;
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const SkinInfluences3D& influence =
                loaded.asset.model.influences[sourceIndices[offset + corner]];
            allForbidden = allForbidden && JointSetWeight(influence, forbidden) >= Threshold;
        }
        if (allForbidden)
            forbiddenSourceTriangles.insert(Canonical(
                {sourceIndices[offset], sourceIndices[offset + 1], sourceIndices[offset + 2]}));
    }
    if (forbiddenSourceTriangles.empty()) return false;

    for (std::size_t offset = 0; offset < derived.model.bindMesh.indices.size(); offset += 3) {
        bool left = true;
        bool right = true;
        Triangle original{};
        for (std::size_t corner = 0; corner < 3; ++corner) {
            const std::uint32_t compact = derived.model.bindMesh.indices[offset + corner];
            if (compact >= derived.model.influences.size()) return false;
            const SkinInfluences3D& influence = derived.model.influences[compact];
            left = left && JointSetWeight(influence, derived.leftArmJoints) >= Threshold;
            right = right && JointSetWeight(influence, derived.rightArmJoints) >= Threshold;
            original[corner] = derived.sourceVertexIndices[compact];
        }
        if ((!left && !right) || forbiddenSourceTriangles.contains(Canonical(original))) return false;
        const auto& provenance=derived.retainedTriangleProvenance[offset/3];
        for(std::size_t corner=0;corner<3;++corner)if(sourceIndices[provenance.sourceFirstIndex+corner]!=original[corner])return false;
    }

    SkinnedModelAssetData missing = loaded.asset;
    const auto hand = std::find(missing.jointNames.begin(), missing.jointNames.end(), "LeftHand");
    if (hand == missing.jointNames.end()) return false;
    *hand = "MissingRequiredJoint";
    const FirstPersonArmsMeshResult3D rejected = BuildFirstPersonArmsSkinnedMesh3D(missing);
    if (rejected.status != FirstPersonArmsMeshStatus3D::MissingJoint || rejected.Complete())
        return false;

    std::cout << path.filename().string() << " sourceTriangles=" << derived.stats.sourceTriangles
              << " retained=" << derived.stats.retainedTriangles
              << " left=" << derived.stats.leftArmTriangles
              << " right=" << derived.stats.rightArmTriangles
              << " vertices=" << derived.model.bindMesh.vertices.size() << '\n';
    std::cout<<"ARMS_CUT_TOPOLOGY status="<<static_cast<unsigned>(derived.cutTopology.status)<<" edges="<<derived.cutTopology.edges.size()
        <<" sourceBoundary="<<derived.cutTopology.sourceBoundaryEdges<<" ambiguous="<<derived.cutTopology.ambiguousEdges
        <<" influenceMismatch="<<derived.cutTopology.influenceMismatchEdges<<" openCutVertices="<<derived.cutTopology.openCutVertices
        <<" retainedBoundary="<<derived.cutTopology.retainedBoundaries.size()<<" retainedSameDirectionSeams="<<derived.cutTopology.retainedSameDirectionSeams.size()
        <<" combinedOpen="<<derived.cutTopology.combinedOpenVertices<<" inventoryAvailable="<<derived.cutTopology.BoundaryInventoryAvailable()
        <<" surfaceManifold="<<derived.cutTopology.surfaceManifold<<'\n';
    return true;
}
}

int main() {
    const std::filesystem::path root{AEVRYND_SOURCE_DIR};
    const std::filesystem::path assets = root / "games" / "character_lab" / "assets" /
                                         "model-packs";
    if (!CheckAsset(assets / "realistic-female" / "realistic-female.aeskin")) return 1;
    if (!CheckAsset(assets / "realistic-male" / "realistic-male.aeskin")) return 2;
    return 0;
}
