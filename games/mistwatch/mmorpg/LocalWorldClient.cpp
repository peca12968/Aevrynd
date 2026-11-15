#include "LocalWorldTcp.h"
#include "LocalWorldClient.h"
#ifdef AEVRYND_MISTWATCH_PROFILES
#include "RiverbankProfileWire.h"
#include "RiverbankTerritoryWire.h"
#endif
#include <algorithm>
#include <chrono>
#include <limits>
#include <optional>

namespace Mistwatch {
using namespace Aevrynd;
using Clock = std::chrono::steady_clock;
using namespace LocalWorldTcp;

#ifdef AEVRYND_MISTWATCH_PROFILES
namespace {
unsigned TicketDigit(wchar_t value) {
    if (value >= L'0' && value <= L'9') return static_cast<unsigned>(value - L'0');
    if (value >= L'a' && value <= L'f') return static_cast<unsigned>(value - L'a') + 10;
    throw std::runtime_error("Invalid local Riverbank character ticket");
}

std::optional<RiverbankTicket> ProfileTicketFromEnvironment() {
    wchar_t text[65]{};
    const auto length = GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_PROFILE_TICKET", text, 65);
    if (!length) return std::nullopt;
    if (length != 64) throw std::runtime_error("Invalid local Riverbank character ticket length");
    RiverbankTicket result{};
    for (std::size_t i = 0; i < result.size(); ++i) result[i] = std::byte((TicketDigit(text[i * 2]) << 4u) | TicketDigit(text[i * 2 + 1]));
    return result;
}
}
#endif

struct LocalWorldClient::State {
    Winsock winsock;
    std::optional<Peer> peer;
    std::optional<ReplicationSnapshot3D> snapshot;
    Clock::time_point lastPacket = Clock::now();
    std::uint64_t epoch{}, nextSequence{1}, lastSubmittedTick{};
    std::uint32_t id{};
    bool connected{};
    RiverbankClass requestedClass{RiverbankClass::None};
#ifdef AEVRYND_MISTWATCH_PROFILES
    std::optional<RiverbankTicket> profileTicket = ProfileTicketFromEnvironment();
    bool profileLookupComplete{};
    std::uint64_t nextGuildSequence{1}, pendingGuildSequence{};
    std::optional<RiverbankGuildWire::Response> guildReply;
    std::uint64_t nextTerritorySequence{1}, pendingTerritorySequence{};
    std::optional<TerritoryState> territory;
#endif
#ifdef AEVRYND_MISTWATCH_BATTLE
    std::optional<RiverbankBattleWire::Encounter> encounter;
    std::uint64_t nextActionSequence{1};
#endif

    explicit State(unsigned short port) {
        Socket socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
        const BOOL immediate = TRUE;
        if (!port || !socket || !Nonblocking(socket.Get()) || setsockopt(socket.Get(), IPPROTO_TCP, TCP_NODELAY,
                reinterpret_cast<const char*>(&immediate), sizeof(immediate)) != 0) return;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        connected = connect(socket.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
        if (!connected && WSAGetLastError() != WSAEWOULDBLOCK) return;
        peer.emplace(Peer{std::move(socket), {}, {}, 0});
    }

    Status Poll(std::int16_t x, std::int16_t z) {
        if (!peer) return Status::Disconnected;
        bool waitingForChoice = id != 0 && requestedClass == RiverbankClass::None;
#ifdef AEVRYND_MISTWATCH_PROFILES
        waitingForChoice = waitingForChoice && (!profileTicket || profileLookupComplete);
#endif
        if (!waitingForChoice && Clock::now() - lastPacket >= std::chrono::seconds(5)) return Disconnect();
        if (!connected) {
            const auto socket = peer->socket.Get();
            fd_set ready{}, failed{};
            FD_SET(socket, &ready);
            FD_SET(socket, &failed);
            timeval immediate{};
            const int result = select(0, nullptr, &ready, &failed, &immediate);
            if (result == SOCKET_ERROR || FD_ISSET(socket, &failed)) return Disconnect();
            if (!FD_ISSET(socket, &ready)) return Status::Connecting;
            int error{};
            int size = sizeof(error);
            if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &size) != 0 || error)
                return Disconnect();
            connected = true;
        }
        if (!peer->Read([this](auto bytes) { return Receive(bytes); })) return Disconnect();
        if ((snapshot && !SubmitMovement(x, z)) || !peer->Flush()) return Disconnect();
        if (!id) return Status::Connecting;
#ifdef AEVRYND_MISTWATCH_PROFILES
        if (profileTicket && !profileLookupComplete) return Status::Joining;
#endif
        if (requestedClass == RiverbankClass::None) return Status::ChoosingClass;
        if (!snapshot) return Status::Joining;
#ifdef AEVRYND_MISTWATCH_BATTLE
        if (!encounter) return Status::Joining;
#endif
        return Status::Ready;
    }

    bool ChooseClass(RiverbankClass playerClass) {
        if (!peer || !id || requestedClass != RiverbankClass::None || !IsRiverbankClass(playerClass)) return false;
#ifdef AEVRYND_MISTWATCH_PROFILES
        if (profileTicket && !profileLookupComplete) return false;
        const bool queued = profileTicket ? peer->Queue(RiverbankProfileWire::EncodeRequest(*profileTicket, playerClass)) :
            peer->Queue(EncodeRiverbankClass(playerClass));
#else
        const bool queued = peer->Queue(EncodeRiverbankClass(playerClass));
#endif
        if (!queued) { Disconnect(); return false; }
        requestedClass = playerClass;
        lastPacket = Clock::now();
        return true;
    }

    bool Receive(std::span<const std::byte> bytes) {
#ifdef AEVRYND_MISTWATCH_PROFILES
        if (RiverbankTerritoryWire::HasMagic(bytes)) {
            auto reply = RiverbankTerritoryWire::DecodeState(bytes);
            if (!profileTicket || !reply || !pendingTerritorySequence || reply->sequence != pendingTerritorySequence ||
                (territory && reply->state.revision < territory->revision)) return false;
            pendingTerritorySequence = 0;
            territory = std::move(reply->state);
            lastPacket = Clock::now();
            return true;
        }
        if (RiverbankGuildWire::HasMagic(bytes)) {
            auto response = RiverbankGuildWire::DecodeResponse(bytes);
            if (!profileTicket || !response || !pendingGuildSequence || response->sequence != pendingGuildSequence) return false;
            pendingGuildSequence = 0;
            guildReply = std::move(response);
            lastPacket = Clock::now();
            return true;
        }
        if (RiverbankProfileWire::HasPrefix(bytes)) {
            const auto found = RiverbankProfileWire::DecodeResponse(bytes);
            if (!id || !profileTicket || !found || snapshot ||
                (requestedClass != RiverbankClass::None && *found != requestedClass) ||
                (requestedClass == RiverbankClass::None && profileLookupComplete)) return false;
            profileLookupComplete = true;
            requestedClass = *found;
            lastPacket = Clock::now();
            return true;
        }
#endif
#ifdef AEVRYND_MISTWATCH_BATTLE
        if (RiverbankBattleWire::HasMagic(bytes)) {
            auto next = RiverbankBattleWire::DecodeEncounter(bytes);
            if (!snapshot || !next || next->epoch != epoch || next->owner != id || next->tick != snapshot->serverTick ||
                (encounter && next->tick <= encounter->tick) || next->player.playerClass != requestedClass) return false;
            encounter = std::move(next);
            lastPacket = Clock::now();
            return true;
        }
#endif
        ReplicationMessage3D message;
        if (!DecodeReplicationPacket3D(bytes, message)) return false;
        if (!id) {
            const auto* hello = std::get_if<ReplicationHandshake3D>(&message);
            if (!hello) return false;
            epoch = hello->sessionEpoch;
            id = hello->clientId;
#ifdef AEVRYND_MISTWATCH_PROFILES
            if (profileTicket && !peer->Queue(RiverbankProfileWire::EncodeRequest(*profileTicket, RiverbankClass::None))) return false;
#endif
        } else {
            auto* next = std::get_if<ReplicationSnapshot3D>(&message);
            if (!next || !next->full || next->sessionEpoch != epoch || next->clientId != id
                || (snapshot && next->serverTick <= snapshot->serverTick)
                || std::ranges::count_if(next->entities, [this](const auto& entry) {
                    return entry.id == NetworkEntityId3D{epoch, id} && ReadRiverbankClassFlags(entry.flags) == requestedClass;
                }) != 1 || std::ranges::any_of(next->entities, [this](const auto& entry) {
                    return entry.id.high != epoch || !IsRiverbankClass(ReadRiverbankClassFlags(entry.flags)) ||
                        bool(entry.flags & 1u) != (entry.id.low == id);
                })) return false;
            snapshot = std::move(*next);
        }
        lastPacket = Clock::now();
        return true;
    }

    bool SubmitMovement(std::int16_t x, std::int16_t z) {
#ifdef AEVRYND_MISTWATCH_BATTLE
        if (!encounter || !encounter->player.health) return true;
#endif
        if ((!x && !z) || snapshot->serverTick == lastSubmittedTick) return true;
        // 以收到的伺服器時鐘排程。一次快照最多提出一筆移動，不補送失焦期間的按鍵。
        constexpr std::uint64_t lead = 3;
        if (snapshot->serverTick > (std::numeric_limits<std::uint64_t>::max)() - lead
            || nextSequence == (std::numeric_limits<std::uint64_t>::max)()) return false;
        const ReplicationInputCommand3D input{epoch, snapshot->serverTick + lead, nextSequence, id, x, z, 0, false};
        std::vector<std::byte> bytes;
        if (!EncodeReplicationPacket3D(input, bytes) || !peer->Queue(bytes)) return false;
        ++nextSequence;
        lastSubmittedTick = snapshot->serverTick;
        return true;
    }

    Status Disconnect() {
        peer.reset();
        snapshot.reset();
        id = 0;
        requestedClass = RiverbankClass::None;
#ifdef AEVRYND_MISTWATCH_BATTLE
        encounter.reset();
#endif
#ifdef AEVRYND_MISTWATCH_PROFILES
        pendingGuildSequence = 0;
        guildReply.reset();
        pendingTerritorySequence = 0;
        territory.reset();
#endif
        return Status::Disconnected;
    }
};

LocalWorldClient::LocalWorldClient(unsigned short port) : state_(std::make_unique<State>(port)) {}
LocalWorldClient::~LocalWorldClient() = default;
LocalWorldClient::Status LocalWorldClient::Poll(std::int16_t x, std::int16_t z) { return state_->Poll(x, z); }
bool LocalWorldClient::ChooseClass(RiverbankClass playerClass) { return state_->ChooseClass(playerClass); }
RiverbankClass LocalWorldClient::SelectedClass() const noexcept {
    return state_->snapshot ? state_->requestedClass : RiverbankClass::None;
}
#ifdef AEVRYND_MISTWATCH_BATTLE
bool LocalWorldClient::RequestBattleAction(RiverbankAction action) {
    const auto* battle = Battle();
    if (!battle || !battle->player.health || state_->nextActionSequence == (std::numeric_limits<std::uint64_t>::max)()) return false;
    const auto message = RiverbankBattleWire::EncodeAction({state_->nextActionSequence, 0, action});
    if (!state_->peer->Queue(message)) return false;
    ++state_->nextActionSequence;
    return true;
}

const RiverbankBattleWire::Encounter* LocalWorldClient::Battle() const noexcept {
    return state_->encounter ? &*state_->encounter : nullptr;
}
#endif
#ifdef AEVRYND_MISTWATCH_PROFILES
bool LocalWorldClient::CanUseGuild() const noexcept {
    return state_->peer && state_->profileTicket && state_->snapshot && state_->encounter;
}

bool LocalWorldClient::RequestGuild(GuildAction action, std::uint64_t target, std::string_view name) {
    if (!CanUseGuild() || state_->pendingGuildSequence || state_->nextGuildSequence == (std::numeric_limits<std::uint64_t>::max)())
        return false;
    const auto frame = RiverbankGuildWire::EncodeRequest({state_->nextGuildSequence, action, target, name});
    if (frame.empty() || !state_->peer->Queue(frame)) return false;
    state_->pendingGuildSequence = state_->nextGuildSequence++;
    return true;
}

const RiverbankGuildWire::Response* LocalWorldClient::GuildReply() const noexcept {
    return state_->guildReply ? &*state_->guildReply : nullptr;
}

bool LocalWorldClient::RequestTerritory() {
    if (!CanUseGuild() || state_->pendingTerritorySequence || state_->nextTerritorySequence == (std::numeric_limits<std::uint64_t>::max)())
        return false;
    if (!state_->peer->Queue(RiverbankTerritoryWire::EncodeQuery(state_->nextTerritorySequence))) return false;
    state_->pendingTerritorySequence = state_->nextTerritorySequence++;
    return true;
}

const TerritoryState* LocalWorldClient::Territory() const noexcept {
    return state_->territory ? &*state_->territory : nullptr;
}
#endif
std::uint32_t LocalWorldClient::PlayerId() const noexcept { return state_->id; }
const ReplicationSnapshot3D* LocalWorldClient::Snapshot() const noexcept {
    return state_->snapshot ? &*state_->snapshot : nullptr;
}
}
