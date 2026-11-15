#include "LocalWorldClient.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

int main(int argc, char** argv) {
    const bool guildView = argc == 3 && std::string_view(argv[2]) == "--guild-view";
    if (argc != 2 && !guildView) return 1;
    unsigned short port{};
    const std::string_view raw(argv[1]);
    const auto parsed = std::from_chars(raw.data(), raw.data() + raw.size(), port);
    if (!port || parsed.ec != std::errc{} || parsed.ptr != raw.data() + raw.size()) return 2;
    Mistwatch::LocalWorldClient client(port);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
#ifdef AEVRYND_MISTWATCH_PROFILES
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
