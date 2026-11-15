#include "LocalWorldTcp.h"
#include "RiverbankClass.h"
#ifdef AEVRYND_MISTWATCH_BATTLE
#include "RiverbankBattleWire.h"
#endif
#ifdef AEVRYND_MISTWATCH_PROFILES
#include "RiverbankProfileWire.h"
#include "RiverbankGuildWire.h"
#include "RiverbankTerritoryWire.h"
#include "RiverbankExpeditionWire.h"
#include "RiverbankPartyWire.h"
#include "RiverbankGearWire.h"
#endif
#include <Aevrynd/ReplicationRuntime3D.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

namespace {
using namespace Aevrynd;
using Mistwatch::RiverbankClass;
using namespace Mistwatch::LocalWorldTcp;
using Clock = std::chrono::steady_clock;
constexpr NetworkEndpoint3D ServerEndpoint{1};
constexpr std::size_t MaxClients = 8;
constexpr auto TickDuration = std::chrono::milliseconds(50);

class WorldServer {
public:
    explicit WorldServer(unsigned short port)
        : listener_(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)), epoch_(MakeEpoch()),
          transport_({MaxClients + 1}), world_(transport_, ServerEndpoint, epoch_, {20, MaxClients}) {
#ifdef AEVRYND_MISTWATCH_PROFILES
        wchar_t database[32768]{};
        const auto length = GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_PROFILE_DB", database, 32768);
        if (length >= 32768) throw std::runtime_error("Riverbank profile database path is too long");
        if (length) {
            profiles_ = std::make_unique<Mistwatch::RiverbankProfileStore>(std::filesystem::path(database));
            territoryOwner_ = profiles_->ReadTerritoryOwner();
            territory_.ApplyOwnership(territoryOwner_.owner);
        }
#endif
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        const BOOL exclusive = TRUE;
        if (!listener_ || setsockopt(listener_.Get(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) != 0
            || bind(listener_.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0
            || listen(listener_.Get(), static_cast<int>(MaxClients)) != 0 || !Nonblocking(listener_.Get())
            || !transport_.RegisterEndpoint(ServerEndpoint)) throw std::runtime_error("Cannot start local world listener");
        int size = sizeof(address);
        if (getsockname(listener_.Get(), reinterpret_cast<sockaddr*>(&address), &size) != 0)
            throw std::runtime_error("Cannot read local world endpoint");
        std::cout << "READY port=" << ntohs(address.sin_port) << " epoch=" << epoch_ << std::endl;
    }

    void Run() {
        auto next = Clock::now();
        for (;;) {
            Accept();
            for (auto it = peers_.begin(); it != peers_.end();) {
                const auto id = it->first;
                if (it->second.transport.Read([&](auto bytes) { return Handle(id, it->second, bytes); })) ++it;
                else it = Remove(it);
            }
            transport_.AdvanceTo(++tick_);
            world_.Tick();
#ifdef AEVRYND_MISTWATCH_BATTLE
#ifdef AEVRYND_MISTWATCH_PROFILES
            std::array<Mistwatch::RiverbankEncounter::SupportMembership, MaxClients> supportGroups{};
            std::size_t grouped = 0;
#endif
            for (const auto& [id, peer] : peers_) {
                if (peer.playerClass == RiverbankClass::None) continue;
#ifdef AEVRYND_MISTWATCH_PROFILES
                const auto members = party_.Members(peer.profileId);
                if (!members.empty()) supportGroups[grouped++] = {id, static_cast<std::uint64_t>(members.front())};
#endif
                Vec3 local{};
                const auto player = world_.Player(id);
                if (!player || !ToLocalPosition3D(player->position, WorldOrigin3D{}, local) ||
                    !encounter_.SetPosition(id, {local.x, local.z})) throw std::runtime_error("Cannot bind combat to world position");
            }
#ifdef AEVRYND_MISTWATCH_PROFILES
            encounter_.Tick(std::span(supportGroups).first(grouped));
#else
            encounter_.Tick();
#endif
#endif
#ifdef AEVRYND_MISTWATCH_PROFILES
            CommitRewards();
            TickTerritory();
            ExpireProfiles();
            party_.ExpireInvites(tick_);
#endif
            for (auto it = peers_.begin(); it != peers_.end();) {
                NetworkPacket3D packet;
                bool ok = true;
                while (ok && transport_.TryReceive(Endpoint(it->first), packet)) ok = it->second.transport.Queue(packet.bytes);
#ifdef AEVRYND_MISTWATCH_BATTLE
                if (const auto player = encounter_.Player(it->first); ok && player) {
                    ok = world_.SetClientMovementEnabled(it->first, player->health != 0) &&
                        it->second.transport.Queue(Mistwatch::RiverbankBattleWire::EncodeEncounter({epoch_, tick_, it->first, *player, encounter_.Boss()}));
                }
#endif
                if (ok && it->second.transport.Flush()) ++it;
                else it = Remove(it);
            }
            next += TickDuration;
            const auto now = Clock::now();
            if (next < now) next = now + TickDuration;
            std::this_thread::sleep_until(next);
        }
    }

private:
    struct Client {
        Peer transport;
        RiverbankClass playerClass{RiverbankClass::None};
#ifdef AEVRYND_MISTWATCH_PROFILES
        std::int64_t profileId{};
        unsigned savedRewards{};
        std::uint64_t guildSequence{}, guildRequestTick{(std::numeric_limits<std::uint64_t>::max)()};
        std::int64_t guildId{};
        Mistwatch::GuildRank rank{};
        std::uint64_t territorySequence{}, territoryQueryTick{(std::numeric_limits<std::uint64_t>::max)()};
        std::uint64_t gearSequence{}, gearRequestTick{(std::numeric_limits<std::uint64_t>::max)()};
        std::uint64_t partySequence{}, partyRequestTick{(std::numeric_limits<std::uint64_t>::max)()};
        std::uint64_t expeditionSequence{}, expeditionQueryTick{(std::numeric_limits<std::uint64_t>::max)()};
#endif
    };
    using Peers = std::map<std::uint32_t, Client>;
    static std::uint64_t MakeEpoch() {
        static_assert(sizeof(std::uint64_t) >= sizeof(Clock::rep));
        const auto ticks = std::chrono::system_clock::now().time_since_epoch().count();
        return static_cast<std::uint64_t>(ticks) | 1u;
    }
    static NetworkEndpoint3D Endpoint(std::uint32_t id) { return {id + 1}; }

    void Accept() {
        for (std::size_t attempt = 0; attempt < MaxClients; ++attempt) {
            Socket socket(accept(listener_.Get(), nullptr, nullptr));
            if (!socket) {
                if (WSAGetLastError() == WSAEWOULDBLOCK) return;
                throw std::runtime_error("Local accept failed");
            }
            const BOOL immediate = TRUE;
            if (peers_.size() == MaxClients || nextId_ == (std::numeric_limits<std::uint32_t>::max)()
                || !Nonblocking(socket.Get()) || setsockopt(socket.Get(), IPPROTO_TCP, TCP_NODELAY,
                    reinterpret_cast<const char*>(&immediate), sizeof(immediate)) != 0) continue;
            const auto id = nextId_++;
            if (!transport_.RegisterEndpoint(Endpoint(id))) continue;
            auto [it, inserted] = peers_.emplace(id, Client{Peer{std::move(socket), {}, {}, 0}});
            if (!inserted) throw std::runtime_error("Duplicate world connection identity");
            std::vector<std::byte> handshake;
            if (!EncodeReplicationPacket3D(ReplicationHandshake3D{epoch_, id}, handshake)
                || !it->second.transport.Queue(handshake)) Remove(it);
        }
    }

    bool Handle(std::uint32_t id, Client& client, std::span<const std::byte> bytes) {
        if (client.playerClass == RiverbankClass::None) {
#ifdef AEVRYND_MISTWATCH_PROFILES
            if (profiles_) return HandleProfile(id, client, bytes);
#endif
            const auto selected = Mistwatch::DecodeRiverbankClass(bytes);
            return JoinWorld(id, client, selected);
        }
#ifdef AEVRYND_MISTWATCH_PROFILES
        if (Mistwatch::RiverbankGuildWire::HasMagic(bytes)) return HandleGuild(client, bytes);
        if (Mistwatch::RiverbankTerritoryWire::HasMagic(bytes)) return HandleTerritory(client, bytes);
        if (Mistwatch::RiverbankGearWire::HasMagic(bytes)) return HandleGear(id, client, bytes);
        if (Mistwatch::RiverbankPartyWire::HasMagic(bytes)) return HandleParty(client, bytes);
        if (Mistwatch::RiverbankExpeditionWire::HasMagic(bytes)) return HandleExpedition(id, client, bytes);
#endif
#ifdef AEVRYND_MISTWATCH_BATTLE
        if (Mistwatch::RiverbankBattleWire::HasMagic(bytes)) {
            const auto action = Mistwatch::RiverbankBattleWire::DecodeAction(bytes);
            if (!action) return false;
            // Cooldown, range and duplicates are ordinary rejected actions, not connection errors.
            (void)encounter_.Submit(id, action->sequence, action->kind, action->ally);
            return true;
        }
#endif
        ReplicationMessage3D message;
        if (!DecodeReplicationPacket3D(bytes, message)) return false;
        auto* input = std::get_if<ReplicationInputCommand3D>(&message);
        // 依連線身分收件，位置狀態只由伺服器的世界更新產生。
        if (!input || input->clientId != id || input->sessionEpoch != epoch_) return false;
        const double length = std::hypot(double(input->moveX), double(input->moveZ));
        if (length > 32767.) {
            input->moveX = static_cast<std::int16_t>(input->moveX * 32767. / length);
            input->moveZ = static_cast<std::int16_t>(input->moveZ * 32767. / length);
        }
        std::vector<std::byte> normalized;
        return EncodeReplicationPacket3D(message, normalized) && transport_.Send(Endpoint(id), ServerEndpoint, normalized);
    }

    bool JoinWorld(std::uint32_t id, Client& client, RiverbankClass selected, GlobalPosition3D position = {}) {
        if (!Mistwatch::IsRiverbankClass(selected) || !world_.AddClient(id, Endpoint(id), position) ||
            !world_.SetClientEntityFlags(id, Mistwatch::RiverbankClassFlags(selected))) return false;
#ifdef AEVRYND_MISTWATCH_BATTLE
        if (!encounter_.Join(id, selected)) return false;
#endif
        client.playerClass = selected;
        return true;
    }

#ifdef AEVRYND_MISTWATCH_PROFILES
    static constexpr std::uint64_t ResumeWindowTicks = 120 * Mistwatch::RiverbankEncounter::TicksPerSecond;
    static constexpr std::size_t ResumeCapacity = 4 * MaxClients;
    struct SuspendedProfile {
        GlobalPosition3D position;
        Mistwatch::SuspendedRiverbankCombat combat;
        std::uint64_t lostTick{};
    };

    bool HandleProfile(std::uint32_t id, Client& client, std::span<const std::byte> bytes) {
        namespace Wire = Mistwatch::RiverbankProfileWire;
        const auto request = Wire::DecodeRequest(bytes);
        if (!request) return false;
        const auto found = request->choice == RiverbankClass::None ? profiles_->Find(request->ticket) :
            profiles_->Create(request->ticket, request->choice);
        if (!found) return request->choice == RiverbankClass::None && client.transport.Queue(Wire::EncodeResponse(RiverbankClass::None));
        if (std::ranges::any_of(peers_, [&](const auto& entry) { return entry.second.profileId == found->id; })) return false;
        auto suspended = suspendedProfiles_.find(found->id);
        if (suspended != suspendedProfiles_.end() && tick_ - suspended->second.lostTick > ResumeWindowTicks) {
            party_.Forget(found->id);
            suspendedProfiles_.erase(suspended);
            suspended = suspendedProfiles_.end();
        }
        const bool resuming = suspended != suspendedProfiles_.end();
        if (!JoinWorld(id, client, found->playerClass, resuming ? suspended->second.position : GlobalPosition3D{}) ||
            !encounter_.RestoreRewards(id, found->rewards) ||
            !encounter_.SetBossGear(id, profiles_->ReadBossGear(found->id).equipped)) return false;
        if (resuming) {
            const auto& saved = suspended->second;
            if (!encounter_.RestoreCombat(id, saved.combat, tick_ - saved.lostTick) ||
                !world_.SetClientMovementEnabled(id, saved.combat.status.health != 0)) return false;
            suspendedProfiles_.erase(suspended);
        }
        client.profileId = found->id;
        client.savedRewards = found->rewards;
        LoadMembership(client);
        return client.transport.Queue(Wire::EncodeResponse(found->playerClass));
    }

    void ExpireProfiles() {
        std::erase_if(suspendedProfiles_, [&](const auto& entry) {
            if (tick_ - entry.second.lostTick <= ResumeWindowTicks) return false;
            party_.Forget(entry.first);
            return true;
        });
    }

    void SuspendProfile(std::uint32_t id, const Client& client) {
        if (!client.profileId) return;
        party_.CancelInvites(client.profileId);
        const auto player = world_.Player(id);
        const auto combat = encounter_.Suspend(id);
        if (!player || !combat) {
            party_.Forget(client.profileId);
            return;
        }
        ExpireProfiles();
        if (!suspendedProfiles_.contains(client.profileId) && suspendedProfiles_.size() >= ResumeCapacity) {
            const auto oldest = std::ranges::min_element(suspendedProfiles_, {}, [](const auto& entry) { return entry.second.lostTick; });
            party_.Forget(oldest->first);
            suspendedProfiles_.erase(oldest);
        }
        suspendedProfiles_.insert_or_assign(client.profileId, SuspendedProfile{player->position, *combat, tick_});
    }

    bool HandleGuild(Client& client, std::span<const std::byte> bytes) {
        using Mistwatch::GuildAction;
        using Mistwatch::GuildResult;
        namespace Wire = Mistwatch::RiverbankGuildWire;
        if (!profiles_ || !client.profileId) return false;
        const auto request = Wire::DecodeRequest(bytes);
        if (!request) return false;
        auto result = GuildResult::Rejected;
        if (request->sequence > client.guildSequence) {
            client.guildSequence = request->sequence;
            if (client.guildRequestTick != tick_) {
                client.guildRequestTick = tick_;
                auto target = static_cast<std::int64_t>(request->target);
                switch (request->action) {
                case GuildAction::Invite:
                case GuildAction::Promote:
                case GuildAction::Demote:
                case GuildAction::Transfer: {
                    const auto member = request->target <= (std::numeric_limits<std::uint32_t>::max)() ?
                        peers_.find(static_cast<std::uint32_t>(request->target)) : peers_.end();
                    target = member == peers_.end() ? 0 : member->second.profileId;
                    break;
                }
                default: break;
                }
                result = profiles_->ChangeGuild(client.profileId, request->action, target, request->name);
                if (result == GuildResult::Ok && request->action != GuildAction::View) {
                    for (auto& [id, member] : peers_) if (member.profileId) LoadMembership(member);
                }
            }
        }
        return client.transport.Queue(Wire::EncodeResponse(request->sequence, result, profiles_->ReadGuild(client.profileId)));
    }

    void LoadMembership(Client& client) {
        const auto membership = profiles_->ReadGuild(client.profileId);
        client.guildId = membership.guildId;
        client.rank = membership.rank;
    }

    bool HandleTerritory(Client& client, std::span<const std::byte> bytes) {
        namespace Wire = Mistwatch::RiverbankTerritoryWire;
        const auto sequence = Wire::DecodeQuery(bytes);
        if (!client.profileId || sequence <= client.territorySequence || client.territoryQueryTick == tick_) return false;
        client.territorySequence = sequence;
        client.territoryQueryTick = tick_;
        auto state = territoryOwner_;
        state.challenger = territory_.Challenger();
        state.challengerName = profiles_->GuildName(state.challenger);
        state.progress = territory_.Progress();
        state.contested = territory_.Contested();
        state.rewards = profiles_->TerritoryRewards(client.profileId);
        return client.transport.Queue(Wire::EncodeState(sequence, state));
    }

    bool HandleGear(std::uint32_t id, Client& client, std::span<const std::byte> bytes) {
        namespace Wire = Mistwatch::RiverbankGearWire;
        using Mistwatch::GearResult;
        const auto request = Wire::DecodeRequest(bytes);
        if (!profiles_ || !client.profileId || !request) return false;
        auto result = GearResult::Rejected;
        if (request->sequence > client.gearSequence) {
            client.gearSequence = request->sequence;
            if (client.gearRequestTick != tick_) {
                client.gearRequestTick = tick_;
                result = profiles_->ChangeBossGear(client.profileId, request->action);
            }
        }
        const auto gear = profiles_->ReadBossGear(client.profileId);
        if (!encounter_.SetBossGear(id, gear.equipped)) return false;
        return client.transport.Queue(Wire::EncodeReply({request->sequence, result, gear}));
    }

    bool HandleParty(Client& client, std::span<const std::byte> bytes) {
        namespace Wire = Mistwatch::RiverbankPartyWire;
        using Mistwatch::PartyResult;
        const auto request = Wire::DecodeRequest(bytes);
        if (!request || !client.profileId) return false;
        auto result = PartyResult::Rejected;
        if (request->sequence > client.partySequence) {
            client.partySequence = request->sequence;
            if (client.partyRequestTick != tick_) {
                client.partyRequestTick = tick_;
                const auto target = peers_.find(request->target);
                result = party_.Apply(client.profileId, request->action,
                    target == peers_.end() ? 0 : target->second.profileId, tick_);
            }
        }

        Wire::Reply reply{};
        reply.sequence = request->sequence;
        reply.result = result;
        const auto group = party_.Members(client.profileId);
        reply.members = static_cast<std::uint8_t>(group.size());
        const auto inviter = party_.Inviter(client.profileId, tick_);
        for (const auto& [id, member] : peers_) {
            if (!member.profileId) continue;
            if (member.profileId == inviter) reply.inviter = id;
            if (std::ranges::find(group, member.profileId) == group.end()) continue;
            const auto status = encounter_.Player(id);
            if (!status) return false;
            if (group.front() == member.profileId) reply.leader = id;
            reply.online.push_back({id, status->playerClass, status->health});
        }
        return client.transport.Queue(Wire::EncodeReply(reply));
    }

    bool HandleExpedition(std::uint32_t id, Client& client, std::span<const std::byte> bytes) {
        namespace Wire = Mistwatch::RiverbankExpeditionWire;
        const auto request = Wire::DecodeRequest(bytes);
        if (!request || !client.profileId || request->sequence <= client.expeditionSequence || client.expeditionQueryTick == tick_)
            return false;
        client.expeditionSequence = request->sequence;
        client.expeditionQueryTick = tick_;
        auto result = Mistwatch::ExpeditionResult::Viewed;
        if (request->interact) {
            const auto fighter = encounter_.Player(id);
            const auto player = world_.Player(id);
            Vec3 position{};
            if (!fighter || !player || !ToLocalPosition3D(player->position, WorldOrigin3D{}, position)) return false;
            result = fighter->health ? profiles_->InteractExpedition(client.profileId, {position.x, position.z}) :
                Mistwatch::ExpeditionResult::Downed;
        }
        return client.transport.Queue(Wire::EncodeReply({request->sequence, profiles_->ReadExpedition(client.profileId), result}));
    }

    void TickTerritory() {
        if (!profiles_) return;
        using Area = Mistwatch::RiverbankTerritory;
        std::array<Area::Presence, MaxClients> present{};
        std::size_t count{};
        for (const auto& [id, peer] : peers_) {
            if (!peer.guildId) continue;
            const auto fighter = encounter_.Player(id);
            const auto player = world_.Player(id);
            Aevrynd::Vec3 local{};
            if (fighter && fighter->health && player && ToLocalPosition3D(player->position, WorldOrigin3D{}, local) && Area::Contains(local.x, local.z))
                present[count++] = {peer.profileId, peer.guildId, peer.rank};
        }
        if (const auto recipients = territory_.Tick(std::span(present).first(count)); !recipients.empty()) {
            profiles_->ClaimTerritory(territory_.Owner(), territory_.Challenger(), recipients);
            territoryOwner_ = profiles_->ReadTerritoryOwner();
            territory_.ApplyOwnership(territoryOwner_.owner);
        }
    }

    void CommitRewards() {
        if (!profiles_ || encounter_.Boss().phase != Mistwatch::EncounterPhase::Defeated) return;
        std::array<std::int64_t, MaxClients> eligible{};
        std::array<Client*, MaxClients> credited{};
        std::size_t count{};
        for (auto& [id, client] : peers_) {
            const auto player = encounter_.Player(id);
            if (!client.profileId || !player || player->rewards == client.savedRewards) continue;
            if (player->rewards <= client.savedRewards || player->rewards - client.savedRewards != 1)
                throw std::runtime_error("Unexpected Riverbank reward count");
            eligible[count] = client.profileId;
            credited[count++] = &client;
        }
        if (!count) return;
        profiles_->Award(epoch_, encounter_.Boss().run, std::span(eligible).first(count));
        for (auto* client : std::span(credited).first(count)) ++client->savedRewards;
    }
#endif

    Peers::iterator Remove(Peers::iterator peer) {
#ifdef AEVRYND_MISTWATCH_PROFILES
        SuspendProfile(peer->first, peer->second);
#endif
#ifdef AEVRYND_MISTWATCH_BATTLE
        encounter_.Leave(peer->first);
#endif
        world_.RemoveClient(peer->first);
        transport_.UnregisterEndpoint(Endpoint(peer->first));
        return peers_.erase(peer);
    }

    Socket listener_;
    std::uint64_t epoch_, tick_{};
    std::uint32_t nextId_{1};
    LoopbackNetworkTransport3D transport_;
    ReplicationServer3D world_;
    Peers peers_;
#ifdef AEVRYND_MISTWATCH_BATTLE
    Mistwatch::RiverbankEncounter encounter_;
#endif
#ifdef AEVRYND_MISTWATCH_PROFILES
    std::unique_ptr<Mistwatch::RiverbankProfileStore> profiles_;
    std::map<std::int64_t, SuspendedProfile> suspendedProfiles_;
    Mistwatch::RiverbankTerritory territory_;
    Mistwatch::RiverbankParty party_;
    Mistwatch::TerritoryState territoryOwner_;
#endif
};
} // namespace

int main(int argc, char** argv) {
    try {
        unsigned short port = 0;
        if (argc > 2) throw std::invalid_argument("Usage: MistwatchWorldServer [local-port]");
        if (argc == 2) {
            const std::string_view value(argv[1]);
            const auto result = std::from_chars(value.data(), value.data() + value.size(), port);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
                throw std::invalid_argument("Invalid local port");
        }
        const Winsock winsock;
        WorldServer server(port);
        server.Run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
