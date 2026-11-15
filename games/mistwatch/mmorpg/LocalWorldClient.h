#pragma once
#include "RiverbankClass.h"
#ifdef AEVRYND_MISTWATCH_BATTLE
#include "RiverbankBattleWire.h"
#endif
#ifdef AEVRYND_MISTWATCH_PROFILES
#include "RiverbankGuildWire.h"
#include "RiverbankTerritory.h"
#endif
#include <Aevrynd/ReplicationProtocol3D.h>
#include <memory>

namespace Mistwatch {
class LocalWorldClient {
public:
    enum class Status { Connecting, ChoosingClass, Joining, Ready, Disconnected };
    explicit LocalWorldClient(unsigned short port);
    ~LocalWorldClient();
    LocalWorldClient(const LocalWorldClient&) = delete;
    LocalWorldClient& operator=(const LocalWorldClient&) = delete;

    Status Poll(std::int16_t moveX, std::int16_t moveZ);
    bool ChooseClass(RiverbankClass playerClass);
    RiverbankClass SelectedClass() const noexcept;
#ifdef AEVRYND_MISTWATCH_BATTLE
    bool RequestBattleAction(RiverbankAction action);
    const RiverbankBattleWire::Encounter* Battle() const noexcept;
#endif
#ifdef AEVRYND_MISTWATCH_PROFILES
    bool CanUseGuild() const noexcept;
    bool RequestGuild(GuildAction action, std::uint64_t target = 0, std::string_view name = {});
    const RiverbankGuildWire::Response* GuildReply() const noexcept;
    bool RequestTerritory();
    const TerritoryState* Territory() const noexcept;
#endif
    std::uint32_t PlayerId() const noexcept;
    const Aevrynd::ReplicationSnapshot3D* Snapshot() const noexcept;

private:
    struct State;
    std::unique_ptr<State> state_;
};
}
