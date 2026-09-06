#include <Aevrynd/FirstPersonArmsSkinnedMesh3D.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <new>
#include <string>
#include <string_view>

namespace Aevrynd {
namespace {
enum class Side : std::uint8_t { Left, Right };

std::string NormalizeJointName(std::string_view name) {
    std::string normalized;
    normalized.reserve(name.size());
    for (const unsigned char value : name) {
        if (std::isalnum(value)) normalized.push_back(static_cast<char>(std::tolower(value)));
    }
    return normalized;
}

bool EndsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

struct JointLookup3D {
    FirstPersonArmsMeshStatus3D status{FirstPersonArmsMeshStatus3D::MissingJoint};
    std::uint16_t index{};
};

JointLookup3D FindJoint(const std::vector<std::string>& names, Side side,
                       std::span<const std::string_view> roles) {
    const std::string_view sideName = side == Side::Left ? "left" : "right";
    for (const std::string_view role : roles) {
        std::vector<std::size_t> matches;
        const std::string sideRole = std::string(sideName) + std::string(role);
        const std::string roleSide = std::string(role) + std::string(sideName);
        for (std::size_t index = 0; index < names.size(); ++index) {
            const std::string normalized = NormalizeJointName(names[index]);
            if (normalized == sideRole || normalized == roleSide || EndsWith(normalized, sideRole) ||
                EndsWith(normalized, roleSide)) {
                matches.push_back(index);
            }
        }
        if (matches.size() > 1) return {FirstPersonArmsMeshStatus3D::AmbiguousJoint, {}};
        if (matches.size() == 1) {
            if (matches.front() > std::numeric_limits<std::uint16_t>::max())
                return {FirstPersonArmsMeshStatus3D::InvalidSource, {}};
            return {FirstPersonArmsMeshStatus3D::Complete,
                    static_cast<std::uint16_t>(matches.front())};
        }
    }
    return {};
}

bool IsDescendantOf(const Skeleton3D& skeleton, std::size_t child, std::size_t ancestor) {
    if (child >= skeleton.joints.size() || ancestor >= skeleton.joints.size()) return false;
    std::size_t remaining = skeleton.joints.size();
    int cursor = static_cast<int>(child);
    while (cursor >= 0 && remaining--) {
        if (static_cast<std::size_t>(cursor) == ancestor) return true;
        cursor = skeleton.joints[static_cast<std::size_t>(cursor)].parent;
    }
    return false;
}

struct ArmChain3D {
    FirstPersonArmsMeshStatus3D status{FirstPersonArmsMeshStatus3D::MissingJoint};
    std::vector<std::uint16_t> joints;
};

ArmChain3D ResolveArmChain(const SkinnedModelAssetData& source, Side side) {
    constexpr std::array<std::string_view, 2> Shoulder{"shoulder", "clavicle"};
    constexpr std::array<std::string_view, 2> Upper{"upperarm", "arm"};
    constexpr std::array<std::string_view, 3> Lower{"lowerarm", "forearm", "elbow"};
    constexpr std::array<std::string_view, 2> Hand{"hand", "wrist"};
    const JointLookup3D shoulder = FindJoint(source.jointNames, side, Shoulder);
    const JointLookup3D upper = FindJoint(source.jointNames, side, Upper);
    const JointLookup3D lower = FindJoint(source.jointNames, side, Lower);
    const JointLookup3D hand = FindJoint(source.jointNames, side, Hand);
    for (const JointLookup3D lookup : {shoulder, upper, lower, hand})
        if (lookup.status != FirstPersonArmsMeshStatus3D::Complete) return {lookup.status, {}};
    const Skeleton3D& skeleton = source.model.skeleton;
    if (!IsDescendantOf(skeleton, upper.index, shoulder.index) ||
        !IsDescendantOf(skeleton, lower.index, upper.index) ||
        !IsDescendantOf(skeleton, hand.index, lower.index))
        return {FirstPersonArmsMeshStatus3D::InvalidHierarchy, {}};
    ArmChain3D result{FirstPersonArmsMeshStatus3D::Complete,
                      {shoulder.index, upper.index, lower.index, hand.index}};
    for (std::size_t joint = 0; joint < skeleton.joints.size(); ++joint) {
        if (joint != hand.index && IsDescendantOf(skeleton, joint, hand.index))
            result.joints.push_back(static_cast<std::uint16_t>(joint));
    }
    std::sort(result.joints.begin(), result.joints.end());
    result.joints.erase(std::unique(result.joints.begin(), result.joints.end()), result.joints.end());
    return result;
}

float ArmWeight(const SkinInfluences3D& influence, const std::vector<std::uint16_t>& joints) {
    std::uint32_t weight{};
    for (std::size_t slot = 0; slot < influence.weights.size(); ++slot) {
        if (std::binary_search(joints.begin(), joints.end(), influence.joints[slot]))
            weight += influence.weights[slot];
    }
    return static_cast<float>(weight) / 65535.f;
}
}

FirstPersonArmsCutTopology3D BuildFirstPersonArmsCutTopology3D(
    const SkinnedModelAssetData& source,const FirstPersonArmsMeshResult3D& arms) {
    FirstPersonArmsCutTopology3D result;
    const auto& mesh=source.model.bindMesh;
    if(!source.model.Valid()||!arms.Complete()||!arms.model.Valid()||
       arms.sourceVertexIndices.size()!=arms.model.bindMesh.vertices.size()||
       arms.retainedTriangleProvenance.size()*3!=arms.model.bindMesh.indices.size()||
       !std::isfinite(arms.minimumVertexArmWeight)||arms.minimumVertexArmWeight<=.5f||arms.minimumVertexArmWeight>1.f)return result;
    // 萃取結果必須沿用原骨架與蒙皮座標；不能僅因 bind 頂點相同就信任變造後的切口。
    if(arms.model.skeletonCompatibilityId!=source.model.skeletonCompatibilityId||
       arms.model.skeleton.joints.size()!=source.model.skeleton.joints.size()||
       arms.model.inverseBindMatrices.size()!=source.model.inverseBindMatrices.size())return result;
    const auto sameFloat=[](float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);};
    const auto sameVector=[&](Vec3 a,Vec3 b){return sameFloat(a.x,b.x)&&sameFloat(a.y,b.y)&&sameFloat(a.z,b.z);};
    for(std::size_t index=0;index<source.model.skeleton.joints.size();++index){
        const auto& original=source.model.skeleton.joints[index];
        const auto& derived=arms.model.skeleton.joints[index];
        const auto& a=original.bindTransform;const auto& b=derived.bindTransform;
        if(original.parent!=derived.parent||a.usesQuaternion!=b.usesQuaternion||
           !sameVector(a.translation,b.translation)||!sameVector(a.rotation,b.rotation)||!sameVector(a.scale,b.scale)||
           !sameFloat(a.quaternion.x,b.quaternion.x)||!sameFloat(a.quaternion.y,b.quaternion.y)||
           !sameFloat(a.quaternion.z,b.quaternion.z)||!sameFloat(a.quaternion.w,b.quaternion.w))return result;
        for(std::size_t row=0;row<4;++row)for(std::size_t column=0;column<4;++column)
            if(!sameFloat(source.model.inverseBindMatrices[index][row][column],arms.model.inverseBindMatrices[index][row][column]))return result;
    }
    const auto authoritativeLeft=ResolveArmChain(source,Side::Left);
    const auto authoritativeRight=ResolveArmChain(source,Side::Right);
    if(authoritativeLeft.status!=FirstPersonArmsMeshStatus3D::Complete||authoritativeRight.status!=FirstPersonArmsMeshStatus3D::Complete||
       authoritativeLeft.joints!=arms.leftArmJoints||authoritativeRight.joints!=arms.rightArmJoints)return result;
    using PositionKey=std::array<std::uint32_t,3>;
    using VertexKey=std::array<std::uint32_t,11>;
    using EdgeKey=std::array<std::uint32_t,2>;
    const auto positionKey=[](const Vertex3D& vertex){return PositionKey{
        std::bit_cast<std::uint32_t>(vertex.x==0.f?0.f:vertex.x),
        std::bit_cast<std::uint32_t>(vertex.y==0.f?0.f:vertex.y),
        std::bit_cast<std::uint32_t>(vertex.z==0.f?0.f:vertex.z)};};
    const auto vertexKey=[&](const Vertex3D& vertex,const SkinInfluences3D& influence){
        VertexKey key{};const auto position=positionKey(vertex);std::copy(position.begin(),position.end(),key.begin());
        std::map<std::uint16_t,std::uint32_t> weights;
        for(std::size_t slot=0;slot<influence.weights.size();++slot)if(influence.weights[slot])weights[influence.joints[slot]]+=influence.weights[slot];
        std::size_t cursor=3;for(const auto& [joint,weight]:weights){key[cursor++]=joint;key[cursor++]=weight;}return key;
    };
    const auto ordered=[](std::uint32_t a,std::uint32_t b){return EdgeKey{(std::min)(a,b),(std::max)(a,b)};};
    std::map<VertexKey,std::uint32_t> welded;
    std::map<PositionKey,std::uint32_t> positions;
    std::vector<std::uint32_t> weldIds(mesh.vertices.size()),positionIds(mesh.vertices.size());
    for(std::size_t index=0;index<mesh.vertices.size();++index){
        const auto& vertex=mesh.vertices[index];
        if(!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z))return result;
        weldIds[index]=welded.try_emplace(vertexKey(vertex,source.model.influences[index]),static_cast<std::uint32_t>(welded.size())).first->second;
        positionIds[index]=positions.try_emplace(positionKey(vertex),static_cast<std::uint32_t>(positions.size())).first->second;
    }
    const auto absent=(std::numeric_limits<std::uint32_t>::max)();
    std::vector<std::uint32_t> compact(mesh.vertices.size(),absent);
    for(std::size_t index=0;index<arms.sourceVertexIndices.size();++index){
        const auto original=arms.sourceVertexIndices[index];
        if(original>=mesh.vertices.size()||compact[original]!=absent||
           vertexKey(mesh.vertices[original],source.model.influences[original])!=
           vertexKey(arms.model.bindMesh.vertices[index],arms.model.influences[index]))return result;
        compact[original]=static_cast<std::uint32_t>(index);
    }
    std::vector<int> retained(mesh.indices.size()/3,-1);
    for(std::size_t index=0;index<arms.retainedTriangleProvenance.size();++index){
        const auto& provenance=arms.retainedTriangleProvenance[index];
        const std::size_t offset=provenance.sourceFirstIndex;
        if(offset%3||offset+3>mesh.indices.size()||retained[offset/3]>=0||
           (provenance.side!=FirstPersonArmSide3D::Left&&provenance.side!=FirstPersonArmSide3D::Right))return result;
        for(std::size_t corner=0;corner<3;++corner){
            const auto outputIndex=arms.model.bindMesh.indices[index*3+corner];
            if(outputIndex>=arms.sourceVertexIndices.size()||arms.sourceVertexIndices[outputIndex]!=mesh.indices[offset+corner])return result;
        }
        retained[offset/3]=provenance.side==FirstPersonArmSide3D::Left?0:1;
    }
    struct Edge {std::uint32_t a{},b{},offset{};int side{-1};};
    std::map<EdgeKey,std::vector<Edge>> rawEdges;
    for(std::size_t offset=0;offset<mesh.indices.size();offset+=3){
        const auto a=mesh.indices[offset],b=mesh.indices[offset+1],c=mesh.indices[offset+2];
        bool isLeft=true,isRight=true;
        for(const auto vertex:{a,b,c}){
            isLeft=isLeft&&ArmWeight(source.model.influences[vertex],authoritativeLeft.joints)>=arms.minimumVertexArmWeight;
            isRight=isRight&&ArmWeight(source.model.influences[vertex],authoritativeRight.joints)>=arms.minimumVertexArmWeight;
        }
        const int expectedSide=isLeft?0:isRight?1:-1;
        if(retained[offset/3]!=expectedSide)return result;
        const auto& va=mesh.vertices[a];const auto& vb=mesh.vertices[b];const auto& vc=mesh.vertices[c];
        const Vec3 area=Cross(Vec3{vb.x-va.x,vb.y-va.y,vb.z-va.z},Vec3{vc.x-va.x,vc.y-va.y,vc.z-va.z});
        if(!std::isfinite(Length(area))||Length(area)==0.f){++result.degenerateTriangles;continue;}
        for(const auto edge:std::array<EdgeKey,3>{EdgeKey{a,b},EdgeKey{b,c},EdgeKey{c,a}})
            rawEdges[ordered(edge[0],edge[1])].push_back({edge[0],edge[1],static_cast<std::uint32_t>(offset),retained[offset/3]});
    }
    std::map<EdgeKey,std::vector<Edge>> seamCandidates;
    const auto boundary=[&](const Edge& edge){return FirstPersonArmsBoundaryEdge3D{
        {edge.a,edge.b},{compact[edge.a],compact[edge.b]},edge.offset,
        edge.side==0?FirstPersonArmSide3D::Left:FirstPersonArmSide3D::Right};};
    const auto classifyWarning=[&](const std::vector<Edge>& edges){
        if(std::any_of(edges.begin(),edges.end(),[](const Edge& edge){return edge.side>=0;}))++result.relevantAmbiguousEdges;
        else ++result.rejectedOnlyWarningEdges;
    };
    const auto pairEdges=[&](const Edge& first,const Edge& second,bool seam){
        if(weldIds[first.a]!=weldIds[second.b]||weldIds[first.b]!=weldIds[second.a]){
            ++result.ambiguousEdges;
            if(first.side<0&&second.side<0)++result.rejectedOnlyWarningEdges;
            else if(seam&&first.side>=0&&first.side==second.side&&
                    weldIds[first.a]==weldIds[second.a]&&weldIds[first.b]==weldIds[second.b])
                result.retainedSameDirectionSeams.push_back({{boundary(first),boundary(second)}});
            else ++result.relevantAmbiguousEdges;
            return;
        }
        if(first.side>=0&&second.side>=0){if(first.side!=second.side){++result.ambiguousEdges;++result.relevantAmbiguousEdges;}return;}
        if(first.side<0&&second.side<0)return;
        const auto& kept=first.side>=0?first:second;const auto& removed=first.side<0?first:second;
        if(kept.side==0)++result.leftCutEdges;else ++result.rightCutEdges;
        result.edges.push_back({{kept.a,kept.b},{compact[kept.a],compact[kept.b]},kept.offset,removed.offset,
            kept.side==0?FirstPersonArmSide3D::Left:FirstPersonArmSide3D::Right});
    };
    // 先尊重原始索引鄰接；只對未配對邊尋找位置與權重完全相同的接縫。
    for(const auto& [key,edges]:rawEdges){
        (void)key;
        if(edges.size()==2)pairEdges(edges[0],edges[1],false);
        else if(edges.size()!=1){++result.ambiguousEdges;classifyWarning(edges);}
        else seamCandidates[ordered(positionIds[edges[0].a],positionIds[edges[0].b])].push_back(edges[0]);
    }
    for(const auto& [key,edges]:seamCandidates){
        (void)key;
        if(edges.size()==1){++result.sourceBoundaryEdges;if(edges[0].side>=0){++result.retainedSourceBoundaryEdges;result.retainedBoundaries.push_back(boundary(edges[0]));}}
        else if(edges.size()!=2){++result.ambiguousEdges;classifyWarning(edges);}
        else if(ordered(weldIds[edges[0].a],weldIds[edges[0].b])!=ordered(weldIds[edges[1].a],weldIds[edges[1].b])){++result.influenceMismatchEdges;classifyWarning(edges);}
        else{++result.seamPairs;pairEdges(edges[0],edges[1],true);}
    }
    std::map<std::pair<unsigned,std::uint32_t>,std::uint32_t> degrees;
    for(const auto& edge:result.edges)for(auto vertex:edge.sourceVertices)++degrees[{static_cast<unsigned>(edge.side),weldIds[vertex]}];
    for(const auto& [key,degree]:degrees){(void)key;if(degree!=2)++result.openCutVertices;}
    for(const auto& edge:result.retainedBoundaries)for(auto vertex:edge.sourceVertices)++degrees[{static_cast<unsigned>(edge.side),weldIds[vertex]}];
    for(const auto& [key,degree]:degrees){(void)key;if(degree!=2)++result.combinedOpenVertices;}
    result.surfaceManifold=!result.ambiguousEdges&&!result.influenceMismatchEdges&&!result.degenerateTriangles;
    result.boundaryInventoryComplete=!result.relevantAmbiguousEdges&&!result.degenerateTriangles&&
        !result.combinedOpenVertices&&result.leftCutEdges&&result.rightCutEdges;
    result.status=result.ambiguousEdges||result.influenceMismatchEdges||result.degenerateTriangles||result.openCutVertices?
        FirstPersonArmsCutStatus3D::AmbiguousTopology:(!result.leftCutEdges||!result.rightCutEdges)?FirstPersonArmsCutStatus3D::NoCutBoundary:FirstPersonArmsCutStatus3D::Complete;
    return result;
}

FirstPersonArmsMeshResult3D BuildFirstPersonArmsSkinnedMesh3D(
    const SkinnedModelAssetData& source, const FirstPersonArmsMeshSettings3D& settings) {
    FirstPersonArmsMeshResult3D result;
    if (!source.model.Valid() || source.jointNames.size() != source.model.skeleton.joints.size())
        return result;
    if (!std::isfinite(settings.minimumVertexArmWeight) ||
        settings.minimumVertexArmWeight <= .5f || settings.minimumVertexArmWeight > 1.f) {
        result.status = FirstPersonArmsMeshStatus3D::InvalidSettings;
        return result;
    }
    const ArmChain3D left = ResolveArmChain(source, Side::Left);
    if (left.status != FirstPersonArmsMeshStatus3D::Complete) {
        result.status = left.status;
        return result;
    }
    const ArmChain3D right = ResolveArmChain(source, Side::Right);
    if (right.status != FirstPersonArmsMeshStatus3D::Complete) {
        result.status = right.status;
        return result;
    }
    result.leftArmJoints = left.joints;
    result.rightArmJoints = right.joints;
    result.minimumVertexArmWeight=settings.minimumVertexArmWeight;

    std::vector<SkinnedSubmesh3D> sourceRanges = source.submeshes;
    if (sourceRanges.empty()) {
        sourceRanges.push_back({0, static_cast<std::uint32_t>(source.model.bindMesh.indices.size()), 0, 0});
    }
    std::size_t rangeCursor{};
    for (const SkinnedSubmesh3D& range : sourceRanges) {
        if (range.firstIndex != rangeCursor || range.indexCount == 0 || range.indexCount % 3 != 0 ||
            std::size_t(range.firstIndex) + range.indexCount > source.model.bindMesh.indices.size()) {
            result.status = FirstPersonArmsMeshStatus3D::InvalidSubmeshRange;
            return result;
        }
        rangeCursor += range.indexCount;
    }
    if (rangeCursor != source.model.bindMesh.indices.size()) {
        result.status = FirstPersonArmsMeshStatus3D::InvalidSubmeshRange;
        return result;
    }

    try {
        result.model.skeleton = source.model.skeleton;
        result.model.inverseBindMatrices = source.model.inverseBindMatrices;
        result.model.skeletonCompatibilityId = source.model.skeletonCompatibilityId;
        std::vector<std::uint32_t> remap(source.model.bindMesh.vertices.size(),
                                         std::numeric_limits<std::uint32_t>::max());
        auto appendVertex = [&](std::uint32_t sourceIndex) {
            std::uint32_t& mapped = remap[sourceIndex];
            if (mapped != std::numeric_limits<std::uint32_t>::max()) return mapped;
            mapped = static_cast<std::uint32_t>(result.model.bindMesh.vertices.size());
            result.model.bindMesh.vertices.push_back(source.model.bindMesh.vertices[sourceIndex]);
            result.model.influences.push_back(source.model.influences[sourceIndex]);
            if (!source.model.tangents.empty()) result.model.tangents.push_back(source.model.tangents[sourceIndex]);
            result.sourceVertexIndices.push_back(sourceIndex);
            return mapped;
        };

        for (const SkinnedSubmesh3D& sourceRange : sourceRanges) {
            const std::uint32_t firstOutput =
                static_cast<std::uint32_t>(result.model.bindMesh.indices.size());
            const std::size_t end = std::size_t(sourceRange.firstIndex) + sourceRange.indexCount;
            for (std::size_t offset = sourceRange.firstIndex; offset < end; offset += 3) {
                ++result.stats.sourceTriangles;
                const std::array<std::uint32_t, 3> triangle{
                    source.model.bindMesh.indices[offset], source.model.bindMesh.indices[offset + 1],
                    source.model.bindMesh.indices[offset + 2]};
                bool isLeft = true;
                bool isRight = true;
                for (const std::uint32_t vertex : triangle) {
                    if (vertex >= source.model.influences.size()) {
                        result.status = FirstPersonArmsMeshStatus3D::InvalidInfluences;
                        return result;
                    }
                    const SkinInfluences3D& influences = source.model.influences[vertex];
                    isLeft = isLeft && ArmWeight(influences, result.leftArmJoints) >=
                                           settings.minimumVertexArmWeight;
                    isRight = isRight && ArmWeight(influences, result.rightArmJoints) >=
                                             settings.minimumVertexArmWeight;
                }
                if (!isLeft && !isRight) {
                    ++result.stats.rejectedTriangles;
                    continue;
                }
                if (isLeft) ++result.stats.leftArmTriangles;
                else ++result.stats.rightArmTriangles;
                ++result.stats.retainedTriangles;
                result.retainedTriangleProvenance.push_back({static_cast<std::uint32_t>(offset),
                    isLeft?FirstPersonArmSide3D::Left:FirstPersonArmSide3D::Right});
                for (const std::uint32_t vertex : triangle)
                    result.model.bindMesh.indices.push_back(appendVertex(vertex));
            }
            const std::uint32_t outputCount =
                static_cast<std::uint32_t>(result.model.bindMesh.indices.size()) - firstOutput;
            if (outputCount)
                result.submeshes.push_back(
                    {firstOutput, outputCount, sourceRange.materialSlot, sourceRange.flags});
        }
        if (!result.stats.leftArmTriangles) {
            result.status = FirstPersonArmsMeshStatus3D::EmptyLeftArm;
            return result;
        }
        if (!result.stats.rightArmTriangles) {
            result.status = FirstPersonArmsMeshStatus3D::EmptyRightArm;
            return result;
        }
        if (!result.stats.retainedTriangles || !result.model.Valid()) {
            result.status = FirstPersonArmsMeshStatus3D::EmptyResult;
            return result;
        }
        result.status = FirstPersonArmsMeshStatus3D::Complete;
        result.cutTopology=BuildFirstPersonArmsCutTopology3D(source,result);
        return result;
    } catch (const std::bad_alloc&) {
        result.status = FirstPersonArmsMeshStatus3D::AllocationFailure;
        return result;
    }
}
}
