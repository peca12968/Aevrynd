#pragma once
#include "Memory.h"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>

namespace Aevrynd {
struct NetworkEndpoint3D {std::uint32_t value{};explicit constexpr operator bool()const{return value!=0;}constexpr auto operator<=>(const NetworkEndpoint3D&)const=default;};
struct NetworkPacket3D {NetworkEndpoint3D source{},destination{};std::uint64_t deliveryTick{};std::vector<std::byte> bytes{};};
struct LoopbackNetworkSettings3D {
 std::size_t maximumEndpoints{8},maximumQueuedPackets{4096},maximumQueuedBytes{4*1024*1024},maximumPacketBytes{64*1024};
 std::uint32_t lossPermille{},duplicatePermille{},reorderPermille{},minimumJitterTicks{},maximumJitterTicks{},burstPeriodTicks{},burstLengthTicks{};
 std::uint64_t seed{0x9e3779b97f4a7c15ull};
};
struct LoopbackNetworkStats3D {std::size_t endpoints{},queuedPackets{},queuedBytes{},peakQueuedPackets{},peakQueuedBytes{};std::uint64_t sent{},delivered{},lost{},duplicated{},reordered{},backpressureRejected{},invalidRejected{},advancedTicks{};};

class LoopbackNetworkTransport3D {
public:
 explicit LoopbackNetworkTransport3D(LoopbackNetworkSettings3D settings={});
 bool RegisterEndpoint(NetworkEndpoint3D endpoint);
 bool UnregisterEndpoint(NetworkEndpoint3D endpoint);
 bool Send(NetworkEndpoint3D source,NetworkEndpoint3D destination,std::span<const std::byte> bytes);
 void AdvanceTo(std::uint64_t tick);
 bool TryReceive(NetworkEndpoint3D endpoint,NetworkPacket3D& packet);
 void Reset();
 std::size_t TrimUnusedMemory();
 LoopbackNetworkStats3D Stats()const{return stats_;}
 std::size_t PacketCapacity()const noexcept{return settings_.maximumQueuedPackets;}
 std::size_t ByteCapacity()const noexcept{return settings_.maximumQueuedBytes;}
private:
 struct Queued {NetworkPacket3D packet{};std::uint64_t order{};};
 std::uint64_t Random()noexcept;
 bool Enqueue(NetworkEndpoint3D source,NetworkEndpoint3D destination,std::span<const std::byte> bytes,std::uint64_t delay,bool duplicate);
 LoopbackNetworkSettings3D settings_{};std::vector<NetworkEndpoint3D> endpoints_{};std::deque<Queued> queue_{};std::uint64_t state_{},tick_{},nextOrder_{1};LoopbackNetworkStats3D stats_{};
};
}
