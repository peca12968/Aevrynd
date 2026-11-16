#include "LocalWorldClient.h"
#include "LocalWorldTcp.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

#ifdef AEVRYND_MISTWATCH_PROFILES
namespace {
int VerifyPartyInvitation(Mistwatch::LocalWorldClient& client) {
    using namespace Mistwatch;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(7);
    bool announced{}, accepted{};
    std::uint64_t lastReply{};
    auto nextView = std::chrono::steady_clock::time_point{};
    while (std::chrono::steady_clock::now() < deadline) {
        const auto status = client.Poll(0, 0);
        if (status == LocalWorldClient::Status::Disconnected) return 20;
        if (status != LocalWorldClient::Status::Ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        if (!announced) {
            announced = true;
            std::cout << "NATIVE_PARTY_READY " << client.PlayerId() << std::endl;
        }
        if (const auto* group = client.Party(); group && group->sequence != lastReply) {
            lastReply = group->sequence;
            if (accepted) {
                if (group->result != PartyResult::Ok || group->members != 4 ||
                    std::ranges::find(group->online, client.PlayerId(), &RiverbankPartyWire::Member::actor) == group->online.end())
                    return 21;
                std::cout << "NATIVE_PARTY_ACCEPT_PASS" << std::endl;
                return 0;
            }
            if (group->inviter) {
                if (!client.RequestParty(PartyAction::Accept, group->inviter)) return 22;
                accepted = true;
            }
        }
        const auto now = std::chrono::steady_clock::now();
        if (!accepted && now >= nextView && client.RequestParty(PartyAction::View))
            nextView = now + std::chrono::milliseconds(100);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 23;
}

int VerifyLiveReconnect(Mistwatch::LocalWorldClient& client) {
    using Status = Mistwatch::LocalWorldClient::Status;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);
    Aevrynd::GlobalPosition3D previous{};
    std::uint64_t stopTick{}, oldId{}, oldEpoch{};
    bool sawReconnect{}, restored{};
    while (std::chrono::steady_clock::now() < deadline) {
        const auto status = client.Poll(stopTick && !restored ? 0 : 32767, 0);
        if (status == Status::Disconnected) return 10;
        if (status == Status::ChoosingClass && (oldId || !client.ChooseClass(Mistwatch::RiverbankClass::Guardian))) return 10;
        sawReconnect |= status == Status::Reconnecting;
        if (status == Status::Ready) {
            const auto* view = client.Snapshot();
            const auto own = std::ranges::find_if(view->entities, [&](const auto& entity) { return entity.id.low == client.PlayerId(); });
            if (own == view->entities.end()) return 11;
            if (!stopTick && own->position.offset.x >= 1.f) stopTick = view->serverTick;
            if (stopTick && !oldId && view->serverTick >= stopTick + 4) {
                oldId = client.PlayerId();
                oldEpoch = view->sessionEpoch;
                previous = own->position;
                std::cout << "READY_FOR_TRANSPORT_DROP " << oldId << std::endl;
            } else if (oldId && client.PlayerId() != oldId) {
                if (!restored) {
                    if (!sawReconnect || view->sessionEpoch != oldEpoch || own->position != previous ||
                        client.SelectedClass() != Mistwatch::RiverbankClass::Guardian) return 12;
                    restored = true;
                } else if (own->position.offset.x > previous.offset.x) {
                    std::cout << "NATIVE_LIVE_RECONNECT_PASS " << client.PlayerId() << std::endl;
                    return 0;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 13;
}
}
#endif

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--tailnet-addresses") {
        using namespace Mistwatch::LocalWorldTcp;
        const Winsock winsock;
        for (const char* value : {"100.64.0.1", "100.127.255.254"})
            if (!TailnetIPv4(value)) return 24;
        for (const char* value : {"", "127.0.0.1", "192.168.1.2", "100.63.0.1", "100.128.0.1", "::1", "example.com"})
            if (TailnetIPv4(value)) return 25;
        std::cout << "RIVERBANK_TAILNET_ADDRESS_VALIDATION_PASS\n";
        return 0;
    }
    const bool guildView = argc == 3 && std::string_view(argv[2]) == "--guild-view";
    const bool reconnect = argc == 3 && std::string_view(argv[2]) == "--live-reconnect";
    const bool party = argc == 3 && std::string_view(argv[2]) == "--party-accept";
    if (argc != 2 && !guildView && !reconnect && !party) return 1;
    unsigned short port{};
    const std::string_view raw(argv[1]);
    const auto parsed = std::from_chars(raw.data(), raw.data() + raw.size(), port);
    if (!port || parsed.ec != std::errc{} || parsed.ptr != raw.data() + raw.size()) return 2;
    Mistwatch::LocalWorldClient client(port);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
#ifdef AEVRYND_MISTWATCH_PROFILES
    if (party) return VerifyPartyInvitation(client);
    if (reconnect) return VerifyLiveReconnect(client);
    if (guildView) {
        bool requested = false;
        while (std::chrono::steady_clock::now() < deadline) {
            if (client.Poll(0, 0) == Mistwatch::LocalWorldClient::Status::Disconnected) return 6;
            if (const auto* reply = client.GuildReply()) {
                if (reply->sequence != 1 || reply->result != Mistwatch::GuildResult::Ok ||
                    reply->state.rank != Mistwatch::GuildRank::Member || reply->state.name != "North Watch") return 7;
                std::cout << "NATIVE_GUILD_VIEW_PASS\n";
                return 0;
            }
            if (!requested && client.CanUseGuild()) {
                if (!client.RequestGuild(Mistwatch::GuildAction::View) || client.RequestGuild(Mistwatch::GuildAction::View)) return 8;
                requested = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return 9;
    }
#endif
    for (;;) {
        const auto status = client.Poll(32767, 0);
        if (std::chrono::steady_clock::now() >= deadline || status == Mistwatch::LocalWorldClient::Status::Disconnected)
            return 3;
        if (status == Mistwatch::LocalWorldClient::Status::ChoosingClass && !client.ChooseClass(Mistwatch::RiverbankClass::Ranger))
            return 4;
        if (const auto* view = client.Snapshot(); view && view->entities.size() == 2) {
            const auto own = std::ranges::find_if(view->entities, [&](const auto& entity) {return entity.id.low == client.PlayerId();});
            if (own != view->entities.end() && own->position.offset.x >= .1f) {
                if (client.SelectedClass() != Mistwatch::RiverbankClass::Ranger ||
                    Mistwatch::ReadRiverbankClassFlags(own->flags) != Mistwatch::RiverbankClass::Ranger) return 5;
                std::cout << client.PlayerId() << ' ' << view->serverTick << ' ' << own->position.offset.x << '\n';
                return 0;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
