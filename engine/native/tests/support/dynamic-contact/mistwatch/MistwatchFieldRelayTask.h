#pragma once
#include <array>

namespace Mistwatch::FieldRelayTask {
inline constexpr unsigned WestRelay=0, NorthWestRelay=2;
inline constexpr unsigned WestCircuitBit=1, NorthWestRouteBit=2, AllTasks=3;
inline constexpr unsigned PatchOptions=3, RouteOptions=3;
enum class CircuitNode : unsigned { Source, ConnectorA, ConnectorB, Receiver, Ground };
enum class CircuitStatus : unsigned { Invalid, Connected, Grounded, Loop };
enum class ExtractionRoute : unsigned { River, RepairedBridge, UnknownNorth };
struct CircuitTrace {
    CircuitStatus status{CircuitStatus::Invalid};
    std::array<CircuitNode,6> path{};
    unsigned pathLength{};
};
inline constexpr std::array<CircuitNode,3> PatchA{
    CircuitNode::Ground,CircuitNode::ConnectorB,CircuitNode::Source};
inline constexpr std::array<CircuitNode,3> PatchB{
    CircuitNode::Receiver,CircuitNode::Ground,CircuitNode::ConnectorA};

// 固定有向圖：電源先到 A；兩個接頭可通到中繼、接地或回路，不用隱藏密碼判斷。
inline constexpr CircuitTrace TraceCircuit(unsigned patchA,unsigned patchB) noexcept {
    if(patchA>=PatchOptions||patchB>=PatchOptions)return {};
    CircuitTrace trace;
    CircuitNode node=CircuitNode::Source;
    unsigned visited{};
    for(unsigned step=0;step<6;++step) {
        trace.path[trace.pathLength++]=node;
        if(node==CircuitNode::Receiver){trace.status=CircuitStatus::Connected;return trace;}
        if(node==CircuitNode::Ground){trace.status=CircuitStatus::Grounded;return trace;}
        const unsigned bit=1u<<static_cast<unsigned>(node);
        if(visited&bit){trace.status=CircuitStatus::Loop;return trace;}
        visited|=bit;
        switch(node) {
        case CircuitNode::Source:node=CircuitNode::ConnectorA;break;
        case CircuitNode::ConnectorA:node=PatchA[patchA];break;
        case CircuitNode::ConnectorB:node=PatchB[patchB];break;
        default:return trace;
        }
    }
    return trace;
}
inline constexpr bool CircuitConnected(unsigned patchA,unsigned patchB) noexcept {
    return TraceCircuit(patchA,patchB).status==CircuitStatus::Connected;
}
// 已修橋是既有旅程的實際證據；涉水與未知捷徑都不宣稱可安全進谷。
inline constexpr bool SafeExtractionRoute(unsigned route,bool bridgeRepaired) noexcept {
    return bridgeRepaired&&route==static_cast<unsigned>(ExtractionRoute::RepairedBridge);
}
}
