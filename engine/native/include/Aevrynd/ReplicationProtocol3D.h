#pragma once
#include "WorldDeltaPersistence3D.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

namespace Aevrynd {
inline constexpr std::uint16_t ReplicationProtocolVersion3D=1;
inline constexpr std::size_t MaximumReplicationPacketBytes3D=64*1024;
struct NetworkEntityId3D {std::uint64_t high{},low{};explicit constexpr operator bool()const{return high||low;}constexpr auto operator<=>(const NetworkEntityId3D&)const=default;};
enum class ReplicationMessageType3D:std::uint8_t {Handshake=1,InputCommand,Snapshot,Ack,MutationRequest,MutationReceipt};
struct ReplicationHandshake3D {std::uint64_t sessionEpoch{};std::uint32_t clientId{};std::uint16_t protocolVersion{ReplicationProtocolVersion3D};};
struct ReplicationInputCommand3D {std::uint64_t sessionEpoch{},tick{},sequence{};std::uint32_t clientId{};std::int16_t moveX{},moveZ{},yaw{};bool jump{},sprint{};};
struct ReplicatedEntityState3D {NetworkEntityId3D id{};GlobalPosition3D position{};Vec3 velocity{};std::uint32_t flags{};};
struct ReplicationSnapshot3D {std::uint64_t sessionEpoch{},serverTick{},baselineTick{},interestEpoch{},originGeneration{};std::uint32_t clientId{};bool full{};std::vector<ReplicatedEntityState3D> entities{};std::vector<NetworkEntityId3D> despawned{};};
struct ReplicationAck3D {std::uint64_t sessionEpoch{},snapshotTick{},inputTick{};std::uint32_t clientId{};};
struct ReplicationMutationRequest3D {std::uint64_t sessionEpoch{},clientRequestId{};std::uint32_t clientId{};WorldDeltaChunkKey3D key{};WorldChunkDeltaOperation3D operation{};};
struct ReplicationMutationReceipt3D {std::uint64_t sessionEpoch{},clientRequestId{},serverTick{},sequence{};std::uint32_t clientId{};bool accepted{},durable{};StableObjectId3D operationId{};};
using ReplicationMessage3D=std::variant<ReplicationHandshake3D,ReplicationInputCommand3D,ReplicationSnapshot3D,ReplicationAck3D,ReplicationMutationRequest3D,ReplicationMutationReceipt3D>;
struct ReplicationCodecLimits3D {std::size_t maximumPacketBytes{MaximumReplicationPacketBytes3D},maximumEntities{2048},maximumDespawns{2048},maximumMutationBytes{32*1024};};
struct ReplicationPacketReceipt3D {ReplicationMessageType3D type{};std::uint16_t version{};std::array<std::byte,32> sha256{};std::uint32_t crc32c{};};
bool EncodeReplicationPacket3D(const ReplicationMessage3D& message,std::vector<std::byte>& bytes,ReplicationPacketReceipt3D* receipt=nullptr,ReplicationCodecLimits3D limits={});
bool DecodeReplicationPacket3D(std::span<const std::byte> bytes,ReplicationMessage3D& message,ReplicationPacketReceipt3D* receipt=nullptr,ReplicationCodecLimits3D limits={});
}
